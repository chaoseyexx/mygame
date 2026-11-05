#include "WeaponRigConfig.hpp"
#include "CharacterRig.hpp"
#include "game/World.hpp"
#include "engine/Audio.hpp"
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace Mercenary {

static glm::vec3 getTerrainNormal(float x, float z) {
    float hL = World::sampleTerrainHeight(x - 0.2f, z);
    float hR = World::sampleTerrainHeight(x + 0.2f, z);
    float hD = World::sampleTerrainHeight(x, z - 0.2f);
    float hU = World::sampleTerrainHeight(x, z + 0.2f);
    return glm::normalize(glm::vec3(hL - hR, 0.4f, hD - hU));
}

CharacterRig::CharacterRig() {
    m_dynamicMesh = std::make_unique<Mesh>();
}

CharacterRig::~CharacterRig() {}

void CharacterRig::update9D(float dt, const glm::vec3& worldPos, float yaw, const glm::vec3& aimTarget,
                            const glm::vec3& velocity, bool isMoving, bool isSprinting, int stance, bool isFiring,
                            bool isAiming, bool hasWeapon, bool isReloading, float reloadProgress) {
    m_rootPos = worldPos;
    m_yaw = yaw;

    if (m_isDead) {
        m_deathTimer += dt;
        return;
    }
    m_velocity = velocity;
    m_isMoving = isMoving;
    m_isSprinting = isSprinting;
    m_stance = stance;
    m_isAiming = isAiming;
    m_hasWeapon = hasWeapon;
    m_isReloading = isReloading;
    m_reloadProgress = reloadProgress;

    // Hip-fire latch: when firing without holding ADS, raise weapon to shoot, then lower back to patrol low-ready
    if (isFiring && hasWeapon) {
        m_hipFireTimer = 0.40f;
    } else if (m_hipFireTimer > 0.0f) {
        m_hipFireTimer -= dt;
    }

    // Smooth ADS interpolation (snappy tactical transitions between Low-Ready and High-Ready ADS)
    bool activeAim = (isAiming || isFiring || m_hipFireTimer > 0.0f) && hasWeapon && !m_isReloading;
    float targetAimWeight = activeAim ? 1.0f : 0.0f;
    m_aimWeight = glm::mix(m_aimWeight, targetAimWeight, std::clamp(dt * 12.0f, 0.0f, 1.0f));

    // 1. Calculate vertical aim pitch towards 3D mouse target
    glm::vec3 toTarget = aimTarget - (worldPos + glm::vec3(0.0f, 1.48f + m_stanceHeight, 0.0f));
    float horizontalDist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    if (horizontalDist > 0.1f) {
        float elev = std::atan2(toTarget.y, horizontalDist);
        m_aimPitch = std::clamp(elev, -0.30f, 0.40f); // Natural elevation range
    }

    // 2. Continuous Organic Stance Transition (Standing <-> Crouching <-> Prone)
    float targetCrouch = (m_stance == 1) ? 1.0f : 0.0f;
    float targetProne  = (m_stance == 2) ? 1.0f : 0.0f;
    // Critically damped stance interpolation (~0.28s smooth transition without snapping)
    m_crouchTransition = glm::mix(m_crouchTransition, targetCrouch, std::clamp(dt * 7.5f, 0.0f, 1.0f));
    m_proneTransition  = glm::mix(m_proneTransition, targetProne, std::clamp(dt * 6.0f, 0.0f, 1.0f));

    float cCurve = glm::smoothstep(0.0f, 1.0f, m_crouchTransition);
    float pCurve = glm::smoothstep(0.0f, 1.0f, m_proneTransition);

    m_stanceHeight = -0.40f * cCurve - 0.78f * pCurve;

    // Equip & Drop Procedural Animation Timers
    if (m_equipAnimTimer > 0.0f) m_equipAnimTimer -= dt;
    if (m_dropAnimTimer > 0.0f) m_dropAnimTimer -= dt;

    // 3. 9D Locomotion Sector Classification & Lower-Body Decoupling
    float speed = glm::length(velocity);
    glm::vec3 aimForward(std::sin(yaw), 0.0f, std::cos(yaw));
    glm::vec3 aimRight(std::cos(yaw), 0.0f, -std::sin(yaw));

    if (isMoving && speed > 0.05f) {
        float vFwd = glm::dot(velocity, aimForward);
        float vRgt = glm::dot(velocity, aimRight);
        m_relMoveAngle = std::atan2(vRgt, vFwd); // [-pi, pi]
        float deg = glm::degrees(m_relMoveAngle);

        // 9D Locomotion Sectors relative to Upper Body Aim:
        if (std::abs(deg) <= 22.5f) {
            m_locoDir = LocomotionDirection::Forward;
        } else if (deg > 22.5f && deg <= 67.5f) {
            m_locoDir = LocomotionDirection::ForwardRight;
        } else if (deg > 67.5f && deg <= 112.5f) {
            m_locoDir = LocomotionDirection::Right;
        } else if (deg > 112.5f && deg <= 157.5f) {
            m_locoDir = LocomotionDirection::BackwardRight;
        } else if (std::abs(deg) > 157.5f) {
            m_locoDir = LocomotionDirection::Backward;
        } else if (deg >= -157.5f && deg < -112.5f) {
            m_locoDir = LocomotionDirection::BackwardLeft;
        } else if (deg >= -112.5f && deg < -67.5f) {
            m_locoDir = LocomotionDirection::Left;
        } else {
            m_locoDir = LocomotionDirection::ForwardLeft;
        }

        // Lower body orientation:
        if (isSprinting && (m_locoDir == LocomotionDirection::Forward ||
                            m_locoDir == LocomotionDirection::ForwardRight ||
                            m_locoDir == LocomotionDirection::ForwardLeft)) {
            // Forward sprint: lower body hips and feet track movement heading
            float moveHeading = std::atan2(velocity.x, velocity.z);
            float diff = moveHeading - m_lowerBodyYaw;
            while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
            while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
            m_lowerBodyYaw += diff * std::clamp(dt * 12.0f, 0.0f, 1.0f);
        } else {
            // Tactical movement (walking, strafing, ADS):
            // Hips track aim yaw smoothly with tactical quarter-angle
            float diff = yaw - m_lowerBodyYaw;
            while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
            while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();
            m_lowerBodyYaw += diff * std::clamp(dt * 9.0f, 0.0f, 1.0f);
        }
        m_turnInPlaceTimer = 0.0f;
        m_stepTurnPhase = StepTurnPhase::Idle;

        // Walk cycle progression driven strictly by ground displacement
        float horizDistTraveled = speed * dt;
        float strideLength = isSprinting ? 1.68f : (m_stance == 1 ? 0.96f : 1.28f);
        strideLength *= (1.0f - 0.20f * cCurve - 0.45f * pCurve);

        float prevNorm = m_prevWalkPhase;
        m_walkCycle += (horizDistTraveled / strideLength) * (2.0f * glm::pi<float>());
        float currNorm = m_walkCycle / (2.0f * glm::pi<float>());
        m_prevWalkPhase = currNorm;

        // Biomechanically accurate double-support dynamic pelvis dip:
        // Hips are highest when passing directly over the supporting leg (phase pi/2 and 3pi/2),
        // and lowest at double-support foot plant (phase 0 and pi), giving outstretched legs reach clearance.
        float bobFactor = (std::sin(2.0f * m_walkCycle - glm::pi<float>() * 0.5f) - 1.0f) * 0.5f;
        m_pelvisBob = bobFactor * (isSprinting ? 0.038f : 0.024f);

        // Deterministic heel-strike event synchronization:
        // Left foot strikes at phaseNorm 0.0 (and 1.0, 2.0...)
        // Right foot strikes at phaseNorm 0.5 (and 1.5, 2.5...)
        int prevStep = static_cast<int>(std::floor(prevNorm * 2.0f));
        int currStep = static_cast<int>(std::floor(currNorm * 2.0f));
        if (currStep > prevStep) {
            m_footstepEvent = (currStep % 2 == 0) ? 1 : 2;
        }

        // Lateral pelvis sway: natural weight transfer between strides
        if (m_locoDir == LocomotionDirection::Right || m_locoDir == LocomotionDirection::Left) {
            m_pelvisSway = std::sin(m_walkCycle) * 0.035f;
        } else {
            m_pelvisSway = std::sin(m_walkCycle) * (isSprinting ? 0.020f : 0.014f);
        }
    } else {
        // Stationary / Idle (State 0)
        m_locoDir = LocomotionDirection::Stationary;
        m_relMoveAngle = 0.0f;
        m_pelvisSway = glm::mix(m_pelvisSway, 0.0f, std::clamp(dt * 10.0f, 0.0f, 1.0f));

        // Smoothly damp walkCycle to nearest foot-plant phase (0 or pi)
        float cycleMod = std::fmod(m_walkCycle, 2.0f * glm::pi<float>());
        if (cycleMod < 0.0f) cycleMod += 2.0f * glm::pi<float>();
        float targetPhase = (cycleMod < glm::pi<float>() * 0.5f || cycleMod > glm::pi<float>() * 1.5f) ? 0.0f : glm::pi<float>();
        float phaseDiff = targetPhase - cycleMod;
        if (phaseDiff > glm::pi<float>()) phaseDiff -= 2.0f * glm::pi<float>();
        if (phaseDiff < -glm::pi<float>()) phaseDiff += 2.0f * glm::pi<float>();
        m_walkCycle += phaseDiff * std::clamp(dt * 12.0f, 0.0f, 1.0f);
        m_prevWalkPhase = m_walkCycle / (2.0f * glm::pi<float>());

        // Stationary Turn-in-Place & Two-Beat Step-Turn
        float diff = yaw - m_lowerBodyYaw;
        while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
        while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();

        if (std::abs(diff) > glm::radians(105.0f)) {
            // Instant realignment on extreme 180 flick turns
            m_stepTurnPhase = StepTurnPhase::Idle;
            m_lowerBodyYaw += diff * std::clamp(dt * 45.0f, 0.0f, 1.0f);
        } else if (std::abs(diff) > glm::radians(65.0f)) {
            // Rapid catch-up on fast mouse flicks to prevent unnatural spine torsion
            m_stepTurnPhase = StepTurnPhase::Idle;
            m_lowerBodyYaw += diff * std::clamp(dt * 24.0f, 0.0f, 1.0f);
        } else if (m_stepTurnPhase == StepTurnPhase::Idle) {
            if (std::abs(diff) > glm::radians(38.0f)) {
                m_stepTurnPhase = StepTurnPhase::Beat1_LeadFoot;
                m_stepTurnTimer = 0.0f;
                m_stepTurnDir = (diff > 0.0f) ? 1.0f : -1.0f;
                m_stepTurnFoot = (diff > 0.0f) ? 1 : -1;
                m_stepTurnStartLowerYaw = m_lowerBodyYaw;
            } else {
                m_lowerBodyYaw += diff * std::clamp(dt * 6.0f, 0.0f, 1.0f);
            }
        } else if (m_stepTurnPhase == StepTurnPhase::Beat1_LeadFoot) {
            m_stepTurnTimer += dt;
            float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat1, 0.0f, 1.0f);
            m_lowerBodyYaw += diff * 0.55f * std::clamp(dt * 16.0f, 0.0f, 1.0f);
            if (t >= 1.0f) {
                m_stepTurnPhase = StepTurnPhase::Beat2_TrailingFoot;
                m_stepTurnTimer = 0.0f;
                m_stepTurnStartLowerYaw = m_lowerBodyYaw;
                m_footstepEvent = 3; // Stationary pivot step event
            }
        } else if (m_stepTurnPhase == StepTurnPhase::Beat2_TrailingFoot) {
            m_stepTurnTimer += dt;
            float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat2, 0.0f, 1.0f);
            m_lowerBodyYaw += diff * std::clamp(dt * 20.0f, 0.0f, 1.0f);
            if (t >= 1.0f) {
                m_stepTurnPhase = StepTurnPhase::Idle;
                m_stepTurnTimer = 0.0f;
                m_footstepEvent = 3; // Stationary trailing step event
            }
        }

        m_idleTime += dt;
        m_pelvisBob = std::sin(m_idleTime * 1.8f) * 0.006f; // Natural breathing rise & fall
    }

    // Decoupled Torso Twist: angle between aim heading and lower-body heading
    float twist = yaw - m_lowerBodyYaw;
    while (twist > glm::pi<float>()) twist -= 2.0f * glm::pi<float>();
    while (twist < -glm::pi<float>()) twist += 2.0f * glm::pi<float>();
    m_torsoTwist = glm::clamp(twist, -1.15f, 1.15f); // Human spine limit (~65 degrees)

    // 4. Trigger finger dynamics
    float targetTrigger = isFiring ? 1.0f : 0.0f;
    m_rightHand.triggerPull = glm::mix(m_rightHand.triggerPull, targetTrigger, std::clamp(dt * 25.0f, 0.0f, 1.0f));

    // 5. Recoil & Muzzle Flash decay (rapid snappy return for crisp tactile kick)
    if (m_recoilKick > 0.0f) {
        m_recoilKick = glm::mix(m_recoilKick, 0.0f, std::clamp(dt * 22.0f, 0.0f, 1.0f));
        if (m_recoilKick < 0.001f) m_recoilKick = 0.0f;
    }
    if (m_muzzleFlashTimer > 0.0f) {
        m_muzzleFlashTimer -= dt;
    }

    // Procedural weapon inertia & rotational lag
    float diffWepYaw = m_yaw - m_weaponLagYaw;
    while (diffWepYaw > glm::pi<float>()) diffWepYaw -= 2.0f * glm::pi<float>();
    while (diffWepYaw < -glm::pi<float>()) diffWepYaw += 2.0f * glm::pi<float>();
    float wepLagSpeed = (m_aimWeight > 0.5f) ? 32.0f : 18.0f;
    m_weaponLagYaw += diffWepYaw * std::clamp(dt * wepLagSpeed, 0.0f, 1.0f);
    m_weaponLagPitch = glm::mix(m_weaponLagPitch, m_renderPitch, std::clamp(dt * wepLagSpeed, 0.0f, 1.0f));

    // Tactical sprint weapon compression / high-ready carry tuck
    bool isSprintingForward = (isSprinting && speed > 2.5f && m_aimWeight < 0.1f);
    float targetSprintCarry = isSprintingForward ? 0.32f : 0.0f;
    m_sprintCarryAngle = glm::mix(m_sprintCarryAngle, targetSprintCarry, std::clamp(dt * 10.0f, 0.0f, 1.0f));

    updateWeaponTransform();
}

void CharacterRig::updateWeaponTransform() {
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);
    glm::vec3 forwardDir(sy, 0.0f, cy);
    glm::vec3 rightDir(cy, 0.0f, -sy);

    std::string weaponToDraw = (!m_hasWeapon && m_dropAnimTimer > 0.0f) ? m_droppingWeaponName : m_activeWeaponName;
    if (weaponToDraw.empty() || weaponToDraw == "None") {
        weaponToDraw = "M4A1";
    }

    if (m_stance == 2) {
        glm::vec3 chestPos = m_rootPos + glm::vec3(0.0f, 0.22f, 0.0f) + forwardDir * 0.15f;
        m_renderPitch = m_aimPitch;
        m_rifleRoot = chestPos + rightDir * 0.08f + forwardDir * 0.24f - glm::vec3(0.0f, 0.02f, 0.0f);
    } else {
        float cCurve = glm::smoothstep(0.0f, 1.0f, m_crouchTransition);
        float sprintLean = (m_isSprinting && m_isMoving) ? 0.30f : ((m_isMoving) ? 0.11f : 0.0f);
        float leanZ = sprintLean + 0.14f * cCurve + m_woundTorsoHunch * 0.25f;

        // Accurate forward direction lean (prevents muzzle detachment when facing arbitrary angles)
        glm::vec3 pelvisPos = m_rootPos + rightDir * (m_pelvisSway + m_coverLeanAngle * 0.20f) + forwardDir * (leanZ * 0.45f) + glm::vec3(0.0f, 0.81f + m_stanceHeight + m_pelvisBob, 0.0f);
        glm::vec3 spine1 = pelvisPos + rightDir * (m_coverLeanAngle * 0.12f) + forwardDir * (leanZ * 0.25f) + glm::vec3(0.0f, 0.18f, 0.0f);
        glm::vec3 chestPos = spine1 + rightDir * (m_coverLeanAngle * 0.18f) + forwardDir * (leanZ * 0.35f) + glm::vec3(0.0f, 0.18f, 0.0f);

        // Tactical Low Cover Peeking & Weapon Mounting
        float coverChestElevation = 0.0f;
        if (m_inCover && m_isLowCover && m_coverAimWeight > 0.001f) {
            float currentCrouchChestY = m_rootPos.y + 0.81f + m_stanceHeight + 0.36f;
            float targetChestY = m_rootPos.y + m_coverHeight + 0.12f;
            coverChestElevation = std::max(0.0f, targetChestY - currentCrouchChestY) * m_coverAimWeight;
        }
        chestPos.y += coverChestElevation;

        const auto& gripCfg = WeaponRigManager::get().getConfig(weaponToDraw);
        m_renderPitch = glm::mix(gripCfg.lowReadyPitch, -m_aimPitch, m_aimWeight) - m_recoilKick * 2.8f;

        // Mount weapon directly on cover rim when aiming in low cover
        if (m_inCover && m_isLowCover && m_coverAimWeight > 0.001f) {
            m_renderPitch = glm::mix(m_renderPitch, -m_aimPitch * 0.85f, m_coverAimWeight);
        }

        float cp = std::cos(m_renderPitch);
        float sp = -std::sin(m_renderPitch);
        glm::vec3 aimForward = forwardDir * cp + glm::vec3(0.0f, sp, 0.0f);

        float rootY = glm::mix(gripCfg.rootOffset.y, gripCfg.rootOffsetADS.y, m_aimWeight);
        float rootRight = glm::mix(gripCfg.rootOffset.x, gripCfg.rootOffsetADS.x, m_aimWeight);
        if (!m_shoulderRight) rootRight = -rootRight;
        float rootForward = glm::mix(gripCfg.rootOffset.z, gripCfg.rootOffsetADS.z, m_aimWeight) - m_recoilKick;

        m_rifleRoot = chestPos + rightDir * rootRight + glm::vec3(0.0f, rootY, 0.0f) + aimForward * rootForward;

        if (m_inCover && m_isLowCover && m_coverAimWeight > 0.001f) {
            float desiredRifleY = m_rootPos.y + m_coverHeight + 0.04f;
            m_rifleRoot.y = glm::mix(m_rifleRoot.y, desiredRifleY, m_coverAimWeight);
            m_rifleRoot += forwardDir * (0.18f * m_coverAimWeight);
        }
    }

    glm::mat4 wModel = glm::translate(glm::mat4(1.0f), m_rifleRoot);
    wModel = glm::rotate(wModel, m_weaponLagYaw, glm::vec3(0.0f, 1.0f, 0.0f));
    float finalPitch = m_weaponLagPitch + m_sprintCarryAngle;
    if (std::abs(finalPitch) > 0.001f) {
        wModel = glm::rotate(wModel, finalPitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    if (m_sprintCarryAngle > 0.01f) {
        wModel = glm::rotate(wModel, -m_sprintCarryAngle * 0.45f, glm::vec3(0.0f, 0.0f, 1.0f));
    }

    glm::vec3 localMuzzle(0.0f, 0.02f, 0.55f);
    glm::vec3 localCasing(0.03f, 0.035f, 0.08f);

    if (weaponToDraw.find("AK") != std::string::npos || weaponToDraw.find("74") != std::string::npos || weaponToDraw.find("Kalash") != std::string::npos) {
        localMuzzle = glm::vec3(0.0f, 0.032f, 0.6425f);
        localCasing = glm::vec3(0.035f, 0.040f, 0.10f);
    } else if (weaponToDraw.find("Mosin") != std::string::npos || weaponToDraw.find("Nagant") != std::string::npos || weaponToDraw.find("Sniper") != std::string::npos) {
        localMuzzle = glm::vec3(0.0f, 0.022f, 0.86f);
        localCasing = glm::vec3(0.040f, 0.032f, -0.04f);
    } else if (weaponToDraw.find("M9") != std::string::npos || weaponToDraw.find("Beretta") != std::string::npos || weaponToDraw.find("Pistol") != std::string::npos) {
        localMuzzle = glm::vec3(0.0f, 0.035f, 0.135f);
        localCasing = glm::vec3(0.02f, 0.035f, 0.03f);
    }

    m_muzzlePos = glm::vec3(wModel * glm::vec4(localMuzzle, 1.0f));
    m_casingPos = glm::vec3(wModel * glm::vec4(localCasing, 1.0f));
}

void CharacterRig::render(Renderer& renderer, const CharacterCustomization& custom) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    float cCurve = glm::smoothstep(0.0f, 1.0f, m_crouchTransition);
    float pCurve = glm::smoothstep(0.0f, 1.0f, m_proneTransition);

    // --- 1. LOCAL ORIENTATION BASES (Decoupled Lower vs Upper Body) ---
    // Lower body basis (Pelvis, Legs, Boots)
    float lcy = std::cos(m_lowerBodyYaw);
    float lsy = std::sin(m_lowerBodyYaw);
    glm::vec3 lowerForward(lsy, 0.0f, lcy);
    glm::vec3 lowerRight(lcy, 0.0f, -lsy);

    auto lowerToWorld = [&](const glm::vec3& l) -> glm::vec3 {
        return m_rootPos + lowerRight * l.x + glm::vec3(0, l.y, 0) + lowerForward * l.z;
    };

    // Upper body basis (Aim heading, Spine, Arms, Head)
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);
    glm::vec3 forwardDir(sy, 0.0f, cy);
    glm::vec3 rightDir(cy, 0.0f, -sy);

    auto localToWorld = [&](const glm::vec3& l) -> glm::vec3 {
        return m_rootPos + rightDir * l.x + glm::vec3(0, l.y, 0) + forwardDir * l.z;
    };

    glm::vec3 leftShoulder(0.0f);
    glm::vec3 rightShoulder(0.0f);
    glm::vec3 leftElbowPos(0.0f);
    glm::vec3 rightElbowPos(0.0f);
    glm::vec3 rifleRoot(0.0f);
    glm::vec3 aimForward = forwardDir;
    float renderPitch = m_aimPitch;
    bool isDroppingWeapon = (m_dropAnimTimer > 0.0f);
    bool shouldDrawWeapon = m_hasWeapon || isDroppingWeapon;
    std::string weaponToDraw = (!m_hasWeapon && isDroppingWeapon) ? m_droppingWeaponName : m_activeWeaponName;

    if (m_isDead) {
        float t = std::clamp(m_deathTimer / 0.95f, 0.0f, 1.0f);
        float fallProg = t * t * (3.0f - 2.0f * t);
        float groundBounce = (t > 0.60f && t < 0.90f) ? std::sin((t - 0.60f) / 0.30f * 3.14159f) * 0.018f : 0.0f;

        float groundY = (m_terrainHeight != 0.0f) ? m_terrainHeight : m_rootPos.y;
        float basePelvisStanding = groundY + 0.94f;
        float finalPelvisY = groundY + 0.15f;

        glm::vec3 pelvisPos;
        glm::vec3 chestPos;
        glm::vec3 headCenter;
        glm::vec3 leftHipPos, rightHipPos;
        glm::vec3 leftFootPos, rightFootPos;
        glm::vec3 leftKneePole, rightKneePole;
        glm::mat3 chestRot3(1.0f);

        if (m_deathType == DeathType::CollapseBackward || m_deathType == DeathType::BlastKnockdown) {
            float slideDist = (m_deathType == DeathType::BlastKnockdown) ? 0.45f : 0.26f;
            pelvisPos = m_rootPos + forwardDir * (slideDist * fallProg) + glm::vec3(0, glm::mix(basePelvisStanding, finalPelvisY, fallProg) + groundBounce, 0);

            float chestTilt = glm::mix(0.0f, -1.36f, fallProg);
            chestRot3 = glm::mat3(glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1.0f), chestTilt, glm::vec3(1, 0, 0)));

            chestPos = pelvisPos - forwardDir * (0.34f * fallProg) + glm::vec3(0, glm::mix(0.36f, 0.09f, fallProg), 0);
            headCenter = chestPos - forwardDir * (0.24f * fallProg) + glm::vec3(0, glm::mix(0.18f, 0.05f, fallProg), 0);

            leftHipPos  = pelvisPos - rightDir * 0.13f;
            rightHipPos = pelvisPos + rightDir * 0.13f;
            leftFootPos  = pelvisPos - rightDir * 0.22f + forwardDir * (0.68f * fallProg) + glm::vec3(0, 0.05f, 0);
            rightFootPos = pelvisPos + rightDir * 0.24f + forwardDir * (0.72f * fallProg) + glm::vec3(0, 0.05f, 0);
            leftKneePole  = pelvisPos - rightDir * 0.26f + forwardDir * (0.32f * fallProg) + glm::vec3(0, 0.16f * (1.0f - fallProg) + 0.08f, 0);
            rightKneePole = pelvisPos + rightDir * 0.28f + forwardDir * (0.34f * fallProg) + glm::vec3(0, 0.16f * (1.0f - fallProg) + 0.08f, 0);

        } else if (m_deathType == DeathType::CollapseCrumple) {
            pelvisPos = m_rootPos + rightDir * (0.16f * fallProg) + glm::vec3(0, glm::mix(basePelvisStanding, finalPelvisY - 0.02f, fallProg) + groundBounce, 0);

            float rollAngle = glm::mix(0.0f, 1.15f, fallProg);
            float pitchAngle = glm::mix(0.0f, 0.40f, fallProg);
            chestRot3 = glm::mat3(glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1.0f), rollAngle, glm::vec3(0, 0, 1)) * glm::rotate(glm::mat4(1.0f), pitchAngle, glm::vec3(1, 0, 0)));

            chestPos = pelvisPos + rightDir * (0.22f * fallProg) + forwardDir * (0.14f * fallProg) + glm::vec3(0, glm::mix(0.36f, 0.12f, fallProg), 0);
            headCenter = chestPos + rightDir * (0.16f * fallProg) + forwardDir * (0.18f * fallProg) + glm::vec3(0, glm::mix(0.18f, 0.05f, fallProg), 0);

            leftHipPos  = pelvisPos - rightDir * 0.11f;
            rightHipPos = pelvisPos + rightDir * 0.11f;
            leftFootPos  = pelvisPos - rightDir * 0.08f - forwardDir * (0.42f * fallProg) + glm::vec3(0, 0.08f, 0);
            rightFootPos = pelvisPos + rightDir * 0.32f - forwardDir * (0.28f * fallProg) + glm::vec3(0, 0.04f, 0);
            leftKneePole  = pelvisPos - rightDir * 0.22f - forwardDir * (0.20f * fallProg) + glm::vec3(0, 0.18f, 0);
            rightKneePole = pelvisPos + rightDir * 0.38f - forwardDir * (0.12f * fallProg) + glm::vec3(0, 0.09f, 0);

        } else {
            pelvisPos = m_rootPos + forwardDir * (0.16f * fallProg) + glm::vec3(0, glm::mix(basePelvisStanding, finalPelvisY, fallProg) + groundBounce, 0);

            float pitchAngle = glm::mix(0.0f, 1.42f, fallProg);
            chestRot3 = glm::mat3(glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1.0f), pitchAngle, glm::vec3(1, 0, 0)));

            chestPos = pelvisPos + forwardDir * (0.36f * fallProg) + glm::vec3(0, glm::mix(0.36f, 0.08f, fallProg), 0);
            headCenter = chestPos + forwardDir * (0.26f * fallProg) + glm::vec3(0, glm::mix(0.18f, 0.05f, fallProg), 0);

            leftHipPos  = pelvisPos - rightDir * 0.13f;
            rightHipPos = pelvisPos + rightDir * 0.13f;
            leftFootPos  = pelvisPos - rightDir * 0.16f - forwardDir * (0.76f * fallProg) + glm::vec3(0, 0.06f, 0);
            rightFootPos = pelvisPos + rightDir * 0.16f - forwardDir * (0.80f * fallProg) + glm::vec3(0, 0.06f, 0);
            leftKneePole  = pelvisPos - rightDir * 0.28f - forwardDir * (0.38f * fallProg) + glm::vec3(0, 0.09f, 0);
            rightKneePole = pelvisPos + rightDir * 0.28f - forwardDir * (0.38f * fallProg) + glm::vec3(0, 0.09f, 0);
        }

        glm::vec3 leftKneePos, rightKneePos;
        glm::quat uRot, lRot;
        InverseKinematics::solveTwoBoneIK(leftHipPos, leftFootPos, leftKneePole, 0.40f, 0.38f, leftKneePos, uRot, lRot);
        InverseKinematics::solveTwoBoneIK(rightHipPos, rightFootPos, rightKneePole, 0.40f, 0.38f, rightKneePos, uRot, lRot);

        glm::vec3 bindChest = m_skinnedModel.getBindJoint(BONE_SPINE_CHEST);
        glm::vec3 bindRShoulder = m_skinnedModel.getBindJoint(BONE_R_UPPER_ARM);
        glm::vec3 bindLShoulder = m_skinnedModel.getBindJoint(BONE_L_UPPER_ARM);
        rightShoulder = chestPos + chestRot3 * (bindRShoulder - bindChest);
        leftShoulder  = chestPos + chestRot3 * (bindLShoulder - bindChest);

        glm::vec3 rightHandTarget, leftHandTarget;
        glm::vec3 rightElbowPole, leftElbowPole;
        glm::vec3 rifleUp(0.0f, 1.0f, 0.0f);

        bool keepWeaponGrip = m_deathGrip && !(m_dismemberMask & DISMEMBER_R_ARM) && shouldDrawWeapon;
        if (keepWeaponGrip) {
            float cp = std::cos(m_yaw);
            float sp = std::sin(m_yaw);
            glm::vec3 flatFwd(sp, 0.0f, cp);
            glm::vec3 flatRgt(cp, 0.0f, -sp);

            if (m_deathType == DeathType::CollapseBackward || m_deathType == DeathType::BlastKnockdown) {
                rifleRoot = chestPos + flatRgt * 0.06f + glm::vec3(0, 0.12f, 0) + flatFwd * 0.10f;
                renderPitch = 0.35f;
                aimForward = flatFwd * std::cos(renderPitch) + glm::vec3(0, -std::sin(renderPitch), 0);
                rifleUp = glm::normalize(glm::cross(aimForward, flatRgt));
            } else {
                rifleRoot = chestPos + flatRgt * 0.08f + glm::vec3(0, 0.05f, 0) + flatFwd * 0.15f;
                renderPitch = 0.10f;
                aimForward = flatFwd * std::cos(renderPitch) + glm::vec3(0, -std::sin(renderPitch), 0);
                rifleUp = glm::vec3(0, 1, 0);
            }

            rightHandTarget = rifleRoot - aimForward * 0.05f - rifleUp * 0.06f;
            leftHandTarget  = rifleRoot + aimForward * 0.18f - rifleUp * 0.03f - flatRgt * 0.02f;
            rightElbowPole  = rightShoulder + flatRgt * 0.22f - glm::vec3(0, 0.08f, 0);
            leftElbowPole   = leftShoulder  - flatRgt * 0.22f - glm::vec3(0, 0.08f, 0);
        } else {
            rightHandTarget = rightShoulder + rightDir * 0.25f - glm::vec3(0, 0.18f, 0) + forwardDir * 0.10f;
            leftHandTarget  = leftShoulder  - rightDir * 0.25f - glm::vec3(0, 0.18f, 0) + forwardDir * 0.10f;
            rightElbowPole  = rightShoulder + rightDir * 0.20f + glm::vec3(0, 0.06f, 0);
            leftElbowPole   = leftShoulder  - rightDir * 0.20f + glm::vec3(0, 0.06f, 0);
        }

        InverseKinematics::solveTwoBoneIK(rightShoulder, rightHandTarget, rightElbowPole, 0.30f, 0.28f, rightElbowPos, uRot, lRot);
        InverseKinematics::solveTwoBoneIK(leftShoulder, leftHandTarget, leftElbowPole, 0.30f, 0.28f, leftElbowPos, uRot, lRot);

        m_pose.bonePositions[BONE_PELVIS] = pelvisPos;
        m_pose.boneRotations[BONE_PELVIS] = chestRot3;

        m_pose.bonePositions[BONE_SPINE_CHEST] = chestPos;
        m_pose.boneRotations[BONE_SPINE_CHEST] = chestRot3;

        m_pose.bonePositions[BONE_HEAD] = headCenter;
        m_pose.boneRotations[BONE_HEAD] = chestRot3;

        glm::vec3 bindLCalf = m_skinnedModel.getBindJoint(BONE_L_CALF);
        glm::vec3 bindLThigh = m_skinnedModel.getBindJoint(BONE_L_THIGH);
        glm::vec3 bindLFoot = m_skinnedModel.getBindJoint(BONE_L_FOOT);
        glm::vec3 bindRCalf = m_skinnedModel.getBindJoint(BONE_R_CALF);
        glm::vec3 bindRThigh = m_skinnedModel.getBindJoint(BONE_R_THIGH);
        glm::vec3 bindRFoot = m_skinnedModel.getBindJoint(BONE_R_FOOT);

        m_pose.bonePositions[BONE_L_THIGH] = leftHipPos;
        m_pose.boneRotations[BONE_L_THIGH] = SkinnedSoldier::computeLimbRotation(bindLCalf - bindLThigh, leftKneePos - leftHipPos, chestRot3);
        m_pose.bonePositions[BONE_L_CALF] = leftKneePos;
        m_pose.boneRotations[BONE_L_CALF] = SkinnedSoldier::computeLimbRotation(bindLFoot - bindLCalf, leftFootPos - leftKneePos, m_pose.boneRotations[BONE_L_THIGH]);
        m_pose.bonePositions[BONE_L_FOOT] = leftFootPos;
        m_pose.boneRotations[BONE_L_FOOT] = chestRot3;

        m_pose.bonePositions[BONE_R_THIGH] = rightHipPos;
        m_pose.boneRotations[BONE_R_THIGH] = SkinnedSoldier::computeLimbRotation(bindRCalf - bindRThigh, rightKneePos - rightHipPos, chestRot3);
        m_pose.bonePositions[BONE_R_CALF] = rightKneePos;
        m_pose.boneRotations[BONE_R_CALF] = SkinnedSoldier::computeLimbRotation(bindRFoot - bindRCalf, rightFootPos - rightKneePos, m_pose.boneRotations[BONE_R_THIGH]);
        m_pose.bonePositions[BONE_R_FOOT] = rightFootPos;
        m_pose.boneRotations[BONE_R_FOOT] = chestRot3;

        glm::vec3 bindLFore = m_skinnedModel.getBindJoint(BONE_L_FOREARM);
        glm::vec3 bindLHand = m_skinnedModel.getBindJoint(BONE_L_HAND);
        glm::vec3 bindRFore = m_skinnedModel.getBindJoint(BONE_R_FOREARM);
        glm::vec3 bindRHand = m_skinnedModel.getBindJoint(BONE_R_HAND);

        m_pose.bonePositions[BONE_L_UPPER_ARM] = leftShoulder;
        m_pose.boneRotations[BONE_L_UPPER_ARM] = SkinnedSoldier::computeLimbRotation(bindLFore - bindLShoulder, leftElbowPos - leftShoulder, chestRot3);
        m_pose.bonePositions[BONE_L_FOREARM] = leftElbowPos;
        m_pose.boneRotations[BONE_L_FOREARM] = SkinnedSoldier::computeLimbRotation(bindLHand - bindLFore, leftHandTarget - leftElbowPos, m_pose.boneRotations[BONE_L_UPPER_ARM]);
        m_pose.bonePositions[BONE_L_HAND] = leftHandTarget;
        m_pose.boneRotations[BONE_L_HAND] = chestRot3;

        m_pose.bonePositions[BONE_R_UPPER_ARM] = rightShoulder;
        m_pose.boneRotations[BONE_R_UPPER_ARM] = SkinnedSoldier::computeLimbRotation(bindRFore - bindRShoulder, rightElbowPos - rightShoulder, chestRot3);
        m_pose.bonePositions[BONE_R_FOREARM] = rightElbowPos;
        m_pose.boneRotations[BONE_R_FOREARM] = SkinnedSoldier::computeLimbRotation(bindRHand - bindRFore, rightHandTarget - rightElbowPos, m_pose.boneRotations[BONE_R_UPPER_ARM]);
        m_pose.bonePositions[BONE_R_HAND] = rightHandTarget;
        m_pose.boneRotations[BONE_R_HAND] = chestRot3;
        m_pose.bonePositions[BONE_R_TRIGGER_FINGER] = rightHandTarget;
        m_pose.boneRotations[BONE_R_TRIGGER_FINGER] = chestRot3;

    } else if (m_stance == 2) {
        // =========================================================================
        // FULL HORIZONTAL GROUND-BASED PRONE BODY & COMMANDO CRAWL
        // =========================================================================
        float crawlPhase = m_walkCycle;
        float crawlBob = (m_isMoving) ? std::sin(crawlPhase * 2.0f) * 0.015f : 0.0f;
        float crawlLateral = (m_isMoving) ? std::sin(crawlPhase) * 0.025f : 0.0f;

        // 1. Pelvis resting low on the ground plane (0.14m)
        glm::vec3 pelvisPos = m_rootPos + rightDir * crawlLateral + glm::vec3(0.0f, 0.14f + crawlBob, 0.0f) - forwardDir * 0.12f;

        // 2. Chest extended forward along ground, propped on elbows (0.22m)
        glm::vec3 chestPos = pelvisPos + forwardDir * 0.38f + glm::vec3(0.0f, 0.08f, 0.0f);

        // 3. Head raised above chest looking down sights toward aimTarget (0.28m)
        glm::vec3 headCenter = chestPos + forwardDir * 0.28f + glm::vec3(0.0f, 0.06f, 0.0f);

        // 4. Commando Crawl Leg Dynamics
        float leftCrawlZ = 0.0f, rightCrawlZ = 0.0f;
        float leftKneeOut = 0.0f, rightKneeOut = 0.0f;
        float leftLift = 0.0f, rightLift = 0.0f;

        if (m_isMoving) {
            float sinP = std::sin(crawlPhase);
            leftCrawlZ   =  sinP * 0.18f;
            rightCrawlZ  = -sinP * 0.18f;
            leftKneeOut  = std::max(0.0f,  sinP) * 0.14f;
            rightKneeOut = std::max(0.0f, -sinP) * 0.14f;
            leftLift     = std::max(0.0f,  sinP * 0.035f);
            rightLift    = std::max(0.0f, -sinP * 0.035f);
        }

        glm::vec3 leftHipPos  = pelvisPos - rightDir * 0.13f;
        glm::vec3 rightHipPos = pelvisPos + rightDir * 0.13f;

        glm::vec3 leftFootPos  = pelvisPos - rightDir * (0.18f + leftKneeOut * 0.6f) - forwardDir * (0.80f - leftCrawlZ) + glm::vec3(0.0f, 0.06f + leftLift, 0.0f);
        glm::vec3 rightFootPos = pelvisPos + rightDir * (0.18f + rightKneeOut * 0.6f) - forwardDir * (0.80f - rightCrawlZ) + glm::vec3(0.0f, 0.06f + rightLift, 0.0f);

        glm::vec3 leftKneePole  = pelvisPos - rightDir * (0.26f + leftKneeOut) - forwardDir * (0.40f - leftCrawlZ * 0.5f) + glm::vec3(0.0f, 0.09f, 0.0f);
        glm::vec3 rightKneePole = pelvisPos + rightDir * (0.26f + rightKneeOut) - forwardDir * (0.40f - rightCrawlZ * 0.5f) + glm::vec3(0.0f, 0.09f, 0.0f);

        glm::vec3 leftKneePos, rightKneePos;
        glm::quat uRot, lRot;
        InverseKinematics::solveTwoBoneIK(leftHipPos, leftFootPos, leftKneePole, 0.40f, 0.38f, leftKneePos, uRot, lRot);
        InverseKinematics::solveTwoBoneIK(rightHipPos, rightFootPos, rightKneePole, 0.40f, 0.38f, rightKneePos, uRot, lRot);

        // 5. Prone Arms & Rifle Ground Support
        rightShoulder = chestPos + rightDir * 0.22f + forwardDir * 0.04f;
        leftShoulder  = chestPos - rightDir * 0.22f + forwardDir * 0.04f;

        aimForward = forwardDir;
        glm::vec3 rifleUp(0.0f, 1.0f, 0.0f);
        renderPitch = m_aimPitch;

        rightElbowPos = chestPos + rightDir * 0.22f + forwardDir * 0.14f - glm::vec3(0.0f, 0.14f, 0.0f);
        leftElbowPos  = chestPos - rightDir * 0.22f + forwardDir * 0.18f - glm::vec3(0.0f, 0.14f, 0.0f);

        rifleRoot = chestPos + rightDir * 0.08f + forwardDir * 0.24f - glm::vec3(0.0f, 0.02f, 0.0f);
        glm::vec3 rightHandTarget = rifleRoot - aimForward * 0.08f - rifleUp * 0.06f;
        glm::vec3 leftHandTarget  = rifleRoot + aimForward * 0.24f - rifleUp * 0.04f;

        if (m_isReloading) {
            float p = m_reloadProgress;
            glm::vec3 magPouch = chestPos - rightDir * 0.12f + forwardDir * 0.06f - glm::vec3(0.0f, 0.08f, 0.0f);
            glm::vec3 magWell = rifleRoot + aimForward * 0.08f - rifleUp * 0.06f;
            glm::vec3 handguardPos = rifleRoot + aimForward * 0.24f - rifleUp * 0.04f;
            if (p < 0.35f) {
                leftHandTarget = glm::mix(handguardPos, magPouch, p / 0.35f);
            } else if (p < 0.70f) {
                leftHandTarget = glm::mix(magPouch, magWell, (p - 0.35f) / 0.35f);
            } else {
                leftHandTarget = glm::mix(magWell, handguardPos, (p - 0.70f) / 0.30f);
            }
        }

        // 6. Assemble Prone Skeleton Pose
        m_pose.bonePositions[BONE_PELVIS] = pelvisPos;
        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
        rotY = glm::rotate(rotY, 1.46f, glm::vec3(1, 0, 0));
        m_pose.boneRotations[BONE_PELVIS] = glm::mat3(rotY);

        m_pose.bonePositions[BONE_SPINE_CHEST] = chestPos;
        glm::mat4 chestRot = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
        chestRot = glm::rotate(chestRot, 1.38f + renderPitch * 0.15f, glm::vec3(1, 0, 0));
        m_pose.boneRotations[BONE_SPINE_CHEST] = glm::mat3(chestRot);

        // Head anchored directly to top of rotated spine
        glm::vec3 bindChest = m_skinnedModel.getBindJoint(BONE_SPINE_CHEST);
        glm::vec3 bindHead  = m_skinnedModel.getBindJoint(BONE_HEAD);
        glm::vec3 headJointOffset = bindHead - bindChest;
        m_pose.bonePositions[BONE_HEAD] = chestPos + glm::mat3(chestRot) * headJointOffset;
        glm::mat4 headRot = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
        headRot = glm::rotate(headRot, renderPitch * 0.45f, glm::vec3(1, 0, 0));
        m_pose.boneRotations[BONE_HEAD] = glm::mat3(headRot);

        glm::mat3 lowerRot = glm::mat3(rotY);
        glm::mat3 chestRot3 = glm::mat3(chestRot);

        glm::vec3 bindThighDir(0.0f, -1.0f, 0.0f);
        glm::vec3 bindCalfDir(0.0f, -1.0f, 0.0f);
        m_pose.bonePositions[BONE_L_THIGH] = leftHipPos;
        glm::mat3 pLThighRot = SkinnedSoldier::computeLimbRotation(bindThighDir, leftKneePos - leftHipPos, lowerRot);
        m_pose.boneRotations[BONE_L_THIGH] = pLThighRot;
        m_pose.bonePositions[BONE_L_CALF] = leftKneePos;
        m_pose.boneRotations[BONE_L_CALF] = SkinnedSoldier::computeLimbRotation(bindCalfDir, leftFootPos - leftKneePos, pLThighRot);
        m_pose.bonePositions[BONE_L_FOOT] = leftFootPos;
        m_pose.boneRotations[BONE_L_FOOT] = glm::mat3(rotY);

        m_pose.bonePositions[BONE_R_THIGH] = rightHipPos;
        glm::mat3 pRThighRot = SkinnedSoldier::computeLimbRotation(bindThighDir, rightKneePos - rightHipPos, lowerRot);
        m_pose.boneRotations[BONE_R_THIGH] = pRThighRot;
        m_pose.bonePositions[BONE_R_CALF] = rightKneePos;
        m_pose.boneRotations[BONE_R_CALF] = SkinnedSoldier::computeLimbRotation(bindCalfDir, rightFootPos - rightKneePos, pRThighRot);
        m_pose.bonePositions[BONE_R_FOOT] = rightFootPos;
        m_pose.boneRotations[BONE_R_FOOT] = glm::mat3(rotY);

        glm::vec3 bindLArmDir = glm::normalize(glm::vec3(-0.08f, -0.30f, 0.0f));
        glm::vec3 bindLForeDir = glm::normalize(glm::vec3(-0.02f, -0.30f, 0.0f));
        glm::vec3 bindRArmDir = glm::normalize(glm::vec3(0.08f, -0.30f, 0.0f));
        glm::vec3 bindRForeDir = glm::normalize(glm::vec3(0.02f, -0.30f, 0.0f));

        m_pose.bonePositions[BONE_L_UPPER_ARM] = leftShoulder;
        glm::mat3 pLArmRot = SkinnedSoldier::computeLimbRotation(bindLArmDir, leftElbowPos - leftShoulder, chestRot3);
        m_pose.boneRotations[BONE_L_UPPER_ARM] = pLArmRot;
        m_pose.bonePositions[BONE_L_FOREARM] = leftElbowPos;
        m_pose.boneRotations[BONE_L_FOREARM] = SkinnedSoldier::computeLimbRotation(bindLForeDir, leftHandTarget - leftElbowPos, pLArmRot);
        m_pose.bonePositions[BONE_L_HAND] = leftHandTarget;
        m_pose.boneRotations[BONE_L_HAND] = glm::mat3(rotY);

        m_pose.bonePositions[BONE_R_UPPER_ARM] = rightShoulder;
        glm::mat3 pRArmRot = SkinnedSoldier::computeLimbRotation(bindRArmDir, rightElbowPos - rightShoulder, chestRot3);
        m_pose.boneRotations[BONE_R_UPPER_ARM] = pRArmRot;
        m_pose.bonePositions[BONE_R_FOREARM] = rightElbowPos;
        m_pose.boneRotations[BONE_R_FOREARM] = SkinnedSoldier::computeLimbRotation(bindRForeDir, rightHandTarget - rightElbowPos, pRArmRot);
        m_pose.bonePositions[BONE_R_HAND] = rightHandTarget;
        m_pose.boneRotations[BONE_R_HAND] = glm::mat3(rotY);

        m_pose.bonePositions[BONE_R_TRIGGER_FINGER] = rightHandTarget;
        m_pose.boneRotations[BONE_R_TRIGGER_FINGER] = glm::mat3(rotY);

        // Muzzle updated by exact weapon transform
    } else {
        // =========================================================================
        // STANDING / CROUCHING / ATHLETIC SPRINT LOCOMOTION
        // =========================================================================
        // Tactical Low Cover Peeking & Weapon Mounting
        float coverChestElevation = 0.0f;
        if (m_inCover && m_isLowCover && m_coverAimWeight > 0.001f) {
            // Smoothly elevate torso and arms over the obstacle rim when aiming
            float targetChestY = m_coverHeight + 0.12f;
            float currentCrouchChestY = 0.75f;
            coverChestElevation = std::max(0.0f, targetChestY - currentCrouchChestY) * m_coverAimWeight;
        }

        // Dynamic Athletic Forward Lean (natural human posture in running/sprint)
        float sprintLean = (m_isSprinting && m_isMoving) ? 0.30f : ((m_isMoving) ? 0.11f : 0.0f);
        float leanZ = sprintLean + 0.14f * cCurve;
        leanZ += m_woundTorsoHunch * 0.25f;

        // Asymmetric limp dip when wounded in lower extremities
        float limpPelvisDip = (m_isMoving && m_woundLimpFactor > 0.05f) ? -std::abs(std::sin(m_walkCycle)) * 0.065f * m_woundLimpFactor : 0.0f;

        // --- 2. PELVIS & SACRUM ---
        glm::vec3 pelvisPos = lowerToWorld(glm::vec3(m_pelvisSway + m_coverLeanAngle * 0.18f, 0.81f + m_stanceHeight + m_pelvisBob + limpPelvisDip + coverChestElevation * 0.45f, leanZ * 0.45f));

        // --- 3. BIOMECHANICALLY REALISTIC TWO-BONE LEG IK LOCOMOTION ---
        // Calibrated to maintain a natural knee bend within the 0.84m leg reach envelope
        float stride = m_isSprinting ? 0.42f : (m_isMoving ? 0.32f : 0.20f);
        float leftStrideZ = 0.0f, rightStrideZ = 0.0f;
        float leftStrideX = 0.0f, rightStrideX = 0.0f;
        float leftLift = 0.0f, rightLift = 0.0f;

        // Resolve Movement Direction Components in Lower-Body Space
        float moveFwd = 0.0f;
        float moveRgt = 0.0f;
        if (m_isMoving && glm::length(m_velocity) > 0.05f) {
            moveFwd = glm::dot(m_velocity, lowerForward);
            moveRgt = glm::dot(m_velocity, lowerRight);
            float mag = std::sqrt(moveFwd * moveFwd + moveRgt * moveRgt);
            if (mag > 0.05f) {
                moveFwd /= mag;
                moveRgt /= mag;
            }
        } else if (m_isMoving) {
            moveFwd = 1.0f;
            moveRgt = 0.0f;
        }

        if (m_isMoving) {
            float phaseNorm = m_walkCycle / (2.0f * glm::pi<float>());
            float uL = std::fmod(phaseNorm, 1.0f);
            if (uL < 0.0f) uL += 1.0f;
            float uR = std::fmod(phaseNorm + 0.5f, 1.0f);
            if (uR < 0.0f) uR += 1.0f;

            float maxLift = (m_isSprinting ? 0.11f : (m_isMoving ? 0.075f : 0.040f));

            // Smooth C-infinity harmonic stride trajectory: eliminates velocity discontinuities & knee hitching
            float stanceZ_L = std::cos(uL * 2.0f * glm::pi<float>()) * stride;
            leftStrideZ = stanceZ_L * moveFwd;
            leftStrideX = stanceZ_L * 0.38f * moveRgt;
            leftLift = (uL >= 0.5f) ? (std::sin((uL - 0.5f) * 2.0f * glm::pi<float>()) * maxLift) : 0.0f;

            float stanceZ_R = std::cos(uR * 2.0f * glm::pi<float>()) * stride;
            rightStrideZ = stanceZ_R * moveFwd;
            rightStrideX = stanceZ_R * 0.38f * moveRgt;
            rightLift = (uR >= 0.5f) ? (std::sin((uR - 0.5f) * 2.0f * glm::pi<float>()) * maxLift) : 0.0f;

            if (m_woundLimpFactor > 0.05f) {
                leftStrideZ *= (1.0f - m_woundLimpFactor * 0.40f);
                leftStrideX *= (1.0f - m_woundLimpFactor * 0.40f);
                leftLift    *= (1.0f - m_woundLimpFactor * 0.50f);
            }

            if (cCurve > 0.01f) {
                float dampZ = glm::mix(1.0f, 0.65f, cCurve);
                leftStrideZ  *= dampZ;
                rightStrideZ *= dampZ;
                leftStrideX  *= dampZ;
                rightStrideX *= dampZ;
                leftLift     *= glm::mix(1.0f, 0.55f, cCurve);
                rightLift    *= glm::mix(1.0f, 0.55f, cCurve);
            }
        } else {
            // Stationary Stance - tactical braced posture
            leftStrideZ = glm::mix(0.0f, -0.14f, cCurve);
            rightStrideZ = glm::mix(0.0f, 0.18f, cCurve);
            leftStrideX = glm::mix(0.0f, -0.04f, cCurve);
            rightStrideX = glm::mix(0.0f, 0.04f, cCurve);

            if (m_stepTurnPhase == StepTurnPhase::Beat1_LeadFoot) {
                float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat1, 0.0f, 1.0f);
                float pivotLift = std::sin(t * glm::pi<float>()) * 0.045f;
                if (m_stepTurnFoot > 0) rightLift = pivotLift;
                else leftLift = pivotLift;
            } else if (m_stepTurnPhase == StepTurnPhase::Beat2_TrailingFoot) {
                float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat2, 0.0f, 1.0f);
                float pivotLift = std::sin(t * glm::pi<float>()) * 0.038f;
                if (m_stepTurnFoot > 0) leftLift = pivotLift;
                else rightLift = pivotLift;
            }
        }

        glm::vec3 leftFootPos  = lowerToWorld(glm::vec3(-0.13f + leftStrideX,  0.07f + leftLift,  leftStrideZ));
        glm::vec3 rightFootPos = lowerToWorld(glm::vec3( 0.13f + rightStrideX, 0.07f + rightLift, rightStrideZ));

        if (m_isVaulting) {
            float tuck = std::sin(m_vaultProgress * 3.14159f) * 0.35f;
            leftFootPos = lowerToWorld(glm::vec3(-0.13f, 0.07f + tuck + 0.12f, -0.15f + m_vaultProgress * 0.35f));
            rightFootPos = lowerToWorld(glm::vec3(0.13f, 0.07f + tuck, -0.22f + m_vaultProgress * 0.40f));
        }

        float pelvisDrop = 0.0f;
        glm::vec3 normalL(0.0f, 1.0f, 0.0f);
        glm::vec3 normalR(0.0f, 1.0f, 0.0f);

        if (!m_isAirborne) {
            float groundL = World::sampleTerrainHeight(leftFootPos.x, leftFootPos.z);
            float groundR = World::sampleTerrainHeight(rightFootPos.x, rightFootPos.z);

            float deltaL = groundL - m_rootPos.y;
            float deltaR = groundR - m_rootPos.y;

            // Dynamic pelvis drop to ensure legs reach lower foot without overextension
            pelvisDrop = std::min(0.0f, std::min(deltaL, deltaR));

            // Planted or swinging foot height clamping to terrain
            leftFootPos.y = groundL + 0.07f + leftLift;
            rightFootPos.y = groundR + 0.07f + rightLift;

            normalL = getTerrainNormal(leftFootPos.x, leftFootPos.z);
            normalR = getTerrainNormal(rightFootPos.x, rightFootPos.z);
        }

        glm::vec3 leftHipPos  = lowerToWorld(glm::vec3(-0.13f + m_pelvisSway, 0.79f + m_stanceHeight + m_pelvisBob, leanZ * 0.35f));
        glm::vec3 rightHipPos = lowerToWorld(glm::vec3( 0.13f + m_pelvisSway, 0.79f + m_stanceHeight + m_pelvisBob, leanZ * 0.35f));

        leftHipPos.y  += pelvisDrop;
        rightHipPos.y += pelvisDrop;
        pelvisPos.y   += pelvisDrop;

        // Anatomically constrained forward knee pole vectors (natural forward bend with zero hyperextension)
        glm::vec3 leftKneePole  = lowerToWorld(glm::vec3(-0.15f + moveRgt * 0.08f, 0.40f + m_stanceHeight * 0.5f, 0.65f + moveFwd * 0.15f + 0.20f * cCurve));
        glm::vec3 rightKneePole = lowerToWorld(glm::vec3( 0.15f + moveRgt * 0.08f, 0.40f + m_stanceHeight * 0.5f, 0.65f + moveFwd * 0.15f + 0.20f * cCurve));

        glm::vec3 leftKneePos, rightKneePos;
        glm::quat uRot, lRot;
        InverseKinematics::solveTwoBoneIK(leftHipPos, leftFootPos, leftKneePole, 0.43f, 0.41f, leftKneePos, uRot, lRot);
        InverseKinematics::solveTwoBoneIK(rightHipPos, rightFootPos, rightKneePole, 0.43f, 0.41f, rightKneePos, uRot, lRot);

        // --- 4. TORSO, CHEST & ANATOMICAL HEAD ---
        glm::vec3 chestPos = localToWorld(glm::vec3(m_coverLeanAngle * 0.38f, 1.15f + m_stanceHeight + m_pelvisBob + coverChestElevation, leanZ + (m_isMoving ? std::sin(m_walkCycle) * 0.02f : 0.0f)));

        // Natural Spine Pitch (follows player's aim heading toward target)
        float chestPitchAngle = -m_aimPitch * 0.15f + (m_isSprinting ? 0.24f : 0.0f) + m_woundTorsoHunch * 0.25f + glm::radians(m_woundFlinchPitch);
        if (m_isVaulting) {
            chestPitchAngle += 0.32f * std::sin(m_vaultProgress * 3.14159f);
        }
        glm::mat4 chestRot = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
        chestRot = glm::rotate(chestRot, chestPitchAngle, glm::vec3(1, 0, 0));
        if (std::abs(m_coverLeanAngle) > 0.001f) {
            chestRot = glm::rotate(chestRot, m_coverLeanAngle * 0.22f, glm::vec3(0, 0, 1));
        }
        if (std::abs(m_woundFlinchRoll) > 0.01f) {
            chestRot = glm::rotate(chestRot, glm::radians(m_woundFlinchRoll), glm::vec3(0, 0, 1));
        }
        glm::mat3 chestRot3 = glm::mat3(chestRot);

        // Head joint anchored precisely at top of rotated chest
        glm::vec3 bindChest = m_skinnedModel.getBindJoint(BONE_SPINE_CHEST);
        glm::vec3 bindHead  = m_skinnedModel.getBindJoint(BONE_HEAD);
        glm::vec3 headJointOffset = bindHead - bindChest;
        glm::vec3 headCenter = chestPos + chestRot3 * headJointOffset;

        // Clavicle & Shoulder sockets strictly anchored to rotated chest
        glm::vec3 bindRShoulder = m_skinnedModel.getBindJoint(BONE_R_UPPER_ARM);
        glm::vec3 bindLShoulder = m_skinnedModel.getBindJoint(BONE_L_UPPER_ARM);
        rightShoulder = chestPos + chestRot3 * (bindRShoulder - bindChest);
        leftShoulder  = chestPos + chestRot3 * (bindLShoulder - bindChest);

        // --- 5. ARMS & WEAPON IK TRACKING ---
        float breathY = std::sin(m_idleTime * 1.8f) * (m_aimWeight > 0.5f ? 0.003f : 0.008f);
        float breathX = std::cos(m_idleTime * 1.2f) * (m_aimWeight > 0.5f ? 0.002f : 0.006f);

        glm::vec3 rightHandTarget;
        glm::vec3 leftHandTarget;
        glm::vec3 rightElbowPole;
        glm::vec3 leftElbowPole;
        rifleRoot = glm::vec3(0.0f);
        aimForward = glm::vec3(0.0f);
        glm::vec3 rifleUp(0.0f, 1.0f, 0.0f);
        renderPitch = m_aimPitch;

        if (shouldDrawWeapon) {
            if (m_isVaulting) {
                // Biomechanical Athletic Vault IK: Left hand plants firmly on obstacle top, right hand tucks weapon
                renderPitch = 0.52f;
                float cp = std::cos(renderPitch);
                float sp = -std::sin(renderPitch);
                aimForward = forwardDir * cp + glm::vec3(0, sp, 0);
                rifleUp = glm::normalize(glm::cross(aimForward, rightDir));

                rifleRoot = chestPos + rightDir * 0.16f - glm::vec3(0.0f, 0.12f, 0.0f) + aimForward * 0.14f;
                rightHandTarget = rifleRoot - aimForward * 0.06f - rifleUp * 0.06f;
                rightElbowPole = chestPos + rightDir * 0.44f - rifleUp * 0.18f;

                if (m_vaultProgress <= 0.72f && glm::length(m_vaultPlantPos) > 0.1f) {
                    leftHandTarget = m_vaultPlantPos - rightDir * 0.08f;
                    leftElbowPole = leftHandTarget + glm::vec3(0.0f, 0.35f, 0.0f) - rightDir * 0.25f;
                } else {
                    float followT = (m_vaultProgress - 0.72f) / 0.28f;
                    glm::vec3 reachForward = chestPos - rightDir * 0.15f + forwardDir * 0.25f - glm::vec3(0.0f, 0.15f, 0.0f);
                    leftHandTarget = glm::mix(m_vaultPlantPos, reachForward, followT);
                    leftElbowPole = chestPos - rightDir * 0.40f - rifleUp * 0.20f;
                }
            } else if (m_isReloading && m_hasWeapon) {
                // Tactical Reload Procedural Inverse Kinematics
                renderPitch = -0.15f;
                float cp = std::cos(renderPitch);
                float sp = -std::sin(renderPitch);
                aimForward = forwardDir * cp + glm::vec3(0, sp, 0);
                rifleUp = glm::normalize(glm::cross(aimForward, rightDir));

                rifleRoot = chestPos + rightDir * 0.12f - glm::vec3(0.0f, 0.05f, 0.0f) + aimForward * 0.30f;
                rightHandTarget = rifleRoot - aimForward * 0.08f - rifleUp * 0.07f;
                rightElbowPole = chestPos + rightDir * 0.45f - aimForward * 0.10f - rifleUp * 0.18f;

                float p = m_reloadProgress;
                glm::vec3 magPouch = chestPos - rightDir * 0.12f - glm::vec3(0.0f, 0.28f, 0.0f) + aimForward * 0.08f;
                glm::vec3 magWellApproach = rifleRoot + aimForward * 0.08f - rifleUp * 0.18f;
                glm::vec3 magWellInsert = rifleRoot + aimForward * 0.08f - rifleUp * 0.06f;
                glm::vec3 boltRelease = rifleRoot + aimForward * 0.04f + rifleUp * 0.04f;
                glm::vec3 handguardPos = rifleRoot + aimForward * 0.26f - rifleUp * 0.05f;

                if (p < 0.25f) {
                    float tRel = p / 0.25f;
                    leftHandTarget = glm::mix(handguardPos, magPouch, tRel);
                } else if (p < 0.55f) {
                    float tRel = (p - 0.25f) / 0.30f;
                    leftHandTarget = glm::mix(magPouch, magWellApproach, tRel);
                } else if (p < 0.75f) {
                    float tRel = (p - 0.55f) / 0.20f;
                    leftHandTarget = glm::mix(magWellApproach, magWellInsert, tRel);
                } else if (p < 0.88f) {
                    float tRel = (p - 0.75f) / 0.13f;
                    leftHandTarget = glm::mix(magWellInsert, boltRelease, tRel);
                } else {
                    float tRel = (p - 0.88f) / 0.12f;
                    leftHandTarget = glm::mix(boltRelease, handguardPos, tRel);
                }
                leftElbowPole = chestPos - rightDir * 0.40f + aimForward * 0.05f - rifleUp * 0.24f;

            } else if (m_isSprinting && !m_isAiming && !isDroppingWeapon) {
                // Modern Tactical High-Ready Sprint Carry: weapon pulled close across chest, athletic locomotion drive
                renderPitch = 0.42f;
                float cp = std::cos(renderPitch);
                float sp = -std::sin(renderPitch);
                aimForward = forwardDir * cp + glm::vec3(0, sp, 0);
                rifleUp = glm::normalize(glm::cross(aimForward, rightDir));

                float sprintBobY = std::abs(std::sin(m_walkCycle)) * 0.040f;
                float sprintBobX = std::sin(m_walkCycle) * 0.028f;
                rifleRoot = chestPos + rightDir * 0.12f - glm::vec3(0.0f, 0.10f - sprintBobY, 0.0f) + aimForward * (0.16f + sprintBobX);

                bool isPistol = (weaponToDraw.find("M9") != std::string::npos ||
                                 weaponToDraw.find("Beretta") != std::string::npos ||
                                 weaponToDraw.find("Pistol") != std::string::npos);
                if (isPistol) {
                    rightHandTarget = rifleRoot - aimForward * 0.02f - rifleUp * 0.05f;
                    leftHandTarget  = rifleRoot - aimForward * 0.00f - rifleUp * 0.055f - rightDir * 0.03f;
                    rightElbowPole = chestPos + rightDir * 0.38f - rifleUp * 0.20f;
                    leftElbowPole  = chestPos - rightDir * 0.38f - rifleUp * 0.20f;
                } else {
                    rightHandTarget = rifleRoot - aimForward * 0.06f - rifleUp * 0.06f;
                    leftHandTarget  = rifleRoot + aimForward * 0.16f - rifleUp * 0.02f - rightDir * 0.02f;
                    rightElbowPole = chestPos + rightDir * 0.42f - aimForward * 0.08f - rifleUp * 0.18f;
                    leftElbowPole  = chestPos - rightDir * 0.42f + aimForward * 0.06f - rifleUp * 0.20f;
                }
            } else {
                // Procedural Equip and Drop Animation Curves
                float equipProg = (m_equipAnimDuration > 0.0f) ? std::clamp(1.0f - (m_equipAnimTimer / m_equipAnimDuration), 0.0f, 1.0f) : 1.0f;
                float equipCurve = glm::smoothstep(0.0f, 1.0f, equipProg);
                float equipDipY = -0.34f * (1.0f - equipCurve);
                float equipPullZ = -0.16f * (1.0f - equipCurve);
                float equipTilt = 0.52f * (1.0f - equipCurve);

                float dropProg = (m_dropAnimDuration > 0.0f) ? std::clamp(m_dropAnimTimer / m_dropAnimDuration, 0.0f, 1.0f) : 0.0f;
                float dropCurve = glm::smoothstep(0.0f, 1.0f, dropProg);
                float dropDipY = -0.36f * dropCurve;
                float dropForwardZ = 0.10f * dropCurve;
                float dropTilt = 0.65f * dropCurve;

                bool isPistol = (weaponToDraw.find("M9") != std::string::npos ||
                                 weaponToDraw.find("Beretta") != std::string::npos ||
                                 weaponToDraw.find("Pistol") != std::string::npos);

                const auto& gripCfg = WeaponRigManager::get().getConfig(weaponToDraw);
                renderPitch = glm::mix(gripCfg.lowReadyPitch, -m_aimPitch, m_aimWeight) - m_recoilKick * 2.8f + equipTilt + dropTilt;
                float cp = std::cos(renderPitch);
                float sp = -std::sin(renderPitch);
                aimForward = forwardDir * cp + glm::vec3(0, sp, 0);
                rifleUp = glm::normalize(glm::cross(aimForward, rightDir));

                float walkBobY = (m_isMoving && m_aimWeight < 0.5f) ? (std::abs(std::sin(m_walkCycle)) * 0.016f) : 0.0f;
                float walkBobX = (m_isMoving && m_aimWeight < 0.5f) ? (std::sin(m_walkCycle) * 0.012f) : 0.0f;

                float rootY = glm::mix(gripCfg.rootOffset.y, gripCfg.rootOffsetADS.y, m_aimWeight) + breathY - walkBobY + equipDipY + dropDipY;
                float rootRight = glm::mix(gripCfg.rootOffset.x, gripCfg.rootOffsetADS.x, m_aimWeight) + breathX + walkBobX;
                if (!m_shoulderRight) rootRight = -rootRight;
                float rootForward = glm::mix(gripCfg.rootOffset.z, gripCfg.rootOffsetADS.z, m_aimWeight) - m_recoilKick + equipPullZ + dropForwardZ;

                rifleRoot = chestPos + rightDir * rootRight + glm::vec3(0, rootY, 0) + aimForward * rootForward;

                // Mount weapon directly on cover rim when aiming in low cover (forward rest + leveled pitch)
                if (m_inCover && m_isLowCover && m_coverAimWeight > 0.001f) {
                    float desiredRifleY = m_rootPos.y + m_coverHeight + 0.04f;
                    rifleRoot.y = glm::mix(rifleRoot.y, desiredRifleY, m_coverAimWeight);
                    rifleRoot += forwardDir * (0.18f * m_coverAimWeight);
                    renderPitch = glm::mix(renderPitch, -m_aimPitch * 0.85f, m_coverAimWeight);
                }

                if (isPistol) {
                    rightHandTarget = rifleRoot - aimForward * 0.02f - rifleUp * 0.05f;
                    leftHandTarget  = rifleRoot + aimForward * 0.00f - rifleUp * 0.055f - rightDir * (m_shoulderRight ? 0.03f : -0.03f);
                    rightElbowPole = chestPos + rightDir * (m_shoulderRight ? 0.36f : 0.18f) - rifleUp * 0.22f + forwardDir * 0.04f;
                    leftElbowPole  = chestPos - rightDir * (m_shoulderRight ? 0.36f : 0.18f) - rifleUp * 0.22f + forwardDir * 0.04f;
                } else {
                    rightHandTarget = rifleRoot + aimForward * gripCfg.rightHandOffset.x + rifleUp * (gripCfg.rightHandOffset.y - m_woundRightArmSag * 0.16f) + rightDir * (m_shoulderRight ? gripCfg.rightHandOffset.z : -gripCfg.rightHandOffset.z);

                    glm::vec3 leftOffset = glm::mix(gripCfg.leftHandOffset, gripCfg.leftHandOffsetADS, m_aimWeight);
                    float leftReachProg = isDroppingWeapon ? 0.0f : std::clamp(equipCurve * 1.5f - 0.5f, 0.0f, 1.0f);
                    glm::vec3 leftRestPos = chestPos - rightDir * (m_shoulderRight ? 0.22f : -0.22f) - glm::vec3(0, 0.35f, 0);
                    glm::vec3 leftHandGrip = rifleRoot + aimForward * leftOffset.x + rifleUp * (leftOffset.y - m_woundLeftArmSag * 0.14f) + rightDir * (m_shoulderRight ? leftOffset.z : -leftOffset.z);
                    leftHandTarget = glm::mix(leftRestPos, leftHandGrip, leftReachProg);

                    if (m_shoulderRight) {
                        rightElbowPole = chestPos + rightDir * 0.42f - rifleUp * 0.22f - aimForward * 0.04f;
                        leftElbowPole  = chestPos - rightDir * 0.46f - rifleUp * 0.24f + aimForward * 0.06f;
                    } else {
                        rightElbowPole = chestPos + rightDir * 0.24f - rifleUp * 0.22f + aimForward * 0.04f;
                        leftElbowPole  = chestPos - rightDir * 0.42f - rifleUp * 0.24f - aimForward * 0.04f;
                    }
                }
            }
        } else {
            // Weapon unequipped / holstered
            if (m_isMoving) {
                float rightSwing = std::sin(m_walkCycle) * (m_isSprinting ? 0.52f : 0.18f);
                float leftSwing  = std::sin(m_walkCycle + 3.14159f) * (m_isSprinting ? 0.52f : 0.18f);

                rightHandTarget = chestPos + rightDir * 0.22f - glm::vec3(0, 0.38f + m_woundRightArmSag * 0.18f, 0) + forwardDir * rightSwing;
                leftHandTarget  = chestPos - rightDir * 0.22f - glm::vec3(0, 0.38f + m_woundLeftArmSag * 0.18f, 0) + forwardDir * leftSwing;
            } else {
                rightHandTarget = chestPos + rightDir * 0.27f - glm::vec3(0, 0.38f - breathY + m_woundRightArmSag * 0.18f, 0) + forwardDir * (0.02f + breathX);
                leftHandTarget  = chestPos - rightDir * 0.27f - glm::vec3(0, 0.38f - breathY + m_woundLeftArmSag * 0.18f, 0) + forwardDir * (0.02f - breathX);
            }

            rightElbowPole = chestPos + rightDir * 0.34f - glm::vec3(0, 0.18f, 0) - forwardDir * 0.04f;
            leftElbowPole  = chestPos - rightDir * 0.34f - glm::vec3(0, 0.18f, 0) - forwardDir * 0.04f;
        }

        InverseKinematics::solveTwoBoneIK(rightShoulder, rightHandTarget, rightElbowPole, 0.30f, 0.28f, rightElbowPos, uRot, lRot);
        InverseKinematics::solveTwoBoneIK(leftShoulder, leftHandTarget, leftElbowPole, 0.30f, 0.28f, leftElbowPos, uRot, lRot);

        // --- 6. ASSEMBLE SKELETON POSE MATRICES ---
        // Pelvis
        m_pose.bonePositions[BONE_PELVIS] = pelvisPos;
        glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), m_lowerBodyYaw, glm::vec3(0, 1, 0));
        if (cCurve > 0.01f) rotY = glm::rotate(rotY, 0.20f * cCurve, glm::vec3(1, 0, 0));
        m_pose.boneRotations[BONE_PELVIS] = glm::mat3(rotY);

        // Spine / Chest
        m_pose.bonePositions[BONE_SPINE_CHEST] = chestPos;
        m_pose.boneRotations[BONE_SPINE_CHEST] = chestRot3;

        // Head looks forward along aim heading with subtle natural pitch tracking
        m_pose.bonePositions[BONE_HEAD] = headCenter;
        float headPitchAngle = -m_aimPitch * 0.45f;
        glm::mat4 headRot = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
        headRot = glm::rotate(headRot, headPitchAngle, glm::vec3(1, 0, 0));
        m_pose.boneRotations[BONE_HEAD] = glm::mat3(headRot);

        // Body orientation bases for smooth limb rotation composition
        glm::mat3 lowerRot = glm::mat3(glm::rotate(glm::mat4(1.0f), m_lowerBodyYaw, glm::vec3(0, 1, 0)));

        // Legs
        glm::vec3 bindThighDir(0.0f, -1.0f, 0.0f);
        glm::vec3 bindCalfDir(0.0f, -1.0f, 0.0f);

        m_pose.bonePositions[BONE_L_THIGH] = leftHipPos;
        glm::mat3 stdLThighRot = SkinnedSoldier::computeLimbRotation(bindThighDir, leftKneePos - leftHipPos, lowerRot);
        m_pose.boneRotations[BONE_L_THIGH] = stdLThighRot;

        m_pose.bonePositions[BONE_L_CALF] = leftKneePos;
        m_pose.boneRotations[BONE_L_CALF] = SkinnedSoldier::computeLimbRotation(bindCalfDir, leftFootPos - leftKneePos, stdLThighRot);

        // Dynamic heel-strike, flat-foot, push-off ankle flexion & terrain slope normal conformity
        auto calcAnklePitch = [](float u, float cCurve) -> float {
            float pitch = 0.0f;
            if (u < 0.12f) {
                // Heel strike: heel contacts ground first with dorsiflexion
                float t = u / 0.12f;
                pitch = glm::mix(-0.18f, 0.0f, t);
            } else if (u < 0.36f) {
                // Full foot plant: flat on ground
                pitch = 0.0f;
            } else if (u < 0.50f) {
                // Toe push-off: heel lifts, driving body forward
                float t = (u - 0.36f) / 0.14f;
                pitch = glm::mix(0.0f, 0.35f, t);
            } else {
                // Swing clearance: toes angle up slightly
                float t = (u - 0.50f) / 0.50f;
                pitch = glm::mix(0.35f, -0.18f, glm::smoothstep(0.0f, 1.0f, t));
            }
            return pitch * (1.0f - cCurve * 0.5f);
        };

        float phaseNorm = m_walkCycle / (2.0f * glm::pi<float>());
        float uL = std::fmod(phaseNorm, 1.0f); if (uL < 0.0f) uL += 1.0f;
        float uR = std::fmod(phaseNorm + 0.5f, 1.0f); if (uR < 0.0f) uR += 1.0f;

        float lAnklePitch = m_isMoving ? calcAnklePitch(uL, cCurve) : 0.0f;
        glm::mat4 lFootRot = glm::rotate(glm::mat4(1.0f), m_lowerBodyYaw, glm::vec3(0, 1, 0));
        lFootRot = glm::rotate(lFootRot, lAnklePitch, glm::vec3(1, 0, 0));
        if (!m_isAirborne) {
            float slopePitchL = glm::dot(normalL, lowerForward);
            float slopeRollL  = glm::dot(normalL, lowerRight);
            lFootRot = glm::rotate(lFootRot, -slopePitchL * 0.75f, glm::vec3(1, 0, 0));
            lFootRot = glm::rotate(lFootRot, slopeRollL * 0.75f, glm::vec3(0, 0, 1));
        }
        m_pose.bonePositions[BONE_L_FOOT] = leftFootPos;
        m_pose.boneRotations[BONE_L_FOOT] = glm::mat3(lFootRot);

        m_pose.bonePositions[BONE_R_THIGH] = rightHipPos;
        glm::mat3 stdRThighRot = SkinnedSoldier::computeLimbRotation(bindThighDir, rightKneePos - rightHipPos, lowerRot);
        m_pose.boneRotations[BONE_R_THIGH] = stdRThighRot;

        m_pose.bonePositions[BONE_R_CALF] = rightKneePos;
        m_pose.boneRotations[BONE_R_CALF] = SkinnedSoldier::computeLimbRotation(bindCalfDir, rightFootPos - rightKneePos, stdRThighRot);

        float rAnklePitch = m_isMoving ? calcAnklePitch(uR, cCurve) : 0.0f;
        glm::mat4 rFootRot = glm::rotate(glm::mat4(1.0f), m_lowerBodyYaw, glm::vec3(0, 1, 0));
        rFootRot = glm::rotate(rFootRot, rAnklePitch, glm::vec3(1, 0, 0));
        if (!m_isAirborne) {
            float slopePitchR = glm::dot(normalR, lowerForward);
            float slopeRollR  = glm::dot(normalR, lowerRight);
            rFootRot = glm::rotate(rFootRot, -slopePitchR * 0.75f, glm::vec3(1, 0, 0));
            rFootRot = glm::rotate(rFootRot, slopeRollR * 0.75f, glm::vec3(0, 0, 1));
        }
        m_pose.bonePositions[BONE_R_FOOT] = rightFootPos;
        m_pose.boneRotations[BONE_R_FOOT] = glm::mat3(rFootRot);

        // Arms
        glm::vec3 bindLArmDir = glm::normalize(glm::vec3(-0.08f, -0.30f, 0.0f));
        glm::vec3 bindLForeDir = glm::normalize(glm::vec3(-0.02f, -0.30f, 0.0f));
        glm::vec3 bindRArmDir = glm::normalize(glm::vec3(0.08f, -0.30f, 0.0f));
        glm::vec3 bindRForeDir = glm::normalize(glm::vec3(0.02f, -0.30f, 0.0f));

        m_pose.bonePositions[BONE_L_UPPER_ARM] = leftShoulder;
        glm::mat3 stdLArmRot = SkinnedSoldier::computeLimbRotation(bindLArmDir, leftElbowPos - leftShoulder, chestRot3);
        m_pose.boneRotations[BONE_L_UPPER_ARM] = stdLArmRot;

        m_pose.bonePositions[BONE_L_FOREARM] = leftElbowPos;
        m_pose.boneRotations[BONE_L_FOREARM] = SkinnedSoldier::computeLimbRotation(bindLForeDir, leftHandTarget - leftElbowPos, stdLArmRot);

        m_pose.bonePositions[BONE_L_HAND] = leftHandTarget;

        m_pose.bonePositions[BONE_R_UPPER_ARM] = rightShoulder;
        glm::mat3 stdRArmRot = SkinnedSoldier::computeLimbRotation(bindRArmDir, rightElbowPos - rightShoulder, chestRot3);
        m_pose.boneRotations[BONE_R_UPPER_ARM] = stdRArmRot;

        m_pose.bonePositions[BONE_R_FOREARM] = rightElbowPos;
        m_pose.boneRotations[BONE_R_FOREARM] = SkinnedSoldier::computeLimbRotation(bindRForeDir, rightHandTarget - rightElbowPos, stdRArmRot);

        m_pose.bonePositions[BONE_R_HAND] = rightHandTarget;

        if (shouldDrawWeapon) {
            glm::vec3 gripAxis = glm::normalize(-rifleUp * 0.94f - aimForward * 0.34f);
            glm::vec3 palmFacing = glm::normalize(aimForward * 0.94f - rifleUp * 0.34f);
            glm::vec3 lateralAxis = glm::normalize(glm::cross(gripAxis, palmFacing));

            m_pose.boneRotations[BONE_R_HAND] = glm::mat3(lateralAxis, -gripAxis, palmFacing);

            glm::vec3 triggerBase = rightHandTarget + palmFacing * 0.010f - lateralAxis * 0.016f;
            m_pose.bonePositions[BONE_R_TRIGGER_FINGER] = triggerBase;

            float curlAngle = m_rightHand.triggerPull * 0.42f;
            glm::mat4 baseTrig = glm::mat4(m_pose.boneRotations[BONE_R_HAND]);
            glm::mat4 trigRot = glm::rotate(baseTrig, curlAngle, glm::vec3(1.0f, 0.0f, 0.0f));
            m_pose.boneRotations[BONE_R_TRIGGER_FINGER] = glm::mat3(trigRot);

            glm::vec3 lHandAxis = glm::normalize(aimForward * 0.88f + rifleUp * 0.22f);
            glm::vec3 lPalmFacing = glm::normalize(rifleUp * 0.88f - aimForward * 0.22f);
            glm::vec3 lLateralAxis = glm::normalize(glm::cross(lPalmFacing, lHandAxis));

            m_pose.boneRotations[BONE_L_HAND] = glm::mat3(lLateralAxis, -lHandAxis, lPalmFacing);
        } else {
            glm::vec3 rForeDir = glm::normalize(rightHandTarget - rightElbowPos);
            glm::vec3 lForeDir = glm::normalize(leftHandTarget - leftElbowPos);

            glm::vec3 rPalm = glm::normalize(-rightDir * 0.90f + forwardDir * 0.25f);
            glm::vec3 rLat = glm::normalize(glm::cross(rForeDir, rPalm));
            m_pose.boneRotations[BONE_R_HAND] = glm::mat3(rLat, -rForeDir, rPalm);

            m_pose.bonePositions[BONE_R_TRIGGER_FINGER] = rightHandTarget + rPalm * 0.008f - rLat * 0.016f;
            m_pose.boneRotations[BONE_R_TRIGGER_FINGER] = m_pose.boneRotations[BONE_R_HAND];

            glm::vec3 lPalm = glm::normalize(rightDir * 0.90f + forwardDir * 0.25f);
            glm::vec3 lLat = glm::normalize(glm::cross(lForeDir, lPalm));
            m_pose.boneRotations[BONE_L_HAND] = glm::mat3(lLat, -lForeDir, lPalm);
        }

        // Muzzle updated by exact weapon transform
    }

    // --- 7. DEFORM MESH VIA LINEAR BLEND SKINNING & UPLOAD ---
    // (IFF Identification Armbands, anatomical head, and gear are fully skinned in SkinnedSoldier)
    m_skinnedModel.deform(m_pose, custom, vertices, indices, m_dismemberMask);

    if (!m_dynamicMesh) m_dynamicMesh = std::make_unique<Mesh>();
    m_dynamicMesh->upload(vertices, indices);

    // Render textured modern military operative with PBR materials
    renderer.drawTexturedMesh(*m_dynamicMesh, glm::mat4(1.0f), renderer.getSoldierTexture(), renderer.getSoldierNormal());

    // --- 8. RENDER ATTACHED PRIMARY WEAPON WITH AIM PITCH & RECOIL ---
    bool drawAttachedWeapon = shouldDrawWeapon;
    if (m_isDead) {
        drawAttachedWeapon = m_deathGrip && !(m_dismemberMask & DISMEMBER_R_ARM) && (weaponToDraw != "None");
    }
    if (drawAttachedWeapon) {
        glm::mat4 wModel = glm::translate(glm::mat4(1.0f), rifleRoot);
        wModel = glm::rotate(wModel, m_yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        if (std::abs(renderPitch) > 0.001f) {
            wModel = glm::rotate(wModel, renderPitch, glm::vec3(1.0f, 0.0f, 0.0f));
        }

        glm::vec3 localMuzzle(0.0f, 0.02f, 0.55f);
        glm::vec3 localCasing(0.03f, 0.035f, 0.08f);
        if (weaponToDraw.find("AK") != std::string::npos || weaponToDraw.find("74") != std::string::npos || weaponToDraw.find("Kalash") != std::string::npos) {
            localMuzzle = glm::vec3(0.0f, 0.032f, 0.6425f);
            localCasing = glm::vec3(0.035f, 0.040f, 0.10f);
        } else if (weaponToDraw.find("Mosin") != std::string::npos || weaponToDraw.find("Nagant") != std::string::npos || weaponToDraw.find("Sniper") != std::string::npos) {
            localMuzzle = glm::vec3(0.0f, 0.022f, 0.86f);
            localCasing = glm::vec3(0.040f, 0.032f, -0.04f);
        } else if (weaponToDraw.find("M9") != std::string::npos || weaponToDraw.find("Beretta") != std::string::npos || weaponToDraw.find("Pistol") != std::string::npos) {
            localMuzzle = glm::vec3(0.0f, 0.035f, 0.135f);
            localCasing = glm::vec3(0.02f, 0.035f, 0.03f);
        }

        m_muzzlePos = glm::vec3(wModel * glm::vec4(localMuzzle, 1.0f));
        m_casingPos = glm::vec3(wModel * glm::vec4(localCasing, 1.0f));
        m_rifleRoot = rifleRoot;
        m_renderPitch = renderPitch;

        renderer.drawWeapon(weaponToDraw, rifleRoot, m_yaw, renderPitch);
    }
}

} // namespace Mercenary

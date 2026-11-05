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

    if (!m_stanceFeetInitialized) {
        m_lowerBodyYaw = yaw;
        m_leftFootPlantedYaw = yaw;
        m_rightFootPlantedYaw = yaw;
        m_stanceFeetInitialized = true;
    }

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

    // 1. Calculate vertical aim pitch from direct camera feed or 3D target ray
    if (m_hasDirectAimPitch) {
        m_aimPitch = std::clamp(m_directAimPitch, -1.22f, 1.15f); // Full -70 deg to +66 deg range
        m_hasDirectAimPitch = false;
    } else {
        glm::vec3 toTarget = aimTarget - (worldPos + glm::vec3(0.0f, 1.48f + m_stanceHeight, 0.0f));
        float horizontalDist = std::sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
        if (horizontalDist > 0.1f) {
            float elev = std::atan2(toTarget.y, horizontalDist);
            m_aimPitch = std::clamp(elev, -1.22f, 1.15f);
        }
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
        m_leftFootPlantedYaw = m_lowerBodyYaw;
        m_rightFootPlantedYaw = m_lowerBodyYaw;
        m_stanceFeetInitialized = true;

        // Walk cycle progression driven strictly by ground displacement
        float horizDistTraveled = speed * dt;
        float strideLength = isSprinting ? 2.10f : (m_stance == 1 ? 0.95f : 1.35f);
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

        // Stationary Turn-in-Place & Progressive Stepping (4 Tiers)
        float diff = yaw - m_lowerBodyYaw;
        while (diff > glm::pi<float>()) diff -= 2.0f * glm::pi<float>();
        while (diff < -glm::pi<float>()) diff += 2.0f * glm::pi<float>();

        if (std::abs(diff) > glm::radians(105.0f)) {
            // Instant realignment on extreme 180 flick turns
            m_stepTurnPhase = StepTurnPhase::Idle;
            m_lowerBodyYaw += diff * std::clamp(dt * 30.0f, 0.0f, 1.0f);
            m_leftFootPlantedYaw = m_lowerBodyYaw;
            m_rightFootPlantedYaw = m_lowerBodyYaw;
        } else if (m_stepTurnPhase == StepTurnPhase::Idle) {
            // Tier 1 (0-24 deg): visage / head glance only. Pelvis & feet 100% frozen.
            // Tier 2 (24-38 deg): torso twists smoothly. Pelvis & feet 100% frozen.
            // Tier 3 (38-70 deg): Single leg steps out! Support leg remains 100% locked.
            // Tier 4 (> 70 deg): Two-beat step turn (lead foot steps, trailing foot squares).
            if (std::abs(diff) > glm::radians(38.0f)) {
                m_stepTurnPhase = StepTurnPhase::Beat1_LeadFoot;
                m_stepTurnTimer = 0.0f;
                m_stepTurnDir = (diff > 0.0f) ? 1.0f : -1.0f;
                m_stepTurnFoot = (diff > 0.0f) ? 1 : -1;
                m_stepTurnStartLowerYaw = (diff > 0.0f) ? m_rightFootPlantedYaw : m_leftFootPlantedYaw;

                if (std::abs(diff) <= glm::radians(70.0f)) {
                    // Tier 3: Moderate Turn -> ONLY 1 LEG STEPS!
                    m_isSingleLegOnly = true;
                    m_stepTurnBeat1Yaw = m_lowerBodyYaw + diff;
                    m_stepTurnTargetLowerYaw = m_stepTurnBeat1Yaw;
                } else {
                    // Tier 4: Large Turn (> 70 deg) -> Two-beat step turn
                    m_isSingleLegOnly = false;
                    m_stepTurnBeat1Yaw = m_lowerBodyYaw + diff * 0.55f;
                    m_stepTurnTargetLowerYaw = m_lowerBodyYaw + diff;
                }
            }
        } else if (m_stepTurnPhase == StepTurnPhase::Beat1_LeadFoot) {
            // Lead foot lifts and pivots hips toward target
            m_stepTurnTimer += dt;
            float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat1, 0.0f, 1.0f);
            float ease = t * t * (3.0f - 2.0f * t);

            if (m_isSingleLegOnly) {
                float targetHips = (m_stepTurnFoot > 0) ?
                    glm::mix(m_leftFootPlantedYaw, m_stepTurnBeat1Yaw, 0.5f) :
                    glm::mix(m_rightFootPlantedYaw, m_stepTurnBeat1Yaw, 0.5f);
                m_lowerBodyYaw = glm::mix(m_stepTurnStartLowerYaw, targetHips, ease);
            } else {
                m_lowerBodyYaw = glm::mix(m_stepTurnStartLowerYaw, m_stepTurnBeat1Yaw, ease);
            }

            if (t >= 1.0f) {
                if (m_stepTurnFoot > 0) {
                    m_rightFootPlantedYaw = m_stepTurnBeat1Yaw;
                } else {
                    m_leftFootPlantedYaw = m_stepTurnBeat1Yaw;
                }
                m_footstepEvent = 3; // Stationary pivot step event

                if (m_isSingleLegOnly) {
                    // ONLY 1 LEG STEPS: Transition directly to Idle! Trailing leg NEVER moves!
                    m_stepTurnPhase = StepTurnPhase::Idle;
                    m_stepTurnTimer = 0.0f;
                    float footDiff = m_rightFootPlantedYaw - m_leftFootPlantedYaw;
                    while (footDiff > glm::pi<float>()) footDiff -= 2.0f * glm::pi<float>();
                    while (footDiff < -glm::pi<float>()) footDiff += 2.0f * glm::pi<float>();
                    m_lowerBodyYaw = m_leftFootPlantedYaw + footDiff * 0.5f;
                } else {
                    // Tier 4: Check residual difference
                    float remDiff = yaw - m_stepTurnBeat1Yaw;
                    while (remDiff > glm::pi<float>()) remDiff -= 2.0f * glm::pi<float>();
                    while (remDiff < -glm::pi<float>()) remDiff += 2.0f * glm::pi<float>();

                    if (std::abs(remDiff) <= glm::radians(34.0f)) {
                        m_stepTurnPhase = StepTurnPhase::Idle;
                        m_stepTurnTimer = 0.0f;
                        float footDiff = m_rightFootPlantedYaw - m_leftFootPlantedYaw;
                        while (footDiff > glm::pi<float>()) footDiff -= 2.0f * glm::pi<float>();
                        while (footDiff < -glm::pi<float>()) footDiff += 2.0f * glm::pi<float>();
                        m_lowerBodyYaw = m_leftFootPlantedYaw + footDiff * 0.5f;
                    } else {
                        m_stepTurnPhase = StepTurnPhase::Beat2_TrailingFoot;
                        m_stepTurnTimer = 0.0f;
                        m_stepTurnTargetLowerYaw = m_stepTurnBeat1Yaw + remDiff;
                    }
                }
            }
        } else if (m_stepTurnPhase == StepTurnPhase::Beat2_TrailingFoot) {
            // Beat 2: Trailing foot lifts and completes catch-up to square the stance
            m_stepTurnTimer += dt;
            float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat2, 0.0f, 1.0f);
            float ease = t * t * (3.0f - 2.0f * t);
            m_lowerBodyYaw = glm::mix(m_stepTurnBeat1Yaw, m_stepTurnTargetLowerYaw, ease);
            if (t >= 1.0f) {
                m_lowerBodyYaw = m_stepTurnTargetLowerYaw;
                m_leftFootPlantedYaw = m_stepTurnTargetLowerYaw;
                m_rightFootPlantedYaw = m_stepTurnTargetLowerYaw;
                m_stepTurnPhase = StepTurnPhase::Idle;
                m_stepTurnTimer = 0.0f;
                m_footstepEvent = 3; // Stationary trailing step event
            }
        }

        while (m_lowerBodyYaw > glm::pi<float>()) m_lowerBodyYaw -= 2.0f * glm::pi<float>();
        while (m_lowerBodyYaw < -glm::pi<float>()) m_lowerBodyYaw += 2.0f * glm::pi<float>();

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

        // Procedural Spine Pitch Distribution (60% thoracic/lumbar bending)
        float spineAimPitch = -m_aimPitch * 0.60f;
        float spineCurveForward = std::sin(spineAimPitch) * 0.22f;
        float spineCurveHeight = (1.0f - std::cos(spineAimPitch)) * 0.08f;

        float pelvisPitchShiftZ = (m_aimPitch < 0.0f) ? (std::sin(m_aimPitch) * 0.065f) : (std::sin(m_aimPitch) * 0.055f);
        float pelvisPitchDropY = -std::abs(std::sin(m_aimPitch)) * 0.035f;

        // Accurate forward direction lean (prevents muzzle detachment when facing arbitrary angles)
        glm::vec3 pelvisPos = m_rootPos + rightDir * (m_pelvisSway + m_coverLeanAngle * 0.20f) 
                              + forwardDir * (leanZ * 0.45f + pelvisPitchShiftZ) 
                              + glm::vec3(0.0f, 0.81f + m_stanceHeight + m_pelvisBob + pelvisPitchDropY, 0.0f);
        glm::vec3 spine1 = pelvisPos + rightDir * (m_coverLeanAngle * 0.12f) + forwardDir * (leanZ * 0.25f + spineCurveForward * 0.5f) + glm::vec3(0.0f, 0.18f - spineCurveHeight * 0.4f, 0.0f);
        glm::vec3 chestPos = spine1 + rightDir * (m_coverLeanAngle * 0.18f) + forwardDir * (leanZ * 0.35f + spineCurveForward * 0.5f) + glm::vec3(0.0f, 0.18f - spineCurveHeight * 0.6f, 0.0f);

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

    } else {
        // =========================================================================
        // PROCEDURAL SKELETON POSE SYNTHESIS & CONTINUOUS PRONE TRANSITION BLEND
        // =========================================================================
        float pCurve = glm::smoothstep(0.0f, 1.0f, m_proneTransition);
        float cCurve = glm::smoothstep(0.0f, 1.0f, m_crouchTransition);

        // -------------------------------------------------------------------------
        // A. UPRIGHT / CROUCHED / ATHLETIC SPRINT LOCOMOTION POSE
        // -------------------------------------------------------------------------
        SkeletonPose uprightPose;
        glm::vec3 uprightRifleRoot(0.0f);
        float uprightRenderPitch = m_aimPitch;

        {
            // Tactical Low Cover Peeking & Weapon Mounting
            float coverChestElevation = 0.0f;
            if (m_inCover && m_isLowCover && m_coverAimWeight > 0.001f) {
                float targetChestY = m_coverHeight + 0.12f;
                float currentCrouchChestY = 0.75f;
                coverChestElevation = std::max(0.0f, targetChestY - currentCrouchChestY) * m_coverAimWeight;
            }

            // Dynamic Athletic Forward Lean
            float sprintLean = (m_isSprinting && m_isMoving) ? 0.30f : ((m_isMoving) ? 0.11f : 0.0f);
            float leanZ = sprintLean + 0.14f * cCurve + m_woundTorsoHunch * 0.25f;

            // Asymmetric limp dip when wounded in lower extremities
            float limpPelvisDip = (m_isMoving && m_woundLimpFactor > 0.05f) ? -std::abs(std::sin(m_walkCycle)) * 0.065f * m_woundLimpFactor : 0.0f;

            // Aim pitch pelvis counterbalance & dynamic step turn weight dip
            float pelvisPitchShiftZ = (m_aimPitch < 0.0f) ? (std::sin(m_aimPitch) * 0.065f) : (std::sin(m_aimPitch) * 0.055f);
            float pelvisPitchDropY = -std::abs(std::sin(m_aimPitch)) * 0.035f;

            float stepTurnPelvisDip = 0.0f;
            float stepTurnSway = 0.0f;
            if (m_stepTurnPhase == StepTurnPhase::Beat1_LeadFoot) {
                float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat1, 0.0f, 1.0f);
                stepTurnPelvisDip = -std::sin(t * glm::pi<float>()) * 0.024f;
                float shiftDir = (m_stepTurnFoot > 0) ? -1.0f : 1.0f;
                stepTurnSway = shiftDir * std::sin(t * glm::pi<float>()) * 0.025f;
            } else if (m_stepTurnPhase == StepTurnPhase::Beat2_TrailingFoot) {
                float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat2, 0.0f, 1.0f);
                stepTurnPelvisDip = -std::sin(t * glm::pi<float>()) * 0.016f;
                float shiftDir = (m_stepTurnFoot > 0) ? 1.0f : -1.0f;
                stepTurnSway = shiftDir * std::sin(t * glm::pi<float>()) * 0.018f;
            }

            // --- 1. PELVIS & SACRUM ---
            glm::vec3 pelvisPos = lowerToWorld(glm::vec3(m_pelvisSway + m_coverLeanAngle * 0.18f + stepTurnSway, 
                                                         0.81f + m_stanceHeight + m_pelvisBob + limpPelvisDip + coverChestElevation * 0.45f + pelvisPitchDropY + stepTurnPelvisDip, 
                                                         leanZ * 0.45f + pelvisPitchShiftZ));

            // --- 2. BIOMECHANICALLY REALISTIC TWO-BONE LEG IK (60/40 Stance/Swing Gait) ---
            // Mathematical Zero-Slip Stride Matching:
            // Stance phase occupies 60% of gait cycle (displacement 2 * stride).
            // Stride displacement stride = 0.30 * strideLength precisely cancels forward body speed.
            float currentStrideLength = m_isSprinting ? 2.10f : (m_stance == 1 ? 0.95f : 1.35f);
            currentStrideLength *= (1.0f - 0.20f * cCurve - 0.45f * pCurve);
            float stride = m_isMoving ? (0.30f * currentStrideLength) : 0.18f;
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

            auto getStanceFootWorld = [&](float bodyYaw, bool isRight) -> glm::vec3 {
                float cy = std::cos(bodyYaw);
                float sy = std::sin(bodyYaw);
                glm::vec3 fwd(sy, 0.0f, cy);
                glm::vec3 rgt(cy, 0.0f, -sy);
                float footBaseX = 0.13f + 0.05f * cCurve;
                float sign = isRight ? 1.0f : -1.0f;
                float strideZ = isRight ? glm::mix(0.0f, 0.18f, cCurve) : glm::mix(0.0f, -0.14f, cCurve);
                float strideX = isRight ? glm::mix(0.0f, 0.04f, cCurve) : glm::mix(0.0f, -0.04f, cCurve);
                return m_rootPos + rgt * (sign * footBaseX + strideX) + fwd * strideZ + glm::vec3(0, 0.07f, 0);
            };

            float footBaseX = 0.13f + 0.05f * cCurve;
            glm::vec3 leftFootPos(0.0f);
            glm::vec3 rightFootPos(0.0f);

            if (m_isMoving) {
                float phaseNorm = m_walkCycle / (2.0f * glm::pi<float>());
                float uL = std::fmod(phaseNorm, 1.0f); if (uL < 0.0f) uL += 1.0f;
                float uR = std::fmod(phaseNorm + 0.5f, 1.0f); if (uR < 0.0f) uR += 1.0f;

                float maxLift = (m_isSprinting ? 0.12f : (m_isMoving ? 0.075f : 0.040f));
                if (cCurve > 0.01f) {
                    maxLift = glm::mix(maxLift, 0.038f, cCurve); // Lower, cautious foot clearance
                }

                // Biological 60% Stance / 40% Swing Gait Function (Foot Pinned to Ground in Stance)
                auto calcGaitDisplacement = [](float u, float strideVal, float liftVal, float fwd, float rgt) -> std::pair<glm::vec3, float> {
                    const float stanceFraction = 0.60f;
                    float disp = 0.0f;
                    float curLift = 0.0f;
                    if (u <= stanceFraction) {
                        // Stance Phase: linear displacement matching forward body speed (firmly pinned foot)
                        float t = u / stanceFraction;
                        disp = strideVal * (1.0f - 2.0f * t);
                        curLift = 0.0f;
                    } else {
                        // Swing Phase: smooth cubic forward sweep
                        float t = (u - stanceFraction) / (1.0f - stanceFraction);
                        float ease = t * t * (3.0f - 2.0f * t);
                        disp = -strideVal + 2.0f * strideVal * ease;
                        float sinLift = std::sin(t * glm::pi<float>());
                        curLift = liftVal * std::pow(sinLift, 1.15f);
                    }
                    return { glm::vec3(disp * rgt * 0.75f, curLift, disp * fwd), curLift };
                };

                auto resL = calcGaitDisplacement(uL, stride, maxLift, moveFwd, moveRgt);
                leftStrideZ = resL.first.z;
                leftStrideX = resL.first.x;
                leftLift    = resL.second;

                auto resR = calcGaitDisplacement(uR, stride, maxLift, moveFwd, moveRgt);
                rightStrideZ = resR.first.z;
                rightStrideX = resR.first.x;
                rightLift    = resR.second;

                if (m_woundLimpFactor > 0.05f) {
                    leftStrideZ *= (1.0f - m_woundLimpFactor * 0.40f);
                    leftStrideX *= (1.0f - m_woundLimpFactor * 0.40f);
                    leftLift    *= (1.0f - m_woundLimpFactor * 0.50f);
                }

                leftFootPos  = lowerToWorld(glm::vec3(-footBaseX + leftStrideX,  0.07f + leftLift,  leftStrideZ));
                rightFootPos = lowerToWorld(glm::vec3( footBaseX + rightStrideX, 0.07f + rightLift, rightStrideZ));
            } else {
                // Independent Leg Turn-in-Place Stepping:
                // Support foot is 100% frozen in world space (zero slip).
                // Lead foot lifts in clearance arc, bends knee, steps, and plants.
                // Trailing foot catches up ONLY on Tier 4 (large turns).
                if (m_stepTurnPhase == StepTurnPhase::Beat1_LeadFoot) {
                    float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat1, 0.0f, 1.0f);
                    float ease = t * t * (3.0f - 2.0f * t);
                    if (m_stepTurnFoot > 0) {
                        // Turning Right: Left foot is SUPPORT (locked in world space), Right foot is LEAD
                        leftFootPos = getStanceFootWorld(m_leftFootPlantedYaw, false);
                        leftLift = 0.0f;

                        glm::vec3 startR = getStanceFootWorld(m_stepTurnStartLowerYaw, true);
                        glm::vec3 targetR = getStanceFootWorld(m_stepTurnBeat1Yaw, true);
                        rightLift = std::sin(t * glm::pi<float>()) * 0.055f;
                        rightFootPos = glm::mix(startR, targetR, ease);
                        rightFootPos.y += rightLift;
                    } else {
                        // Turning Left: Right foot is SUPPORT (locked in world space), Left foot is LEAD
                        rightFootPos = getStanceFootWorld(m_rightFootPlantedYaw, true);
                        rightLift = 0.0f;

                        glm::vec3 startL = getStanceFootWorld(m_stepTurnStartLowerYaw, false);
                        glm::vec3 targetL = getStanceFootWorld(m_stepTurnBeat1Yaw, false);
                        leftLift = std::sin(t * glm::pi<float>()) * 0.055f;
                        leftFootPos = glm::mix(startL, targetL, ease);
                        leftFootPos.y += leftLift;
                    }
                } else if (m_stepTurnPhase == StepTurnPhase::Beat2_TrailingFoot) {
                    float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat2, 0.0f, 1.0f);
                    float ease = t * t * (3.0f - 2.0f * t);
                    if (m_stepTurnFoot > 0) {
                        // Turning Right: Right foot is NOW SUPPORT (locked where planted), Left foot is TRAILING
                        rightFootPos = getStanceFootWorld(m_rightFootPlantedYaw, true);
                        rightLift = 0.0f;

                        glm::vec3 startL = getStanceFootWorld(m_leftFootPlantedYaw, false);
                        glm::vec3 targetL = getStanceFootWorld(m_stepTurnTargetLowerYaw, false);
                        leftLift = std::sin(t * glm::pi<float>()) * 0.042f;
                        leftFootPos = glm::mix(startL, targetL, ease);
                        leftFootPos.y += leftLift;
                    } else {
                        // Turning Left: Left foot is NOW SUPPORT (locked where planted), Right foot is TRAILING
                        leftFootPos = getStanceFootWorld(m_leftFootPlantedYaw, false);
                        leftLift = 0.0f;

                        glm::vec3 startR = getStanceFootWorld(m_rightFootPlantedYaw, true);
                        glm::vec3 targetR = getStanceFootWorld(m_stepTurnTargetLowerYaw, true);
                        rightLift = std::sin(t * glm::pi<float>()) * 0.042f;
                        rightFootPos = glm::mix(startR, targetR, ease);
                        rightFootPos.y += rightLift;
                    }
                } else {
                    // Stationary idle: each foot remains firmly anchored at its own planted yaw
                    leftFootPos = getStanceFootWorld(m_leftFootPlantedYaw, false);
                    rightFootPos = getStanceFootWorld(m_rightFootPlantedYaw, true);
                }
            }

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
                pelvisDrop = std::min(0.0f, std::min(deltaL, deltaR));

                leftFootPos.y = groundL + 0.07f + leftLift;
                rightFootPos.y = groundR + 0.07f + rightLift;

                normalL = getTerrainNormal(leftFootPos.x, leftFootPos.z);
                normalR = getTerrainNormal(rightFootPos.x, rightFootPos.z);
            }

            float hipBaseX = 0.13f + 0.03f * cCurve;
            glm::vec3 leftHipPos  = lowerToWorld(glm::vec3(-hipBaseX + m_pelvisSway + stepTurnSway, 0.79f + m_stanceHeight + m_pelvisBob + pelvisPitchDropY + stepTurnPelvisDip, leanZ * 0.35f + pelvisPitchShiftZ));
            glm::vec3 rightHipPos = lowerToWorld(glm::vec3( hipBaseX + m_pelvisSway + stepTurnSway, 0.79f + m_stanceHeight + m_pelvisBob + pelvisPitchDropY + stepTurnPelvisDip, leanZ * 0.35f + pelvisPitchShiftZ));

            leftHipPos.y  += pelvisDrop;
            rightHipPos.y += pelvisDrop;
            pelvisPos.y   += pelvisDrop;

            // Knee Abduction (wider knee splay in crouch prevents chest/weapon clipping)
            float kneePitchFlex = (m_aimPitch < 0.0f) ? (-std::sin(m_aimPitch) * 0.14f) : 0.0f;
            float kneeSplay = 0.16f + 0.18f * cCurve;
            glm::vec3 leftKneePole  = lowerToWorld(glm::vec3(-kneeSplay + moveRgt * 0.08f, 0.38f + m_stanceHeight * 0.45f, 0.65f + moveFwd * 0.15f + 0.25f * cCurve + kneePitchFlex));
            glm::vec3 rightKneePole = lowerToWorld(glm::vec3( kneeSplay + moveRgt * 0.08f, 0.38f + m_stanceHeight * 0.45f, 0.65f + moveFwd * 0.15f + 0.25f * cCurve + kneePitchFlex));

            // Procedural knee flexion when stepping during turn in place
            if (!m_isMoving) {
                if (m_stepTurnPhase == StepTurnPhase::Beat1_LeadFoot) {
                    float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat1, 0.0f, 1.0f);
                    float kLift = std::sin(t * glm::pi<float>()) * 0.10f;
                    if (m_stepTurnFoot > 0) {
                        rightKneePole += lowerRight * 0.09f + glm::vec3(0.0f, kLift, 0.0f);
                    } else {
                        leftKneePole  -= lowerRight * 0.09f - glm::vec3(0.0f, kLift, 0.0f);
                    }
                } else if (m_stepTurnPhase == StepTurnPhase::Beat2_TrailingFoot) {
                    float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat2, 0.0f, 1.0f);
                    float kLift = std::sin(t * glm::pi<float>()) * 0.08f;
                    if (m_stepTurnFoot > 0) {
                        leftKneePole  -= lowerRight * 0.07f - glm::vec3(0.0f, kLift, 0.0f);
                    } else {
                        rightKneePole += lowerRight * 0.07f + glm::vec3(0.0f, kLift, 0.0f);
                    }
                }
            }

            glm::vec3 leftKneePos, rightKneePos;
            glm::quat uRot, lRot;
            InverseKinematics::solveTwoBoneIK(leftHipPos, leftFootPos, leftKneePole, 0.43f, 0.41f, leftKneePos, uRot, lRot);
            InverseKinematics::solveTwoBoneIK(rightHipPos, rightFootPos, rightKneePole, 0.43f, 0.41f, rightKneePos, uRot, lRot);

            // --- 3. TORSO, CHEST & ANATOMICAL HEAD ---
            // 4-Tier Hierarchical Look-At:
            // Tier 1 (0 - 24 deg): Visage/head only. Chest remains locked with lower body.
            // Tier 2 (24 - 38 deg): Thoracic spine twist. Pelvis and boots remain 100% grounded.
            // Tier 3 (38 - 70 deg): Single-leg step out. Support foot remains 100% grounded.
            // Tier 4 (> 70 deg): Two-beat step turn with lead and trailing foot lifts.
            float deltaYaw = m_yaw - m_lowerBodyYaw;
            while (deltaYaw > glm::pi<float>()) deltaYaw -= 2.0f * glm::pi<float>();
            while (deltaYaw < -glm::pi<float>()) deltaYaw += 2.0f * glm::pi<float>();

            float chestYaw = m_yaw;
            if (!m_isMoving) {
                float absDelta = std::abs(deltaYaw);
                float sign = (deltaYaw > 0.0f) ? 1.0f : -1.0f;
                if (absDelta <= glm::radians(24.0f)) {
                    // Tier 1: Micro-glance (< 24 deg) - Chest stays locked with hips/lower body
                    chestYaw = m_lowerBodyYaw;
                } else if (absDelta <= glm::radians(38.0f)) {
                    // Tier 2: Torso twists smoothly with view (24 to 38 deg)
                    float excess = absDelta - glm::radians(24.0f);
                    chestYaw = m_lowerBodyYaw + sign * (excess * 0.70f);
                } else {
                    // Tier 3 & 4: Upper body tracks view smoothly while stepping
                    float excess = absDelta - glm::radians(24.0f);
                    chestYaw = m_lowerBodyYaw + sign * std::min(excess * 0.70f, glm::radians(32.0f));
                }
            }

            float ccy = std::cos(chestYaw);
            float csy = std::sin(chestYaw);
            glm::vec3 chestForward(csy, 0.0f, ccy);
            glm::vec3 chestRight(ccy, 0.0f, -csy);

            // Procedural Spine Pitch Distribution (60% thoracic/lumbar bending)
            float spineAimPitch = -m_aimPitch * 0.60f;
            float spineCurveForward = std::sin(spineAimPitch) * 0.22f;
            float spineCurveHeight = (1.0f - std::cos(spineAimPitch)) * 0.08f;

            glm::vec3 chestPos = pelvisPos + glm::vec3(0.0f, (0.34f - spineCurveHeight) + coverChestElevation * 0.55f, 0.0f)
                                 + chestForward * (leanZ * 0.35f + spineCurveForward)
                                 + chestRight * (m_coverLeanAngle * 0.20f)
                                 + glm::vec3(0.0f, m_isMoving ? std::sin(m_walkCycle) * 0.02f : 0.0f, 0.0f);

            float chestPitchAngle = spineAimPitch + (m_isSprinting ? 0.24f : 0.0f) + m_woundTorsoHunch * 0.25f + glm::radians(m_woundFlinchPitch);
            if (m_isVaulting) {
                chestPitchAngle += 0.32f * std::sin(m_vaultProgress * 3.14159f);
            }
            glm::mat4 chestRot = glm::rotate(glm::mat4(1.0f), chestYaw, glm::vec3(0, 1, 0));
            chestRot = glm::rotate(chestRot, chestPitchAngle, glm::vec3(1, 0, 0));
            if (std::abs(m_coverLeanAngle) > 0.001f) {
                chestRot = glm::rotate(chestRot, m_coverLeanAngle * 0.22f, glm::vec3(0, 0, 1));
            }
            if (std::abs(m_woundFlinchRoll) > 0.01f) {
                chestRot = glm::rotate(chestRot, glm::radians(m_woundFlinchRoll), glm::vec3(0, 0, 1));
            }
            glm::mat3 chestRot3 = glm::mat3(chestRot);

            glm::vec3 bindChest = m_skinnedModel.getBindJoint(BONE_SPINE_CHEST);
            glm::vec3 bindHead  = m_skinnedModel.getBindJoint(BONE_HEAD);
            glm::vec3 headJointOffset = bindHead - bindChest;
            glm::vec3 headCenter = chestPos + chestRot3 * headJointOffset;

            glm::vec3 bindRShoulder = m_skinnedModel.getBindJoint(BONE_R_UPPER_ARM);
            glm::vec3 bindLShoulder = m_skinnedModel.getBindJoint(BONE_L_UPPER_ARM);
            rightShoulder = chestPos + chestRot3 * (bindRShoulder - bindChest);
            leftShoulder  = chestPos + chestRot3 * (bindLShoulder - bindChest);

            // --- 4. ARMS & WEAPON TRACKING IK ---
            float breathY = std::sin(m_idleTime * 1.8f) * (m_aimWeight > 0.5f ? 0.003f : 0.008f);
            float breathX = std::cos(m_idleTime * 1.2f) * (m_aimWeight > 0.5f ? 0.002f : 0.006f);

            glm::vec3 rightHandTarget;
            glm::vec3 leftHandTarget;
            glm::vec3 rightElbowPole;
            glm::vec3 leftElbowPole;
            uprightRifleRoot = glm::vec3(0.0f);
            aimForward = glm::vec3(0.0f);
            glm::vec3 rifleUp(0.0f, 1.0f, 0.0f);
            uprightRenderPitch = m_aimPitch;

            if (shouldDrawWeapon) {
                if (m_isVaulting) {
                    uprightRenderPitch = 0.52f;
                    float cp = std::cos(uprightRenderPitch);
                    float sp = -std::sin(uprightRenderPitch);
                    aimForward = forwardDir * cp + glm::vec3(0, sp, 0);
                    rifleUp = glm::normalize(glm::cross(aimForward, rightDir));

                    uprightRifleRoot = chestPos + rightDir * 0.16f - glm::vec3(0.0f, 0.12f, 0.0f) + aimForward * 0.14f;
                    rightHandTarget = uprightRifleRoot - aimForward * 0.06f - rifleUp * 0.06f;
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
                    uprightRenderPitch = -0.15f;
                    float cp = std::cos(uprightRenderPitch);
                    float sp = -std::sin(uprightRenderPitch);
                    aimForward = forwardDir * cp + glm::vec3(0, sp, 0);
                    rifleUp = glm::normalize(glm::cross(aimForward, rightDir));

                    uprightRifleRoot = chestPos + rightDir * 0.12f - glm::vec3(0.0f, 0.05f, 0.0f) + aimForward * 0.30f;
                    rightHandTarget = uprightRifleRoot - aimForward * 0.08f - rifleUp * 0.07f;
                    rightElbowPole = chestPos + rightDir * 0.45f - aimForward * 0.10f - rifleUp * 0.18f;

                    float p = m_reloadProgress;
                    glm::vec3 magPouch = chestPos - rightDir * 0.12f - glm::vec3(0.0f, 0.28f, 0.0f) + aimForward * 0.08f;
                    glm::vec3 magWellApproach = uprightRifleRoot + aimForward * 0.08f - rifleUp * 0.18f;
                    glm::vec3 magWellInsert = uprightRifleRoot + aimForward * 0.08f - rifleUp * 0.06f;
                    glm::vec3 boltRelease = uprightRifleRoot + aimForward * 0.04f + rifleUp * 0.04f;
                    glm::vec3 handguardPos = uprightRifleRoot + aimForward * 0.26f - rifleUp * 0.05f;

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
                    uprightRenderPitch = 0.42f;
                    float cp = std::cos(uprightRenderPitch);
                    float sp = -std::sin(uprightRenderPitch);
                    aimForward = forwardDir * cp + glm::vec3(0, sp, 0);
                    rifleUp = glm::normalize(glm::cross(aimForward, rightDir));

                    float sprintBobY = std::abs(std::sin(m_walkCycle)) * 0.040f;
                    float sprintBobX = std::sin(m_walkCycle) * 0.028f;
                    uprightRifleRoot = chestPos + rightDir * 0.12f - glm::vec3(0.0f, 0.10f - sprintBobY, 0.0f) + aimForward * (0.16f + sprintBobX);

                    bool isPistol = (weaponToDraw.find("M9") != std::string::npos ||
                                     weaponToDraw.find("Beretta") != std::string::npos ||
                                     weaponToDraw.find("Pistol") != std::string::npos);
                    if (isPistol) {
                        rightHandTarget = uprightRifleRoot - aimForward * 0.02f - rifleUp * 0.05f;
                        leftHandTarget  = uprightRifleRoot - aimForward * 0.00f - rifleUp * 0.055f - rightDir * 0.03f;
                        rightElbowPole = chestPos + rightDir * 0.38f - rifleUp * 0.20f;
                        leftElbowPole  = chestPos - rightDir * 0.38f - rifleUp * 0.20f;
                    } else {
                        rightHandTarget = uprightRifleRoot - aimForward * 0.06f - rifleUp * 0.06f;
                        leftHandTarget  = uprightRifleRoot + aimForward * 0.16f - rifleUp * 0.02f - rightDir * 0.02f;
                        rightElbowPole = chestPos + rightDir * 0.42f - aimForward * 0.08f - rifleUp * 0.18f;
                        leftElbowPole  = chestPos - rightDir * 0.42f + aimForward * 0.06f - rifleUp * 0.20f;
                    }

                } else {
                    const auto& gripCfg = WeaponRigManager::get().getConfig(weaponToDraw);
                    uprightRenderPitch = glm::mix(gripCfg.lowReadyPitch, -m_aimPitch, m_aimWeight) - m_recoilKick * 2.8f;
                    float cp = std::cos(uprightRenderPitch);
                    float sp = -std::sin(uprightRenderPitch);
                    aimForward = forwardDir * cp + glm::vec3(0, sp, 0);
                    rifleUp = glm::normalize(glm::cross(aimForward, rightDir));

                    float rootY = glm::mix(gripCfg.rootOffset.y, gripCfg.rootOffsetADS.y, m_aimWeight) + breathY;
                    float rootRight = glm::mix(gripCfg.rootOffset.x, gripCfg.rootOffsetADS.x, m_aimWeight) + breathX;
                    if (!m_shoulderRight) rootRight = -rootRight;
                    float rootForward = glm::mix(gripCfg.rootOffset.z, gripCfg.rootOffsetADS.z, m_aimWeight) - m_recoilKick;

                    uprightRifleRoot = chestPos + rightDir * rootRight + glm::vec3(0, rootY, 0) + aimForward * rootForward;

                    if (m_inCover && m_isLowCover && m_coverAimWeight > 0.001f) {
                        float desiredRifleY = m_rootPos.y + m_coverHeight + 0.04f;
                        uprightRifleRoot.y = glm::mix(uprightRifleRoot.y, desiredRifleY, m_coverAimWeight);
                        uprightRifleRoot += forwardDir * (0.18f * m_coverAimWeight);
                        uprightRenderPitch = glm::mix(uprightRenderPitch, -m_aimPitch * 0.85f, m_coverAimWeight);
                    }

                    glm::vec3 rHandOffset = gripCfg.rightHandOffset;
                    rightHandTarget = uprightRifleRoot + rightDir * rHandOffset.x + rifleUp * rHandOffset.y + aimForward * rHandOffset.z;

                    glm::vec3 lHandOffset = glm::mix(gripCfg.leftHandOffset, gripCfg.leftHandOffsetADS, m_aimWeight);
                    leftHandTarget = uprightRifleRoot + rightDir * lHandOffset.x + rifleUp * lHandOffset.y + aimForward * lHandOffset.z;

                    if (m_shoulderRight) {
                        rightElbowPole = chestPos + rightDir * 0.42f - rifleUp * 0.22f - aimForward * 0.04f;
                        leftElbowPole  = chestPos - rightDir * 0.46f - rifleUp * 0.24f + aimForward * 0.06f;
                    } else {
                        rightElbowPole = chestPos + rightDir * 0.24f - rifleUp * 0.22f + aimForward * 0.04f;
                        leftElbowPole  = chestPos - rightDir * 0.42f - rifleUp * 0.24f - aimForward * 0.04f;
                    }
                }
            } else {
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

            // --- 5. ASSEMBLE UPRIGHT SKELETON POSE ---
            // Dynamic Pelvis 3D Roll (Trendelenburg gait weight transfer) & Yaw Twist
            float pelvisRoll = (m_isMoving ? std::sin(m_walkCycle) * 0.045f : 0.0f) * (1.0f - 0.40f * cCurve);
            float pelvisYawTwist = m_isMoving ? -std::sin(m_walkCycle) * 0.032f : 0.0f;

            uprightPose.bonePositions[BONE_PELVIS] = pelvisPos;
            glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), m_lowerBodyYaw + pelvisYawTwist, glm::vec3(0, 1, 0));
            rotY = glm::rotate(rotY, pelvisRoll, glm::vec3(0, 0, 1));
            float pelvisPitchAngle = (m_aimPitch < 0.0f) ? (-m_aimPitch * 0.12f) : (-m_aimPitch * 0.08f);
            rotY = glm::rotate(rotY, pelvisPitchAngle, glm::vec3(1, 0, 0));
            if (cCurve > 0.01f) rotY = glm::rotate(rotY, 0.22f * cCurve, glm::vec3(1, 0, 0));
            uprightPose.boneRotations[BONE_PELVIS] = glm::mat3(rotY);

            uprightPose.bonePositions[BONE_SPINE_CHEST] = chestPos;
            uprightPose.boneRotations[BONE_SPINE_CHEST] = chestRot3;

            uprightPose.bonePositions[BONE_HEAD] = headCenter;
            float totalHeadPitch = -m_aimPitch;
            glm::mat4 headRot = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
            headRot = glm::rotate(headRot, totalHeadPitch, glm::vec3(1, 0, 0));
            uprightPose.boneRotations[BONE_HEAD] = glm::mat3(headRot);

            float lFootYaw = m_leftFootPlantedYaw;
            float rFootYaw = m_rightFootPlantedYaw;

            if (m_isMoving) {
                lFootYaw = m_lowerBodyYaw;
                rFootYaw = m_lowerBodyYaw;
            } else {
                if (m_stepTurnPhase == StepTurnPhase::Beat1_LeadFoot) {
                    float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat1, 0.0f, 1.0f);
                    float ease = t * t * (3.0f - 2.0f * t);
                    if (m_stepTurnFoot > 0) {
                        // Turning Right: Left foot is SUPPORT (100% frozen in world space), Right foot is LEAD
                        lFootYaw = m_leftFootPlantedYaw;
                        rFootYaw = glm::mix(m_stepTurnStartLowerYaw, m_stepTurnBeat1Yaw, ease);
                    } else {
                        // Turning Left: Right foot is SUPPORT (100% frozen in world space), Left foot is LEAD
                        rFootYaw = m_rightFootPlantedYaw;
                        lFootYaw = glm::mix(m_stepTurnStartLowerYaw, m_stepTurnBeat1Yaw, ease);
                    }
                } else if (m_stepTurnPhase == StepTurnPhase::Beat2_TrailingFoot) {
                    float t = std::clamp(m_stepTurnTimer / m_stepTurnDurationBeat2, 0.0f, 1.0f);
                    float ease = t * t * (3.0f - 2.0f * t);
                    if (m_stepTurnFoot > 0) {
                        // Turning Right: Right foot is SUPPORT (frozen at beat1 plant), Left foot is TRAILING
                        rFootYaw = m_rightFootPlantedYaw;
                        lFootYaw = glm::mix(m_leftFootPlantedYaw, m_stepTurnTargetLowerYaw, ease);
                    } else {
                        // Turning Left: Left foot is SUPPORT (frozen at beat1 plant), Right foot is TRAILING
                        lFootYaw = m_leftFootPlantedYaw;
                        rFootYaw = glm::mix(m_rightFootPlantedYaw, m_stepTurnTargetLowerYaw, ease);
                    }
                }
            }

            glm::mat3 lowerRotL = glm::mat3(glm::rotate(glm::mat4(1.0f), lFootYaw, glm::vec3(0, 1, 0)));
            glm::mat3 lowerRotR = glm::mat3(glm::rotate(glm::mat4(1.0f), rFootYaw, glm::vec3(0, 1, 0)));
            glm::vec3 bindThighDir(0.0f, -1.0f, 0.0f);
            glm::vec3 bindCalfDir(0.0f, -1.0f, 0.0f);

            uprightPose.bonePositions[BONE_L_THIGH] = leftHipPos;
            glm::mat3 stdLThighRot = SkinnedSoldier::computeLimbRotation(bindThighDir, leftKneePos - leftHipPos, lowerRotL);
            uprightPose.boneRotations[BONE_L_THIGH] = stdLThighRot;

            uprightPose.bonePositions[BONE_L_CALF] = leftKneePos;
            uprightPose.boneRotations[BONE_L_CALF] = SkinnedSoldier::computeLimbRotation(bindCalfDir, leftFootPos - leftKneePos, stdLThighRot);

            // Dynamic Foot Roll & Push-off Ankle Flexion
            auto calcAnklePitch = [](float u, float crouchC) -> float {
                const float stanceFraction = 0.60f;
                float pitch = 0.0f;
                if (u <= stanceFraction) {
                    float t = u / stanceFraction;
                    if (t < 0.18f) pitch = glm::mix(-0.20f, 0.0f, t / 0.18f); // Heel-strike
                    else if (t < 0.75f) pitch = 0.0f;                           // Full plant
                    else pitch = glm::mix(0.0f, 0.36f, (t - 0.75f) / 0.25f);   // Toe push-off
                } else {
                    float t = (u - stanceFraction) / (1.0f - stanceFraction);
                    pitch = glm::mix(0.36f, -0.20f, glm::smoothstep(0.0f, 1.0f, t));
                }
                return pitch * (1.0f - crouchC * 0.5f);
            };

            float phaseNorm = m_walkCycle / (2.0f * glm::pi<float>());
            float uL = std::fmod(phaseNorm, 1.0f); if (uL < 0.0f) uL += 1.0f;
            float uR = std::fmod(phaseNorm + 0.5f, 1.0f); if (uR < 0.0f) uR += 1.0f;

            float lAnklePitch = m_isMoving ? calcAnklePitch(uL, cCurve) : 0.0f;
            glm::mat4 lFootRot = glm::rotate(glm::mat4(1.0f), lFootYaw, glm::vec3(0, 1, 0));
            lFootRot = glm::rotate(lFootRot, lAnklePitch, glm::vec3(1, 0, 0));
            if (!m_isAirborne) {
                float slopePitchL = glm::dot(normalL, lowerForward);
                float slopeRollL  = glm::dot(normalL, lowerRight);
                lFootRot = glm::rotate(lFootRot, -slopePitchL * 0.75f, glm::vec3(1, 0, 0));
                lFootRot = glm::rotate(lFootRot, slopeRollL * 0.75f, glm::vec3(0, 0, 1));
            }
            uprightPose.bonePositions[BONE_L_FOOT] = leftFootPos;
            uprightPose.boneRotations[BONE_L_FOOT] = glm::mat3(lFootRot);

            uprightPose.bonePositions[BONE_R_THIGH] = rightHipPos;
            glm::mat3 stdRThighRot = SkinnedSoldier::computeLimbRotation(bindThighDir, rightKneePos - rightHipPos, lowerRotR);
            uprightPose.boneRotations[BONE_R_THIGH] = stdRThighRot;

            uprightPose.bonePositions[BONE_R_CALF] = rightKneePos;
            uprightPose.boneRotations[BONE_R_CALF] = SkinnedSoldier::computeLimbRotation(bindCalfDir, rightFootPos - rightKneePos, stdRThighRot);

            float rAnklePitch = m_isMoving ? calcAnklePitch(uR, cCurve) : 0.0f;
            glm::mat4 rFootRot = glm::rotate(glm::mat4(1.0f), rFootYaw, glm::vec3(0, 1, 0));
            rFootRot = glm::rotate(rFootRot, rAnklePitch, glm::vec3(1, 0, 0));
            if (!m_isAirborne) {
                float slopePitchR = glm::dot(normalR, lowerForward);
                float slopeRollR  = glm::dot(normalR, lowerRight);
                rFootRot = glm::rotate(rFootRot, -slopePitchR * 0.75f, glm::vec3(1, 0, 0));
                rFootRot = glm::rotate(rFootRot, slopeRollR * 0.75f, glm::vec3(0, 0, 1));
            }
            uprightPose.bonePositions[BONE_R_FOOT] = rightFootPos;
            uprightPose.boneRotations[BONE_R_FOOT] = glm::mat3(rFootRot);

            // Upper Extremities
            glm::vec3 bindLArmDir = glm::normalize(glm::vec3(-0.08f, -0.30f, 0.0f));
            glm::vec3 bindLForeDir = glm::normalize(glm::vec3(-0.02f, -0.30f, 0.0f));
            glm::vec3 bindRArmDir = glm::normalize(glm::vec3(0.08f, -0.30f, 0.0f));
            glm::vec3 bindRForeDir = glm::normalize(glm::vec3(0.02f, -0.30f, 0.0f));

            uprightPose.bonePositions[BONE_L_UPPER_ARM] = leftShoulder;
            glm::mat3 stdLArmRot = SkinnedSoldier::computeLimbRotation(bindLArmDir, leftElbowPos - leftShoulder, chestRot3);
            uprightPose.boneRotations[BONE_L_UPPER_ARM] = stdLArmRot;

            uprightPose.bonePositions[BONE_L_FOREARM] = leftElbowPos;
            uprightPose.boneRotations[BONE_L_FOREARM] = SkinnedSoldier::computeLimbRotation(bindLForeDir, leftHandTarget - leftElbowPos, stdLArmRot);

            uprightPose.bonePositions[BONE_L_HAND] = leftHandTarget;

            uprightPose.bonePositions[BONE_R_UPPER_ARM] = rightShoulder;
            glm::mat3 stdRArmRot = SkinnedSoldier::computeLimbRotation(bindRArmDir, rightElbowPos - rightShoulder, chestRot3);
            uprightPose.boneRotations[BONE_R_UPPER_ARM] = stdRArmRot;

            uprightPose.bonePositions[BONE_R_FOREARM] = rightElbowPos;
            uprightPose.boneRotations[BONE_R_FOREARM] = SkinnedSoldier::computeLimbRotation(bindRForeDir, rightHandTarget - rightElbowPos, stdRArmRot);

            uprightPose.bonePositions[BONE_R_HAND] = rightHandTarget;

            if (shouldDrawWeapon) {
                glm::vec3 gripAxis = glm::normalize(-rifleUp * 0.94f - aimForward * 0.34f);
                glm::vec3 palmFacing = glm::normalize(aimForward * 0.94f - rifleUp * 0.34f);
                glm::vec3 lateralAxis = glm::normalize(glm::cross(gripAxis, palmFacing));

                uprightPose.boneRotations[BONE_R_HAND] = glm::mat3(lateralAxis, -gripAxis, palmFacing);

                glm::vec3 triggerBase = rightHandTarget + palmFacing * 0.010f - lateralAxis * 0.016f;
                uprightPose.bonePositions[BONE_R_TRIGGER_FINGER] = triggerBase;

                float curlAngle = m_rightHand.triggerPull * 0.42f;
                glm::mat4 baseTrig = glm::mat4(uprightPose.boneRotations[BONE_R_HAND]);
                glm::mat4 trigRot = glm::rotate(baseTrig, curlAngle, glm::vec3(1.0f, 0.0f, 0.0f));
                uprightPose.boneRotations[BONE_R_TRIGGER_FINGER] = glm::mat3(trigRot);

                glm::vec3 lHandAxis = glm::normalize(aimForward * 0.88f + rifleUp * 0.22f);
                glm::vec3 lPalmFacing = glm::normalize(rifleUp * 0.88f - aimForward * 0.22f);
                glm::vec3 lLateralAxis = glm::normalize(glm::cross(lPalmFacing, lHandAxis));

                uprightPose.boneRotations[BONE_L_HAND] = glm::mat3(lLateralAxis, -lHandAxis, lPalmFacing);
            } else {
                glm::vec3 rForeDir = glm::normalize(rightHandTarget - rightElbowPos);
                glm::vec3 lForeDir = glm::normalize(leftHandTarget - leftElbowPos);

                glm::vec3 rPalm = glm::normalize(-rightDir * 0.90f + forwardDir * 0.25f);
                glm::vec3 rLat = glm::normalize(glm::cross(rForeDir, rPalm));
                uprightPose.boneRotations[BONE_R_HAND] = glm::mat3(rLat, -rForeDir, rPalm);

                uprightPose.bonePositions[BONE_R_TRIGGER_FINGER] = rightHandTarget + rPalm * 0.008f - rLat * 0.016f;
                uprightPose.boneRotations[BONE_R_TRIGGER_FINGER] = uprightPose.boneRotations[BONE_R_HAND];

                glm::vec3 lPalm = glm::normalize(rightDir * 0.90f + forwardDir * 0.25f);
                glm::vec3 lLat = glm::normalize(glm::cross(lForeDir, lPalm));
                uprightPose.boneRotations[BONE_L_HAND] = glm::mat3(lLat, -lForeDir, lPalm);
            }
        }

        // -------------------------------------------------------------------------
        // B. PRONE SKELETON SYNTHESIS (Only calculated when pCurve > 0.001f)
        // -------------------------------------------------------------------------
        if (pCurve <= 0.001f) {
            m_pose = uprightPose;
            rifleRoot = uprightRifleRoot;
            renderPitch = uprightRenderPitch;
        } else {
            SkeletonPose pronePose;
            glm::vec3 proneRifleRoot(0.0f);
            float proneRenderPitch = m_aimPitch;

            {
                float crawlPhase = m_walkCycle;
                float crawlBob = (m_isMoving) ? std::sin(crawlPhase * 2.0f) * 0.015f : 0.0f;
                float crawlLateral = (m_isMoving) ? std::sin(crawlPhase) * 0.025f : 0.0f;

                glm::vec3 pelvisPosProne = m_rootPos + rightDir * crawlLateral + glm::vec3(0.0f, 0.14f + crawlBob, 0.0f) - forwardDir * 0.12f;
                glm::vec3 chestPosProne = pelvisPosProne + forwardDir * 0.38f + glm::vec3(0.0f, 0.08f, 0.0f);
                glm::vec3 headCenterProne = chestPosProne + forwardDir * 0.28f + glm::vec3(0.0f, 0.06f, 0.0f);

                float leftCrawlZ = 0.0f, rightCrawlZ = 0.0f;
                float leftKneeOut = 0.0f, rightKneeOut = 0.0f;
                float leftLiftProne = 0.0f, rightLiftProne = 0.0f;

                if (m_isMoving) {
                    float sinP = std::sin(crawlPhase);
                    leftCrawlZ   =  sinP * 0.18f;
                    rightCrawlZ  = -sinP * 0.18f;
                    leftKneeOut  = std::max(0.0f,  sinP) * 0.14f;
                    rightKneeOut = std::max(0.0f, -sinP) * 0.14f;
                    leftLiftProne = std::max(0.0f,  sinP * 0.035f);
                    rightLiftProne = std::max(0.0f, -sinP * 0.035f);
                }

                glm::vec3 leftHipProne  = pelvisPosProne - rightDir * 0.13f;
                glm::vec3 rightHipProne = pelvisPosProne + rightDir * 0.13f;

                glm::vec3 leftFootProne  = pelvisPosProne - rightDir * (0.18f + leftKneeOut * 0.6f) - forwardDir * (0.80f - leftCrawlZ) + glm::vec3(0.0f, 0.06f + leftLiftProne, 0.0f);
                glm::vec3 rightFootProne = pelvisPosProne + rightDir * (0.18f + rightKneeOut * 0.6f) - forwardDir * (0.80f - rightCrawlZ) + glm::vec3(0.0f, 0.06f + rightLiftProne, 0.0f);

                glm::vec3 leftKneePoleProne  = pelvisPosProne - rightDir * (0.26f + leftKneeOut) - forwardDir * (0.40f - leftCrawlZ * 0.5f) + glm::vec3(0.0f, 0.09f, 0.0f);
                glm::vec3 rightKneePoleProne = pelvisPosProne + rightDir * (0.26f + rightKneeOut) - forwardDir * (0.40f - rightCrawlZ * 0.5f) + glm::vec3(0.0f, 0.09f, 0.0f);

                glm::vec3 leftKneeProne, rightKneeProne;
                glm::quat uRotP, lRotP;
                InverseKinematics::solveTwoBoneIK(leftHipProne, leftFootProne, leftKneePoleProne, 0.40f, 0.38f, leftKneeProne, uRotP, lRotP);
                InverseKinematics::solveTwoBoneIK(rightHipProne, rightFootProne, rightKneePoleProne, 0.40f, 0.38f, rightKneeProne, uRotP, lRotP);

                glm::vec3 rShoulderProne = chestPosProne + rightDir * 0.22f + forwardDir * 0.04f;
                glm::vec3 lShoulderProne = chestPosProne - rightDir * 0.22f + forwardDir * 0.04f;

                glm::vec3 aimFwdProne = forwardDir;
                glm::vec3 rifleUpProne(0.0f, 1.0f, 0.0f);

                glm::vec3 rElbowProne = chestPosProne + rightDir * 0.22f + forwardDir * 0.14f - glm::vec3(0.0f, 0.14f, 0.0f);
                glm::vec3 lElbowProne = chestPosProne - rightDir * 0.22f + forwardDir * 0.18f - glm::vec3(0.0f, 0.14f, 0.0f);

                proneRifleRoot = chestPosProne + rightDir * 0.08f + forwardDir * 0.24f - glm::vec3(0.0f, 0.02f, 0.0f);
                glm::vec3 rHandProne = proneRifleRoot - aimFwdProne * 0.08f - rifleUpProne * 0.06f;
                glm::vec3 lHandProne = proneRifleRoot + aimFwdProne * 0.24f - rifleUpProne * 0.04f;

                if (m_isReloading) {
                    float p = m_reloadProgress;
                    glm::vec3 magPouch = chestPosProne - rightDir * 0.12f + forwardDir * 0.06f - glm::vec3(0.0f, 0.08f, 0.0f);
                    glm::vec3 magWell = proneRifleRoot + aimFwdProne * 0.08f - rifleUpProne * 0.06f;
                    glm::vec3 handguardPos = proneRifleRoot + aimFwdProne * 0.24f - rifleUpProne * 0.04f;
                    if (p < 0.35f) lHandProne = glm::mix(handguardPos, magPouch, p / 0.35f);
                    else if (p < 0.70f) lHandProne = glm::mix(magPouch, magWell, (p - 0.35f) / 0.35f);
                    else lHandProne = glm::mix(magWell, handguardPos, (p - 0.70f) / 0.30f);
                }

                pronePose.bonePositions[BONE_PELVIS] = pelvisPosProne;
                glm::mat4 rotYProne = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
                rotYProne = glm::rotate(rotYProne, 1.46f, glm::vec3(1, 0, 0));
                pronePose.boneRotations[BONE_PELVIS] = glm::mat3(rotYProne);

                pronePose.bonePositions[BONE_SPINE_CHEST] = chestPosProne;
                glm::mat4 chestRotProne = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
                chestRotProne = glm::rotate(chestRotProne, 1.38f + proneRenderPitch * 0.15f, glm::vec3(1, 0, 0));
                pronePose.boneRotations[BONE_SPINE_CHEST] = glm::mat3(chestRotProne);

                glm::vec3 bindChestP = m_skinnedModel.getBindJoint(BONE_SPINE_CHEST);
                glm::vec3 bindHeadP  = m_skinnedModel.getBindJoint(BONE_HEAD);
                glm::vec3 headJointOffsetP = bindHeadP - bindChestP;
                pronePose.bonePositions[BONE_HEAD] = chestPosProne + glm::mat3(chestRotProne) * headJointOffsetP;
                glm::mat4 headRotProne = glm::rotate(glm::mat4(1.0f), m_yaw, glm::vec3(0, 1, 0));
                headRotProne = glm::rotate(headRotProne, proneRenderPitch * 0.45f, glm::vec3(1, 0, 0));
                pronePose.boneRotations[BONE_HEAD] = glm::mat3(headRotProne);

                glm::mat3 lowerRotP = glm::mat3(rotYProne);
                glm::mat3 chestRot3P = glm::mat3(chestRotProne);
                glm::vec3 bindThighDirP(0.0f, -1.0f, 0.0f);
                glm::vec3 bindCalfDirP(0.0f, -1.0f, 0.0f);

                pronePose.bonePositions[BONE_L_THIGH] = leftHipProne;
                glm::mat3 pLThighRot = SkinnedSoldier::computeLimbRotation(bindThighDirP, leftKneeProne - leftHipProne, lowerRotP);
                pronePose.boneRotations[BONE_L_THIGH] = pLThighRot;
                pronePose.bonePositions[BONE_L_CALF] = leftKneeProne;
                pronePose.boneRotations[BONE_L_CALF] = SkinnedSoldier::computeLimbRotation(bindCalfDirP, leftFootProne - leftKneeProne, pLThighRot);
                pronePose.bonePositions[BONE_L_FOOT] = leftFootProne;
                pronePose.boneRotations[BONE_L_FOOT] = glm::mat3(rotYProne);

                pronePose.bonePositions[BONE_R_THIGH] = rightHipProne;
                glm::mat3 pRThighRot = SkinnedSoldier::computeLimbRotation(bindThighDirP, rightKneeProne - rightHipProne, lowerRotP);
                pronePose.boneRotations[BONE_R_THIGH] = pRThighRot;
                pronePose.bonePositions[BONE_R_CALF] = rightKneeProne;
                pronePose.boneRotations[BONE_R_CALF] = SkinnedSoldier::computeLimbRotation(bindCalfDirP, rightFootProne - rightKneeProne, pRThighRot);
                pronePose.bonePositions[BONE_R_FOOT] = rightFootProne;
                pronePose.boneRotations[BONE_R_FOOT] = glm::mat3(rotYProne);

                glm::vec3 bindLArmDirP = glm::normalize(glm::vec3(-0.08f, -0.30f, 0.0f));
                glm::vec3 bindLForeDirP = glm::normalize(glm::vec3(-0.02f, -0.30f, 0.0f));
                glm::vec3 bindRArmDirP = glm::normalize(glm::vec3(0.08f, -0.30f, 0.0f));
                glm::vec3 bindRForeDirP = glm::normalize(glm::vec3(0.02f, -0.30f, 0.0f));

                pronePose.bonePositions[BONE_L_UPPER_ARM] = lShoulderProne;
                glm::mat3 pLArmRot = SkinnedSoldier::computeLimbRotation(bindLArmDirP, lElbowProne - lShoulderProne, chestRot3P);
                pronePose.boneRotations[BONE_L_UPPER_ARM] = pLArmRot;
                pronePose.bonePositions[BONE_L_FOREARM] = lElbowProne;
                pronePose.boneRotations[BONE_L_FOREARM] = SkinnedSoldier::computeLimbRotation(bindLForeDirP, lHandProne - lElbowProne, pLArmRot);
                pronePose.bonePositions[BONE_L_HAND] = lHandProne;
                pronePose.boneRotations[BONE_L_HAND] = glm::mat3(rotYProne);

                pronePose.bonePositions[BONE_R_UPPER_ARM] = rShoulderProne;
                glm::mat3 pRArmRot = SkinnedSoldier::computeLimbRotation(bindRArmDirP, rElbowProne - rShoulderProne, chestRot3P);
                pronePose.boneRotations[BONE_R_UPPER_ARM] = pRArmRot;
                pronePose.bonePositions[BONE_R_FOREARM] = rElbowProne;
                pronePose.boneRotations[BONE_R_FOREARM] = SkinnedSoldier::computeLimbRotation(bindRForeDirP, rHandProne - rElbowProne, pRArmRot);
                pronePose.bonePositions[BONE_R_HAND] = rHandProne;
                pronePose.boneRotations[BONE_R_HAND] = glm::mat3(rotYProne);

                pronePose.bonePositions[BONE_R_TRIGGER_FINGER] = rHandProne;
                pronePose.boneRotations[BONE_R_TRIGGER_FINGER] = glm::mat3(rotYProne);
            }

            if (pCurve >= 0.999f) {
                m_pose = pronePose;
                rifleRoot = proneRifleRoot;
                renderPitch = proneRenderPitch;
            } else {
                // Spherical interpolation across all skeleton bones
                for (int b = 0; b < NUM_CHARACTER_BONES; ++b) {
                    m_pose.bonePositions[b] = glm::mix(uprightPose.bonePositions[b], pronePose.bonePositions[b], pCurve);
                    glm::quat qA = glm::quat_cast(uprightPose.boneRotations[b]);
                    glm::quat qB = glm::quat_cast(pronePose.boneRotations[b]);
                    m_pose.boneRotations[b] = glm::mat3_cast(glm::slerp(qA, qB, pCurve));
                }
                rifleRoot = glm::mix(uprightRifleRoot, proneRifleRoot, pCurve);
                renderPitch = glm::mix(uprightRenderPitch, proneRenderPitch, pCurve);

                // Procedural Dive-to-Prone Torso Pitch & Ground Brace
                float diveArc = std::sin(pCurve * glm::pi<float>());
                glm::mat4 divePelvis = glm::rotate(glm::mat4(m_pose.boneRotations[BONE_PELVIS]), diveArc * 0.35f, glm::vec3(1, 0, 0));
                m_pose.boneRotations[BONE_PELVIS] = glm::mat3(divePelvis);
                glm::mat4 diveChest = glm::rotate(glm::mat4(m_pose.boneRotations[BONE_SPINE_CHEST]), diveArc * 0.28f, glm::vec3(1, 0, 0));
                m_pose.boneRotations[BONE_SPINE_CHEST] = glm::mat3(diveChest);

                // Left hand reaches to brace ground impact during mid-descent
                if (pCurve > 0.20f && pCurve < 0.80f) {
                    float braceFactor = std::sin((pCurve - 0.20f) / 0.60f * glm::pi<float>());
                    float groundTargetY = World::sampleTerrainHeight(m_pose.bonePositions[BONE_L_HAND].x, m_pose.bonePositions[BONE_L_HAND].z) + 0.05f;
                    m_pose.bonePositions[BONE_L_HAND].y = glm::mix(m_pose.bonePositions[BONE_L_HAND].y, groundTargetY, braceFactor * 0.70f);
                }
            }
        }
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

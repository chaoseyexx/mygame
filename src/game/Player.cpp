#include "Player.hpp"
#include "engine/InputConfig.hpp"
#include "ChatSystem.hpp"
#include "engine/Audio.hpp"
#include "engine/Camera.hpp"
#include "engine/Network.hpp"
#include <imgui.h>
#include <cmath>
#include <random>

namespace Mercenary {

Player::Player() {
    m_position = glm::vec3(0.0f, 0.0f, 3.0f);
    m_prevPosition = m_position;
}

struct CoverCandidate {
    bool valid = false;
    glm::vec3 snapPos{0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    glm::vec3 wallDir{1.0f, 0.0f, 0.0f};
    float height = 1.0f;
    float depth = 0.6f;
    bool isLowCover = true;
    float dist = 999.0f;
    float halfWidth = 1.0f;
    float wallOffset = 0.0f;
    glm::vec3 center{0.0f};
    bool isAtEdge = false;
    float edgeDir = 0.0f; // -1 for left edge, +1 for right edge
};

static CoverCandidate findBestCover(const glm::vec3& playerPos, const World& world) {
    CoverCandidate best;
    float bestDist = 2.4f;
    const float standOff = 0.26f; // Torso clearance from wall face

    for (const auto& obs : world.getObstacles()) {
        bool providesCover = (
            obs.type == ObstacleType::Sandbag ||
            obs.type == ObstacleType::CargoPallet ||
            obs.type == ObstacleType::WoodenCrate ||
            obs.type == ObstacleType::DragonTooth ||
            obs.type == ObstacleType::HescoBastion ||
            obs.type == ObstacleType::RuinedWall ||
            obs.type == ObstacleType::ConcreteBunker ||
            obs.type == ObstacleType::DamagedBuilding ||
            obs.type == ObstacleType::MilitaryTruck ||
            obs.type == ObstacleType::TankWreck ||
            obs.type == ObstacleType::ConcreteBarrier ||
            obs.type == ObstacleType::ObstacleWall
        );
        if (!providesCover) continue;

        // World to local (3D Y-axis inverse rotation)
        float cosY = std::cos(-obs.yaw);
        float sinY = std::sin(-obs.yaw);
        glm::vec3 rel = playerPos - obs.position;
        glm::vec3 localPos(
            cosY * rel.x - sinY * rel.z,
            rel.y,
            sinY * rel.x + cosY * rel.z
        );

        glm::vec3 half = obs.size * 0.5f;
        float clampedX = std::clamp(localPos.x, -half.x, half.x);
        float clampedZ = std::clamp(localPos.z, -half.z, half.z);

        float dx1 = std::abs(localPos.x - (-half.x));
        float dx2 = std::abs(localPos.x - half.x);
        float dz1 = std::abs(localPos.z - (-half.z));
        float dz2 = std::abs(localPos.z - half.z);

        glm::vec3 localNormal(-1.0f, 0.0f, 0.0f);
        glm::vec3 localWallDir(0.0f, 0.0f, 1.0f);
        glm::vec3 localSnap(-half.x - standOff, 0.0f, clampedZ);
        float minSideDist = dx1;
        float sideDepth = obs.size.x;
        float faceHalfWidth = half.z;
        float faceWallOffset = clampedZ;
        bool atEdge = (half.z - std::abs(clampedZ) <= 0.45f);
        float atEdgeDir = (clampedZ >= 0.0f) ? 1.0f : -1.0f;

        if (dx2 < minSideDist) {
            minSideDist = dx2;
            localNormal = glm::vec3(1.0f, 0.0f, 0.0f);
            localWallDir = glm::vec3(0.0f, 0.0f, -1.0f);
            localSnap = glm::vec3(half.x + standOff, 0.0f, clampedZ);
            sideDepth = obs.size.x;
            faceHalfWidth = half.z;
            faceWallOffset = -clampedZ;
            atEdge = (half.z - std::abs(clampedZ) <= 0.45f);
            atEdgeDir = (clampedZ <= 0.0f) ? 1.0f : -1.0f;
        }
        if (dz1 < minSideDist) {
            minSideDist = dz1;
            localNormal = glm::vec3(0.0f, 0.0f, -1.0f);
            localWallDir = glm::vec3(-1.0f, 0.0f, 0.0f);
            localSnap = glm::vec3(clampedX, 0.0f, -half.z - standOff);
            sideDepth = obs.size.z;
            faceHalfWidth = half.x;
            faceWallOffset = -clampedX;
            atEdge = (half.x - std::abs(clampedX) <= 0.45f);
            atEdgeDir = (clampedX <= 0.0f) ? 1.0f : -1.0f;
        }
        if (dz2 < minSideDist) {
            minSideDist = dz2;
            localNormal = glm::vec3(0.0f, 0.0f, 1.0f);
            localWallDir = glm::vec3(1.0f, 0.0f, 0.0f);
            localSnap = glm::vec3(clampedX, 0.0f, half.z + standOff);
            sideDepth = obs.size.z;
            faceHalfWidth = half.x;
            faceWallOffset = clampedX;
            atEdge = (half.x - std::abs(clampedX) <= 0.45f);
            atEdgeDir = (clampedX >= 0.0f) ? 1.0f : -1.0f;
        }

        // Local to world (3D Y-axis forward rotation)
        float cosW = std::cos(obs.yaw);
        float sinW = std::sin(obs.yaw);
        glm::vec3 worldNormal(
            cosW * localNormal.x - sinW * localNormal.z,
            0.0f,
            sinW * localNormal.x + cosW * localNormal.z
        );
        glm::vec3 worldWallDir(
            cosW * localWallDir.x - sinW * localWallDir.z,
            0.0f,
            sinW * localWallDir.x + cosW * localWallDir.z
        );
        glm::vec3 worldSnap(
            cosW * localSnap.x - sinW * localSnap.z + obs.position.x,
            playerPos.y,
            sinW * localSnap.x + cosW * localSnap.z + obs.position.z
        );

        float d = glm::distance(glm::vec2(playerPos.x, playerPos.z), glm::vec2(worldSnap.x, worldSnap.z));
        if (d < bestDist) {
            bestDist = d;
            best.valid = true;
            best.dist = d;
            best.snapPos = worldSnap;
            best.normal = glm::normalize(worldNormal);
            best.wallDir = glm::normalize(worldWallDir);
            best.height = obs.size.y;
            best.depth = sideDepth;
            best.halfWidth = faceHalfWidth;
            best.wallOffset = faceWallOffset;
            best.center = obs.position;
            best.isAtEdge = atEdge;
            best.edgeDir = atEdgeDir;
            best.isLowCover = (obs.size.y <= 1.45f);
        }
    }

    // Supply Crates provide cover and vaulting
    for (const auto& crate : world.getCrates()) {
        if (crate.isGroundItem) continue;

        float cosY = std::cos(-crate.yaw);
        float sinY = std::sin(-crate.yaw);
        glm::vec3 rel = playerPos - crate.position;
        glm::vec3 localPos(
            cosY * rel.x - sinY * rel.z,
            rel.y,
            sinY * rel.x + cosY * rel.z
        );

        glm::vec3 crateSize(1.40f, 0.90f, 1.10f);
        glm::vec3 half = crateSize * 0.5f;
        float clampedX = std::clamp(localPos.x, -half.x, half.x);
        float clampedZ = std::clamp(localPos.z, -half.z, half.z);

        float dx1 = std::abs(localPos.x - (-half.x));
        float dx2 = std::abs(localPos.x - half.x);
        float dz1 = std::abs(localPos.z - (-half.z));
        float dz2 = std::abs(localPos.z - half.z);

        glm::vec3 localNormal(0.0f, 0.0f, -1.0f);
        glm::vec3 localWallDir(-1.0f, 0.0f, 0.0f);
        glm::vec3 localSnap(clampedX, 0.0f, -half.z - standOff);
        float minSideDist = dz1;
        float sideDepth = crateSize.z;
        float faceHalfWidth = half.x;
        float faceWallOffset = -clampedX;
        bool atEdge = (half.x - std::abs(clampedX) <= 0.45f);
        float atEdgeDir = (clampedX <= 0.0f) ? 1.0f : -1.0f;

        if (dz2 < minSideDist) {
            minSideDist = dz2;
            localNormal = glm::vec3(0.0f, 0.0f, 1.0f);
            localWallDir = glm::vec3(1.0f, 0.0f, 0.0f);
            localSnap = glm::vec3(clampedX, 0.0f, half.z + standOff);
            sideDepth = crateSize.z;
            faceHalfWidth = half.x;
            faceWallOffset = clampedX;
            atEdge = (half.x - std::abs(clampedX) <= 0.45f);
            atEdgeDir = (clampedX >= 0.0f) ? 1.0f : -1.0f;
        }
        if (dx1 < minSideDist) {
            minSideDist = dx1;
            localNormal = glm::vec3(-1.0f, 0.0f, 0.0f);
            localWallDir = glm::vec3(0.0f, 0.0f, 1.0f);
            localSnap = glm::vec3(-half.x - standOff, 0.0f, clampedZ);
            sideDepth = crateSize.x;
            faceHalfWidth = half.z;
            faceWallOffset = clampedZ;
            atEdge = (half.z - std::abs(clampedZ) <= 0.45f);
            atEdgeDir = (clampedZ >= 0.0f) ? 1.0f : -1.0f;
        }
        if (dx2 < minSideDist) {
            minSideDist = dx2;
            localNormal = glm::vec3(1.0f, 0.0f, 0.0f);
            localWallDir = glm::vec3(0.0f, 0.0f, -1.0f);
            localSnap = glm::vec3(half.x + standOff, 0.0f, clampedZ);
            sideDepth = crateSize.x;
            faceHalfWidth = half.z;
            faceWallOffset = -clampedZ;
            atEdge = (half.z - std::abs(clampedZ) <= 0.45f);
            atEdgeDir = (clampedZ <= 0.0f) ? 1.0f : -1.0f;
        }

        float cosW = std::cos(crate.yaw);
        float sinW = std::sin(crate.yaw);
        glm::vec3 worldNormal(
            cosW * localNormal.x - sinW * localNormal.z,
            0.0f,
            sinW * localNormal.x + cosW * localNormal.z
        );
        glm::vec3 worldWallDir(
            cosW * localWallDir.x - sinW * localWallDir.z,
            0.0f,
            sinW * localWallDir.x + cosW * localWallDir.z
        );
        glm::vec3 worldSnap(
            cosW * localSnap.x - sinW * localSnap.z + crate.position.x,
            playerPos.y,
            sinW * localSnap.x + cosW * localSnap.z + crate.position.z
        );

        float d = glm::distance(glm::vec2(playerPos.x, playerPos.z), glm::vec2(worldSnap.x, worldSnap.z));
        if (d < bestDist) {
            bestDist = d;
            best.valid = true;
            best.dist = d;
            best.snapPos = worldSnap;
            best.normal = glm::normalize(worldNormal);
            best.wallDir = glm::normalize(worldWallDir);
            best.height = crateSize.y;
            best.depth = sideDepth;
            best.halfWidth = faceHalfWidth;
            best.wallOffset = faceWallOffset;
            best.center = crate.position;
            best.isAtEdge = atEdge;
            best.edgeDir = atEdgeDir;
            best.isLowCover = true;
        }
    }
    return best;
}

static SoundType getSurfaceFootstepSound(GroundSurface surface) {
    switch (surface) {
        case GroundSurface::Concrete: return SoundType::FootstepConcrete;
        case GroundSurface::Wood:     return SoundType::FootstepWood;
        case GroundSurface::Mud:      return SoundType::FootstepMud;
        case GroundSurface::Water:    return SoundType::FootstepWater;
        case GroundSurface::Dirt:
        default:                      return SoundType::FootstepDirt;
    }
}

void Player::update(float dt, const Window& window, const glm::vec3& mouseWorldPos, World& world, BallisticsSystem& ballistics, float cameraYaw, Camera* camera, const AimTargetInfo* aimInfo) {
    bool isChatOpen = ChatSystem::get().isChatOpen();
    bool captureKb = isChatOpen || (ImGui::GetCurrentContext() && (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput));
    bool uiWantsMouse = isChatOpen || (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse);
    InputConfig& inCfg = InputConfig::get();
    if (m_playerVoiceBarkTimer > 0.0f) m_playerVoiceBarkTimer -= dt;
    updateSpawnProtection(dt);

    if (m_stats.health <= 0.0f) {
        if (!m_rig.isDead()) {
            m_rig.triggerDeath(DeathType::CollapseBackward, glm::vec3(0.0f), true, DISMEMBER_NONE);
        }
        m_rig.update9D(dt, m_position, m_yaw, m_position + glm::vec3(0, 1, 0), glm::vec3(0.0f), false, false, 0, false, false, true, false, 0.0f);
        return;
    }

    // Heavy Suppression Combat Bark
    if (m_stats.suppression > 55.0f && m_playerVoiceBarkTimer <= 0.0f) {
        m_playerVoiceBarkTimer = 7.0f;
        AudioEngine::get().playSound(SoundType::SoldierBarkSuppressed, 0.70f, 1.0f);
    }

    // High Wall Climbing / Mantle Kinematics (Two-Phase Athletic Ledge Climb)
    if (m_isClimbing) {
        m_climbTimer += dt;
        float t = std::clamp(m_climbTimer / m_climbDuration, 0.0f, 1.0f);
        
        // Phase 1 (t: 0 -> 0.45): Vertical pull-up to clear wall top
        // Phase 2 (t: 0.45 -> 1.0): Forward vault & landing on opposite side
        if (t < 0.45f) {
            float p1 = t / 0.45f;
            float s1 = p1 * p1 * (3.0f - 2.0f * p1); // Smoothstep easing
            m_position.x = m_climbStartPos.x;
            m_position.z = m_climbStartPos.z;
            m_position.y = glm::mix(m_climbStartPos.y, m_climbPeakY, s1);
        } else {
            float p2 = (t - 0.45f) / 0.55f;
            float s2 = p2 * p2 * (3.0f - 2.0f * p2); // Smoothstep easing
            m_position.x = glm::mix(m_climbStartPos.x, m_climbTargetPos.x, s2);
            m_position.z = glm::mix(m_climbStartPos.z, m_climbTargetPos.z, s2);
            float targetGroundY = World::sampleTerrainHeight(m_climbTargetPos.x, m_climbTargetPos.z);
            m_position.y = glm::mix(m_climbPeakY, targetGroundY, s2);
        }

        bool hasWeapon = m_inventory.getEquipped(m_activeWeaponSlot).has_value();
        m_rig.update9D(dt, m_position, m_yaw, m_aimTarget, m_velocity, false, true, 0, false, false, hasWeapon, false, 0.0f);

        if (t >= 1.0f) {
            m_isClimbing = false;
            m_position = m_climbTargetPos;
            m_position.y = World::sampleTerrainHeight(m_position.x, m_position.z);
            glm::vec3 climbDir = glm::normalize(m_climbTargetPos - m_climbStartPos);
            m_velocity = climbDir * 5.5f;
            m_stats.isSprinting = true;
            GroundSurface surf = world.getGroundSurface(m_position);
            AudioEngine::get().playSound(getSurfaceFootstepSound(surf), 0.60f);
            AudioEngine::get().playSound(SoundType::GearRustle, 0.45f, 1.0f);
        }
        return;
    }

    // Barrier Vaulting Kinematics (Athletic 3-Phase Hurdle over Low Barricades)
    if (m_isVaulting) {
        m_vaultTimer += dt;
        float t = std::clamp(m_vaultTimer / m_vaultDuration, 0.0f, 1.0f);
        float horizEase = t * t * (3.0f - 2.0f * t);
        m_position.x = glm::mix(m_vaultStartPos.x, m_vaultTargetPos.x, horizEase);
        m_position.z = glm::mix(m_vaultStartPos.z, m_vaultTargetPos.z, horizEase);
        float groundBase = World::sampleTerrainHeight(m_position.x, m_position.z);
        float vertCurve = std::sin(3.14159265f * std::pow(t, 0.85f));
        float hurdleHeight = std::max(0.12f, m_vaultPeakY - groundBase);
        m_position.y = groundBase + hurdleHeight * vertCurve;

        bool hasWeapon = m_inventory.getEquipped(m_activeWeaponSlot).has_value();
        m_rig.setVaultParameters(true, t, m_vaultHandPlantPos);
        m_rig.update9D(dt, m_position, m_yaw, m_aimTarget, m_velocity, false, true, 0, false, false, hasWeapon, false, 0.0f);

        if (t >= 1.0f) {
            m_isVaulting = false;
            m_rig.setVaultParameters(false, 0.0f, glm::vec3(0.0f));
            m_position.y = groundBase;
            // Carry forward momentum directly into sprint!
            m_velocity = m_vaultTraverseDir * 6.5f;
            m_stats.isSprinting = true;
            GroundSurface surf = world.getGroundSurface(m_position);
            AudioEngine::get().playSound(getSurfaceFootstepSound(surf), 0.65f, 0.95f);
            AudioEngine::get().playSound(SoundType::GearRustle, 0.45f, 1.0f);
        }
        return;
    }

    // Mounted Heavy Machine Gun (DShK 12.7mm) Loop
    if (m_isMounted && m_activeTurret) {
        bool fPressed = !captureKb && window.isKeyPressed(GLFW_KEY_F);
        bool spacePressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::Jump)) || window.isKeyPressed(GLFW_KEY_SPACE));
        if ((fPressed && !m_fPressedLast) || (spacePressed && !m_spacePressedLast)) {
            dismountTurret();
            m_fPressedLast = fPressed;
            m_spacePressedLast = spacePressed;
            return;
        }
        m_fPressedLast = fPressed;
        m_spacePressedLast = spacePressed;

        float cy = std::cos(m_yaw);
        float sy = std::sin(m_yaw);
        m_position = m_activeTurret->position - glm::vec3(sy, 0.0f, cy) * 0.45f;
        m_position.y = m_activeTurret->position.y;

        glm::vec3 aimDiff = mouseWorldPos - m_activeTurret->position;
        float desiredYaw = std::atan2(aimDiff.x, aimDiff.z);
        float deltaYaw = desiredYaw - m_turretBaseYaw;
        while (deltaYaw > 3.14159f) deltaYaw -= 6.28318f;
        while (deltaYaw < -3.14159f) deltaYaw += 6.28318f;
        deltaYaw = std::clamp(deltaYaw, -1.35f, 1.35f);
        m_yaw = m_turretBaseYaw + deltaYaw;
        m_activeTurret->yaw = m_yaw;

        float distXZ = std::max(0.2f, glm::distance(glm::vec2(m_activeTurret->position.x, m_activeTurret->position.z), glm::vec2(mouseWorldPos.x, mouseWorldPos.z)));
        float deltaY = mouseWorldPos.y - (m_activeTurret->position.y + 0.96f);
        m_activeTurret->pitch = std::clamp(std::atan2(-deltaY, distXZ), -0.32f, 0.42f);

        bool lClick = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !uiWantsMouse;
        if (lClick && m_activeTurret->ammo > 0 && m_activeTurret->fireCooldown <= 0.0f) {
            m_activeTurret->fireCooldown = 0.12f;
            m_activeTurret->ammo--;

            AcousticEnvironment turretEnv = world.detectAcousticEnvironment(m_position);
            AudioEngine::get().playSound(SoundType::HeavyMGShot, 0.95f, 0.96f + (std::rand() % 10) * 0.01f, turretEnv);

            glm::vec3 fwd(std::sin(m_yaw), 0.0f, std::cos(m_yaw));
            glm::vec3 right(std::cos(m_yaw), 0.0f, -std::sin(m_yaw));
            glm::vec3 muzzle = m_activeTurret->position + fwd * 1.45f + glm::vec3(0.0f, 0.96f, 0.0f);
            glm::vec3 bulletVelDir = glm::normalize(mouseWorldPos - muzzle);

            // Heavy 12.7mm Muzzle Flash & Volumetric Smoke
            ballistics.spawnMuzzleFlash(muzzle, bulletVelDir, 1.95f, 3);
            ballistics.spawnMuzzleSmoke(muzzle, bulletVelDir, 2.2f);

            // Heavy 12.7mm Brass Ejection to right-rear
            glm::vec3 casingPos = m_activeTurret->position + fwd * 0.45f + right * 0.28f + glm::vec3(0.0f, 0.98f, 0.0f);
            ballistics.spawnShellCasing(casingPos, right, glm::vec3(0.0f, 1.0f, 0.0f), bulletVelDir, 3);

            if (camera) {
                camera->addTrauma(0.18f);
            }

            ballistics.fireBullet(muzzle, bulletVelDir, 880.0f, 92.0f, 0.92f, true, m_faction, -1, 65.0f, 3);

            BulletFireEvent fireEvt;
            fireEvt.origin = muzzle;
            fireEvt.direction = bulletVelDir;
            fireEvt.speed = 880.0f;
            fireEvt.damage = 92.0f;
            fireEvt.weaponType = 3; // HeavyMG
            fireEvt.faction = static_cast<uint8_t>(m_faction);
            NetworkEngine::get().broadcastBulletFire(fireEvt);
        }

        m_rig.setActiveWeaponName("None");
        m_rig.update(dt, m_position, m_yaw, mouseWorldPos, false, false, 0, true, 0.0f, true);
        return;
    }
    // Smooth aim target interpolation with 3D target micro-friction
    float camRad = glm::radians(cameraYaw);
    glm::vec3 camHorizFwd(-std::sin(camRad), 0.0f, -std::cos(camRad));

    if (aimInfo && aimInfo->hasTarget && glm::dot(aimInfo->worldPos - m_position, camHorizFwd) >= 0.5f) {
        m_hasTargetLock = true;
        m_isHeadshotAim = aimInfo->isHeadshot;
        m_aimTargetInfo = *aimInfo;
        m_aimTarget = aimInfo->worldPos;
    } else {
        m_hasTargetLock = false;
        m_isHeadshotAim = false;
        m_aimTargetInfo = AimTargetInfo{};
        m_aimTarget = mouseWorldPos;
    }

    // GTA 5 / Ghost Recon Tactical Cover Assessment
    CoverCandidate cand = findBestCover(m_position, world);
    m_nearCover = cand.valid && (cand.dist <= 1.8f);

    // Q KEY: Dynamic Cover Snap / Exit Toggle with 0.14s Hermite Glide
    bool qPressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::Cover)) || window.isKeyPressed(GLFW_KEY_Q));
    if (qPressed && !m_qPressedLast) {
        if (m_inCover) {
            m_inCover = false;
            m_isSnappingToCover = false;
            m_isAtCoverEdge = false;
            m_coverLeanAngle = 0.0f;
            AudioEngine::get().playSound(SoundType::GearRustle, 0.35f, 1.05f);
        } else if (cand.valid && cand.dist <= 1.8f) {
            m_inCover = true;
            m_coverNormal = cand.normal;
            m_coverWallDir = cand.wallDir;
            m_coverSnapPos = cand.snapPos;
            m_coverHeight = cand.height;
            m_coverDepth = cand.depth;
            m_isLowCover = cand.isLowCover;
            m_isSnappingToCover = true;
            m_coverSnapTimer = 0.0f;
            m_coverSnapDuration = 0.14f;
            m_coverSnapStartPos = m_position;
            if (m_isLowCover) {
                m_stats.stance = PlayerStance::Crouching;
            }
            AudioEngine::get().playSound(SoundType::GearRustle, 0.45f, 0.95f);
        }
    }
    m_qPressedLast = qPressed;

    // Smooth Hermite Cover Snap Progress
    if (m_isSnappingToCover) {
        m_coverSnapTimer += dt;
        float snapT = std::clamp(m_coverSnapTimer / m_coverSnapDuration, 0.0f, 1.0f);
        float s = snapT * snapT * (3.0f - 2.0f * snapT);
        m_position.x = glm::mix(m_coverSnapStartPos.x, m_coverSnapPos.x, s);
        m_position.z = glm::mix(m_coverSnapStartPos.z, m_coverSnapPos.z, s);
        m_position.y = World::sampleTerrainHeight(m_position.x, m_position.z);
        if (snapT >= 1.0f) {
            m_isSnappingToCover = false;
        }
    }

    // SPACE KEY: Jump, or Athletic Vault / Mantle with Landing Clearance Verification
    bool spacePressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::Jump)) || window.isKeyPressed(GLFW_KEY_SPACE));
    if (spacePressed && !m_spacePressedLast) {
        if (m_inCover) {
            glm::vec3 traverseDir = -m_coverNormal;
            float depth = std::max(0.40f, m_coverDepth);
            float clearDist = depth + 0.85f;
            glm::vec3 landingTarget = m_position + traverseDir * clearDist;
            landingTarget.y = World::sampleTerrainHeight(landingTarget.x, landingTarget.z);

            glm::vec3 landingCheck = landingTarget;
            world.checkCollision(landingCheck, 0.24f);
            float pushback = glm::distance(glm::vec2(landingTarget.x, landingTarget.z), glm::vec2(landingCheck.x, landingCheck.z));

            if (pushback < 0.28f && m_isLowCover && m_coverHeight <= 1.45f) {
                // Smooth Low Barricade Vault with Hand Plant IK
                m_isVaulting = true;
                m_vaultTimer = 0.0f;
                m_vaultDuration = 0.44f;
                m_vaultStartPos = m_position;
                m_vaultTargetPos = landingCheck;
                m_vaultTraverseDir = traverseDir;
                m_vaultPeakY = m_position.y + m_coverHeight + 0.16f;
                m_vaultHandPlantPos = (m_vaultStartPos + m_vaultTargetPos) * 0.5f;
                m_vaultHandPlantPos.y = m_position.y + m_coverHeight;
                m_yaw = std::atan2(traverseDir.x, traverseDir.z);
                m_inCover = false;
                m_isAtCoverEdge = false;
                m_coverLeanAngle = 0.0f;
                AudioEngine::get().playSound(SoundType::GearRustle, 0.50f, 1.20f);
            } else if (pushback < 0.28f && m_coverHeight <= 3.2f) {
                // Smooth High Wall Climb / Mantle
                m_isClimbing = true;
                m_climbTimer = 0.0f;
                m_climbDuration = 0.72f;
                m_climbWallHeight = m_coverHeight;
                m_climbStartPos = m_position;
                m_climbTargetPos = landingCheck;
                m_climbPeakY = m_position.y + m_coverHeight + 0.20f;
                m_yaw = std::atan2(traverseDir.x, traverseDir.z);
                m_inCover = false;
                m_isAtCoverEdge = false;
                m_coverLeanAngle = 0.0f;
                AudioEngine::get().playSound(SoundType::GearRustle, 0.55f, 1.10f);
            } else {
                // Obstacle too tall or landing blocked: detach and vertical jump
                m_inCover = false;
                m_isAtCoverEdge = false;
                m_coverLeanAngle = 0.0f;
                if (!m_isJumping && m_stats.stamina > 4.0f) {
                    m_isJumping = true;
                    m_jumpTimer = 0.0f;
                    m_jumpBaseY = m_position.y;
                    m_stats.stamina = std::max(0.0f, m_stats.stamina - 2.5f);
                    GroundSurface surf = world.getGroundSurface(m_position);
                    AudioEngine::get().playSound(getSurfaceFootstepSound(surf), 0.35f, 1.2f);
                }
            }
        } else {
            // Not in Cover: check if running directly into vaultable or climbable obstacle
            bool isMovingToward = (glm::length(m_velocity) > 2.0f) || m_stats.isSprinting;
            if (isMovingToward && cand.valid && cand.dist <= 1.40f) {
                glm::vec3 traverseDir = (glm::length(m_velocity) > 0.5f) ? glm::normalize(m_velocity) : -cand.normal;
                float depth = std::max(0.40f, cand.depth);
                float clearDist = depth + 0.85f;
                glm::vec3 landingTarget = m_position + traverseDir * clearDist;
                landingTarget.y = World::sampleTerrainHeight(landingTarget.x, landingTarget.z);

                glm::vec3 landingCheck = landingTarget;
                world.checkCollision(landingCheck, 0.24f);
                float pushback = glm::distance(glm::vec2(landingTarget.x, landingTarget.z), glm::vec2(landingCheck.x, landingCheck.z));

                if (pushback < 0.28f && cand.isLowCover && cand.height <= 1.45f && !m_isVaulting) {
                    m_isVaulting = true;
                    m_vaultTimer = 0.0f;
                    m_vaultDuration = 0.44f;
                    m_vaultStartPos = m_position;
                    m_vaultTargetPos = landingCheck;
                    m_vaultTraverseDir = traverseDir;
                    m_vaultPeakY = m_position.y + cand.height + 0.16f;
                    m_vaultHandPlantPos = (m_vaultStartPos + m_vaultTargetPos) * 0.5f;
                    m_vaultHandPlantPos.y = m_position.y + cand.height;
                    m_yaw = std::atan2(traverseDir.x, traverseDir.z);
                    AudioEngine::get().playSound(SoundType::GearRustle, 0.50f, 1.20f);
                } else if (pushback < 0.28f && cand.height <= 3.2f && !m_isClimbing) {
                    m_isClimbing = true;
                    m_climbTimer = 0.0f;
                    m_climbDuration = 0.72f;
                    m_climbWallHeight = cand.height;
                    m_climbStartPos = m_position;
                    m_climbTargetPos = landingCheck;
                    m_climbPeakY = m_position.y + cand.height + 0.20f;
                    m_yaw = std::atan2(traverseDir.x, traverseDir.z);
                    AudioEngine::get().playSound(SoundType::GearRustle, 0.55f, 1.10f);
                }
            } else if (!m_isJumping && !m_isVaulting && !m_isClimbing && m_stats.stance != PlayerStance::Prone) {
                m_isJumping = true;
                m_jumpTimer = 0.0f;
                m_jumpBaseY = m_position.y;
                m_stats.stamina = std::max(0.0f, m_stats.stamina - 2.5f);
                GroundSurface surf = world.getGroundSurface(m_position);
                AudioEngine::get().playSound(getSurfaceFootstepSound(surf), 0.40f, 1.25f);
            }
        }
    }
    m_spacePressedLast = spacePressed;

    // Dynamic cover sliding: refresh normal and wall direction while hugging wall
    if (m_inCover && cand.valid && cand.dist <= 1.50f) {
        if (glm::dot(cand.normal, m_coverNormal) > 0.65f) {
            m_coverNormal = cand.normal;
            m_coverWallDir = cand.wallDir;
            m_coverHeight = cand.height;
            m_coverDepth = cand.depth;
            m_isLowCover = cand.isLowCover;
        }
    }

    if (m_inCover) {
        if (!cand.valid || cand.dist > 2.2f) {
            m_inCover = false;
            m_isAtCoverEdge = false;
            m_coverLeanAngle = 0.0f;
        }
    }

    // Cover edge detection & dynamic corner peeking
    m_isAtCoverEdge = cand.valid && cand.isAtEdge;
    m_coverEdgeDir = cand.valid ? cand.edgeDir : 0.0f;

    float targetCoverLean = 0.0f;
    if (m_inCover && m_isAtCoverEdge && m_isAiming) {
        targetCoverLean = m_coverEdgeDir * 0.35f;
    }
    m_coverLeanAngle = glm::mix(m_coverLeanAngle, targetCoverLean, std::clamp(dt * 14.0f, 0.0f, 1.0f));

    // 1. Calculate Body Heading Yaw (Hugs wall in cover unless aiming)
    if (m_inCover && !m_isAiming) {
        float targetYaw = std::atan2(m_coverNormal.x, m_coverNormal.z);
        if (glm::length(m_velocity) > 0.2f) {
            float wallMoveDot = glm::dot(m_velocity, m_coverWallDir);
            glm::vec3 angledHeading = glm::normalize(m_coverNormal * 0.70f + m_coverWallDir * (wallMoveDot > 0.0f ? 0.35f : -0.35f));
            targetYaw = std::atan2(angledHeading.x, angledHeading.z);
        }
        float deltaYaw = targetYaw - m_yaw;
        while (deltaYaw > 3.14159265f) deltaYaw -= 2.0f * 3.14159265f;
        while (deltaYaw < -3.14159265f) deltaYaw += 2.0f * 3.14159265f;
        m_yaw += deltaYaw * std::clamp(dt * 20.0f, 0.0f, 1.0f);
    } else {
        float targetYaw = m_yaw;
        if (m_stats.isSprinting && glm::length(m_velocity) > 2.5f) {
            targetYaw = std::atan2(m_velocity.x, m_velocity.z);
        } else {
            float cRad = glm::radians(cameraYaw);
            glm::vec3 camFwd(-std::sin(cRad), 0.0f, -std::cos(cRad));
            targetYaw = std::atan2(camFwd.x, camFwd.z);
        }
        float deltaYaw = targetYaw - m_yaw;
        while (deltaYaw > 3.14159265f) deltaYaw -= 2.0f * 3.14159265f;
        while (deltaYaw < -3.14159265f) deltaYaw += 2.0f * 3.14159265f;
        m_yaw += deltaYaw * std::clamp(dt * 26.0f, 0.0f, 1.0f);
    }
    while (m_yaw > 3.14159265f) m_yaw -= 6.2831853f;
    while (m_yaw < -3.14159265f) m_yaw += 6.2831853f;

    // 2. Handle Stance & Reload Key inputs

    bool cPressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::CrouchSlide)) || window.isKeyPressed(GLFW_KEY_C));
    bool xPressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::Prone)) || window.isKeyPressed(GLFW_KEY_X));
    bool fPressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::Interact)) || window.isKeyPressed(GLFW_KEY_F));
    bool rPressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::Reload)) || window.isKeyPressed(GLFW_KEY_R));
    bool gPressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::DropWeapon)) || window.isKeyPressed(GLFW_KEY_G));
    bool hPressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::FirstAid)) || window.isKeyPressed(GLFW_KEY_H));
    bool fourPressed = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::ThrowGrenade)) || window.isKeyPressed(GLFW_KEY_4));


    bool onePressed = !captureKb && window.isKeyPressed(GLFW_KEY_1);
    bool twoPressed = !captureKb && window.isKeyPressed(GLFW_KEY_2);
    bool threePressed = !captureKb && window.isKeyPressed(GLFW_KEY_3);

    if (onePressed && !m_onePressedLast) switchWeapon(EquipSlot::PrimaryWeapon);
    if (twoPressed && !m_twoPressedLast) switchWeapon(EquipSlot::SecondaryWeapon);
    if (threePressed && !m_threePressedLast) {
        if (m_inventory.getEquipped(EquipSlot::SecondaryWeapon).has_value()) {
            switchWeapon(EquipSlot::SecondaryWeapon);
        }
    }
    m_onePressedLast = onePressed;
    m_twoPressedLast = twoPressed;
    m_threePressedLast = threePressed;

    // Weapon switch progression (Smooth holster -> draw sequence)
    if (m_isSwitchingWeapon) {
        m_weaponSwitchTimer -= dt;
        if (m_weaponSwitchTimer <= 0.0f) {
            m_isSwitchingWeapon = false;
            m_activeWeaponSlot = m_pendingSwitchSlot;
            auto& destWeapon = m_inventory.getEquipped(m_activeWeaponSlot);
            m_adsZoomLevel = destWeapon.has_value() ? destWeapon->opticZoomLevel : 0;
            m_rig.setActiveWeaponName(destWeapon.has_value() ? destWeapon->name : "None");
            m_rig.triggerEquipAnim(0.35f);

            if (destWeapon.has_value() && destWeapon->category == ItemCategory::Weapon) {
                if (destWeapon->currentMag > 0) {
                    AudioEngine::get().playSound(SoundType::ChamberRound, 0.65f, 1.0f);
                } else {
                    AudioEngine::get().playSound(SoundType::ItemEquip, 0.55f, 1.05f);
                }
            } else {
                AudioEngine::get().playSound(SoundType::GearRustle, 0.30f, 1.0f);
            }
        }
    }

    // Dedicated Weapon Optic Assessment (Each weapon has its own specific optic profile)
    auto& activeEquip = m_inventory.getEquipped(m_activeWeaponSlot);
    int weaponOptic = 0;
    bool canToggleOptic = false;
    if (activeEquip.has_value() && activeEquip->category == ItemCategory::Weapon) {
        weaponOptic = activeEquip->opticZoomLevel;
        canToggleOptic = activeEquip->hasVariableOptic;
    }

    // Tactical Aim Down Sights (ADS) Mode on Right-Click
    bool aimInput = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) && !uiWantsMouse;
    if (aimInput != m_isAiming) {
        m_isAiming = aimInput;
        if (m_isAiming) {
            AudioEngine::get().playSound(SoundType::AimDownSight, 0.45f, 1.0f);
            m_adsZoomLevel = weaponOptic; // Automatically initialize to the weapon's dedicated optic!
        } else {
            AudioEngine::get().playSound(SoundType::AimDownSightOut, 0.35f, 1.05f);
            m_isHoldingBreath = false;
            m_breathHoldTimer = 0.0f;
            m_adsZoomLevel = weaponOptic; // Preserve current weapon's configured optic
        }
    }

    // Dedicated Optic Magnification Toggle (ONLY for weapons with variable optics, e.g. M4A1 1.0x CQB <-> 2.5x Combat ACOG)
    // Completely decoupled from movement keys (NEVER triggers on W or Z on any keyboard layout)
    int toggleOpticKey = inCfg.getKey(GameAction::ToggleOptic);
    bool toggleOpticPressed = !captureKb && m_isAiming && canToggleOptic &&
        ((toggleOpticKey != GLFW_KEY_UNKNOWN && window.isKeyPressed(toggleOpticKey)) ||
         window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE));

    if (toggleOpticPressed && !m_toggleOpticPressedLast) {
        // Toggle magnification for variable optics (e.g. 1.0x CQB <-> 2.5x Combat ACOG)
        m_adsZoomLevel = (m_adsZoomLevel == 1) ? 0 : 1;
        if (activeEquip.has_value()) {
            activeEquip->opticZoomLevel = m_adsZoomLevel; // Persist zoom level per weapon!
        }
        AudioEngine::get().playSound(SoundType::ItemEquip, 0.45f, 1.35f + static_cast<float>(m_adsZoomLevel) * 0.15f);
    }
    m_toggleOpticPressedLast = toggleOpticPressed;

    // Steady Aim / Hold Breath with Left Alt while aiming
    bool wantsHoldBreath = !captureKb && m_isAiming && (window.isKeyPressed(inCfg.getKey(GameAction::HoldBreath)) || window.isKeyPressed(GLFW_KEY_LEFT_ALT)) && (m_stats.stamina > 4.0f);
    if (wantsHoldBreath) {
        if (!m_isHoldingBreath) {
            AudioEngine::get().playSound(SoundType::HeavyBreathing, 0.22f, 1.25f);
        }
        m_isHoldingBreath = true;
        m_breathHoldTimer += dt;
        m_stats.stamina = std::max(0.0f, m_stats.stamina - dt * 8.5f);
    } else {
        if (m_isHoldingBreath && m_stats.stamina <= 4.0f) {
            AudioEngine::get().playSound(SoundType::HeavyBreathing, 0.45f, 0.95f);
        }
        m_isHoldingBreath = false;
        m_breathHoldTimer = 0.0f;
    }

    // G Key: Drop Current Equipped Weapon in World (CS:GO / CS2 Standard)
    if (gPressed && !m_gPressedLast) {
        dropCurrentWeapon(world);
    }

    // 4 Key: Tactical Ordnance Deployment (F-1 Frag or Smoke Grenade)
    if (fourPressed && !m_fourPressedLast) {
        int grenadeSlot = -1;
        bool isSmoke = false;
        for (size_t i = 0; i < m_inventory.getItems().size(); ++i) {
            const auto& item = m_inventory.getItems()[i];
            if (item.name.find("Frag") != std::string::npos || item.name.find("F-1") != std::string::npos) {
                grenadeSlot = static_cast<int>(i);
                isSmoke = false;
                break;
            } else if (item.name.find("Smoke") != std::string::npos || item.name.find("RDG") != std::string::npos) {
                grenadeSlot = static_cast<int>(i);
                isSmoke = true;
            }
        }
        if (grenadeSlot >= 0) {
            Item gItem = m_inventory.getItems()[grenadeSlot];
            if (gItem.quantity > 1) {
                m_inventory.getItems()[grenadeSlot].quantity--;
            } else {
                m_inventory.getItems().erase(m_inventory.getItems().begin() + grenadeSlot);
            }

            AudioEngine::get().playSound(SoundType::GrenadePin, 0.85f);
            AudioEngine::get().playSound(SoundType::GearRustle, 0.50f, 1.15f);

            glm::vec3 throwOrigin = m_position + glm::vec3(0.0f, 1.35f, 0.0f);
            glm::vec3 throwAim = mouseWorldPos - throwOrigin;
            if (glm::length(throwAim) > 0.001f) {
                throwAim = glm::normalize(throwAim);
            } else {
                throwAim = glm::vec3(std::sin(m_yaw), 0.0f, std::cos(m_yaw));
            }
            throwOrigin += throwAim * 0.40f;
            float throwDist = std::min(30.0f, glm::distance(m_position, mouseWorldPos));
            float throwSpeed = std::max(11.0f, throwDist * 0.82f);
            glm::vec3 throwVel = throwAim * throwSpeed + glm::vec3(0.0f, 3.6f + throwDist * 0.10f, 0.0f);

            ballistics.throwGrenade(throwOrigin, throwVel, isSmoke, true, m_faction, -1);
        } else {
            AudioEngine::get().playSound(SoundType::DryFire, 0.4f, 1.3f);
        }
    }

    if (cPressed && !m_cPressedLast) {
        if (m_stats.isSprinting && glm::length(m_velocity) > 4.2f && !m_isSliding && !m_inCover) {
            m_isSliding = true;
            m_slideTimer = 0.0f;
            m_slideDirection = glm::normalize(m_velocity);
            m_stats.stance = PlayerStance::Crouching;
            AudioEngine::get().playSound(SoundType::SprintSlide, 0.75f);
        } else {
            if (m_stats.stance == PlayerStance::Crouching) m_stats.stance = PlayerStance::Standing;
            else m_stats.stance = PlayerStance::Crouching;
            AudioEngine::get().playSound(SoundType::GearRustle, 0.25f, 1.0f);
        }
    }
    if (xPressed && !m_xPressedLast) {
        if (m_stats.stance == PlayerStance::Prone) m_stats.stance = PlayerStance::Standing;
        else m_stats.stance = PlayerStance::Prone;
        AudioEngine::get().playSound(SoundType::GearRustle, 0.35f, 0.88f);
    }
    if (rPressed && !m_rPressedLast) {
        reloadCurrentWeapon();
    }
    // H or 4 Key: Tactical First-Aid / Bandaging & Anatomical Trauma Treatment
    bool healPressed = (hPressed && !m_hPressedLast);
    if (healPressed) {
        useQuickMedical();
    }
    if (fPressed && !m_fPressedLast) {
        MountedTurret* nearTurret = world.getNearbyTurret(m_position, 2.2f);
        if (nearTurret && !nearTurret->isOccupied) {
            mountTurret(nearTurret);
            m_fPressedLast = fPressed;
            return;
        }

        SupplyCrate* crate = world.getNearbyCrate(m_position, 2.8f);
        if (crate && !crate->looted) {
            if (crate->lootId > 0 && NetworkEngine::get().isClientConnected()) {
                NetworkEngine::get().broadcastLootPickup(crate->lootId);
            }
            auto it = crate->contents.begin();
            int lootedCount = 0;
            while (it != crate->contents.end()) {
                if (m_inventory.addItem(*it)) {
                    lootedCount++;
                    // If unarmed, automatically equip the newly picked up weapon
                    if (it->category == ItemCategory::Weapon && !m_inventory.getEquipped(m_activeWeaponSlot).has_value()) {
                        std::string pickedName = it->name;
                        m_inventory.equipItem(m_inventory.getItems().size() - 1, m_activeWeaponSlot);
                        m_rig.setActiveWeaponName(pickedName);
                        m_rig.triggerEquipAnim(0.38f);
                        AudioEngine::get().playSound(SoundType::ChamberRound, 0.70f, 1.0f);
                    }
                    it = crate->contents.erase(it);
                } else {
                    // Inventory capacity full; leave remaining items in crate
                    ++it;
                }
            }
            if (crate->contents.empty()) {
                crate->looted = true;
            }
            if (lootedCount > 0) {
                AudioEngine::get().playSound(SoundType::Reload, 0.8f, 0.9f);
            } else {
                AudioEngine::get().playSound(SoundType::DryFire, 0.5f, 1.2f);
            }
        }
    }

    m_cPressedLast = cPressed;
    m_xPressedLast = xPressed;
    m_fPressedLast = fPressed;
    m_hPressedLast = hPressed;
    m_fourPressedLast = fourPressed;
    m_rPressedLast = rPressed;
    m_gPressedLast = gPressed;

    // 3. Movement calculation (Camera-Relative Direction)
    glm::vec3 inputDir(0.0f);
    if (!captureKb) {
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveForward)) || window.isKeyPressed(GLFW_KEY_W)) inputDir.z -= 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveBackward)) || window.isKeyPressed(GLFW_KEY_S)) inputDir.z += 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveLeft)) || window.isKeyPressed(GLFW_KEY_A)) inputDir.x -= 1.0f;
        if (window.isKeyPressed(inCfg.getKey(GameAction::MoveRight)) || window.isKeyPressed(GLFW_KEY_D)) inputDir.x += 1.0f;
    }

    bool isMoving = glm::length(inputDir) > 0.01f;
    glm::vec3 moveDir(0.0f);
    if (isMoving) {
        inputDir = glm::normalize(inputDir);
        float rad = glm::radians(cameraYaw);
        glm::vec3 forward = glm::vec3(-std::sin(rad), 0.0f, -std::cos(rad));
        glm::vec3 right   = glm::vec3(std::cos(rad), 0.0f, -std::sin(rad));
        moveDir = forward * (-inputDir.z) + right * inputDir.x;
        if (glm::length(moveDir) > 0.01f) {
            moveDir = glm::normalize(moveDir);
        }

        if (m_inCover) {
            float moveNormalDot = glm::dot(moveDir, m_coverNormal);
            if (moveNormalDot > 0.65f) {
                m_inCover = false; // Detached cleanly by deliberately pushing away from wall!
                m_isAtCoverEdge = false;
                m_coverLeanAngle = 0.0f;
            } else {
                float wallDot = glm::dot(moveDir, m_coverWallDir);
                moveDir = m_coverWallDir * wallDot;

                // Clamp wall sliding at obstacle edges to prevent corner jitter & normal flips
                if (cand.valid && cand.isAtEdge) {
                    if ((cand.edgeDir < 0.0f && wallDot < 0.0f) || (cand.edgeDir > 0.0f && wallDot > 0.0f)) {
                        moveDir = glm::vec3(0.0f);
                    }
                }
            }
        }
    }

    auto& equippedWeapon = m_inventory.getEquipped(m_activeWeaponSlot);
    float weaponSpeedPen = equippedWeapon.has_value() ? equippedWeapon->moveSpeedPenalty : 0.0f;

    // Dynamic ADS Breathing & Sight Sway
    m_breathSwayTime += dt;
    float rawSwayX = std::sin(m_breathSwayTime * 1.4f);
    float rawSwayY = std::sin(m_breathSwayTime * 2.8f) * 0.5f;
    float swayAmp = 0.0035f;
    if (isMoving) swayAmp += 0.009f;
    if (m_stats.stamina < 40.0f) swayAmp += 0.007f * (1.0f - m_stats.stamina / 40.0f);
    if (m_isHoldingBreath) swayAmp *= 0.08f;
    if (m_inCover && m_isLowCover && m_isAiming) {
        swayAmp *= (1.0f - 0.85f * m_coverAimWeight); // -85% sight sway when weapon rested on cover rim
    }
    m_currentSightSway = glm::vec2(rawSwayX, rawSwayY) * swayAmp;

    // Sprint Slide Physics Execution
    if (m_isSliding) {
        m_slideTimer += dt;
        float t = std::clamp(m_slideTimer / m_slideDuration, 0.0f, 1.0f);
        float slideSpeed = glm::mix(8.2f, 2.4f, t);
        m_velocity = m_slideDirection * slideSpeed;
        m_stats.stance = PlayerStance::Crouching;
        if (static_cast<int>(m_slideTimer * 60.0f) % 6 == 0) {
            ballistics.spawnImpactFX(m_position, glm::vec3(0, 1, 0), 0);
        }
        if (t >= 1.0f || spacePressed || m_isAiming) {
            m_isSliding = false;
        }
    }

    bool leftClickFire = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !uiWantsMouse;
    bool isForwardMoving = inputDir.z < -0.1f;
    bool sprintKey = !captureKb && (window.isKeyPressed(inCfg.getKey(GameAction::Sprint)) || window.isKeyPressed(GLFW_KEY_LEFT_SHIFT));
    bool canPhysicallySprint = !m_isSliding && isMoving && isForwardMoving && (!m_stats.isExhausted && m_stats.stamina > 15.0f) && !m_isAiming && !leftClickFire && m_body.canSprint();

    // Sprint-break from crouch or prone (AAA fluid responsive stance recovery)
    if (sprintKey && canPhysicallySprint && m_stats.stance != PlayerStance::Standing) {
        m_stats.stance = PlayerStance::Standing;
        AudioEngine::get().playSound(SoundType::GearRustle, 0.40f, 1.10f);
    }

    bool wantsSprint = sprintKey && canPhysicallySprint && (m_stats.stance == PlayerStance::Standing);

    // Instant sprint-to-fire / sprint-out cancel when aiming or firing
    if (m_stats.isSprinting && (m_isAiming || leftClickFire)) {
        m_stats.isSprinting = false;
    }

    // Stance speed multipliers (Fluid athletic tactical pacing)
    float speedMultiplier = 1.0f;
    if (m_isAiming) speedMultiplier = 0.45f; // Deliberate tactical aiming walk (~2.6 m/s)
    else if (m_stats.stance == PlayerStance::Crouching) speedMultiplier = 0.58f;
    else if (m_stats.stance == PlayerStance::Prone) speedMultiplier = 0.30f;
    else if (wantsSprint) speedMultiplier = 1.45f; // High-mobility athletic sprint (~8.4 m/s)

    // Anatomical Body Leg & Organ Trauma Movement Multiplier
    speedMultiplier *= m_body.getMovementMultiplier();
    speedMultiplier = std::max(0.15f, speedMultiplier - weaponSpeedPen);

    // Suppression & Pinning Penalties
    if (m_stats.isPinned) {
        speedMultiplier *= 0.35f;
    } else if (m_stats.isSuppressed) {
        speedMultiplier *= 0.65f;
    }

    // Realistic graduated kit weight penalty
    float totalWeight = m_inventory.getTotalWeightKg();
    float weightPenalty = std::clamp(1.0f - (std::max(0.0f, totalWeight - 15.0f) / 140.0f), 0.82f, 1.0f);

    float currentSpeed = m_baseMoveSpeed * speedMultiplier * weightPenalty;
    if (world.isPositionInRiver(m_position)) {
        currentSpeed *= 0.45f; // Deep river water & mud wading slowdown
    }
    glm::vec3 targetVelocity = moveDir * currentSpeed;

    // AAA Athletic Acceleration, Directional Cutting & Ground Friction
    float targetSpeed = glm::length(targetVelocity);
    float currentVelSpeed = glm::length(m_velocity);
    float accelRate;
    if (targetSpeed < 0.01f) {
        accelRate = 18.0f; // Responsive deceleration friction to a grounded stop
    } else if (currentVelSpeed > 0.1f && glm::dot(m_velocity, targetVelocity) < 0.0f) {
        accelRate = 22.0f; // Foot plant directional cut
    } else if (wantsSprint) {
        accelRate = 12.0f; // Athletic sprint acceleration curve
    } else {
        accelRate = 16.0f; // Snappy tactical walk/strafe start
    }
    m_velocity = glm::mix(m_velocity, targetVelocity, std::clamp(dt * accelRate, 0.0f, 1.0f));

    glm::vec3 actualMove = m_velocity * dt;
    m_position += actualMove;

    // Softly maintain distance to wall while hugging cover (no 1-frame snaps)
    if (m_inCover && !m_isSnappingToCover && cand.valid) {
        glm::vec3 toSnap = cand.snapPos - m_position;
        glm::vec3 normalPull = m_coverNormal * glm::dot(toSnap, m_coverNormal);
        m_position += normalPull * std::clamp(dt * 14.0f, 0.0f, 1.0f);
    }

    // World collision: Authentic physical human torso radius (0.24m, 0.20m in cover hugging)
    float colRadius = m_inCover ? 0.20f : 0.24f;
    world.checkCollision(m_position, colRadius);

    // Apply jumping vertical kinematics & seamless terrain adherence
    if (m_isJumping) {
        m_jumpTimer += dt;
        float jt = std::clamp(m_jumpTimer / m_jumpDuration, 0.0f, 1.0f);
        float currentGroundY = World::sampleTerrainHeight(m_position.x, m_position.z);
        float baseY = glm::mix(m_jumpBaseY, currentGroundY, jt);
        float jumpArc = std::sin(3.14159265f * jt) * m_jumpHeight;
        m_position.y = baseY + jumpArc;
        if (jt >= 1.0f) {
            m_isJumping = false;
            m_position.y = currentGroundY;
            GroundSurface surf = world.getGroundSurface(m_position);
            AudioEngine::get().playSound(getSurfaceFootstepSound(surf), 0.50f, 0.95f);
            AudioEngine::get().playSound(SoundType::GearRustle, 0.35f, 1.0f);
        }
    } else if (!m_isClimbing && !m_isVaulting) {
        // High-fidelity terrain adherence across hills, trenches, craters, and slopes
        float groundY = World::sampleTerrainHeight(m_position.x, m_position.z);
        float vertDiff = std::abs(groundY - m_position.y);
        if (vertDiff > 3.0f) {
            m_position.y = groundY;
        } else {
            m_position.y = glm::mix(m_position.y, groundY, std::clamp(dt * 28.0f, 0.0f, 1.0f));
        }
    }

    // Calculate actual 2D ground movement distance this frame
    float frameDistance = glm::length(glm::vec2(m_position.x - m_prevPosition.x, m_position.z - m_prevPosition.z));
    m_prevPosition = m_position;

    // Actual sprint state with hysteresis to eliminate boundary oscillation
    float groundSpeed = frameDistance / std::max(0.0001f, dt);
    if (m_stats.isSprinting) {
        m_stats.isSprinting = wantsSprint && (groundSpeed > 2.0f);
    } else {
        m_stats.isSprinting = wantsSprint && (groundSpeed > 3.2f);
    }

    // Surface Detection & Realistic Ground-Distance Locomotion Footsteps
    GroundSurface surface = world.getGroundSurface(m_position);
    float stepIntervalDistance = 1.15f; // Meters per step
    if (m_stats.isSprinting) stepIntervalDistance = 1.70f;
    else if (m_stats.stance == PlayerStance::Crouching) stepIntervalDistance = 0.85f;
    else if (m_stats.stance == PlayerStance::Prone) stepIntervalDistance = 0.65f;

    if (frameDistance > 0.0005f) {
        m_footstepDistanceAccumulator += frameDistance;
        if (m_footstepDistanceAccumulator >= stepIntervalDistance) {
            m_footstepDistanceAccumulator -= stepIntervalDistance;
            m_totalSteps++;
            m_lastFootLeft = !m_lastFootLeft;

            // Environmental acoustic zone detection for physical footsteps
            AcousticEnvironment playerEnv = world.detectAcousticEnvironment(m_position);
            if (playerEnv == AcousticEnvironment::ConcreteBunker || playerEnv == AcousticEnvironment::IndustrialInterior) {
                surface = GroundSurface::Concrete;
            }

            // Determine sound and properties based on physical surface & stance
            SoundType stepSnd = SoundType::FootstepDirt;
            float stepVol = 0.38f;
            float stepPitch = 0.96f + (static_cast<float>(std::rand() % 10) * 0.01f);

            stepSnd = getSurfaceFootstepSound(surface);
            if (m_stats.stance == PlayerStance::Prone) {
                stepSnd = SoundType::GearRustle;
                stepVol = 0.22f;
                stepPitch = 0.92f;
            } else if (m_stats.stance == PlayerStance::Crouching) {
                stepVol = 0.24f;
                stepPitch *= 0.90f;
            } else if (m_stats.isSprinting) {
                stepVol = 0.55f;
                stepPitch *= 1.08f;
                if (m_totalSteps % 2 == 0) {
                    AudioEngine::get().playSound(SoundType::GearRustle, 0.20f, 1.0f);
                }
            }

            AudioEngine::get().playSound(stepSnd, stepVol, stepPitch, playerEnv);
            m_lastStepPitch = stepPitch;
            m_lastStepVolume = stepVol;
            m_lastSoundTriggered = getSoundTypeName(stepSnd);

        }
    } else {
        m_footstepDistanceAccumulator = std::max(0.0f, m_footstepDistanceAccumulator - dt * 2.0f);
    }

    // Combat Engineering Fortification Cooldown
    if (m_sandbagCooldown > 0.0f) m_sandbagCooldown -= dt;


    // 4. Survival & Anatomical Body Systems Update
    m_stats.update(dt, totalWeight);
    m_body.update(dt, m_stats.blood, m_stats.health);

    // Sync procedural wound inverse kinematics state
    m_rig.setWoundState(
        m_body.getTorsoHunch(),
        m_body.getLeftArmSag(),
        m_body.getRightArmSag(),
        m_body.getLimpFactor(),
        m_body.getFlinchPitch(),
        m_body.getFlinchRoll()
    );

    // Update active weapon name on rig
    auto& equippedForRig = m_inventory.getEquipped(m_activeWeaponSlot);
    m_rig.setActiveWeaponName(equippedForRig.has_value() ? equippedForRig->name : "None");

    bool hasWeapon = equippedForRig.has_value();
    float reloadProg = m_isReloading ? std::clamp(1.0f - (m_reloadTimer / 1.8f), 0.0f, 1.0f) : 0.0f;
    bool fireInput = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !uiWantsMouse;

    // Unified aim target height across crosshair raycast, IK character aim, and ballistics trajectory
    // Smooth consistent target (no vertical snap between locked/unlocked states)
    glm::vec3 unifiedTarget = m_aimTarget;

    // Smooth Cover Aim & Mounting Transition
    bool isLowCoverAiming = m_inCover && m_isLowCover && m_isAiming;
    float targetCoverAim = isLowCoverAiming ? 1.0f : 0.0f;
    m_coverAimWeight = glm::mix(m_coverAimWeight, targetCoverAim, std::clamp(dt * 12.0f, 0.0f, 1.0f));
    m_rig.setCoverParameters(m_inCover, m_isLowCover, m_coverHeight, m_coverAimWeight, m_coverLeanAngle);
    m_rig.setVaultParameters(m_isVaulting, (m_vaultDuration > 0.0f ? (m_vaultTimer / m_vaultDuration) : 0.0f), m_vaultHandPlantPos);
    m_rig.setAirborne(m_isVaulting || m_isClimbing || m_isJumping);
    m_rig.setGroundSurface(surface);
    if (camera) {
        m_rig.setShoulderRight(camera->isShoulderRight());
        // Directly drive character rig aim pitch from camera pitch (full elevation range)
        // camera->getPitch(): positive = looking down, negative = looking up
        // CharacterRig sign convention: negative = looking down, positive = looking up
        m_rig.setDirectAimPitch(glm::radians(-camera->getPitch()));
    }

    // Update character rig IK & frame-accurate weapon matrix before raycasting and weapon firing
    m_rig.update9D(dt, m_position, m_yaw, unifiedTarget, m_velocity, isMoving, m_stats.isSprinting,
                   static_cast<int>(m_stats.stance), fireInput, m_isAiming, hasWeapon, m_isReloading, reloadProg);

    // 5. Directional Cover Evaluation & Ballistic Trajectory Raycast
    if (m_inCover) {
        m_stats.cover = CoverType::Heavy;
    } else {
        m_stats.cover = world.evaluateCoverAt(m_position, m_aimTarget);
    }

    glm::vec3 muzzle = (glm::length(m_rig.getMuzzlePosition()) > 0.1f)
        ? m_rig.getMuzzlePosition()
        : (m_position + glm::vec3(0.0f, 1.25f, 0.0f) + glm::vec3(std::sin(m_yaw), 0.0f, std::cos(m_yaw)) * 0.75f);
    glm::vec3 hitPoint, hitNormal;
    WorldObstacle* hitObs = nullptr;
    m_aimBlocked = world.raycast(muzzle, unifiedTarget, hitPoint, hitNormal, hitObs);
    glm::vec3 targetHit = m_aimBlocked ? hitPoint : unifiedTarget;
    if (glm::length(m_aimHitPoint) < 0.001f) {
        m_aimHitPoint = targetHit;
    } else {
        // Faster interpolation when aiming to eliminate reticle lag on zoom mode switches
        float aimHitSpeed = m_isAiming ? 65.0f : 36.0f;
        m_aimHitPoint = glm::mix(m_aimHitPoint, targetHit, std::clamp(dt * aimHitSpeed, 0.0f, 1.0f));
    }

    // 6. Reload Handling (With Slot & Weapon Identity Validation)
    if (m_isReloading) {
        m_reloadTimer -= dt;
        if (m_reloadTimer <= 0.0f) {
            auto& equipped = m_inventory.getEquipped(m_activeWeaponSlot);
            // Ensure slot and weapon match what originally began reloading
            if (m_activeWeaponSlot == m_reloadingSlot && equipped.has_value() && equipped->id == m_reloadingWeaponId) {
                Item& weapon = equipped.value();
                int needed = weapon.maxMag - weapon.currentMag;
                int available = m_inventory.getAmmoCount(weapon.caliber);
                if (available <= 0) available = m_inventory.getAmmoCount(weapon.weaponType);
                int loaded = std::min(needed, available);
                if (loaded > 0) {
                    int actuallyLoaded = m_inventory.consumeAmmoCount(weapon.caliber, loaded);
                    if (actuallyLoaded <= 0) actuallyLoaded = m_inventory.consumeAmmoCount(weapon.weaponType, loaded);
                    if (actuallyLoaded > 0) {
                        weapon.currentMag += actuallyLoaded;
                    }
                }
            }
            m_isReloading = false;
            m_reloadingWeaponId = -1;
        }
    }

    // 7. Aim Bloom & Weapon Recoil Dynamics
    auto& equipped = m_inventory.getEquipped(m_activeWeaponSlot);
    float targetBloom = 0.035f;
    float recoilRecovRate = 9.0f;

    if (equipped.has_value()) {
        const Item& w = equipped.value();
        recoilRecovRate = w.recoilRecovery;
        targetBloom = m_isAiming ? w.baseSpread : w.hipFireSpread;
    }

    if (m_isAiming) {
        targetBloom *= 0.45f;
        if (m_isHoldingBreath) {
            targetBloom *= 0.22f; // Steady Aim locked rock-steady!
        }
    } else if (m_stats.stance == PlayerStance::Crouching) {
        targetBloom *= 0.70f;
    } else if (m_stats.stance == PlayerStance::Prone) {
        targetBloom *= 0.40f;
    }

    // Body organ trauma aim sway (pneumothorax / fractured arm)
    targetBloom += m_body.getAimSwayAmount();

    // Suppression expands accuracy bloom
    if (m_stats.isPinned) targetBloom *= 2.2f;
    else if (m_stats.isSuppressed) targetBloom *= 1.45f;

    if (isMoving) targetBloom += wantsSprint ? 0.012f : 0.003f;

    // Organic 2nd-order damped harmonic spring-mass recoil recovery
    // d^2x/dt^2 = -2 * zeta * omega * dx/dt - omega^2 * x
    float omega = recoilRecovRate * 4.2f;
    float zeta = 0.88f; // Near-critical damping for crisp tactical return without rubber-banding

    // Pitch spring
    float accelPitch = -2.0f * zeta * omega * m_recoilPitchVel - (omega * omega) * m_recoilPitch;
    m_recoilPitchVel += accelPitch * dt;
    m_recoilPitch += m_recoilPitchVel * dt;
    if (m_recoilPitch < 0.0f && m_recoilPitchVel < 0.0f) {
        m_recoilPitch = 0.0f;
        m_recoilPitchVel = 0.0f;
    }

    // Yaw spring (settles twice as fast laterally)
    float omegaYaw = omega * 1.4f;
    float accelYaw = -2.0f * zeta * omegaYaw * m_recoilYawVel - (omegaYaw * omegaYaw) * m_recoilYaw;
    m_recoilYawVel += accelYaw * dt;
    m_recoilYaw += m_recoilYawVel * dt;
    if (std::abs(m_recoilYaw) < 0.0005f && std::abs(m_recoilYawVel) < 0.001f) {
        m_recoilYaw = 0.0f;
        m_recoilYawVel = 0.0f;
    }

    // View kick compensation (tactical weapon recentering after burst firing)
    if (camera && m_recoilClimbAccum > 0.001f && (!fireInput || m_fireCooldown > 0.02f)) {
        float recoverRate = 8.5f;
        float recoverStep = m_recoilClimbAccum * std::clamp(dt * recoverRate, 0.0f, 1.0f);
        if (recoverStep < 0.0005f) recoverStep = m_recoilClimbAccum;
        camera->rotatePitch(recoverStep);
        m_recoilClimbAccum -= recoverStep;
        if (m_recoilClimbAccum < 0.0005f) m_recoilClimbAccum = 0.0f;
    }

    float bloomRecoveryRate = (m_recoilPitch > 0.005f) ? 28.0f : 40.0f;
    m_currentBloom = glm::mix(m_currentBloom, targetBloom + m_recoilPitch * 0.15f, std::clamp(dt * bloomRecoveryRate, 0.0f, 1.0f));

    // 8. Weapon Firing Handling (Crisp Single-Shot Tapping & Exact RPM on Auto)
    m_fireCooldown = std::max(0.0f, m_fireCooldown - dt);

    fireInput = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) && !uiWantsMouse;

    bool freshClick = fireInput && !m_firePressedLast;

    if (equipped.has_value()) {
        Item& weapon = equipped.value();
        bool canFire = weapon.isAutomatic ? fireInput : freshClick;
        float fireInterval = 60.0f / std::max(60.0f, weapon.fireRateRpm);

        static std::mt19937 randGen([](){
            std::random_device rd;
            return rd();
        }());
        std::uniform_real_distribution<float> spreadDist(-1.0f, 1.0f);

        if (canFire && m_fireCooldown <= 0.0f && !m_isReloading && !m_isSwitchingWeapon && !m_isVaulting) {
            if (weapon.currentMag > 0) {
                weapon.currentMag--;
                m_fireCooldown = fireInterval;

                // Weapon recoil impulse scaled by anatomical arm/body health and stance
                float recoilMod = m_body.getRecoilMultiplier();
                if (m_isHoldingBreath) recoilMod *= 0.65f;
                if (m_inCover && m_isLowCover) {
                    recoilMod *= (1.0f - 0.50f * m_coverAimWeight); // -50% recoil on cover mount
                }
                float vRecoil = (weapon.recoilVertical * 0.00045f) * recoilMod;
                float hRecoil = (weapon.recoilHorizontal * 0.00028f) * recoilMod;

                if (m_isAiming) {
                    vRecoil *= 0.55f;
                    hRecoil *= 0.45f;
                }
                if (m_inCover) {
                    vRecoil *= 0.50f; // Braced on cover!
                    hRecoil *= 0.40f;
                } else if (m_stats.stance == PlayerStance::Crouching) {
                    vRecoil *= 0.70f;
                } else if (m_stats.stance == PlayerStance::Prone) {
                    vRecoil *= 0.40f;
                }

                // Dynamic spring-mass impulse injection
                m_recoilPitchVel += vRecoil * 18.0f;
                m_recoilPitch += vRecoil * 0.35f;
                float yawSpread = spreadDist(randGen);
                m_recoilYawVel += yawSpread * (hRecoil * 16.0f);
                m_recoilYaw += yawSpread * (hRecoil * 0.25f);
                m_currentBloom = std::min(0.09f, m_currentBloom + vRecoil * 0.35f);

                // Frame-accurate muzzle spawn position directly from 3D weapon barrel matrix
                glm::vec3 muzzlePos = m_rig.getMuzzlePosition();

                glm::vec3 targetPoint = unifiedTarget;
                glm::vec3 playerFwd(std::sin(m_yaw), 0.0f, std::cos(m_yaw));
                float fwdDist = glm::dot(targetPoint - muzzlePos, playerFwd);
                if (fwdDist < 0.20f) {
                    targetPoint += playerFwd * (0.20f - fwdDist);
                }

                glm::vec3 aimDir = targetPoint - muzzlePos;
                if (glm::length(aimDir) > 0.001f) {
                    aimDir = glm::normalize(aimDir);
                } else {
                    aimDir = playerFwd;
                }

                glm::vec3 upRef = (std::abs(aimDir.y) > 0.95f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
                glm::vec3 rightDir = glm::normalize(glm::cross(aimDir, upRef));
                glm::vec3 trueUp   = glm::normalize(glm::cross(rightDir, aimDir));

                float spreadMult = m_isAiming ? 0.15f : 1.0f;
                float spreadX = spreadDist(randGen) * (m_currentBloom * spreadMult);
                float spreadY = spreadDist(randGen) * (m_currentBloom * spreadMult);
                aimDir = glm::normalize(aimDir + rightDir * spreadX + trueUp * spreadY);

                float kickMagnitude = 0.035f * (weapon.recoilVertical / 40.0f) * recoilMod;
                m_rig.triggerMuzzleFlash(kickMagnitude);

                // Caliber and weapon classification
                int wTypeInt = 0;
                float flashScale = 1.0f;
                float smokeScale = 1.0f;
                float trauma = 0.09f;
                if (weapon.weaponType == WeaponType::Sniper) {
                    wTypeInt = 1;
                    flashScale = 1.55f;
                    smokeScale = 1.60f;
                    trauma = 0.22f;
                } else if (weapon.weaponType == WeaponType::Pistol) {
                    wTypeInt = 2;
                    flashScale = 0.75f;
                    smokeScale = 0.65f;
                    trauma = 0.045f;
                }

                breakSpawnProtection();

                // Procedural Multi-petal Muzzle Flash & Volumetric Gunsmoke exactly at muzzle crown
                ballistics.spawnMuzzleFlash(muzzlePos, aimDir, flashScale, wTypeInt);
                ballistics.spawnMuzzleSmoke(muzzlePos, aimDir, smokeScale);

                // 3D Brass Shell Casing with physical ejection directly from chamber ejection port
                glm::vec3 casingPos = (glm::length(m_rig.getCasingPosition()) > 0.1f)
                    ? m_rig.getCasingPosition()
                    : (muzzlePos - aimDir * 0.28f + rightDir * 0.08f + trueUp * 0.03f);
                ballistics.spawnShellCasing(casingPos, rightDir, trueUp, aimDir, wTypeInt);

                if (camera) {
                    float pitchClimb = (weapon.recoilVertical * 0.014f) * recoilMod;
                    if (m_isAiming) pitchClimb *= 0.50f;
                    if (m_inCover) pitchClimb *= 0.45f;
                    if (m_isHoldingBreath) pitchClimb *= 0.60f;
                    if (m_stats.stance == PlayerStance::Crouching) pitchClimb *= 0.70f;
                    else if (m_stats.stance == PlayerStance::Prone) pitchClimb *= 0.35f;

                    camera->rotatePitch(-pitchClimb);
                    camera->addTrauma(trauma);
                    m_recoilClimbAccum = std::min(6.5f, m_recoilClimbAccum + pitchClimb);
                }

                float maxRange = weapon.effectiveRange;
                if (maxRange < 20.0f) maxRange = 25.0f;

                ballistics.fireBullet(
                    muzzlePos,
                    aimDir,
                    weapon.muzzleVelocityMs,
                    weapon.damage,
                    weapon.penetrationPower,
                    true,
                    m_faction,
                    -1,
                    maxRange,
                    wTypeInt
                );

                BulletFireEvent fireEvt;
                fireEvt.origin = muzzlePos;
                fireEvt.direction = aimDir;
                fireEvt.speed = weapon.muzzleVelocityMs;
                fireEvt.damage = weapon.damage;
                fireEvt.weaponType = (weapon.weaponType == WeaponType::Sniper) ? 1 : ((weapon.weaponType == WeaponType::Pistol) ? 2 : 0);
                fireEvt.faction = static_cast<uint8_t>(m_faction);
                NetworkEngine::get().broadcastBulletFire(fireEvt);

                AcousticEnvironment fireEnv = world.detectAcousticEnvironment(m_position);
                if (weapon.weaponType == WeaponType::Rifle) AudioEngine::get().playSound(SoundType::RifleShot, 1.0f, 1.0f, fireEnv);
                else if (weapon.weaponType == WeaponType::Sniper) AudioEngine::get().playSound(SoundType::SniperShot, 1.0f, 1.0f, fireEnv);
                else if (weapon.weaponType == WeaponType::Pistol) AudioEngine::get().playSound(SoundType::PistolShot, 1.0f, 1.0f, fireEnv);
            } else {
                if (freshClick) {
                    AudioEngine::get().playSound(SoundType::DryFire);
                }
            }
        }
    }
    m_firePressedLast = fireInput;


    // Exhausted tactical breathing when running out of stamina
    if (m_stats.stamina < 15.0f && isMoving) {
        m_exhaustionTimer += dt;
        if (m_exhaustionTimer >= 1.6f) {
            m_exhaustionTimer = 0.0f;
            AudioEngine::get().playSound(SoundType::HeavyBreathing, 0.32f, 1.0f);
        }
    } else {
        m_exhaustionTimer = 0.0f;
    }

    // Character rig IK update was performed earlier with fresh locomotion for frame-accurate muzzle alignment
}

void Player::updateLobby(float dt, const glm::vec3& cameraPos) {
    // Face directly towards camera with subtle quarter-turn operator stance and interactive MMO turntable
    glm::vec3 lookDir = cameraPos - m_position;
    m_yaw = std::atan2(lookDir.x, lookDir.z) + m_lobbyTurntableOffset;
    m_rig.snapYaw(m_yaw);
    // Eye-contact aim target: 1.58m eye level straight towards camera
    glm::vec3 aimTarget = m_position + glm::vec3(0.0f, 1.48f, 0.0f) + glm::vec3(std::sin(m_yaw), 0.0f, std::cos(m_yaw)) * 6.0f;
    bool hasWeapon = m_inventory.getEquipped(m_activeWeaponSlot).has_value();
    m_rig.update9D(dt, m_position, m_yaw, aimTarget, glm::vec3(0.0f), false, false, 0, false, false, hasWeapon, false, 0.0f);
}

void Player::render(Renderer& renderer, const CharacterCustomization& custom, bool isInGame) {
    // 1. Company of Heroes Directional Cover Ground Ring Indicator
    if (isInGame) {
        float coverOpacity = (m_stats.cover == CoverType::Exposed) ? 0.30f : 0.75f;
    }

    // 2. Render Tactical Operator with articulated skeletal rig & modular gear
    if (m_isMounted) {
        m_rig.setActiveWeaponName("None");
    } else {
        auto& weaponOpt = m_inventory.getEquipped(m_activeWeaponSlot);
        m_rig.setActiveWeaponName(weaponOpt.has_value() ? weaponOpt->name : "None");
    }
    m_rig.render(renderer, custom);
}


} // namespace Mercenary

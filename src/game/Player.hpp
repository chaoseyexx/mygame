#pragma once
#include "engine/Types.hpp"
#include "engine/Window.hpp"
#include "engine/Renderer.hpp"
#include "engine/CharacterRig.hpp"
#include "SurvivalStats.hpp"
#include "Inventory.hpp"
#include "Ballistics.hpp"
#include "World.hpp"
#include "BodySystem.hpp"

namespace Mercenary {

struct MountedTurret;

struct AimTargetInfo {
    bool hasTarget = false;
    bool isHeadshot = false;
    glm::vec3 worldPos{0.0f};
    float bodyHeight = 0.0f; // 0.0m to 1.85m on body
    float bodyHoriz = 0.0f;  // horizontal offset from center spine
    int targetId = -1;
    bool isObstacle = false;
};

class Player {
public:
    Player();

    void update(float dt, const Window& window, const glm::vec3& mouseWorldPos, World& world, BallisticsSystem& ballistics, float cameraYaw = 0.0f, Camera* camera = nullptr, const AimTargetInfo* aimInfo = nullptr);
    bool hasTargetLock() const { return m_hasTargetLock; }
    bool isHeadshotAim() const { return m_isHeadshotAim; }
    const AimTargetInfo& getAimTargetInfo() const { return m_aimTargetInfo; }
    void updateLobby(float dt, const glm::vec3& cameraPos);
    void render(Renderer& renderer, const CharacterCustomization& custom, bool isInGame = false);

    glm::vec3 getPosition() const { return m_position; }
    glm::vec3 getAimTarget() const { return m_aimTarget; }
    float getAimBloom() const { return m_currentBloom; }
    float getYaw() const { return m_yaw; }
    void setYaw(float yaw) { m_yaw = yaw; }

    SurvivalStats& getStats() { return m_stats; }
    const SurvivalStats& getStats() const { return m_stats; }

    BodySystem& getBody() { return m_body; }
    const BodySystem& getBody() const { return m_body; }

    const CharacterRig& getRig() const { return m_rig; }
    CharacterRig& getRig() { return m_rig; }
    glm::vec3 getHeadPosition() const { return m_rig.getHeadPosition(); }

    Inventory& getInventory() { return m_inventory; }
    const Inventory& getInventory() const { return m_inventory; }

    void reloadCurrentWeapon();
    void switchWeapon(EquipSlot slot);
    void spawnAtFactionBase(Faction faction);
    void resetFullHealthAndVitals();
    void replenishTdmLoadout();
    bool hasSpawnProtection() const { return m_spawnProtectionTimer > 0.0f; }
    float getSpawnProtectionTimer() const { return m_spawnProtectionTimer; }
    void setSpawnProtection(float sec) { m_spawnProtectionTimer = sec; }
    void updateSpawnProtection(float dt) { if (m_spawnProtectionTimer > 0.0f) m_spawnProtectionTimer = std::max(0.0f, m_spawnProtectionTimer - dt); }
    void breakSpawnProtection() { m_spawnProtectionTimer = 0.0f; }
    void setPosition(const glm::vec3& pos) { m_position = pos; }
    void setFaction(Faction f) { m_faction = f; }
    Faction getFaction() const { return m_faction; }

    bool isAiming() const { return m_isAiming; }
    bool isSprinting() const { return m_stats.isSprinting; }
    glm::vec3 getVelocity() const { return m_velocity; }
    void setVelocity(const glm::vec3& vel) { m_velocity = vel; }
    void addVelocity(const glm::vec3& delta) { m_velocity += delta; }
    bool isVaulting() const { return m_isVaulting; }
    bool isClimbing() const { return m_isClimbing; }
    bool isJumping() const { return m_isJumping; }
    bool isSliding() const { return m_isSliding; }
    glm::vec2 getSightSway() const { return m_currentSightSway; }
    void setAiming(bool a) { m_isAiming = a; }
    bool dropCurrentWeapon(World& world);
    bool dropItem(World& world, size_t inventoryIndex);
    bool dropEquippedItem(World& world, EquipSlot slot);
    bool useMedicalItem(size_t index);
    bool useSurvivalItem(size_t index);
    bool useQuickMedical();
    void updateArmorFromEquipped();
    EquipSlot getActiveWeaponSlot() const { return m_activeWeaponSlot; }
    bool hasEquippedWeapon() const { return m_inventory.getEquipped(m_activeWeaponSlot).has_value(); }
    bool isAimBlocked() const { return m_aimBlocked; }
    CoverType getCover() const { return m_stats.cover; }

    // GTA 5 / Ghost Recon Dynamic Cover Mechanics
    bool isInCover() const { return m_inCover; }
    bool isAtCoverEdge() const { return m_isAtCoverEdge; }
    float getCoverEdgeDir() const { return m_coverEdgeDir; }
    float getCoverLeanAngle() const { return m_coverLeanAngle; }
    float getVaultProgress() const { return m_vaultDuration > 0.0f ? (m_vaultTimer / m_vaultDuration) : 0.0f; }
    glm::vec3 getVaultHandPlantPos() const { return m_vaultHandPlantPos; }
    bool isLowCover() const { return m_isLowCover; }
    bool isNearCover() const { return m_nearCover; }
    glm::vec3 getCoverNormal() const { return m_coverNormal; }
    void exitCover() { m_inCover = false; }
    void setInCover(bool inCover, bool isLowCover = true, float coverHeight = 0.95f, const glm::vec3& normal = glm::vec3(0.0f, 0.0f, 1.0f)) {
        m_inCover = inCover;
        m_isLowCover = isLowCover;
        m_coverHeight = coverHeight;
        m_coverNormal = normal;
        if (m_isLowCover) m_stats.stance = PlayerStance::Crouching;
    }

    bool deployFortification(World& world);

    bool isMounted() const { return m_isMounted; }
    MountedTurret* getActiveTurret() { return m_activeTurret; }
    void mountTurret(MountedTurret* turret);
    void dismountTurret();

    float getLobbyTurntableOffset() const { return m_lobbyTurntableOffset; }
    void setLobbyTurntableOffset(float off) { m_lobbyTurntableOffset = off; }
    void addLobbyTurntableOffset(float delta) { m_lobbyTurntableOffset += delta; }

    int getAdsZoomLevel() const { return m_adsZoomLevel; }
    float getEyeHeight() const {
        if (m_stats.stance == PlayerStance::Prone) return 0.38f;
        if (m_stats.stance == PlayerStance::Crouching) return 1.05f;
        return 1.55f;
    }
    bool isHoldingBreath() const { return m_isHoldingBreath; }
    float getCurrentBloom() const { return m_currentBloom; }
    float getRecoilPitch() const { return m_recoilPitch; }
    float getRecoilYaw() const { return m_recoilYaw; }
    bool isAirborne() const { return m_isJumping || m_isVaulting || m_isClimbing; }
    const glm::vec3& getAimHitPoint() const { return m_aimHitPoint; }

private:
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_aimTarget{0.0f};
    float m_yaw = 0.0f; // Body rotation angle
    float m_lobbyTurntableOffset = 0.0f; // Interactive turntable in hideout & character creator
    Faction m_faction = Faction::FactionA;

    bool m_isAiming = false;
    float m_exhaustionTimer = 0.0f;

    float m_spawnProtectionTimer = 0.0f;
    float m_baseMoveSpeed = 5.8f; // m/s
    float m_currentBloom = 0.04f;
    float m_fireCooldown = 0.0f;
    float m_reloadTimer = 0.0f;
    bool m_isReloading = false;
    EquipSlot m_reloadingSlot = EquipSlot::PrimaryWeapon;
    int m_reloadingWeaponId = -1;

    glm::vec3 m_prevPosition{0.0f, 0.0f, 3.0f};
    float m_footstepDistanceAccumulator = 0.0f;
    float m_footstepTimer = 0.0f;
    bool m_lastFootLeft = false;
    int m_totalSteps = 0;
    float m_lastStepPitch = 1.0f;
    float m_lastStepVolume = 1.0f;
    std::string m_lastSoundTriggered = "None";
    float m_sandbagCooldown = 0.0f;
    bool m_aimBlocked = false;
    glm::vec3 m_aimHitPoint{0.0f};
    bool m_hasTargetLock = false;
    bool m_isHeadshotAim = false;
    AimTargetInfo m_aimTargetInfo;

    bool m_isSwitchingWeapon = false;
    float m_weaponSwitchTimer = 0.0f;
    EquipSlot m_pendingSwitchSlot = EquipSlot::PrimaryWeapon;

    EquipSlot m_activeWeaponSlot = EquipSlot::PrimaryWeapon;
    SurvivalStats m_stats;
    BodySystem m_body;
    Inventory m_inventory;
    CharacterRig m_rig;

    // Advanced Weapon Handling & Recoil Kinematics
    float m_adsProgress = 0.0f;
    float m_recoilPitch = 0.0f;
    float m_recoilYaw = 0.0f;
    float m_recoilPitchVel = 0.0f;
    float m_recoilYawVel = 0.0f;
    float m_recoilClimbAccum = 0.0f;

    // Dynamic Tactical Cover System (Q to Cover)
    bool m_inCover = false;
    bool m_nearCover = false;
    bool m_isLowCover = true;
    glm::vec3 m_coverNormal{0.0f, 0.0f, 1.0f};
    glm::vec3 m_coverWallDir{1.0f, 0.0f, 0.0f};
    glm::vec3 m_coverSnapPos{0.0f};
    float m_coverHeight = 1.0f;
    float m_coverDepth = 0.6f;
    float m_coverAimWeight = 0.0f;
    bool m_isSnappingToCover = false;
    float m_coverSnapTimer = 0.0f;
    float m_coverSnapDuration = 0.14f;
    glm::vec3 m_coverSnapStartPos{0.0f};
    bool m_isAtCoverEdge = false;
    float m_coverEdgeDir = 0.0f;
    float m_coverLeanAngle = 0.0f;
    bool m_qPressedLast = false;
    bool m_spacePressedLast = false;
    bool m_fPressedLast = false;

    // Heavy Machine Gun Emplacement Controls
    bool m_isMounted = false;
    MountedTurret* m_activeTurret = nullptr;
    float m_turretBaseYaw = 0.0f;
    float m_playerVoiceBarkTimer = 0.0f;

    // Athletic Locomotion: Tactical Jump, Barrier Vaulting, High Wall Mantle & Sprint Slide
    bool m_isJumping = false;
    float m_jumpTimer = 0.0f;
    float m_jumpDuration = 0.50f;
    float m_jumpHeight = 0.85f;
    float m_jumpBaseY = 0.0f;

    bool m_isVaulting = false;
    float m_vaultTimer = 0.0f;
    float m_vaultDuration = 0.42f;
    glm::vec3 m_vaultStartPos{0.0f};
    glm::vec3 m_vaultTargetPos{0.0f};
    float m_vaultPeakY = 0.0f;
    glm::vec3 m_vaultHandPlantPos{0.0f};
    glm::vec3 m_vaultTraverseDir{0.0f, 0.0f, 1.0f};

    bool m_isClimbing = false;
    float m_climbTimer = 0.0f;
    float m_climbDuration = 0.72f;
    glm::vec3 m_climbStartPos{0.0f};
    glm::vec3 m_climbTargetPos{0.0f};
    float m_climbPeakY = 0.0f;
    float m_climbWallHeight = 2.2f;

    bool m_isSliding = false;
    float m_slideTimer = 0.0f;
    float m_slideDuration = 0.65f;
    glm::vec3 m_slideDirection{0.0f};

    // Dynamic ADS Breathing & Sight Sway
    float m_breathSwayTime = 0.0f;
    glm::vec2 m_currentSightSway{0.0f};

    // Advanced Aiming & Ballistic Controls
    int m_adsZoomLevel = 0; // 0: 1.0x Reflex, 1: 2.5x Combat ACOG, 2: 4x/6x Precision Scope
    bool m_isHoldingBreath = false;
    float m_breathHoldTimer = 0.0f;
    bool m_hPressedLast = false;
    bool m_toggleOpticPressedLast = false;
    bool m_cPressedLast = false;
    bool m_xPressedLast = false;
    bool m_rPressedLast = false;
    bool m_gPressedLast = false;
    bool m_fourPressedLast = false;
    bool m_onePressedLast = false;
    bool m_twoPressedLast = false;
    bool m_threePressedLast = false;
    bool m_firePressedLast = false;
};

} // namespace Mercenary

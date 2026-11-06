#pragma once

#include "Types.hpp"

#include "Renderer.hpp"

#include "IK.hpp"

#include "Mesh.hpp"

#include "SkinnedSoldier.hpp"

#include "game/CharacterProfile.hpp"



namespace Mercenary {



enum class LocomotionDirection : uint8_t {

    Stationary = 0,   // Idle / Aiming in place

    Forward = 1,      // Forward sprint/jog

    ForwardRight = 2, // Diagonal right forward

    Right = 3,        // Lateral strafe right

    BackwardRight = 4,// Diagonal right backward

    Backward = 5,     // Direct backpedal

    BackwardLeft = 6, // Diagonal left backward

    Left = 7,         // Lateral strafe left

    ForwardLeft = 8   // Diagonal left forward

};



enum class StepTurnPhase : uint8_t {

    Idle = 0,

    Beat1_LeadFoot,

    Beat2_TrailingFoot

};



enum class DeathType : uint8_t {

    None = 0,

    CollapseForward,

    CollapseBackward,

    CollapseCrumple,

    BlastKnockdown

};



struct HandPose {

    float triggerPull = 0.0f; // 0.0 = finger resting on guard, 1.0 = finger pulled trigger

    float gripTightness = 1.0f;

};



class CharacterRig {

public:

    CharacterRig();

    ~CharacterRig();



    void update(float dt, const glm::vec3& worldPos, float yaw, const glm::vec3& aimTarget,

                bool isMoving, bool isSprinting, int stance, bool isFiring,

                bool isAiming = false, bool hasWeapon = true, bool isReloading = false, float reloadProgress = 0.0f) {

        update9D(dt, worldPos, yaw, aimTarget, glm::vec3(0.0f), isMoving, isSprinting, stance, isFiring, isAiming, hasWeapon, isReloading, reloadProgress);

    }



    void update9D(float dt, const glm::vec3& worldPos, float yaw, const glm::vec3& aimTarget,

                  const glm::vec3& velocity, bool isMoving, bool isSprinting, int stance, bool isFiring,

                  bool isAiming = false, bool hasWeapon = true, bool isReloading = false, float reloadProgress = 0.0f);



    void render(Renderer& renderer, const CharacterCustomization& custom);



    void setActiveWeaponName(const std::string& name) { m_activeWeaponName = name; }

    const std::string& getActiveWeaponName() const { return m_activeWeaponName; }



    void triggerMuzzleFlash(float kickMagnitude = 0.08f) {

        m_muzzleFlashTimer = 0.07f;

        m_recoilKick = std::min(0.065f, m_recoilKick + kickMagnitude * 0.60f);

    }



    void setWoundState(float torsoHunch, float leftArmSag, float rightArmSag, float limpFactor, float flinchPitch, float flinchRoll) {

        m_woundTorsoHunch = torsoHunch;

        m_woundLeftArmSag = leftArmSag;

        m_woundRightArmSag = rightArmSag;

        m_woundLimpFactor = limpFactor;

        m_woundFlinchPitch = flinchPitch;

        m_woundFlinchRoll = flinchRoll;

    }



    void snapYaw(float yaw) {
        m_yaw = yaw;
        m_lowerBodyYaw = yaw;
        m_leftFootPlantedYaw = yaw;
        m_rightFootPlantedYaw = yaw;
        m_stanceFeetInitialized = true;
    }



    glm::vec3 getMuzzlePosition() const { return m_muzzlePos; }
    glm::vec3 getMuzzleForward() const { return m_muzzleForward; }
    float getAimWeight() const { return m_aimWeight; }

    glm::vec3 getCasingPosition() const { return m_casingPos; }

    glm::vec3 getRifleRoot() const { return m_rifleRoot; }

    float getRenderPitch() const { return m_renderPitch; }

    void updateWeaponTransform();

    glm::vec3 getHeadPosition() const { return m_pose.bonePositions[BONE_HEAD]; }

    Mesh* getMesh() const { return m_dynamicMesh.get(); }

    void setDirectAimPitch(float pitchRad) {
        m_directAimPitch = pitchRad;
        m_hasDirectAimPitch = true;
    }

    void setLookYaw(float yawRad) {
        m_lookYaw = yawRad;
        m_hasLookYaw = true;
    }

    float getAimPitch() const { return m_aimPitch; }

    bool isAiming() const { return m_isAiming; }

    void triggerEquipAnim(float duration = 0.35f) {

        m_equipAnimTimer = duration;

        m_equipAnimDuration = duration;

        m_dropAnimTimer = 0.0f;

    }

    void triggerDropAnim(const std::string& weaponName = "", float duration = 0.28f) {

        m_dropAnimTimer = duration;

        m_dropAnimDuration = duration;

        if (!weaponName.empty()) {

            m_droppingWeaponName = weaponName;

        } else {

            m_droppingWeaponName = m_activeWeaponName;

        }

        m_equipAnimTimer = 0.0f;

    }

    bool isDropping() const { return m_dropAnimTimer > 0.0f; }
    bool isEquipping() const { return m_equipAnimTimer > 0.0f; }
    float getCrouchWeight() const { return m_crouchTransition; }
    bool hasWeapon() const { return m_hasWeapon; }

    // Biomechanically synchronized heel-strike footstep events (1 = Left heel strike, 2 = Right heel strike, 3 = Turn shuffle)
    int consumeFootstepEvent() {
        int evt = m_footstepEvent;
        m_footstepEvent = 0;
        return evt;
    }

    LocomotionDirection getLocomotionDirection() const { return m_locoDir; }

    float getRelativeMoveAngle() const { return m_relMoveAngle; }

    float getTorsoTwist() const { return m_torsoTwist; }



    void triggerDeath(DeathType type, const glm::vec3& hitImpulse = glm::vec3(0.0f), bool deathGrip = false, uint32_t dismemberMask = 0) {

        m_isDead = true;

        m_deathType = type;

        m_hitImpulse = hitImpulse;

        m_deathGrip = deathGrip;

        m_dismemberMask = dismemberMask;

        m_deathTimer = 0.0f;

    }

    void resetDeath() {

        m_isDead = false;

        m_deathType = DeathType::None;

        m_deathTimer = 0.0f;

        m_dismemberMask = 0;

        m_deathGrip = false;

        m_hitImpulse = glm::vec3(0.0f);

    }

    bool isDead() const { return m_isDead; }

    DeathType getDeathType() const { return m_deathType; }

    float getDeathProgress() const { return std::clamp(m_deathTimer / 1.0f, 0.0f, 1.0f); }

    void setDismemberMask(uint32_t mask) { m_dismemberMask = mask; }

    uint32_t getDismemberMask() const { return m_dismemberMask; }

    bool hasDeathGrip() const { return m_deathGrip; }

    void setTerrainHeight(float y) { m_terrainHeight = y; }

    void setAirborne(bool airborne) { m_isAirborne = airborne; }
    bool isAirborne() const { return m_isAirborne; }
    void setSliding(bool sliding) { m_isSliding = sliding; }
    bool isSliding() const { return m_isSliding; }
    void setShoulderRight(bool right) { m_shoulderRight = right; }
    bool isShoulderRight() const { return m_shoulderRight; }

    void setGroundSurface(GroundSurface surf) { m_groundSurface = surf; }

    StepTurnPhase getStepTurnPhase() const { return m_stepTurnPhase; }

    float getLowerBodyYaw() const { return m_lowerBodyYaw; }

    void setCoverParameters(bool inCover, bool isLowCover, float coverHeight, float coverAimWeight, float coverLeanAngle = 0.0f) {
        m_inCover = inCover;
        m_isLowCover = isLowCover;
        m_coverHeight = coverHeight;
        m_coverAimWeight = coverAimWeight;
        m_coverLeanAngle = coverLeanAngle;
    }

    void setCoverLeanAngle(float leanAngle) {
        m_coverLeanAngle = leanAngle;
    }

    void setVaultParameters(bool isVaulting, float progress, const glm::vec3& plantPos) {
        m_isVaulting = isVaulting;
        m_vaultProgress = progress;
        m_vaultPlantPos = plantPos;
    }



private:

    std::string m_activeWeaponName = "M4A1";

    glm::vec3 m_rootPos{0.0f};

    float m_yaw = 0.0f;

    float m_aimPitch = 0.0f;



    // Tactical aim & weapon state

    bool m_isAiming = false;
    bool m_shoulderRight = true;

    float m_aimWeight = 0.0f; // Smooth ADS transition

    bool m_hasWeapon = true;

    bool m_isReloading = false;

    float m_reloadProgress = 0.0f;

    float m_hipFireTimer = 0.0f;



    // 9D Locomotion System State

    glm::vec3 m_velocity{0.0f};

    LocomotionDirection m_locoDir = LocomotionDirection::Stationary;

    float m_relMoveAngle = 0.0f;

    float m_torsoTwist = 0.0f;

    float m_lowerBodyYaw = 0.0f;

    float m_pelvisSway = 0.0f;

    float m_turnInPlaceTimer = 0.0f;

    float m_turnInPlaceDir = 0.0f;

    float m_strideCycle = 0.0f;



    // World-Space Planted Foot IK & Two-Beat Step-Turn State

    bool m_feetInitialized = false;

    glm::vec3 m_leftFootWorldPos{0.0f};

    glm::vec3 m_rightFootWorldPos{0.0f};

    float m_leftFootYaw = 0.0f;

    float m_rightFootYaw = 0.0f;



    StepTurnPhase m_stepTurnPhase = StepTurnPhase::Idle;

    float m_stepTurnTimer = 0.0f;

    float m_stepTurnDurationBeat1 = 0.20f;

    float m_stepTurnDurationBeat2 = 0.18f;

    int m_stepTurnFoot = 0; // +1 = Right lead, -1 = Left lead

    glm::vec3 m_stepStartPos{0.0f};

    glm::vec3 m_stepTargetPos{0.0f};

    float m_stepStartYaw = 0.0f;

    float m_stepTargetYaw = 0.0f;

    float m_pelvisStepSway = 0.0f;



    bool m_isAirborne = false;
    bool m_isSliding = false;
    GroundSurface m_groundSurface = GroundSurface::Dirt;
    float m_stepTurnDir = 1.0f;
    float m_stepTurnStartLowerYaw = 0.0f;
    float m_stepTurnBeat1Yaw = 0.0f;
    float m_stepTurnTargetLowerYaw = 0.0f;
    bool m_isSingleLegOnly = false;
    float m_leftFootPlantedYaw = 0.0f;
    float m_rightFootPlantedYaw = 0.0f;
    bool m_stanceFeetInitialized = false;

    // Locomotion & walk cycle
    float m_walkCycle = 0.0f;
    float m_prevWalkPhase = 0.0f;
    int m_footstepEvent = 0;
    float m_idleTime = 0.0f;
    float m_pelvisBob = 0.0f;
    bool m_isMoving = false;
    int m_prevStance = 0;
    bool m_isSprinting = false;
    int m_stance = 0; // 0 = standing, 1 = crouching, 2 = prone
    float m_stanceHeight = 0.0f;
    float m_crouchTransition = 0.0f;
    float m_proneTransition = 0.0f;

    float m_equipAnimTimer = 0.0f;

    float m_equipAnimDuration = 0.38f;

    float m_dropAnimTimer = 0.0f;

    float m_dropAnimDuration = 0.28f;

    std::string m_droppingWeaponName = "M4A1";



    // Recoil & muzzle flash

    float m_recoilKick = 0.0f;

    float m_recoilPitchKick = 0.0f;

    float m_recoilRollKick = 0.0f;

    float m_muzzleFlashTimer = 0.0f;

    glm::vec3 m_rifleRoot{0.0f};

    float m_directAimPitch = 0.0f;
    bool m_hasDirectAimPitch = false;

    float m_lookYaw = 0.0f;
    bool m_hasLookYaw = false;
    float m_currentChestYaw = 0.0f;

    float m_renderPitch = 0.0f;
    float m_weaponLagYaw = 0.0f;
    float m_weaponLagPitch = 0.0f;
    float m_sprintCarryAngle = 0.0f;

    glm::vec3 m_muzzlePos{0.0f};
    glm::vec3 m_muzzleForward{0.0f, 0.0f, 1.0f};

    glm::vec3 m_casingPos{0.0f};



    // Procedural Inverse Kinematics Wound & Trauma State

    float m_woundTorsoHunch = 0.0f;

    float m_woundLeftArmSag = 0.0f;

    float m_woundRightArmSag = 0.0f;

    float m_woundLimpFactor = 0.0f;

    float m_woundFlinchPitch = 0.0f;

    float m_woundFlinchRoll = 0.0f;



    // Hand & finger states

    HandPose m_rightHand;

    HandPose m_leftHand;



    SkinnedSoldier m_skinnedModel;

    SkeletonPose m_pose;

    std::unique_ptr<Mesh> m_dynamicMesh;
    std::vector<Vertex> m_renderVertices;
    std::vector<uint32_t> m_renderIndices;



    // Procedural death collapse & dismemberment

    bool m_isDead = false;

    DeathType m_deathType = DeathType::None;

    float m_deathTimer = 0.0f;

    glm::vec3 m_hitImpulse{0.0f};

    bool m_deathGrip = false;

    uint32_t m_dismemberMask = 0;

    float m_terrainHeight = 0.0f;
    bool m_inCover = false;

    bool m_isLowCover = false;

    float m_coverHeight = 0.95f;

    float m_coverAimWeight = 0.0f;
    float m_coverLeanAngle = 0.0f;
    bool m_isVaulting = false;
    float m_vaultProgress = 0.0f;
    glm::vec3 m_vaultPlantPos{0.0f};

};



} // namespace Mercenary


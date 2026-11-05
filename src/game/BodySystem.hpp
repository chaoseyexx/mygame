#pragma once
#include "engine/Types.hpp"
#include <vector>
#include <string>

namespace Mercenary {

enum class BodyRegion : uint8_t {
    Head = 0,
    Neck,
    UpperChest,   // Heart, upper lungs, thoracic aorta
    LowerChest,   // Lower lungs, liver, spleen
    Abdomen,      // Stomach, intestines, kidneys
    LeftArm,
    RightArm,
    LeftLeg,
    RightLeg,
    NUM_REGIONS
};

inline const char* getBodyRegionName(BodyRegion r) {
    switch (r) {
        case BodyRegion::Head:        return "Head";
        case BodyRegion::Neck:        return "Neck";
        case BodyRegion::UpperChest:  return "Upper Chest";
        case BodyRegion::LowerChest:  return "Lower Chest";
        case BodyRegion::Abdomen:     return "Abdomen";
        case BodyRegion::LeftArm:     return "Left Arm";
        case BodyRegion::RightArm:    return "Right Arm";
        case BodyRegion::LeftLeg:     return "Left Leg";
        case BodyRegion::RightLeg:    return "Right Leg";
        default:                      return "Unknown";
    }
}

enum class Organ : uint8_t {
    None = 0,
    Brain,
    Spine,
    Heart,
    LeftLung,
    RightLung,
    Liver,
    Spleen,
    LeftKidney,
    RightKidney,
    Stomach,
    Intestines
};

inline const char* getOrganName(Organ o) {
    switch (o) {
        case Organ::Brain:       return "Brain";
        case Organ::Spine:       return "Spine";
        case Organ::Heart:       return "Heart";
        case Organ::LeftLung:    return "Left Lung";
        case Organ::RightLung:   return "Right Lung";
        case Organ::Liver:       return "Liver";
        case Organ::Spleen:      return "Spleen";
        case Organ::LeftKidney:  return "Left Kidney";
        case Organ::RightKidney: return "Right Kidney";
        case Organ::Stomach:     return "Stomach";
        case Organ::Intestines:  return "Intestines";
        default:                 return "None";
    }
}

struct OrganState {
    Organ organ = Organ::None;
    float health = 100.0f;
    float bleedRate = 0.0f;    // ml/s
    bool isVital = false;
    bool isFunctioning = true;
};

struct BodyPartState {
    BodyRegion region = BodyRegion::Head;
    float health = 100.0f;
    float maxHealth = 100.0f;
    float armorDurability = 0.0f;
    float armorMax = 0.0f;
    int armorClass = 0;       // Level 0 - 6
    bool isFractured = false;
    bool isBleeding = false;
    float bleedRate = 0.0f;   // ml/s
    std::vector<OrganState> organs;
};

struct WoundEffect {
    BodyRegion hitRegion = BodyRegion::Head;
    Organ hitOrgan = Organ::None;
    float damageDealt = 0.0f;
    float bleedAdded = 0.0f;
    bool armorPenetrated = true;
    bool causedFracture = false;
    bool instantKill = false;
    
    // Procedural IK wound feedback
    float recoilPenalty = 0.0f;      // Multiplier to weapon recoil kick
    float movementPenalty = 0.0f;    // Multiplier to movement speed (0=no penalty, 0.5=50% slower)
    float aimSwayPenalty = 0.0f;     // Additional aim tremor radians
    float flinchIntensity = 0.0f;    // Angular impulse in degrees
    glm::vec3 flinchDirection{0.0f}; // Direction vector of impulse
    
    std::string description;
};

class BodySystem {
public:
    BodySystem();

    void init();
    void update(float dt, float& outBloodVolume, float& outOverallHealth);

    // Traces a bullet entry into the anatomical body cylinder
    // localY: Hit height in meters above player feet (0.0 to 1.82)
    // hitAngleRadians: Horizontal arrival angle relative to character forward
    // bulletDamage: Base bullet kinetic energy
    // bulletPenetration: Armor penetration level
    WoundEffect traceHit(float localY, float hitAngleRadians, float bulletDamage, float bulletPenetration);

    // Determines body region based on physical hit height & angle
    static BodyRegion getRegionFromHeight(float localY, float hitAngleRadians);

    // Checks anatomical organ hit probabilities within region
    static Organ rollOrganHit(BodyRegion region, float hitAngleRadians);

    // Telemetry and gameplay modifiers
    float getRecoilMultiplier() const;
    float getMovementMultiplier() const;
    float getAimSwayAmount() const;
    bool canSprint() const;
    bool isAlive() const { return m_alive; }
    void setAlive(bool a) { m_alive = a; }

    // IK procedural feedback factors
    float getTorsoHunch() const { return m_torsoHunch; }
    float getLeftArmSag() const { return m_leftArmSag; }
    float getRightArmSag() const { return m_rightArmSag; }
    float getLimpFactor() const { return m_limpFactor; }
    float getFlinchPitch() const { return m_flinchPitch; }
    float getFlinchRoll() const { return m_flinchRoll; }

    float getTotalBleedRate() const;
    float getOverallHealth() const;

    // First-aid treatments
    void applyArmor(BodyRegion region, int armorClass, float durability);
    void treatWounds(float healthAmount);
    bool applyBandage();
    bool applyTourniquet();
    bool applySplint();

    const std::vector<BodyPartState>& getBodyParts() const { return m_parts; }
    std::vector<BodyPartState>& getBodyParts() { return m_parts; }

    const BodyPartState* getPart(BodyRegion region) const;
    BodyPartState* getPart(BodyRegion region);

private:
    std::vector<BodyPartState> m_parts;
    bool m_alive = true;

    // Dynamic IK wound damping variables
    float m_torsoHunch = 0.0f;
    float m_leftArmSag = 0.0f;
    float m_rightArmSag = 0.0f;
    float m_limpFactor = 0.0f;
    float m_flinchPitch = 0.0f;
    float m_flinchRoll = 0.0f;
};

} // namespace Mercenary

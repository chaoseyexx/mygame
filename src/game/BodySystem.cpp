#include "BodySystem.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace Mercenary {

BodySystem::BodySystem() {
    init();
}

void BodySystem::init() {
    m_parts.clear();
    m_alive = true;
    m_torsoHunch = 0.0f;
    m_leftArmSag = 0.0f;
    m_rightArmSag = 0.0f;
    m_limpFactor = 0.0f;
    m_flinchPitch = 0.0f;
    m_flinchRoll = 0.0f;

    // 1. Head (Cranium & Face)
    {
        BodyPartState head;
        head.region = BodyRegion::Head;
        head.health = 35.0f;
        head.maxHealth = 35.0f;
        OrganState brain{Organ::Brain, 100.0f, 0.0f, true, true};
        head.organs.push_back(brain);
        m_parts.push_back(head);
    }

    // 2. Neck
    {
        BodyPartState neck;
        neck.region = BodyRegion::Neck;
        neck.health = 40.0f;
        neck.maxHealth = 40.0f;
        OrganState spine{Organ::Spine, 100.0f, 0.0f, true, true};
        neck.organs.push_back(spine);
        m_parts.push_back(neck);
    }

    // 3. Upper Chest (Heart, Lungs, Spine)
    {
        BodyPartState chest;
        chest.region = BodyRegion::UpperChest;
        chest.health = 85.0f;
        chest.maxHealth = 85.0f;
        OrganState heart{Organ::Heart, 100.0f, 0.0f, true, true};
        OrganState lLung{Organ::LeftLung, 100.0f, 0.0f, false, true};
        OrganState rLung{Organ::RightLung, 100.0f, 0.0f, false, true};
        OrganState spine{Organ::Spine, 100.0f, 0.0f, true, true};
        chest.organs.push_back(heart);
        chest.organs.push_back(lLung);
        chest.organs.push_back(rLung);
        chest.organs.push_back(spine);
        m_parts.push_back(chest);
    }

    // 4. Lower Chest / Diaphragm (Liver, Spleen)
    {
        BodyPartState lChest;
        lChest.region = BodyRegion::LowerChest;
        lChest.health = 75.0f;
        lChest.maxHealth = 75.0f;
        OrganState liver{Organ::Liver, 100.0f, 0.0f, false, true};
        OrganState spleen{Organ::Spleen, 100.0f, 0.0f, false, true};
        lChest.organs.push_back(liver);
        lChest.organs.push_back(spleen);
        m_parts.push_back(lChest);
    }

    // 5. Abdomen (Kidneys, Stomach, Intestines)
    {
        BodyPartState stomach;
        stomach.region = BodyRegion::Abdomen;
        stomach.health = 70.0f;
        stomach.maxHealth = 70.0f;
        OrganState lKidney{Organ::LeftKidney, 100.0f, 0.0f, false, true};
        OrganState rKidney{Organ::RightKidney, 100.0f, 0.0f, false, true};
        OrganState stm{Organ::Stomach, 100.0f, 0.0f, false, true};
        OrganState intestine{Organ::Intestines, 100.0f, 0.0f, false, true};
        stomach.organs.push_back(lKidney);
        stomach.organs.push_back(rKidney);
        stomach.organs.push_back(stm);
        stomach.organs.push_back(intestine);
        m_parts.push_back(stomach);
    }

    // 6. Left Arm
    {
        BodyPartState lArm;
        lArm.region = BodyRegion::LeftArm;
        lArm.health = 60.0f;
        lArm.maxHealth = 60.0f;
        m_parts.push_back(lArm);
    }

    // 7. Right Arm
    {
        BodyPartState rArm;
        rArm.region = BodyRegion::RightArm;
        rArm.health = 60.0f;
        rArm.maxHealth = 60.0f;
        m_parts.push_back(rArm);
    }

    // 8. Left Leg
    {
        BodyPartState lLeg;
        lLeg.region = BodyRegion::LeftLeg;
        lLeg.health = 65.0f;
        lLeg.maxHealth = 65.0f;
        m_parts.push_back(lLeg);
    }

    // 9. Right Leg
    {
        BodyPartState rLeg;
        rLeg.region = BodyRegion::RightLeg;
        rLeg.health = 65.0f;
        rLeg.maxHealth = 65.0f;
        m_parts.push_back(rLeg);
    }
}

const BodyPartState* BodySystem::getPart(BodyRegion region) const {
    for (const auto& p : m_parts) {
        if (p.region == region) return &p;
    }
    return nullptr;
}

BodyPartState* BodySystem::getPart(BodyRegion region) {
    for (auto& p : m_parts) {
        if (p.region == region) return &p;
    }
    return nullptr;
}

BodyRegion BodySystem::getRegionFromHeight(float localY, float hitAngleRadians) {
    // Character height: 0.00m to 1.82m (Golden-ratio anatomical human operator)
    if (localY >= 1.40f) {
        return BodyRegion::Head;
    }
    if (localY >= 1.34f) {
        return BodyRegion::Neck;
    }
    if (localY >= 1.15f) {
        // Upper torso vs arms depending on impact angle from lateral sides
        float cosA = std::cos(hitAngleRadians);
        float sinA = std::sin(hitAngleRadians);
        if (std::abs(sinA) > 0.62f) {
            return (sinA > 0.0f) ? BodyRegion::RightArm : BodyRegion::LeftArm;
        }
        return BodyRegion::UpperChest;
    }
    if (localY >= 1.02f) {
        float sinA = std::sin(hitAngleRadians);
        if (std::abs(sinA) > 0.68f) {
            return (sinA > 0.0f) ? BodyRegion::RightArm : BodyRegion::LeftArm;
        }
        return BodyRegion::LowerChest;
    }
    if (localY >= 0.82f) {
        return BodyRegion::Abdomen;
    }
    // Legs: 0.00m to 0.82m
    float sinA = std::sin(hitAngleRadians);
    if (sinA > 0.0f) {
        return BodyRegion::RightLeg;
    } else {
        return BodyRegion::LeftLeg;
    }
}

Organ BodySystem::rollOrganHit(BodyRegion region, float hitAngleRadians) {
    static std::mt19937 rng(1337);
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    float roll = dis(rng);
    float cosA = std::cos(hitAngleRadians); // > 0 front, < 0 back

    switch (region) {
        case BodyRegion::Head:
            // 75% brain hit if cranium penetrated
            return (roll < 0.75f) ? Organ::Brain : Organ::None;

        case BodyRegion::Neck:
            // 45% spine hit
            return (roll < 0.45f) ? Organ::Spine : Organ::None;

        case BodyRegion::UpperChest:
            if (cosA < -0.3f && roll < 0.40f) {
                return Organ::Spine; // Penetrated from back
            }
            if (roll < 0.28f) return Organ::Heart;
            if (roll < 0.58f) return Organ::LeftLung;
            if (roll < 0.88f) return Organ::RightLung;
            return Organ::None;

        case BodyRegion::LowerChest:
            if (roll < 0.45f) return Organ::Liver;
            if (roll < 0.75f) return Organ::Spleen;
            return Organ::None;

        case BodyRegion::Abdomen:
            if (cosA < -0.2f && roll < 0.35f) {
                return (roll < 0.18f) ? Organ::LeftKidney : Organ::RightKidney;
            }
            if (roll < 0.35f) return Organ::Stomach;
            if (roll < 0.80f) return Organ::Intestines;
            return Organ::None;

        default:
            return Organ::None;
    }
}

void BodySystem::applyArmor(BodyRegion region, int armorClass, float durability) {
    BodyPartState* part = getPart(region);
    if (part) {
        part->armorClass = armorClass;
        part->armorDurability = durability;
        part->armorMax = durability;
    }
}

WoundEffect BodySystem::traceHit(float localY, float hitAngleRadians, float bulletDamage, float bulletPenetration) {
    WoundEffect effect;
    effect.hitRegion = getRegionFromHeight(localY, hitAngleRadians);

    BodyPartState* part = getPart(effect.hitRegion);
    if (!part) return effect;

    // Check armor protection
    float netDamage = bulletDamage;
    bool penetrated = true;

    if (part->armorDurability > 0.0f && part->armorClass > 0) {
        float penRatio = bulletPenetration / static_cast<float>(part->armorClass);
        if (penRatio < 0.80f) {
            // Armor stopped the round (blunt trauma only)
            penetrated = false;
            netDamage *= 0.18f;
            part->armorDurability = std::max(0.0f, part->armorDurability - bulletDamage * 0.4f);
            effect.armorPenetrated = false;
        } else {
            // Armor was penetrated, but absorbed kinetic energy
            penetrated = true;
            float absorbed = std::min(bulletDamage * 0.35f, part->armorDurability * 0.5f);
            netDamage = std::max(10.0f, bulletDamage - absorbed);
            part->armorDurability = std::max(0.0f, part->armorDurability - bulletDamage * 0.6f);
            effect.armorPenetrated = true;
        }
    }

    // Apply damage to body part
    part->health = std::max(0.0f, part->health - netDamage);
    effect.damageDealt = netDamage;

    // Flinch dynamics
    effect.flinchIntensity = std::min(24.0f, netDamage * 0.45f);
    m_flinchPitch = effect.flinchIntensity;
    m_flinchRoll = (hitAngleRadians > 0.0f ? 1.0f : -1.0f) * effect.flinchIntensity * 0.7f;

    // Penetrating organ check
    if (penetrated) {
        effect.hitOrgan = rollOrganHit(effect.hitRegion, hitAngleRadians);

        for (auto& o : part->organs) {
            if (o.organ == effect.hitOrgan) {
                o.health = std::max(0.0f, o.health - netDamage * 1.5f);
                if (o.health <= 0.0f) o.isFunctioning = false;

                // Specific organ hemorrhage & lethal trauma
                switch (o.organ) {
                    case Organ::Brain:
                        o.bleedRate = 250.0f;
                        effect.instantKill = true;
                        m_alive = false;
                        effect.description = "Lethal Cranial Trauma (Brain Penetration)";
                        break;

                    case Organ::Heart:
                        o.bleedRate = 650.0f; // Catastrophic thoracic bleed
                        effect.description = "Catastrophic Heart Laceration";
                        m_torsoHunch += 0.45f;
                        break;

                    case Organ::Spine:
                        o.bleedRate = 180.0f;
                        effect.instantKill = true; // High spinal shock / paralysis
                        m_alive = false;
                        effect.description = "Severed Spinal Cord Trauma";
                        break;

                    case Organ::LeftLung:
                    case Organ::RightLung:
                        o.bleedRate = 200.0f;
                        effect.aimSwayPenalty += 0.15f;
                        effect.description = "Perforated Lung (Pneumothorax)";
                        m_torsoHunch += 0.25f;
                        break;

                    case Organ::Liver:
                        o.bleedRate = 280.0f;
                        effect.description = "Hepatic Laceration (Internal Hemorrhage)";
                        m_torsoHunch += 0.30f;
                        break;

                    case Organ::Spleen:
                        o.bleedRate = 240.0f;
                        effect.description = "Ruptured Spleen";
                        m_torsoHunch += 0.25f;
                        break;

                    case Organ::LeftKidney:
                    case Organ::RightKidney:
                        o.bleedRate = 160.0f;
                        effect.description = "Renal Laceration";
                        m_torsoHunch += 0.20f;
                        break;

                    case Organ::Stomach:
                    case Organ::Intestines:
                        o.bleedRate = 140.0f;
                        effect.description = "Abdominal Perforation";
                        m_torsoHunch += 0.22f;
                        break;

                    default:
                        break;
                }
                effect.bleedAdded += o.bleedRate;
                break;
            }
        }

        // Standard flesh bleeding
        if (!part->isBleeding) {
            part->isBleeding = true;
            float baseBleed = (netDamage > 40.0f) ? 65.0f : 35.0f;
            part->bleedRate += baseBleed;
            effect.bleedAdded += baseBleed;
        }
    }

    // Fractures & Limb Trauma
    if (netDamage > 32.0f) {
        if (effect.hitRegion == BodyRegion::LeftArm) {
            part->isFractured = true;
            effect.causedFracture = true;
            effect.recoilPenalty += 0.65f;
            m_leftArmSag += 0.35f;
            if (effect.description.empty()) effect.description = "Left Arm Compound Fracture";
        } else if (effect.hitRegion == BodyRegion::RightArm) {
            part->isFractured = true;
            effect.causedFracture = true;
            effect.recoilPenalty += 0.90f;
            m_rightArmSag += 0.40f;
            if (effect.description.empty()) effect.description = "Right Arm Gun-Arm Fracture";
        } else if (effect.hitRegion == BodyRegion::LeftLeg || effect.hitRegion == BodyRegion::RightLeg) {
            part->isFractured = true;
            effect.causedFracture = true;
            effect.movementPenalty += 0.55f;
            m_limpFactor += 0.60f;
            if (effect.description.empty()) effect.description = "Lower Limb Bone Fracture";
        }
    }

    if (effect.description.empty()) {
        effect.description = std::string("Flesh Wound: ") + getBodyRegionName(effect.hitRegion);
    }

    return effect;
}

void BodySystem::update(float dt, float& outBloodVolume, float& outOverallHealth) {
    float totalBleed = getTotalBleedRate();
    if (totalBleed > 0.0f) {
        outBloodVolume = std::max(0.0f, outBloodVolume - totalBleed * dt);
        if (outBloodVolume < 2000.0f) {
            outOverallHealth = std::max(0.0f, outOverallHealth - 12.0f * dt); // Hypovolemic shock
        }
    }

    // Natural IK flinch relaxation
    m_flinchPitch = std::max(0.0f, m_flinchPitch - dt * 26.0f);
    m_flinchRoll *= std::max(0.0f, 1.0f - dt * 14.0f);

    // Natural wound recovery damping
    m_torsoHunch = std::clamp(m_torsoHunch, 0.0f, 0.65f);
    m_leftArmSag = std::clamp(m_leftArmSag, 0.0f, 0.55f);
    m_rightArmSag = std::clamp(m_rightArmSag, 0.0f, 0.55f);
    m_limpFactor = std::clamp(m_limpFactor, 0.0f, 0.85f);

    // Check vitality
    if (outBloodVolume <= 0.0f || outOverallHealth <= 0.0f) {
        m_alive = false;
    }
}

float BodySystem::getTotalBleedRate() const {
    float sum = 0.0f;
    for (const auto& p : m_parts) {
        if (p.isBleeding) sum += p.bleedRate;
        for (const auto& o : p.organs) {
            sum += o.bleedRate;
        }
    }
    return sum;
}

float BodySystem::getOverallHealth() const {
    float current = 0.0f;
    float maxH = 0.0f;
    for (const auto& p : m_parts) {
        current += p.health;
        maxH += p.maxHealth;
    }
    return (maxH > 0.0f) ? (current / maxH * 100.0f) : 100.0f;
}

float BodySystem::getRecoilMultiplier() const {
    float mult = 1.0f;
    const BodyPartState* rArm = getPart(BodyRegion::RightArm);
    const BodyPartState* lArm = getPart(BodyRegion::LeftArm);
    const BodyPartState* chest = getPart(BodyRegion::UpperChest);

    if (rArm && rArm->isFractured) mult += 0.85f;
    else if (rArm && rArm->health < 30.0f) mult += 0.35f;

    if (lArm && lArm->isFractured) mult += 0.50f;
    else if (lArm && lArm->health < 30.0f) mult += 0.20f;

    if (chest && chest->health < 40.0f) mult += 0.30f;
    return mult;
}

float BodySystem::getMovementMultiplier() const {
    float mult = 1.0f;
    const BodyPartState* lLeg = getPart(BodyRegion::LeftLeg);
    const BodyPartState* rLeg = getPart(BodyRegion::RightLeg);

    int legFractures = 0;
    if (lLeg && lLeg->isFractured) legFractures++;
    if (rLeg && rLeg->isFractured) legFractures++;

    if (legFractures == 2) mult *= 0.32f;
    else if (legFractures == 1) mult *= 0.58f;

    if (m_torsoHunch > 0.2f) mult *= (1.0f - m_torsoHunch * 0.4f);
    return std::max(0.25f, mult);
}

float BodySystem::getAimSwayAmount() const {
    float sway = 0.002f;
    const BodyPartState* chest = getPart(BodyRegion::UpperChest);
    if (chest) {
        for (const auto& o : chest->organs) {
            if ((o.organ == Organ::LeftLung || o.organ == Organ::RightLung) && !o.isFunctioning) {
                sway += 0.055f; // Pneumothorax lung tremor
            }
        }
    }
    const BodyPartState* rArm = getPart(BodyRegion::RightArm);
    if (rArm && rArm->health < 40.0f) sway += 0.035f;
    return sway;
}

bool BodySystem::canSprint() const {
    const BodyPartState* lLeg = getPart(BodyRegion::LeftLeg);
    const BodyPartState* rLeg = getPart(BodyRegion::RightLeg);
    if ((lLeg && lLeg->isFractured) || (rLeg && rLeg->isFractured)) {
        return false;
    }
    return true;
}

void BodySystem::treatWounds(float healthAmount) {
    for (auto& p : m_parts) {
        p.health = std::min(p.maxHealth, p.health + healthAmount);
        for (auto& o : p.organs) {
            o.health = std::min(100.0f, o.health + healthAmount);
            if (o.health > 20.0f) o.isFunctioning = true;
        }
    }
}

bool BodySystem::applyBandage() {
    for (auto& p : m_parts) {
        if (p.isBleeding && p.bleedRate > 0.0f) {
            p.bleedRate = 0.0f;
            p.isBleeding = false;
            return true;
        }
    }
    return false;
}

bool BodySystem::applyTourniquet() {
    // Stops limb arterial bleeding immediately
    for (auto reg : {BodyRegion::LeftArm, BodyRegion::RightArm, BodyRegion::LeftLeg, BodyRegion::RightLeg}) {
        BodyPartState* p = getPart(reg);
        if (p && p->isBleeding) {
            p->bleedRate = 0.0f;
            p->isBleeding = false;
            return true;
        }
    }
    return applyBandage();
}

bool BodySystem::applySplint() {
    for (auto& p : m_parts) {
        if (p.isFractured) {
            p.isFractured = false;
            m_limpFactor = std::max(0.0f, m_limpFactor - 0.4f);
            m_leftArmSag = std::max(0.0f, m_leftArmSag - 0.3f);
            m_rightArmSag = std::max(0.0f, m_rightArmSag - 0.3f);
            return true;
        }
    }
    return false;
}

} // namespace Mercenary

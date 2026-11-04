#include "Player.hpp"
#include "World.hpp"
#include "Ballistics.hpp"
#include "ChatSystem.hpp"
#include "engine/Audio.hpp"
#include "engine/InputConfig.hpp"
#include "engine/Network.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace Mercenary {
void Player::switchWeapon(EquipSlot slot) {
    if (slot == EquipSlot::PrimaryWeapon || slot == EquipSlot::SecondaryWeapon) {
        if (m_activeWeaponSlot != slot && !m_isSwitchingWeapon) {
            // Cancel any active reload on weapon switch
            if (m_isReloading) {
                m_isReloading = false;
                m_reloadTimer = 0.0f;
                m_reloadingWeaponId = -1;
            }

            auto& currentWeapon = m_inventory.getEquipped(m_activeWeaponSlot);
            auto& destWeapon = m_inventory.getEquipped(slot);

            // If we currently have a weapon equipped, lower it first with smooth animation
            if (currentWeapon.has_value()) {
                m_isSwitchingWeapon = true;
                m_weaponSwitchTimer = 0.16f;
                m_pendingSwitchSlot = slot;
                m_rig.triggerDropAnim(currentWeapon->name, 0.16f);
                AudioEngine::get().playSound(SoundType::GearRustle, 0.35f, 1.05f);
            } else {
                // If unarmed, draw immediately
                m_activeWeaponSlot = slot;
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
    }
}

bool Player::dropCurrentWeapon(World& world) {
    auto& equipped = m_inventory.getEquipped(m_activeWeaponSlot);
    if (!equipped.has_value()) {
        // Try other weapon slot if current is empty
        EquipSlot otherSlot = (m_activeWeaponSlot == EquipSlot::PrimaryWeapon) ? EquipSlot::SecondaryWeapon : EquipSlot::PrimaryWeapon;
        auto& other = m_inventory.getEquipped(otherSlot);
        if (other.has_value()) {
            m_activeWeaponSlot = otherSlot;
        } else {
            return false;
        }
    }

    auto& weaponOpt = m_inventory.getEquipped(m_activeWeaponSlot);
    if (!weaponOpt.has_value()) return false;

    // Cancel reload if dropping the reloading weapon
    if (m_isReloading && m_reloadingSlot == m_activeWeaponSlot) {
        m_isReloading = false;
        m_reloadTimer = 0.0f;
        m_reloadingWeaponId = -1;
    }

    Item droppedItem = weaponOpt.value();
    std::string droppedName = droppedItem.name;
    m_inventory.getEquipped(m_activeWeaponSlot).reset();
    m_rig.triggerDropAnim(droppedName, 0.28f);

    // Compute drop position in front of operator
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);
    glm::vec3 forwardDir(sy, 0.0f, cy);
    glm::vec3 dropPos = m_position + forwardDir * 0.85f;
    dropPos.y = world.sampleTerrainHeight(dropPos.x, dropPos.z);

    SupplyCrate groundLoot;
    groundLoot.position = dropPos;
    groundLoot.name = droppedItem.name;
    groundLoot.contents.push_back(droppedItem);
    groundLoot.looted = false;
    groundLoot.isGroundItem = true;
    groundLoot.yaw = m_yaw + 1.5707f;
    world.getCrates().push_back(groundLoot);
    if (NetworkEngine::get().isClientConnected()) {
        NetworkEngine::get().broadcastLootDrop(
            droppedItem.name,
            static_cast<uint8_t>(droppedItem.category),
            droppedItem.quantity,
            droppedItem.currentMag,
            dropPos,
            groundLoot.yaw
        );
    }

    AudioEngine::get().playSound(SoundType::ItemDrop, 0.85f, 0.98f);

    // Switch to remaining secondary if available
    EquipSlot nextSlot = (m_activeWeaponSlot == EquipSlot::PrimaryWeapon) ? EquipSlot::SecondaryWeapon : EquipSlot::PrimaryWeapon;
    auto& nextW = m_inventory.getEquipped(nextSlot);
    if (nextW.has_value()) {
        m_isSwitchingWeapon = true;
        m_weaponSwitchTimer = 0.20f;
        m_pendingSwitchSlot = nextSlot;
    } else {
        m_rig.setActiveWeaponName("None");
    }

    return true;
}

bool Player::dropItem(World& world, size_t inventoryIndex) {
    if (inventoryIndex >= m_inventory.getItems().size()) return false;
    Item droppedItem = m_inventory.getItems()[inventoryIndex];
    m_inventory.removeItem(inventoryIndex);

    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);
    glm::vec3 forwardDir(sy, 0.0f, cy);
    glm::vec3 dropPos = m_position + forwardDir * 0.85f;
    dropPos.y = world.sampleTerrainHeight(dropPos.x, dropPos.z);

    SupplyCrate groundLoot;
    groundLoot.position = dropPos;
    groundLoot.name = droppedItem.name;
    groundLoot.contents.push_back(droppedItem);
    groundLoot.looted = false;
    groundLoot.isGroundItem = true;
    groundLoot.yaw = m_yaw;
    world.getCrates().push_back(groundLoot);

    AudioEngine::get().playSound(SoundType::ItemDrop, 0.85f, 0.98f);
    return true;
}

bool Player::dropEquippedItem(World& world, EquipSlot slot) {
    auto& eq = m_inventory.getEquipped(slot);
    if (!eq.has_value()) return false;

    if (slot == m_activeWeaponSlot) {
        return dropCurrentWeapon(world);
    }

    Item droppedItem = eq.value();
    m_inventory.getEquipped(slot).reset();

    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);
    glm::vec3 forwardDir(sy, 0.0f, cy);
    glm::vec3 dropPos = m_position + forwardDir * 0.85f;
    dropPos.y = world.sampleTerrainHeight(dropPos.x, dropPos.z);

    SupplyCrate groundLoot;
    groundLoot.position = dropPos;
    groundLoot.name = droppedItem.name;
    groundLoot.contents.push_back(droppedItem);
    groundLoot.looted = false;
    groundLoot.isGroundItem = true;
    groundLoot.yaw = m_yaw;
    world.getCrates().push_back(groundLoot);

    AudioEngine::get().playSound(SoundType::ItemDrop, 0.85f, 0.98f);
    return true;
}

void Player::updateArmorFromEquipped() {
    for (auto reg : {BodyRegion::Head, BodyRegion::Neck, BodyRegion::UpperChest, BodyRegion::LowerChest, BodyRegion::Abdomen}) {
        auto* p = m_body.getPart(reg);
        if (p) {
            p->armorClass = 0;
            p->armorDurability = 0.0f;
            p->armorMax = 0.0f;
        }
    }

    auto& helm = m_inventory.getEquipped(EquipSlot::Helmet);
    if (helm.has_value()) {
        int aClass = 3;
        float dur = 45.0f;
        if (helm->name.find("FAST") != std::string::npos || helm->name.find("High-Cut") != std::string::npos) {
            aClass = 4;
            dur = 55.0f;
        } else if (helm->name.find("6B47") != std::string::npos) {
            aClass = 3;
            dur = 45.0f;
        }
        m_body.applyArmor(BodyRegion::Head, aClass, dur);
    }

    auto& vest = m_inventory.getEquipped(EquipSlot::Vest);
    if (vest.has_value()) {
        int aClass = 4;
        float dur = 75.0f;
        if (vest->name.find("6B45") != std::string::npos) {
            aClass = 5;
            dur = 90.0f;
        } else if (vest->name.find("JPC") != std::string::npos) {
            aClass = 4;
            dur = 65.0f;
        } else if (vest->name.find("Slick") != std::string::npos) {
            aClass = 3;
            dur = 45.0f;
        }
        m_body.applyArmor(BodyRegion::UpperChest, aClass, dur);
        m_body.applyArmor(BodyRegion::LowerChest, aClass, dur);
        m_body.applyArmor(BodyRegion::Abdomen, std::max(1, aClass - 1), dur * 0.7f);
    }
}

bool Player::useMedicalItem(size_t index) {
    auto& items = m_inventory.getItems();
    if (index >= items.size()) return false;
    Item& item = items[index];
    if (item.category != ItemCategory::Medical) return false;

    bool used = false;
    std::string name = item.name;

    if (name.find("Bandage") != std::string::npos) {
        m_body.applyBandage();
        m_stats.bleedCuts = std::max(0, m_stats.bleedCuts - 1);
        m_stats.health = std::min(100.0f, m_stats.health + 18.0f);
        m_body.treatWounds(12.0f);
        used = true;
    } else if (name.find("Tourniquet") != std::string::npos) {
        m_body.applyTourniquet();
        m_stats.bleedCuts = std::max(0, m_stats.bleedCuts - 2);
        m_body.treatWounds(8.0f);
        used = true;
    } else if (name.find("Splint") != std::string::npos) {
        used = m_body.applySplint();
        m_stats.health = std::min(100.0f, m_stats.health + 10.0f);
        used = true;
    } else if (name.find("IFAK") != std::string::npos || name.find("Medkit") != std::string::npos) {
        m_body.applyBandage();
        m_stats.bleedCuts = 0;
        m_stats.health = std::min(100.0f, m_stats.health + 45.0f);
        m_body.treatWounds(35.0f);
        used = true;
    } else {
        m_body.applyBandage();
        if (m_stats.bleedCuts > 0) m_stats.bleedCuts--;
        m_stats.health = std::min(100.0f, m_stats.health + 25.0f);
        m_body.treatWounds(20.0f);
        used = true;
    }

    if (used) {
        item.quantity--;
        if (item.quantity <= 0) {
            items.erase(items.begin() + index);
        }
        AudioEngine::get().playSound(SoundType::Bandage);
        m_stats.startBandaging();
        return true;
    }
    return false;
}

bool Player::useSurvivalItem(size_t index) {
    auto& items = m_inventory.getItems();
    if (index >= items.size()) return false;
    Item& item = items[index];
    if (item.category != ItemCategory::Survival) return false;

    if (item.name.find("MRE") != std::string::npos || item.name.find("Ration") != std::string::npos) {
        m_stats.eatFood(450.0f);
        m_stats.health = std::min(100.0f, m_stats.health + 12.0f);
    } else if (item.name.find("Canteen") != std::string::npos || item.name.find("Water") != std::string::npos) {
        m_stats.drinkWater(500.0f);
        m_stats.stamina = std::min(100.0f, m_stats.stamina + 40.0f);
    }

    item.quantity--;
    if (item.quantity <= 0) {
        items.erase(items.begin() + index);
    }
    AudioEngine::get().playSound(SoundType::ItemEquip, 0.90f, 1.10f);
    return true;
}

bool Player::useQuickMedical() {
    auto& items = m_inventory.getItems();
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].category == ItemCategory::Medical) {
            return useMedicalItem(i);
        }
    }
    return false;
}

void Player::resetFullHealthAndVitals() {
    m_stats.health = 100.0f;
    m_stats.blood = 5000.0f;
    m_stats.bleedCuts = 0;
    m_stats.stamina = 100.0f;
    m_stats.isExhausted = false;
    m_stats.staminaExhaustDelay = 0.0f;
    m_stats.suppression = 0.0f;
    m_stats.isSuppressed = false;
    m_stats.isPinned = false;
    m_stats.isSprinting = false;
    m_stats.isBandaging = false;
    m_stats.bandageProgress = 0.0f;
    m_body.init();
    updateArmorFromEquipped();
    m_rig.resetDeath();
}

void Player::replenishTdmLoadout() {
    // 1. Reset Full Health, Blood, Armor, and Vitals
    resetFullHealthAndVitals();

    // 2. Replenish Equipped Primary Weapon Mag & Durability
    auto& primary = m_inventory.getEquipped(EquipSlot::PrimaryWeapon);
    if (primary.has_value()) {
        primary->currentMag = primary->maxMag;
        primary->durability = primary->maxDurability;
    } else {
        if (m_faction == Faction::FactionB) {
            m_inventory.getEquipped(EquipSlot::PrimaryWeapon) = Inventory::createAK74();
        } else {
            m_inventory.getEquipped(EquipSlot::PrimaryWeapon) = Inventory::createM4A1();
        }
    }

    // 3. Replenish Equipped Secondary Weapon Mag
    auto& secondary = m_inventory.getEquipped(EquipSlot::SecondaryWeapon);
    if (secondary.has_value()) {
        secondary->currentMag = secondary->maxMag;
        secondary->durability = secondary->maxDurability;
    } else {
        m_inventory.getEquipped(EquipSlot::SecondaryWeapon) = Inventory::createM9Pistol();
    }

    // 4. Ensure Tactical Vest and Helmet are present and at 100% durability
    auto& vest = m_inventory.getEquipped(EquipSlot::Vest);
    if (vest.has_value()) {
        vest->durability = vest->maxDurability;
    } else {
        m_inventory.getEquipped(EquipSlot::Vest) = Inventory::createPlateCarrier();
    }

    auto& helmet = m_inventory.getEquipped(EquipSlot::Helmet);
    if (helmet.has_value()) {
        helmet->durability = helmet->maxDurability;
    } else {
        m_inventory.getEquipped(EquipSlot::Helmet) = Inventory::createFastHelmet();
    }

    // 5. Replenish Spare Reserve Ammunition in Inventory
    std::string primCaliber = "5.56x45mm";
    if (m_inventory.getEquipped(EquipSlot::PrimaryWeapon).has_value()) {
        primCaliber = m_inventory.getEquipped(EquipSlot::PrimaryWeapon)->caliber;
    }

    int curReserve = m_inventory.getAmmoCount(primCaliber);
    int targetReserve = (primCaliber == "7.62x54mmR") ? 40 : 180;
    if (curReserve < targetReserve) {
        int needed = targetReserve - curReserve;
        if (primCaliber == "5.45x39mm") {
            m_inventory.addItem(Inventory::create545Ammo(needed));
        } else if (primCaliber == "7.62x54mmR") {
            m_inventory.addItem(Inventory::create762Ammo(needed));
        } else {
            m_inventory.addItem(Inventory::create556Ammo(needed));
        }
    }

    int curPistolReserve = m_inventory.getAmmoCount("9x19mm");
    if (curPistolReserve < 60) {
        m_inventory.addItem(Inventory::create9mmAmmo(60 - curPistolReserve));
    }

    // 6. Ensure at least 3 Bandages
    int curBandages = 0;
    for (const auto& item : m_inventory.getItems()) {
        if (item.name == "Sterile Medical Bandage") {
            curBandages += item.quantity;
        }
    }
    if (curBandages < 3) {
        m_inventory.addItem(Inventory::createBandage(3 - curBandages));
    }

    // 7. Grant 4.0 seconds of Tactical Spawn Protection
    m_spawnProtectionTimer = 4.0f;
    updateArmorFromEquipped();
}

void Player::spawnAtFactionBase(Faction faction) {
    m_faction = faction;
    uint32_t netId = NetworkEngine::get().getLocalPlayerId();
    float xOffset = 0.0f;
    if (netId > 1) {
        float dir = (netId % 2 == 0) ? 1.0f : -1.0f;
        float mult = static_cast<float>(netId / 2);
        xOffset = dir * mult * 2.2f;
    }
    float baseZ = (faction == Faction::FactionA) ? -225.0f : 225.0f;
    m_position = glm::vec3(xOffset, World::sampleTerrainHeight(xOffset, baseZ), baseZ);
    m_yaw = (faction == Faction::FactionA) ? 0.0f : 3.14159265f;
    m_velocity = glm::vec3(0.0f);
    resetFullHealthAndVitals();
}

void Player::reloadCurrentWeapon() {
    auto& equipped = m_inventory.getEquipped(m_activeWeaponSlot);
    if (!equipped.has_value() || m_isReloading) return;

    Item& weapon = equipped.value();
    if (weapon.category != ItemCategory::Weapon) return;
    if (weapon.currentMag >= weapon.maxMag) return;

    int needed = weapon.maxMag - weapon.currentMag;
    int available = m_inventory.getAmmoCount(weapon.caliber);
    if (available <= 0) available = m_inventory.getAmmoCount(weapon.weaponType);
    if (available <= 0) {
        AudioEngine::get().playSound(SoundType::DryFire);
        return;
    }

    m_isReloading = true;
    m_reloadingSlot = m_activeWeaponSlot;
    m_reloadingWeaponId = weapon.id;
    m_reloadTimer = 1.8f;
    AudioEngine::get().playSound(SoundType::Reload);
    if (m_playerVoiceBarkTimer <= 0.0f) {
        m_playerVoiceBarkTimer = 7.5f;
        AudioEngine::get().playSound(SoundType::SoldierBarkReloading, 0.70f, 1.0f);
    }
}

void Player::mountTurret(MountedTurret* turret) {
    if (!turret || turret->isOccupied) return;
    if (m_inCover) exitCover();

    m_isMounted = true;
    m_activeTurret = turret;
    turret->isOccupied = true;
    m_turretBaseYaw = turret->yaw;
    m_yaw = turret->yaw;

    float cy = std::cos(turret->yaw);
    float sy = std::sin(turret->yaw);
    m_position = turret->position - glm::vec3(sy, 0.0f, cy) * 0.45f;
    m_position.y = turret->position.y;

    m_stats.stance = PlayerStance::Standing;
    m_rig.triggerEquipAnim(0.15f);
    AudioEngine::get().playSound(SoundType::GearRustle, 0.60f, 0.95f);
}

void Player::dismountTurret() {
    if (!m_isMounted) return;
    if (m_activeTurret) {
        m_activeTurret->isOccupied = false;
        m_activeTurret = nullptr;
    }
    m_isMounted = false;
    m_rig.triggerEquipAnim(0.25f);
    AudioEngine::get().playSound(SoundType::GearRustle, 0.50f, 1.05f);
}

bool Player::deployFortification(World& world) {
    return world.deploySandbagBarricade(m_position, m_yaw);
}

} // namespace Mercenary

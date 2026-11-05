#include "Inventory.hpp"
#include <algorithm>

namespace Mercenary {

Inventory::Inventory() {
    // Starting Arena Breakout / PMC Tactical Loadout
    m_equippedPrimary = createM4A1();
    m_equippedSecondary = createM9Pistol();
    m_equippedHelmet = createFastHelmet();
    m_equippedVest = createPlateCarrier();
    m_equippedRig = createTacticalRig();
    m_equippedBackpack = createAssaultBackpack();

    addItem(create556Ammo(90));
    addItem(create9mmAmmo(45));
    addItem(createIFAK());
    addItem(createBandage(3));
    addItem(createTourniquet());
    addItem(createSplint(2));
    addItem(createF1Grenade(2));
    addItem(createSmokeGrenade(2));
}

bool Inventory::addItem(const Item& item) {
    // Try to stack if Ammo or Medical
    if (item.maxStack > 1) {
        for (auto& existing : m_items) {
            if (existing.name == item.name && existing.quantity < existing.maxStack) {
                int space = existing.maxStack - existing.quantity;
                int toAdd = std::min(space, item.quantity);
                existing.quantity += toAdd;
                if (toAdd == item.quantity) return true;
            }
        }
    }

    if (m_items.size() < (size_t)m_maxSlots) {
        m_items.push_back(item);
        return true;
    }
    return false;
}

void Inventory::removeItem(size_t index) {
    if (index < m_items.size()) {
        m_items.erase(m_items.begin() + index);
    }
}

std::optional<Item>& Inventory::getEquipped(EquipSlot slot) {
    switch (slot) {
    case EquipSlot::PrimaryWeapon:   return m_equippedPrimary;
    case EquipSlot::SecondaryWeapon: return m_equippedSecondary;
    case EquipSlot::Vest:            return m_equippedVest;
    case EquipSlot::Backpack:        return m_equippedBackpack;
    case EquipSlot::Helmet:          return m_equippedHelmet;
    case EquipSlot::TacticalRig:     return m_equippedRig;
    }
    return m_equippedPrimary;
}

const std::optional<Item>& Inventory::getEquipped(EquipSlot slot) const {
    switch (slot) {
    case EquipSlot::PrimaryWeapon:   return m_equippedPrimary;
    case EquipSlot::SecondaryWeapon: return m_equippedSecondary;
    case EquipSlot::Vest:            return m_equippedVest;
    case EquipSlot::Backpack:        return m_equippedBackpack;
    case EquipSlot::Helmet:          return m_equippedHelmet;
    case EquipSlot::TacticalRig:     return m_equippedRig;
    }
    return m_equippedPrimary;
}

bool Inventory::equipItem(size_t inventoryIndex, EquipSlot slot) {
    if (inventoryIndex >= m_items.size()) return false;

    Item itemToEquip = m_items[inventoryIndex];
    removeItem(inventoryIndex);

    std::optional<Item>& targetSlot = getEquipped(slot);
    if (targetSlot.has_value()) {
        addItem(targetSlot.value());
    }

    targetSlot = itemToEquip;
    return true;
}

void Inventory::unequipItem(EquipSlot slot) {
    std::optional<Item>& targetSlot = getEquipped(slot);
    if (targetSlot.has_value()) {
        if (addItem(targetSlot.value())) {
            targetSlot.reset();
        }
    }
}

float Inventory::getTotalWeightKg() const {
    float total = 0.0f;
    for (const auto& item : m_items) {
        total += item.weightKg * item.quantity;
    }
    if (m_equippedPrimary.has_value()) total += m_equippedPrimary->weightKg;
    if (m_equippedSecondary.has_value()) total += m_equippedSecondary->weightKg;
    if (m_equippedVest.has_value()) total += m_equippedVest->weightKg;
    if (m_equippedBackpack.has_value()) total += m_equippedBackpack->weightKg;
    if (m_equippedHelmet.has_value()) total += m_equippedHelmet->weightKg;
    if (m_equippedRig.has_value()) total += m_equippedRig->weightKg;
    return total;
}

static std::string extractCaliberKey(const std::string& str) {
    if (str.find("5.45") != std::string::npos) return "5.45";
    if (str.find("5.56") != std::string::npos) return "5.56";
    if (str.find("7.62") != std::string::npos) return "7.62";
    if (str.find("9x19") != std::string::npos || str.find("9mm") != std::string::npos) return "9";
    return str;
}

int Inventory::getAmmoCount(WeaponType type) const {
    if (type == WeaponType::Rifle) {
        return getAmmoCount("5.45") + getAmmoCount("5.56") + getAmmoCount("7.62x39");
    }
    if (type == WeaponType::Sniper) return getAmmoCount("7.62");
    if (type == WeaponType::Pistol) return getAmmoCount("9");
    return 0;
}

int Inventory::getAmmoCount(const std::string& caliber) const {
    std::string key = extractCaliberKey(caliber);
    int total = 0;
    for (const auto& item : m_items) {
        if (item.category == ItemCategory::Ammo) {
            if (extractCaliberKey(item.caliber) == key || extractCaliberKey(item.name) == key) {
                total += item.quantity;
            }
        }
    }
    return total;
}

void Inventory::sortItems() {
    std::sort(m_items.begin(), m_items.end(), [](const Item& a, const Item& b) {
        if (a.category != b.category) {
            return static_cast<int>(a.category) < static_cast<int>(b.category);
        }
        if (a.name != b.name) {
            return a.name < b.name;
        }
        return a.quantity > b.quantity;
    });
}

bool Inventory::isItemValidForSlot(const Item& item, EquipSlot slot) {
    switch (slot) {
        case EquipSlot::PrimaryWeapon:
            return item.category == ItemCategory::Weapon && item.weaponType != WeaponType::Pistol;
        case EquipSlot::SecondaryWeapon:
            return item.category == ItemCategory::Weapon && item.weaponType == WeaponType::Pistol;
        case EquipSlot::Vest:
            return item.category == ItemCategory::Gear && (item.name.find("Vest") != std::string::npos || item.name.find("JPC") != std::string::npos || item.name.find("Carrier") != std::string::npos || item.name.find("Armor") != std::string::npos || item.name.find("6B45") != std::string::npos);
        case EquipSlot::Helmet:
            return item.category == ItemCategory::Gear && (item.name.find("Helmet") != std::string::npos || item.name.find("FAST") != std::string::npos || item.name.find("6B47") != std::string::npos);
        case EquipSlot::Backpack:
            return item.category == ItemCategory::Gear && (item.name.find("Backpack") != std::string::npos || item.name.find("Pack") != std::string::npos);
        case EquipSlot::TacticalRig:
            return item.category == ItemCategory::Gear && (item.name.find("Rig") != std::string::npos || item.name.find("D3CRX") != std::string::npos);
    }
    return false;
}

int Inventory::consumeAmmoCount(const std::string& caliber, int count) {
    if (count <= 0) return 0;
    std::string key = extractCaliberKey(caliber);
    int consumed = 0;
    auto it = m_items.begin();
    while (it != m_items.end() && consumed < count) {
        if (it->category == ItemCategory::Ammo) {
            if (extractCaliberKey(it->caliber) == key || extractCaliberKey(it->name) == key) {
                int needed = count - consumed;
                if (it->quantity <= needed) {
                    consumed += it->quantity;
                    it = m_items.erase(it);
                    continue;
                } else {
                    it->quantity -= needed;
                    consumed += needed;
                    break;
                }
            }
        }
        ++it;
    }
    return consumed;
}

int Inventory::consumeAmmoCount(WeaponType type, int count) {
    if (type == WeaponType::Rifle) {
        int c545 = consumeAmmoCount("5.45", count);
        if (c545 >= count) return c545;
        return c545 + consumeAmmoCount("5.56", count - c545);
    }
    if (type == WeaponType::Sniper) return consumeAmmoCount("7.62", count);
    if (type == WeaponType::Pistol) return consumeAmmoCount("9", count);
    return 0;
}

bool Inventory::consumeAmmo(const std::string& caliber, int count) {
    return consumeAmmoCount(caliber, count) > 0;
}

bool Inventory::consumeAmmo(WeaponType type, int count) {
    return consumeAmmoCount(type, count) > 0;
}

bool Inventory::useFirstMedical() {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->category == ItemCategory::Medical) {
            it->quantity--;
            if (it->quantity <= 0) {
                m_items.erase(it);
            }
            return true;
        }
    }
    return false;
}

bool Inventory::useFirstFood() {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->category == ItemCategory::Survival && it->name.find("MRE") != std::string::npos) {
            it->quantity--;
            if (it->quantity <= 0) {
                m_items.erase(it);
            }
            return true;
        }
    }
    return false;
}

bool Inventory::useFirstWater() {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->category == ItemCategory::Survival && it->name.find("Canteen") != std::string::npos) {
            it->quantity--;
            if (it->quantity <= 0) {
                m_items.erase(it);
            }
            return true;
        }
    }
    return false;
}

// Preset factory helpers
Item Inventory::createAK74() {
    Item ak;
    ak.name = "Kalashnikov AK-74M 5.45x39";
    ak.description = "Russian mil-spec assault rifle. Outstanding durability and reliability in mud/water.";
    ak.category = ItemCategory::Weapon;
    ak.weaponType = WeaponType::Rifle;
    ak.caliber = "5.45x39mm PP";
    ak.weightKg = 3.6f;
    ak.currentMag = 30;
    ak.maxMag = 30;
    ak.fireRateRpm = 650.0f;
    ak.muzzleVelocityMs = 900.0f;
    ak.damage = 38.0f;
    ak.baseSpread = 0.0045f;
    ak.isAutomatic = true;
    ak.ergonomics = 58.0f;
    ak.recoilVertical = 52.0f;
    ak.recoilHorizontal = 64.0f;
    ak.effectiveRange = 300.0f;
    ak.aimRange = 120.0f;
    ak.aimDownSightTime = 0.30f;
    ak.hipFireSpread = 0.024f;
    ak.recoilRecovery = 8.5f;
    ak.recoilPattern = 0.65f;
    ak.penetrationPower = 3.2f;
    ak.moveSpeedPenalty = 0.06f;
    ak.opticZoomLevel = 0; // Dedicated 1.0x CQB Kobra Reflex
    ak.hasVariableOptic = false;
    ak.durability = 100.0f;
    ak.maxDurability = 100.0f;
    ak.gridW = 4;
    ak.gridH = 2;
    ak.traderPriceGold = 1;
    return ak;
}

Item Inventory::createM4A1() {
    Item m4;
    m4.name = "Colt M4A1 SOPMOD 5.56x45";
    m4.description = "Allied NATO assault carbine. Exceptional modularity, low recoil, high rate of fire.";
    m4.category = ItemCategory::Weapon;
    m4.weaponType = WeaponType::Rifle;
    m4.caliber = "5.56x45mm M855A1";
    m4.weightKg = 3.2f;
    m4.currentMag = 30;
    m4.maxMag = 30;
    m4.fireRateRpm = 800.0f;
    m4.muzzleVelocityMs = 880.0f;
    m4.damage = 36.0f;
    m4.baseSpread = 0.0035f;
    m4.isAutomatic = true;
    m4.ergonomics = 68.0f;
    m4.recoilVertical = 38.0f;
    m4.recoilHorizontal = 50.0f;
    m4.effectiveRange = 320.0f;
    m4.aimRange = 140.0f;
    m4.aimDownSightTime = 0.25f;
    m4.hipFireSpread = 0.020f;
    m4.recoilRecovery = 10.5f;
    m4.recoilPattern = 0.50f;
    m4.penetrationPower = 3.6f;
    m4.moveSpeedPenalty = 0.04f;
    m4.opticZoomLevel = 1; // Dedicated 2.5x Combat ACOG
    m4.hasVariableOptic = true; // Supports toggling between 1.0x CQB and 2.5x Combat ACOG
    m4.durability = 100.0f;
    m4.maxDurability = 100.0f;
    m4.gridW = 4;
    m4.gridH = 2;
    m4.traderPriceGold = 1;
    return m4;
}

Item Inventory::createMosin() {
    Item mosin;
    mosin.name = "Mosin-Nagant M91/30 Sniper";
    mosin.description = "Heavy bolt-action infantry rifle firing 7.62x54mmR. Lethal Level 5 armor penetration.";
    mosin.category = ItemCategory::Weapon;
    mosin.weaponType = WeaponType::Sniper;
    mosin.caliber = "7.62x54mmR LPS";
    mosin.weightKg = 4.0f;
    mosin.currentMag = 5;
    mosin.maxMag = 5;
    mosin.fireRateRpm = 45.0f;
    mosin.muzzleVelocityMs = 865.0f;
    mosin.damage = 95.0f;
    mosin.baseSpread = 0.0012f;
    mosin.isAutomatic = false;
    mosin.ergonomics = 34.0f;
    mosin.recoilVertical = 110.0f;
    mosin.recoilHorizontal = 85.0f;
    mosin.effectiveRange = 500.0f;
    mosin.aimRange = 250.0f;
    mosin.aimDownSightTime = 0.52f;
    mosin.hipFireSpread = 0.038f;
    mosin.recoilRecovery = 4.2f;
    mosin.recoilPattern = 0.85f;
    pen:
    mosin.penetrationPower = 5.5f;
    mosin.moveSpeedPenalty = 0.10f;
    mosin.opticZoomLevel = 2; // Dedicated 4x/6x PU Precision Sniper Scope
    mosin.hasVariableOptic = false; // Fixed precision sniper optic
    mosin.durability = 100.0f;
    mosin.maxDurability = 100.0f;
    mosin.gridW = 5;
    mosin.gridH = 1;
    mosin.traderPriceGold = 1;
    return mosin;
}

Item Inventory::createM9Pistol() {
    Item m9;
    m9.name = "Beretta M9A3 9x19mm";
    m9.description = "Tactical sidearm with threaded barrel and 17-round magazine.";
    m9.category = ItemCategory::Weapon;
    m9.weaponType = WeaponType::Pistol;
    m9.caliber = "9x19mm Parabellum";
    m9.weightKg = 1.1f;
    m9.currentMag = 15;
    m9.maxMag = 15;
    m9.fireRateRpm = 450.0f;
    m9.muzzleVelocityMs = 380.0f;
    m9.damage = 24.0f;
    m9.baseSpread = 0.0075f;
    m9.isAutomatic = false;
    m9.ergonomics = 82.0f;
    m9.recoilVertical = 22.0f;
    m9.recoilHorizontal = 28.0f;
    m9.effectiveRange = 80.0f;
    m9.aimRange = 40.0f;
    m9.aimDownSightTime = 0.16f;
    m9.hipFireSpread = 0.022f;
    m9.recoilRecovery = 15.0f;
    m9.recoilPattern = 0.40f;
    m9.penetrationPower = 1.8f;
    m9.moveSpeedPenalty = 0.0f;
    m9.opticZoomLevel = 0; // Dedicated 1.0x CQB Iron Sights / Micro Reflex
    m9.hasVariableOptic = false; // Pistols do not mount sniper scopes
    m9.durability = 100.0f;
    m9.maxDurability = 100.0f;
    m9.gridW = 2;
    m9.gridH = 1;
    m9.traderPriceGold = 1;
    return m9;
}

Item Inventory::create545Ammo(int count) {
    Item ammo;
    ammo.name = "5.45x39mm Ammo Box";
    ammo.description = "Standard rifle ammunition for AK-74.";
    ammo.category = ItemCategory::Ammo;
    ammo.caliber = "5.45x39mm";
    ammo.weightKg = 0.012f * count;
    ammo.quantity = count;
    ammo.maxStack = 180;
    return ammo;
}

Item Inventory::create556Ammo(int count) {
    Item ammo;
    ammo.name = "5.56x45mm NATO Box";
    ammo.description = "Standard rifle ammunition for M4A1.";
    ammo.category = ItemCategory::Ammo;
    ammo.caliber = "5.56x45mm";
    ammo.weightKg = 0.012f * count;
    ammo.quantity = count;
    ammo.maxStack = 180;
    return ammo;
}

Item Inventory::create762Ammo(int count) {
    Item ammo;
    ammo.name = "7.62x54mmR Ammo Box";
    ammo.description = "High-powered full-length rifle cartridges.";
    ammo.category = ItemCategory::Ammo;
    ammo.caliber = "7.62x54mmR";
    ammo.weightKg = 0.024f * count;
    ammo.quantity = count;
    ammo.maxStack = 60;
    return ammo;
}

Item Inventory::create9mmAmmo(int count) {
    Item ammo;
    ammo.name = "9x19mm Parabellum Box";
    ammo.description = "Standard pistol ammunition.";
    ammo.category = ItemCategory::Ammo;
    ammo.caliber = "9x19mm";
    ammo.weightKg = 0.009f * count;
    ammo.quantity = count;
    ammo.maxStack = 150;
    return ammo;
}

Item Inventory::createBandage(int count) {
    Item med;
    med.name = "Military Field Bandage";
    med.description = "Sterile combat dressing used to pack and stop active bleeding.";
    med.category = ItemCategory::Medical;
    med.weightKg = 0.15f * count;
    med.quantity = count;
    med.maxStack = 5;
    return med;
}

Item Inventory::createTourniquet() {
    Item t;
    t.name = "CAT Combat Tourniquet";
    t.description = "Rapid arterial occlusive device. Instantly stops major arterial bleed.";
    t.category = ItemCategory::Medical;
    t.weightKg = 0.10f;
    t.quantity = 1;
    t.maxStack = 3;
    return t;
}

Item Inventory::createSplint(int count) {
    Item s;
    s.name = "Aluminum Field Splint";
    s.description = "Immobilizes fractured extremities and restores mobility.";
    s.category = ItemCategory::Medical;
    s.weightKg = 0.20f * count;
    s.quantity = count;
    s.maxStack = 2;
    return s;
}

Item Inventory::createF1Grenade(int count) {
    Item g;
    g.name = "F-1 Defensive Frag Grenade";
    g.description = "Serrated fragmentation hand grenade with 60g TNT charge and lethal shrapnel radius.";
    g.category = ItemCategory::Weapon;
    g.weightKg = 0.60f * count;
    g.quantity = count;
    g.maxStack = 4;
    g.traderPriceGold = 1;
    return g;
}

Item Inventory::createSmokeGrenade(int count) {
    Item s;
    s.name = "RDG-2 Tactical Smoke Grenade";
    s.description = "Dense aerosol screening smoke canister for rapid visual and infrared concealment.";
    s.category = ItemCategory::Gear;
    s.weightKg = 0.50f * count;
    s.quantity = count;
    s.maxStack = 4;
    s.traderPriceGold = 1;
    return s;
}

Item Inventory::createMRE(int count) {
    Item food;
    food.name = "Combat Ration MRE";
    food.description = "Individual 24h combat field meal. Restores 400 kcal energy.";
    food.category = ItemCategory::Survival;
    food.weightKg = 0.5f * count;
    food.quantity = count;
    food.maxStack = 4;
    return food;
}

Item Inventory::createCanteen() {
    Item water;
    water.name = "Field Canteen (Full)";
    water.description = "Filtered drinking water. Restores 450 ml hydration.";
    water.category = ItemCategory::Survival;
    water.weightKg = 0.8f;
    water.quantity = 1;
    water.maxStack = 2;
    return water;
}

Item Inventory::createFastHelmet() {
    Item h;
    h.name = "FAST MT Ballistic Helmet";
    h.description = "Level IV high-cut combat helmet with ARC rails and NVG shroud.";
    h.category = ItemCategory::Gear;
    h.weightKg = 1.4f;
    h.durability = 55.0f;
    h.maxDurability = 55.0f;
    h.traderPriceGold = 1;
    return h;
}

Item Inventory::createPlateCarrier() {
    Item v;
    v.name = "Crye Precision JPC 2.0";
    v.description = "Level IV lightweight plate carrier with ceramic multicurve strike plates.";
    v.category = ItemCategory::Gear;
    v.weightKg = 6.8f;
    v.durability = 65.0f;
    v.maxDurability = 65.0f;
    v.traderPriceGold = 1;
    return v;
}

Item Inventory::createTacticalRig() {
    Item r;
    r.name = "Haley Strategic D3CRX";
    r.description = "Modular multi-mission chest rig with 4 rifle mag pouches and utility compartments.";
    r.category = ItemCategory::Gear;
    r.weightKg = 1.2f;
    r.durability = 100.0f;
    r.maxDurability = 100.0f;
    r.traderPriceGold = 1;
    return r;
}

Item Inventory::createAssaultBackpack() {
    Item b;
    b.name = "Mystery Ranch 3-Day Pack";
    b.description = "Heavy duty tactical patrol rucksack (32 Liters volume capacity).";
    b.category = ItemCategory::Gear;
    b.weightKg = 1.8f;
    b.durability = 100.0f;
    b.maxDurability = 100.0f;
    b.traderPriceGold = 1;
    return b;
}

Item Inventory::createIFAK() {
    Item med;
    med.name = "Tactical IFAK Trauma Kit";
    med.description = "Individual First Aid Kit containing hemostatic gauze, chest seal, and trauma shear.";
    med.category = ItemCategory::Medical;
    med.weightKg = 0.5f;
    med.quantity = 1;
    med.durability = 300.0f;
    med.maxDurability = 300.0f;
    med.traderPriceGold = 1;
    return med;
}

} // namespace Mercenary

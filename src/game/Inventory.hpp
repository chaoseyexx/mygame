#pragma once
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace Mercenary {

enum class ItemCategory {
    Weapon,
    Ammo,
    Medical,
    Survival,
    Gear
};

enum class WeaponType {
    None,
    Rifle,
    Sniper,
    Pistol
};

struct Item {
    int id = 0;
    std::string name;
    std::string description;
    ItemCategory category;
    float weightKg = 1.0f;
    int quantity = 1;
    int maxStack = 1;

    // Tarkov Stash & Modding Metrics
    std::string caliber = "5.56x45mm";
    float ergonomics = 65.0f;         // Handling speed & ADS time (0 - 100)
    float recoilVertical = 48.0f;     // Vertical muzzle climb
    float recoilHorizontal = 76.0f;   // Horizontal kick
    float effectiveRange = 400.0f;    // In meters
    float durability = 100.0f;
    float maxDurability = 100.0f;
    int gridW = 2;
    int gridH = 1;
    int traderPriceGold = 2;

    // Weapon specific
    WeaponType weaponType = WeaponType::None;
    int currentMag = 0;
    int maxMag = 0;
    float fireRateRpm = 600.0f;
    float muzzleVelocityMs = 850.0f;
    float damage = 35.0f;
    float baseSpread = 0.04f;
    bool isAutomatic = false;

    // Dedicated Weapon Optic & Sighting Profile
    int opticZoomLevel = 0;           // 0: 1.0x CQB Reflex/Iron, 1: 2.5x Combat ACOG, 2: 4x/6x Precision Scope
    bool hasVariableOptic = false;    // Whether this weapon optic supports variable magnification toggle

    // Advanced Weapon Ballistics & Handling
    float aimRange = 55.0f;           // Max clear sighting / optical zoom range (meters)
    float aimDownSightTime = 0.28f;   // Seconds to transition into ADS
    float hipFireSpread = 0.11f;      // Cone spread when firing from the hip
    float recoilRecovery = 9.0f;      // Recoil center-recovery rate (deg/s)
    float recoilPattern = 0.5f;       // Recoil verticality bias
    float penetrationPower = 1.0f;    // Armor penetration capability (1.0 = standard, 4.0+ = AP)
    float moveSpeedPenalty = 0.04f;   // Locomotion speed penalty while equipped
};

enum class EquipSlot {
    PrimaryWeapon,
    SecondaryWeapon,
    Vest,
    Backpack,
    Helmet,
    TacticalRig
};

class Inventory {
public:
    Inventory();

    bool addItem(const Item& item);
    void removeItem(size_t index);
    bool equipItem(size_t inventoryIndex, EquipSlot slot);
    void unequipItem(EquipSlot slot);

    std::optional<Item>& getEquipped(EquipSlot slot);
    const std::optional<Item>& getEquipped(EquipSlot slot) const;

    std::vector<Item>& getItems() { return m_items; }
    const std::vector<Item>& getItems() const { return m_items; }

    float getTotalWeightKg() const;
    int getAmmoCount(WeaponType type) const;
    int getAmmoCount(const std::string& caliber) const;
    bool consumeAmmo(WeaponType type, int count);
    bool consumeAmmo(const std::string& caliber, int count);
    int consumeAmmoCount(const std::string& caliber, int count);
    int consumeAmmoCount(WeaponType type, int count);
    void sortItems();
    static bool isItemValidForSlot(const Item& item, EquipSlot slot);
    bool useFirstMedical();
    bool useFirstFood();
    bool useFirstWater();

    static Item createAK74();
    static Item createM4A1();
    static Item createMosin();
    static Item createM9Pistol();
    static Item create545Ammo(int count = 60);
    static Item create556Ammo(int count = 60);
    static Item create762Ammo(int count = 20);
    static Item create9mmAmmo(int count = 50);
    static Item createFastHelmet();
    static Item createPlateCarrier();
    static Item createTacticalRig();
    static Item createAssaultBackpack();
    static Item createIFAK();
    static Item createBandage(int count = 2);
    static Item createTourniquet();
    static Item createSplint(int count = 1);
    static Item createF1Grenade(int count = 2);
    static Item createSmokeGrenade(int count = 2);
    static Item createMRE(int count = 1);
    static Item createCanteen();

private:
    std::vector<Item> m_items;
    std::optional<Item> m_equippedPrimary;
    std::optional<Item> m_equippedSecondary;
    std::optional<Item> m_equippedVest;
    std::optional<Item> m_equippedBackpack;
    std::optional<Item> m_equippedHelmet;
    std::optional<Item> m_equippedRig;

    int m_maxSlots = 48;
};

} // namespace Mercenary

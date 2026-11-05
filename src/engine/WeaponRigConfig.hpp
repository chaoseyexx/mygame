#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

namespace Mercenary {

struct WeaponGripConfig {
    // Weapon root offset relative to soldier chest
    glm::vec3 rootOffset{0.14f, -0.10f, 0.18f};       // Hip / Low-ready: right, Y, forward
    glm::vec3 rootOffsetADS{0.08f, 0.03f, 0.26f};     // ADS: right, Y, forward

    // Hand contact sockets relative to weapon root
    glm::vec3 rightHandOffset{-0.06f, -0.06f, 0.0f};  // Firing hand: forward, up, right
    glm::vec3 leftHandOffset{0.22f, -0.02f, -0.02f};  // Support hand (hip): forward, up, right
    glm::vec3 leftHandOffsetADS{0.22f, -0.02f, -0.02f};// Support hand (ADS): forward, up, right

    // Orientation trims (low ready angle & cant)
    float lowReadyPitch = 0.22f; // Radians (positive points down)
    float rollTrim = 0.0f;       // Degrees
    float yawTrim = 0.0f;        // Degrees

    // Ground placement & contact shadow
    float groundClearance = 0.045f; // Elevation clearance above terrain (m)
    float shadowRadius = 0.35f;     // Tight contact shadow width (m)
    float shadowAlpha = 0.65f;      // Contact shadow darkness
};

class WeaponRigManager {
public:
    static WeaponRigManager& get() {
        static WeaponRigManager instance;
        return instance;
    }

    WeaponGripConfig& getConfig(const std::string& weaponName);
    const WeaponGripConfig& getConfig(const std::string& weaponName) const;
    void setConfig(const std::string& weaponName, const WeaponGripConfig& cfg);

    std::vector<std::string> getAvailableWeapons() const;

    bool loadFromFile(const std::string& path = "assets/config/weapon_rig.json");
    bool saveToFile(const std::string& path = "assets/config/weapon_rig.json");

    void resetToDefaults();

private:
    WeaponRigManager();
    std::unordered_map<std::string, WeaponGripConfig> m_configs;
    WeaponGripConfig m_defaultConfig;
};

} // namespace Mercenary

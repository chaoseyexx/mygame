#include "WeaponRigConfig.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <iomanip>

namespace fs = std::filesystem;

namespace Mercenary {

WeaponRigManager::WeaponRigManager() {
    resetToDefaults();
    loadFromFile();
}

void WeaponRigManager::resetToDefaults() {
    m_configs.clear();

    // Default Assault Rifle (M4A1 SOPMOD) - calibrated tactical forward mount
    WeaponGripConfig m4;
    m4.rootOffset = glm::vec3(0.12f, 0.06f, 0.35f);   // Buttstock rests in shoulder pocket; receiver/grip in front of chest
    m4.rootOffsetADS = glm::vec3(0.04f, 0.14f, 0.28f); // Optic aligned directly with sightline
    m4.rightHandOffset = glm::vec3(0.00f, -0.065f, -0.080f); // Firm trigger grip on pistol grip
    m4.leftHandOffset = glm::vec3(-0.015f, -0.055f, 0.080f); // Support hand cupping magazine well / rear handguard
    m4.leftHandOffsetADS = glm::vec3(-0.015f, -0.055f, 0.080f);
    m4.lowReadyPitch = 0.22f; // Clean ~12.6 deg depressed muzzle in low-ready
    m4.rollTrim = 0.0f;
    m4.yawTrim = 0.0f;
    m4.groundClearance = 0.045f;
    m4.shadowRadius = 0.35f;
    m4.shadowAlpha = 0.65f;
    m_configs["M4A1"] = m4;

    // AK-74M
    WeaponGripConfig ak;
    ak.rootOffset = glm::vec3(0.12f, 0.06f, 0.35f);
    ak.rootOffsetADS = glm::vec3(0.04f, 0.14f, 0.28f);
    ak.rightHandOffset = glm::vec3(0.00f, -0.065f, -0.060f);
    ak.leftHandOffset = glm::vec3(-0.015f, -0.045f, 0.100f); // Support hand on lower polymer handguard
    ak.leftHandOffsetADS = glm::vec3(-0.015f, -0.045f, 0.100f);
    ak.lowReadyPitch = 0.22f;
    ak.rollTrim = 0.0f;
    ak.yawTrim = 0.0f;
    ak.groundClearance = 0.045f;
    ak.shadowRadius = 0.36f;
    ak.shadowAlpha = 0.65f;
    m_configs["AK-74M"] = ak;

    // Mosin-Nagant Bolt-Action Sniper
    WeaponGripConfig mosin;
    mosin.rootOffset = glm::vec3(0.12f, 0.06f, 0.36f);
    mosin.rootOffsetADS = glm::vec3(0.04f, 0.14f, 0.26f); // PU 3.5x scope centered to eye
    mosin.rightHandOffset = glm::vec3(0.00f, -0.035f, -0.160f); // Classic semi-pistol grip wrist
    mosin.leftHandOffset = glm::vec3(-0.015f, -0.035f, 0.120f); // Wooden fore-end forward support
    mosin.leftHandOffsetADS = glm::vec3(-0.015f, -0.035f, 0.120f);
    mosin.lowReadyPitch = 0.24f; // ~13.7 deg downward angle
    mosin.rollTrim = 0.0f;
    mosin.yawTrim = 0.0f;
    mosin.groundClearance = 0.045f;
    mosin.shadowRadius = 0.42f;
    mosin.shadowAlpha = 0.65f;
    m_configs["Mosin-Nagant"] = mosin;

    // Beretta M9 Handgun (Cupped two-handed grip)
    WeaponGripConfig m9;
    m9.rootOffset = glm::vec3(0.08f, 0.02f, 0.32f);
    m9.rootOffsetADS = glm::vec3(0.03f, 0.15f, 0.36f); // Two-handed punched out Isosceles stance
    m9.rightHandOffset = glm::vec3(0.00f, -0.055f, -0.030f);
    m9.leftHandOffset = glm::vec3(-0.015f, -0.065f, -0.025f); // Support hand cupping bottom of grip
    m9.leftHandOffsetADS = glm::vec3(-0.015f, -0.065f, -0.025f);
    m9.lowReadyPitch = 0.28f; // Compressed ready pointing downward
    m9.rollTrim = 0.0f;
    m9.yawTrim = 0.0f;
    m9.groundClearance = 0.030f;
    m9.shadowRadius = 0.22f;
    m9.shadowAlpha = 0.55f;
    m_configs["Beretta M9"] = m9;

    m_defaultConfig = m4;
}

WeaponGripConfig& WeaponRigManager::getConfig(const std::string& weaponName) {
    for (auto& pair : m_configs) {
        if (weaponName.find(pair.first) != std::string::npos || pair.first.find(weaponName) != std::string::npos) {
            return pair.second;
        }
    }
    if (weaponName.find("Pistol") != std::string::npos || weaponName.find("M9") != std::string::npos || weaponName.find("Beretta") != std::string::npos) {
        return m_configs["Beretta M9"];
    }
    if (weaponName.find("Mosin") != std::string::npos || weaponName.find("Sniper") != std::string::npos) {
        return m_configs["Mosin-Nagant"];
    }
    if (weaponName.find("AK") != std::string::npos || weaponName.find("74") != std::string::npos) {
        return m_configs["AK-74M"];
    }
    if (m_configs.find(weaponName) != m_configs.end()) {
        return m_configs[weaponName];
    }
    return m_configs["M4A1"];
}

const WeaponGripConfig& WeaponRigManager::getConfig(const std::string& weaponName) const {
    for (const auto& pair : m_configs) {
        if (weaponName.find(pair.first) != std::string::npos || pair.first.find(weaponName) != std::string::npos) {
            return pair.second;
        }
    }
    if (weaponName.find("Pistol") != std::string::npos || weaponName.find("M9") != std::string::npos || weaponName.find("Beretta") != std::string::npos) {
        auto it = m_configs.find("Beretta M9");
        if (it != m_configs.end()) return it->second;
    }
    if (weaponName.find("Mosin") != std::string::npos || weaponName.find("Sniper") != std::string::npos) {
        auto it = m_configs.find("Mosin-Nagant");
        if (it != m_configs.end()) return it->second;
    }
    if (weaponName.find("AK") != std::string::npos || weaponName.find("74") != std::string::npos) {
        auto it = m_configs.find("AK-74M");
        if (it != m_configs.end()) return it->second;
    }
    auto it = m_configs.find(weaponName);
    if (it != m_configs.end()) return it->second;
    it = m_configs.find("M4A1");
    if (it != m_configs.end()) return it->second;
    return m_defaultConfig;
}

void WeaponRigManager::setConfig(const std::string& weaponName, const WeaponGripConfig& cfg) {
    m_configs[weaponName] = cfg;
}

std::vector<std::string> WeaponRigManager::getAvailableWeapons() const {
    std::vector<std::string> names;
    for (const auto& pair : m_configs) {
        names.push_back(pair.first);
    }
    return names;
}

static void writeVec3(std::ostream& os, const char* name, const glm::vec3& v, bool comma = true) {
    os << "      \"" << name << "\": [" << std::fixed << std::setprecision(4)
       << v.x << ", " << v.y << ", " << v.z << "]" << (comma ? "," : "") << "\n";
}

static void writeFloat(std::ostream& os, const char* name, float val, bool comma = true) {
    os << "      \"" << name << "\": " << std::fixed << std::setprecision(4)
       << val << (comma ? "," : "") << "\n";
}

bool WeaponRigManager::saveToFile(const std::string& path) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[WeaponRigManager] Failed to open " << path << " for writing\n";
        return false;
    }

    out << "{\n  \"weapons\": {\n";
    size_t count = 0;
    for (const auto& [name, cfg] : m_configs) {
        out << "    \"" << name << "\": {\n";
        writeVec3(out, "rootOffset", cfg.rootOffset);
        writeVec3(out, "rootOffsetADS", cfg.rootOffsetADS);
        writeVec3(out, "rightHandOffset", cfg.rightHandOffset);
        writeVec3(out, "leftHandOffset", cfg.leftHandOffset);
        writeVec3(out, "leftHandOffsetADS", cfg.leftHandOffsetADS);
        writeFloat(out, "lowReadyPitch", cfg.lowReadyPitch);
        writeFloat(out, "rollTrim", cfg.rollTrim);
        writeFloat(out, "yawTrim", cfg.yawTrim);
        writeFloat(out, "groundClearance", cfg.groundClearance);
        writeFloat(out, "shadowRadius", cfg.shadowRadius);
        writeFloat(out, "shadowAlpha", cfg.shadowAlpha, false);
        out << "    }" << (++count < m_configs.size() ? "," : "") << "\n";
    }
    out << "  }\n}\n";
    out.close();
    std::cout << "[WeaponRigManager] Saved weapon rig configurations to " << path << "\n";
    return true;
}

static glm::vec3 parseVec3(const std::string& text) {
    size_t start = text.find('[');
    size_t end = text.find(']');
    if (start != std::string::npos && end != std::string::npos && end > start) {
        std::string inner = text.substr(start + 1, end - start - 1);
        std::stringstream ss(inner);
        std::string a, b, c;
        if (std::getline(ss, a, ',') && std::getline(ss, b, ',') && std::getline(ss, c, ',')) {
            try {
                return glm::vec3(std::stof(a), std::stof(b), std::stof(c));
            } catch (...) {}
        }
    }
    return glm::vec3(0.0f);
}

static float parseNum(const std::string& line) {
    size_t col = line.find(':');
    if (col != std::string::npos) {
        std::string sub = line.substr(col + 1);
        while (!sub.empty() && (sub.back() == ',' || sub.back() == ' ' || sub.back() == '\r')) sub.pop_back();
        try {
            return std::stof(sub);
        } catch (...) {}
    }
    return 0.0f;
}

bool WeaponRigManager::loadFromFile(const std::string& path) {
    if (!fs::exists(path)) {
        saveToFile(path);
        return true;
    }

    std::ifstream in(path);
    if (!in.is_open()) return false;

    std::string line;
    std::string currentWeapon = "";
    std::string currentVecKey = "";
    std::vector<float> pendingVec;
    WeaponGripConfig curCfg;

    while (std::getline(in, line)) {
        // If we are accumulating elements of a multi-line array
        if (!currentVecKey.empty()) {
            for (char ch : line) {
                if (ch == ']' || ch == '}') break;
            }
            std::stringstream ss(line);
            float val;
            while (ss >> val) {
                pendingVec.push_back(val);
                char comma;
                ss >> comma;
            }
            if (line.find(']') != std::string::npos || pendingVec.size() >= 3) {
                glm::vec3 v(0.0f);
                if (pendingVec.size() >= 1) v.x = pendingVec[0];
                if (pendingVec.size() >= 2) v.y = pendingVec[1];
                if (pendingVec.size() >= 3) v.z = pendingVec[2];

                if (currentVecKey == "rootOffset") curCfg.rootOffset = v;
                else if (currentVecKey == "rootOffsetADS") curCfg.rootOffsetADS = v;
                else if (currentVecKey == "rightHandOffset") curCfg.rightHandOffset = v;
                else if (currentVecKey == "leftHandOffset") curCfg.leftHandOffset = v;
                else if (currentVecKey == "leftHandOffsetADS") curCfg.leftHandOffsetADS = v;

                currentVecKey = "";
                pendingVec.clear();
            }
            continue;
        }

        size_t q1 = line.find('"');
        if (q1 != std::string::npos) {
            size_t q2 = line.find('"', q1 + 1);
            if (q2 != std::string::npos) {
                std::string key = line.substr(q1 + 1, q2 - q1 - 1);
                if (line.find('{') != std::string::npos && key != "weapons") {
                    currentWeapon = key;
                    curCfg = m_configs.count(key) ? m_configs[key] : m_defaultConfig;
                } else if (!currentWeapon.empty()) {
                    if (key == "rootOffset" || key == "rootOffsetADS" || key == "rightHandOffset" ||
                        key == "leftHandOffset" || key == "leftHandOffsetADS") {
                        if (line.find('[') != std::string::npos && line.find(']') != std::string::npos) {
                            glm::vec3 v = parseVec3(line);
                            if (key == "rootOffset") curCfg.rootOffset = v;
                            else if (key == "rootOffsetADS") curCfg.rootOffsetADS = v;
                            else if (key == "rightHandOffset") curCfg.rightHandOffset = v;
                            else if (key == "leftHandOffset") curCfg.leftHandOffset = v;
                            else if (key == "leftHandOffsetADS") curCfg.leftHandOffsetADS = v;
                        } else if (line.find('[') != std::string::npos) {
                            currentVecKey = key;
                            pendingVec.clear();
                        }
                    }
                    else if (key == "lowReadyPitch") curCfg.lowReadyPitch = parseNum(line);
                    else if (key == "rollTrim") curCfg.rollTrim = parseNum(line);
                    else if (key == "yawTrim") curCfg.yawTrim = parseNum(line);
                    else if (key == "groundClearance") curCfg.groundClearance = parseNum(line);
                    else if (key == "shadowRadius") curCfg.shadowRadius = parseNum(line);
                    else if (key == "shadowAlpha") curCfg.shadowAlpha = parseNum(line);
                }
            }
        }
        if (line.find('}') != std::string::npos && !currentWeapon.empty() && currentVecKey.empty()) {
            // Sanitize weapon grip config: ensure weapon does not penetrate torso and hands reach properly
            bool isPistol = (currentWeapon.find("M9") != std::string::npos || currentWeapon.find("Pistol") != std::string::npos || currentWeapon.find("Beretta") != std::string::npos);
            if (!isPistol) {
                if (curCfg.rootOffset.z < 0.28f) {
                    curCfg.rootOffset = (m_configs.count(currentWeapon) ? m_configs[currentWeapon].rootOffset : m_defaultConfig.rootOffset);
                    curCfg.rootOffsetADS = (m_configs.count(currentWeapon) ? m_configs[currentWeapon].rootOffsetADS : m_defaultConfig.rootOffsetADS);
                }
                if (curCfg.leftHandOffset.x > 0.05f || curCfg.leftHandOffset.z < 0.04f || curCfg.leftHandOffset.z > 0.16f) {
                    curCfg.leftHandOffset = (m_configs.count(currentWeapon) ? m_configs[currentWeapon].leftHandOffset : m_defaultConfig.leftHandOffset);
                    curCfg.leftHandOffsetADS = (m_configs.count(currentWeapon) ? m_configs[currentWeapon].leftHandOffsetADS : m_defaultConfig.leftHandOffsetADS);
                }
                if (curCfg.rightHandOffset.z > -0.02f) {
                    curCfg.rightHandOffset = (m_configs.count(currentWeapon) ? m_configs[currentWeapon].rightHandOffset : m_defaultConfig.rightHandOffset);
                }
            } else {
                if (curCfg.rootOffset.z < 0.22f) {
                    curCfg.rootOffset = (m_configs.count(currentWeapon) ? m_configs[currentWeapon].rootOffset : m_defaultConfig.rootOffset);
                    curCfg.rootOffsetADS = (m_configs.count(currentWeapon) ? m_configs[currentWeapon].rootOffsetADS : m_defaultConfig.rootOffsetADS);
                }
            }
            m_configs[currentWeapon] = curCfg;
            currentWeapon = "";
        }
    }

    in.close();
    std::cout << "[WeaponRigManager] Loaded " << m_configs.size() << " weapon configurations from " << path << "\n";
    return true;
}

} // namespace Mercenary


#pragma once
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace Mercenary {

enum class GameAction {
    MoveForward = 0,
    MoveBackward,
    MoveLeft,
    MoveRight,
    Sprint,
    CrouchSlide,
    Prone,
    Jump,
    Cover,
    Reload,
    ThrowGrenade,
    DropWeapon,
    Inventory,
    FirstAid,
    Interact,
    HoldBreath,
    VoiceProximity,
    VoiceSquad,
    TacticalMap,
    ShoulderSwap,
    ToggleOptic,
    Count
};

struct KeybindInfo {
    GameAction action;
    std::string name;
    std::string category;
    int currentKey;
    int defaultKey;
};

class InputConfig {
public:
    static InputConfig& get() {
        static InputConfig instance;
        return instance;
    }

    int getKey(GameAction action) const {
        for (const auto& bind : m_binds) {
            if (bind.action == action) return bind.currentKey;
        }
        return GLFW_KEY_UNKNOWN;
    }

    void setKey(GameAction action, int newKey) {
        for (auto& bind : m_binds) {
            if (bind.action == action) {
                bind.currentKey = newKey;
                saveToFile();
                return;
            }
        }
    }

    std::vector<KeybindInfo>& getBinds() { return m_binds; }
    const std::vector<KeybindInfo>& getBinds() const { return m_binds; }

    static std::string getKeyDisplayName(int key) {
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
            return std::string(1, static_cast<char>('A' + (key - GLFW_KEY_A)));
        }
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
            return std::string(1, static_cast<char>('0' + (key - GLFW_KEY_0)));
        }
        if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
            return "F" + std::to_string(key - GLFW_KEY_F1 + 1);
        }
        switch (key) {
            case GLFW_KEY_SPACE: return "Space";
            case GLFW_KEY_LEFT_SHIFT: return "LShift";
            case GLFW_KEY_RIGHT_SHIFT: return "RShift";
            case GLFW_KEY_LEFT_CONTROL: return "LCtrl";
            case GLFW_KEY_RIGHT_CONTROL: return "RCtrl";
            case GLFW_KEY_LEFT_ALT: return "LAlt";
            case GLFW_KEY_RIGHT_ALT: return "RAlt";
            case GLFW_KEY_TAB: return "Tab";
            case GLFW_KEY_ENTER: return "Enter";
            case GLFW_KEY_ESCAPE: return "Esc";
            case GLFW_KEY_BACKSPACE: return "Backspace";
            case GLFW_KEY_CAPS_LOCK: return "Caps";
            case GLFW_KEY_UP: return "Up";
            case GLFW_KEY_DOWN: return "Down";
            case GLFW_KEY_LEFT: return "Left";
            case GLFW_KEY_RIGHT: return "Right";
            case GLFW_KEY_HOME: return "Home";
            case GLFW_KEY_END: return "End";
            case GLFW_KEY_PAGE_UP: return "PgUp";
            case GLFW_KEY_PAGE_DOWN: return "PgDown";
            case GLFW_KEY_INSERT: return "Insert";
            case GLFW_KEY_DELETE: return "Delete";
            case GLFW_KEY_KP_ENTER: return "NumEnter";
            default: break;
        }
        const char* name = glfwGetKeyName(key, 0);
        if (name && name[0] != '\0') {
            std::string s(name);
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return s;
        }
        return "Key " + std::to_string(key);
    }

    void resetDefaults() {
        for (auto& bind : m_binds) {
            bind.currentKey = bind.defaultKey;
        }
        saveToFile();
    }

    void saveToFile(const std::string& path = "config/keybinds.cfg") {
        try {
            std::filesystem::create_directories("config");
            std::ofstream out(path);
            if (!out.is_open()) return;
            for (const auto& bind : m_binds) {
                out << static_cast<int>(bind.action) << "=" << bind.currentKey << "\n";
            }
        } catch (...) {}
    }

    void loadFromFile(const std::string& path = "config/keybinds.cfg") {
        std::ifstream in(path);
        if (!in.is_open()) return;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto eq = line.find('=');
            if (eq != std::string::npos) {
                int actInt = std::stoi(line.substr(0, eq));
                int keyInt = std::stoi(line.substr(eq + 1));
                for (auto& bind : m_binds) {
                    if (static_cast<int>(bind.action) == actInt) {
                        bind.currentKey = keyInt;
                        break;
                    }
                }
            }
        }
    }

private:
    InputConfig() {
        m_binds = {
            { GameAction::MoveForward,    "Move Forward",             "Movement", GLFW_KEY_W,          GLFW_KEY_W },
            { GameAction::MoveBackward,   "Move Backward",            "Movement", GLFW_KEY_S,          GLFW_KEY_S },
            { GameAction::MoveLeft,       "Move Left (Strafe)",       "Movement", GLFW_KEY_A,          GLFW_KEY_A },
            { GameAction::MoveRight,      "Move Right (Strafe)",      "Movement", GLFW_KEY_D,          GLFW_KEY_D },
            { GameAction::Sprint,         "Tactical Sprint",          "Movement", GLFW_KEY_LEFT_SHIFT, GLFW_KEY_LEFT_SHIFT },
            { GameAction::CrouchSlide,    "Crouch / Sprint Slide",    "Movement", GLFW_KEY_C,          GLFW_KEY_C },
            { GameAction::Prone,          "Go Prone",                 "Movement", GLFW_KEY_X,          GLFW_KEY_X },
            { GameAction::Jump,           "Jump / Vault / Mantle",    "Movement", GLFW_KEY_SPACE,      GLFW_KEY_SPACE },
            { GameAction::Cover,          "Take Cover",               "Movement", GLFW_KEY_Q,          GLFW_KEY_Q },
            { GameAction::Reload,         "Tactical Reload",          "Combat",   GLFW_KEY_R,          GLFW_KEY_R },
            { GameAction::ThrowGrenade,   "Throw Frag/Smoke Grenade", "Combat",   GLFW_KEY_4,          GLFW_KEY_4 },
            { GameAction::DropWeapon,     "Drop Current Weapon",      "Combat",   GLFW_KEY_G,          GLFW_KEY_G },
            { GameAction::HoldBreath,     "Hold Breath (Steady ADS)", "Combat",   GLFW_KEY_LEFT_ALT,   GLFW_KEY_LEFT_ALT },
            { GameAction::ShoulderSwap,   "Swap Camera Shoulder",     "Combat",   GLFW_KEY_T,          GLFW_KEY_T },
            { GameAction::ToggleOptic,    "Toggle Optic Magnification", "Combat", GLFW_KEY_V,          GLFW_KEY_V },
            { GameAction::Interact,       "Interact / Mount Turret",  "Interaction", GLFW_KEY_F,       GLFW_KEY_F },
            { GameAction::FirstAid,       "Field Medical / Bandage",  "Medical",  GLFW_KEY_H,          GLFW_KEY_H },
            { GameAction::VoiceProximity, "Proximity Radio PTT",      "Comms",    GLFW_KEY_V,          GLFW_KEY_V },
            { GameAction::VoiceSquad,     "Squad Datalink Radio PTT", "Comms",    GLFW_KEY_B,          GLFW_KEY_B },
            { GameAction::Inventory,      "Tactical Inventory Grid",  "UI",       GLFW_KEY_TAB,        GLFW_KEY_TAB },
            { GameAction::TacticalMap,    "Strategic Warzone Map",    "UI",       GLFW_KEY_M,          GLFW_KEY_M }
        };
        loadFromFile();
    }

    std::vector<KeybindInfo> m_binds;
};

} // namespace Mercenary

#include "GameApp.hpp"
#include "engine/Types.hpp"
#include "engine/Audio.hpp"
#include "engine/VoiceChat.hpp"
#include "engine/InputConfig.hpp"
#include "game/ChatSystem.hpp"
#include "engine/Network.hpp"
#include "engine/SteamIntegration.hpp"
#include "engine/PhysicsEngine.hpp"
#include "engine/NavMeshManager.hpp"
#include "game/CharacterProfile.hpp"
#include "game/GoreSystem.hpp"
#include "game/DecalSystem.hpp"
#include "game/PunctureSystem.hpp"
#include "game/ArmoryShop.hpp"
#include "game/ExchangeMarket.hpp"
#include <imgui.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Mercenary {

inline float distSqPointToSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, glm::vec3& closestOnSeg) {
    glm::vec3 ab = b - a;
    float l2 = glm::dot(ab, ab);
    if (l2 < 1e-6f) {
        closestOnSeg = a;
        glm::vec3 diff = p - a;
        return glm::dot(diff, diff);
    }
    float t = std::clamp(glm::dot(p - a, ab) / l2, 0.0f, 1.0f);
    closestOnSeg = a + ab * t;
    glm::vec3 diff = p - closestOnSeg;
    return glm::dot(diff, diff);
}

GameApp::GameApp() {}
GameApp::~GameApp() {}

bool GameApp::init(int argc, char* argv[]) {
    Window& window = m_window;
    Renderer& renderer = m_renderer;
    UIManager& ui = m_ui;
    World& world = m_world;
    Player& player = m_player;
    FrontlineWarfare& frontline = m_frontline;
    MissionSystem& missions = m_missions;
    BallisticsSystem& ballistics = m_ballistics;
    Camera& camera = m_camera;
    PostProcess& postProcess = m_postProcess;

    // 0. Headless Dedicated Server Check (Foxhole Persistent Frontline Shard)
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--server") {
            uint16_t srvPort = 27015;
            if (i + 1 < argc && argv[i+1][0] != '-') {
                srvPort = static_cast<uint16_t>(std::stoi(argv[i+1]));
            }
            std::cout << "=========================================================\n";
            std::cout << "   MERCENARY: PERSISTENT FRONTLINE DEDICATED SHARD\n";
            std::cout << "   MMO 9-Chunk Area of Interest (AOI) Spatial Grid Active\n";
            std::cout << "=========================================================\n";
            NetworkEngine& net = NetworkEngine::get();
            if (!net.startServer(srvPort)) {
                std::cerr << "[Server Error] Failed to bind to UDP port " << srvPort << "\n";
                return false;
            }
            std::cout << "[Server] Dedicated persistent shard listening on UDP port " << srvPort << ".\n";
            std::cout << "[Server] Press Ctrl+C in this terminal to shut down server.\n";
            while (true) {
                net.updateServer(0.033f);
#ifdef _WIN32
                Sleep(33); // 30 ticks per second
#endif
            }
            return 0;
        }
    }

    std::cout << "=========================================================\n";
    std::cout << "  MERCENARY: FRONTLINE OPERATIONS (Mercenary Engine C++20)\n";
    std::cout << "=========================================================\n";

    std::string cfgServerIP = "auto";
    uint16_t cfgServerPort = 7777;

// Check if running in automated test capture or explicit windowed mode
    bool isCaptureOrWindowedTest = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--capture" || a == "--windowed") {
            isCaptureOrWindowedTest = true;
        } else if (a == "--connect" || a == "--server-ip" || a == "-s") {
            if (i + 1 < argc && argv[i+1][0] != '-') {
                std::string target = argv[++i];
                size_t colon = target.find(':');
                if (colon != std::string::npos) {
                    cfgServerIP = target.substr(0, colon);
                    cfgServerPort = static_cast<uint16_t>(std::stoi(target.substr(colon + 1)));
                } else {
                    cfgServerIP = target;
                }
            }
        }
    }

    // Synchronize settings with launcher_config.ini
    int cfgResIdx = 0; // Default 1920x1080
    bool cfgFullscreen = true;
    bool cfgVsync = true;
    int cfgShadowQuality = 2; // High (4096)
    int cfgSsaoQuality = 1;
    int cfgGrassTufts = 3500;
    std::string cfgActiveMap = "training_base.vmap";
    float cfgMasterVol = 1.0f;
    float cfgMusicVol = 0.5f;
    float cfgSfxVol = 1.0f;
    float cfgVoiceVol = 1.0f;

    std::ifstream cfgIn("assets/launcher_config.ini");
    if (!cfgIn.is_open()) cfgIn.open("ASSETS/launcher_config.ini");
    if (cfgIn.is_open()) {
        std::string line;
        while (std::getline(cfgIn, line)) {
            if (line.empty() || line[0] == '#' || line[0] == '[') continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            while (!k.empty() && isspace(k.back())) k.pop_back();
            while (!k.empty() && isspace(k.front())) k.erase(0, 1);
            while (!v.empty() && isspace(v.back())) v.pop_back();
            while (!v.empty() && isspace(v.front())) v.erase(0, 1);

            try {
                if (k == "resolutionIndex") cfgResIdx = std::stoi(v);
                else if (k == "fullscreen") cfgFullscreen = (v == "1" || v == "true");
                else if (k == "vsync") cfgVsync = (v == "1" || v == "true");
                else if (k == "shadowQuality") cfgShadowQuality = std::stoi(v);
                else if (k == "ssaoQuality") cfgSsaoQuality = std::stoi(v);
                else if (k == "grassTufts") cfgGrassTufts = std::stoi(v);
                else if (k == "activeMap") cfgActiveMap = v;
                else if (k == "serverIP") {
                    if (v != "127.0.0.1" && !v.empty()) cfgServerIP = v;
                }
                else if (k == "serverPort") {
                    uint16_t p = static_cast<uint16_t>(std::stoi(v));
                    if (p != 27015 && p > 0) cfgServerPort = p;
                }
                else if (k == "masterVolume") cfgMasterVol = std::stof(v);
                else if (k == "musicVolume") cfgMusicVol = std::stof(v);
                else if (k == "sfxVolume") cfgSfxVol = std::stof(v);
                else if (k == "voiceVolume") cfgVoiceVol = std::stof(v);
            } catch (...) {}
        }
    }

    // 1. Initialize Window & Context synchronized with launcher configuration
    WindowConfig config;
    int resWidths[]  = { 1920, 1600, 1366, 1280, 2560 };
    int resHeights[] = { 1080,  900,  768,  720, 1440 };
    if (cfgResIdx >= 0 && cfgResIdx < 5) {
        config.width = resWidths[cfgResIdx];
        config.height = resHeights[cfgResIdx];
    } else {
        config.width = 1600;
        config.height = 900;
    }
    config.title = "Mercenary: Frontline Operations [PvPvE Faction Warfare]";
    config.vsync = cfgVsync;
    config.displayMode = isCaptureOrWindowedTest ? WindowDisplayMode::Windowed : 
                         (cfgFullscreen ? WindowDisplayMode::ExclusiveFullscreen : WindowDisplayMode::FullscreenWindowed);

    if (!window.init(config)) {
        std::cerr << "Engine fatal: Window init failed.\n";
        return false;
    }

    // 2. Initialize Core Systems
    if (!renderer.init()) {
        std::cerr << "Engine fatal: Renderer init failed.\n";
        return false;
    }

    DecalSystem::get().init();
    if (cfgServerIP != "auto" && !cfgServerIP.empty()) {
        NetworkEngine::get().setServerHost(cfgServerIP);
    }

    if (!AudioEngine::get().init()) {
        std::cerr << "Warning: Audio engine could not be initialized.\n";
    }

    // Initialize Steamworks API
    SteamIntegration::get().init();

    // Initialize Jolt Physics Engine (Rigid Bodies, Slicing & Destruction)
    if (!PhysicsEngine::get().init()) {
        std::cerr << "Warning: Jolt Physics Engine could not be initialized.\n";
    }

    ui.init(window.getNativeWindow());
    ui.loadSettings();

    ui.setWindowMode(window.getDisplayMode() == WindowDisplayMode::ExclusiveFullscreen ? 1 : 0);
    ui.setWindowModeCallback([&window](int mode) {
        window.setDisplayMode(mode == 1 ? WindowDisplayMode::ExclusiveFullscreen : WindowDisplayMode::FullscreenWindowed);
    });
    ui.setResolutionCallback([&window](int resIdx) {
        int w = 1920, h = 1080;
        if (resIdx == 0) { w = 1600; h = 900; }
        else if (resIdx == 1) { w = 1920; h = 1080; }
        else if (resIdx == 2) { w = 2560; h = 1440; }
        window.setResolution(w, h);
    });
    ui.setFullscreenCallbacks([&window, &ui]() {
        window.toggleFullscreen();
        ui.setWindowMode(window.getDisplayMode() == WindowDisplayMode::ExclusiveFullscreen ? 1 : 0);
    }, [&window]() { return window.isFullscreen(); });
    ui.setVsyncCallback([&window](bool vsync) {
        window.setVsync(vsync);
    });

    // Synchronize persistent audio settings
    AudioEngine::get().setMasterVolume(ui.getMasterVolume());
    AudioEngine::get().setSfxVolume(ui.getSfxVolume());
    AudioEngine::get().setAmbientVolume(ui.getAmbientVolume());
    VoiceChatManager::get().setVoiceVolume(ui.getVoiceVolume());

    int shadowRes = (cfgShadowQuality == 0) ? 1024 : ((cfgShadowQuality == 1) ? 2048 : 4096);
    m_shadowMap = std::make_unique<ShadowMap>(shadowRes);
    ShadowMap& shadowMap = *m_shadowMap;
    if (!shadowMap.init()) {
        std::cerr << "Warning: Shadow map init failed.\n";
    }

    if (!postProcess.init(window.getWidth(), window.getHeight())) {
        std::cerr << "Warning: PostProcess HDR pipeline init failed.\n";
    }

    // 3. Initialize Gameplay & Mercenary Systems

    // Initialize Recast & Detour Navigation Mesh
    NavMeshManager::instance().initialize(world);

    frontline.init(10); // 10 bots for Team A, 10 bots for Team B (20 active combatants)

    bool& startInGame = m_startInGame;
    bool& testAim = m_testAim;
    bool& testRemote = m_testRemote;
    bool& testChat = m_testChat;
    bool& testVoice = m_testVoice;
    bool& testCrouch = m_testCrouch;
    bool& testMove = m_testMove;
    bool& testFire = m_testFire;
    bool& closeupCamera = m_closeupCamera;
    bool& inspectFaceCamera = m_inspectFaceCamera;
    bool& spawnFrontline = m_spawnFrontline;
    bool& testGore = m_testGore;
    bool& testSeverPole = m_testSeverPole;
    std::string& autoCaptureFile = m_autoCaptureFile;
    int& autoCaptureFrames = m_autoCaptureFrames;
    float& customPitch = m_customPitch;
    float& customZoom = m_customZoom;
    float& customYaw = m_customYaw;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--ingame") {
            startInGame = true;
        } else if (arg == "--closeup") {
            closeupCamera = true;
        } else if (arg == "--inspect-face") {
            closeupCamera = true;
            inspectFaceCamera = true;
        } else if (arg == "--pitch" && i + 1 < argc) {
            customPitch = std::stof(argv[++i]);
        } else if (arg == "--zoom" && i + 1 < argc) {
            customZoom = std::stof(argv[++i]);
        } else if (arg == "--yaw" && i + 1 < argc) {
            customYaw = std::stof(argv[++i]);
        } else if (arg == "--tab" && i + 1 < argc) {
            std::string t = argv[++i];
            ui.setGameState(GameState::MainMenu);
            if (t == "customize") {
                ui.setMenuTab(MainMenuTab::Customize);
                float z = 1.30f;
                camera.setZoom(z);
                camera.setPitch(0.0f);
                camera.setYaw(0.0f);
                camera.snapToTarget(player.getPosition() + glm::vec3(-0.42f, 1.58f, 0.0f));
            } else if (t == "uniform") {
                ui.setMenuTab(MainMenuTab::Customize);
                ui.setCustomizeTab(1);
                ui.setCustomizeFaceZoom(false);
                float z = 3.10f;
                camera.setZoom(z);
                camera.setPitch(2.0f);
                camera.setYaw(0.0f);
                camera.snapToTarget(player.getPosition() + glm::vec3(-0.48f, 1.12f, 0.0f));
            } else if (t == "stash" || t == "loadout" || t == "armory" || t == "deploy" || t == "deployment") {
                ui.setMenuTab(MainMenuTab::Deployment);
            } else if (t == "overview") {
                ui.setMenuTab(MainMenuTab::Overview);
            } else if (t == "traders" || t == "shop" || t == "vendor") {
                ui.setMenuTab(MainMenuTab::Deployment);
            } else if (t == "exchange" || t == "market" || t == "gold") {
                ui.setMenuTab(MainMenuTab::ExchangeMarket);
            } else if (t == "map" || t == "warzone") {
                ui.setMenuTab(MainMenuTab::TacticalMap);
            
            }
        } else if (arg == "--capture" && i + 1 < argc) {
            autoCaptureFile = argv[++i];
            if (autoCaptureFrames < 0) autoCaptureFrames = 35;
        } else if (arg == "--frames" && i + 1 < argc) {
            autoCaptureFrames = std::stoi(argv[++i]);
        } else if (arg == "--map" && i + 1 < argc) {
            cfgActiveMap = argv[++i];
            if (cfgActiveMap == "training_base.vmap") {
                ui.setTrainingMode(true);
            }
        } else if (arg == "--training") {
            cfgActiveMap = "training_base.vmap";
            ui.setTrainingMode(true);
            startInGame = true;
        } else if (arg == "--test-gore") {
            startInGame = true;
            testGore = true;
        } else if (arg == "--frontline") {
            startInGame = true;
            spawnFrontline = true;
        } else if (arg == "--building") {
            startInGame = true;
            spawnFrontline = false;
        } else if (arg == "--inventory") {
            startInGame = true;
            ui.toggleInventory();
        } else if (arg == "--wheel") {
            startInGame = true;
            ui.setWeaponWheelOpen(true);
        } else if (arg == "--alert-wheel") {
            startInGame = true;
            ui.setAlertWheelOpen(true);
        } else if (arg == "--settings" || arg == "--options") {
            ui.setGameState(GameState::MainMenu);
            ui.toggleOptions();
        } else if (arg == "--display") {
            ui.setGameState(GameState::MainMenu);
            ui.toggleOptions();
            ui.setSettingsTab(0);
        } else if (arg == "--graphics") {
            ui.setGameState(GameState::MainMenu);
            ui.toggleOptions();
            ui.setSettingsTab(1);
        } else if (arg == "--sound" || arg == "--voip") {
            ui.setGameState(GameState::MainMenu);
            ui.toggleOptions();
            ui.setSettingsTab(2);
            VoiceChatManager::get().setMicTestLoopback(true);
        } else if (arg == "--controls") {
            ui.setGameState(GameState::MainMenu);
            ui.toggleOptions();
            ui.setSettingsTab(3);
        } else if (arg == "--binds") {
            ui.setGameState(GameState::MainMenu);
            ui.toggleOptions();
            ui.setSettingsTab(4);
        } else if (arg == "--charselect") {
            ui.setGameState(GameState::CharacterSelect);
        } else if (arg == "--recruit") {
            ui.setGameState(GameState::CharacterSelect);
            ui.openRecruitModal(1);
        } else if (arg == "--briefing" || arg == "--deploying") {
            ui.setGameState(GameState::MainMenu);
            ui.setTrainingMode(true);
            ui.setDeploying(true);
        } else if (arg == "--server-offline") {
            ui.setGameState(GameState::MainMenu);
            ui.setServerOfflineModalOpen(true, "Dedicated server shard (Port 27015) unreachable. Connection timed out.");
        } else if (arg == "--base" || arg == "--training" || arg == "--patrol" || arg == "--range" || arg == "--arsenal" || arg == "--shoothouse") {
            startInGame = true;
            ui.setTrainingMode(true);
        } else if (arg == "--training-esc") {
            startInGame = true;
            ui.setTrainingMode(true);
            ui.setEscapeMenuOpen(true);
        } else if (arg == "--crouch") {
            startInGame = true;
            testCrouch = true;
            player.getStats().stance = PlayerStance::Crouching;
        } else if (arg == "--prone") {
            startInGame = true;
            player.getStats().stance = PlayerStance::Prone;
        } else if (arg == "--fire") {
            startInGame = true;
            testAim = true;
            testFire = true;
        } else if (arg == "--aim") {
            startInGame = true;
            testAim = true;
        } else if (arg == "--sprint") {
            startInGame = true;
            player.getStats().isSprinting = true;
        } else if (arg == "--move" || arg == "--run") {
            startInGame = true;
            testMove = true;
        } else if (arg == "--shoulder-left" || arg == "--left-shoulder") {
            startInGame = true;
            camera.setShoulderRight(false);
            ui.setCameraPreset(1);
        } else if (arg == "--turret") {
            startInGame = true;
        } else if (arg == "--test-remote" || arg == "--remote") {
            startInGame = true;
            testRemote = true;
        } else if (arg == "--test-sever" || arg == "--sever") {
            startInGame = true;
            ui.setTrainingMode(true);
            testSeverPole = true;
            camera.snapToTarget(glm::vec3(-4.2f, 1.8f, 12.0f));
            camera.setYaw(15.0f);
            camera.setPitch(10.0f);
            camera.setZoom(5.5f);
                } else if (arg == "--chat") {
            startInGame = true;
            ui.setTrainingMode(true);
            testChat = true;
        } else if (arg == "--voice") {
            startInGame = true;
            ui.setTrainingMode(true);
            testVoice = true;
        } else if (arg == "--extract" || arg == "--extract-instant") {
            startInGame = true;
        }
    }

    if (!ProfileManager::get().getActiveProfile()) {
        for (int s = 0; s < 3; ++s) {
            if (ProfileManager::get().getSlot(s).created) {
                ProfileManager::get().setActiveSlotIndex(s);
                break;
            }
        }
        if (!ProfileManager::get().getActiveProfile()) {
            std::string defaultName = SteamIntegration::get().getPersonaName().empty() ? "Mehdi" : SteamIntegration::get().getPersonaName();
            ProfileManager::get().createCharacter(0, defaultName, Faction::FactionA);
            ProfileManager::get().setActiveSlotIndex(0);
        }
    }

    CharacterProfile* activeProf = ProfileManager::get().getActiveProfile();
    if (activeProf) {
        player.setFaction(activeProf->faction);
        if (!activeProf->savedInventory.getItems().empty() ||
            activeProf->savedInventory.getEquipped(EquipSlot::PrimaryWeapon).has_value() ||
            activeProf->savedInventory.getEquipped(EquipSlot::SecondaryWeapon).has_value()) {
            player.getInventory() = activeProf->savedInventory;
        }
        player.updateArmorFromEquipped();
    }

    if (cfgActiveMap == "training_base.vmap") {
        ui.setTrainingMode(true);
    }

    if (startInGame) {
        ui.setGameState(GameState::InGame);
        if (ui.isTrainingMode()) {
            world.generateMilitaryBase();
            renderer.rebuildTerrainMesh(true);
            CharacterProfile* pAct = ProfileManager::get().getActiveProfile();
            Faction pFaction = pAct ? pAct->faction : Faction::FactionA;
            player.setFaction(pFaction);
            frontline.initTrainingGarrison(world, pFaction);
            missions.resetContract(ProfileManager::get().getActiveProfile() ? ProfileManager::get().getActiveProfile()->faction : Faction::FactionA, true);
            float startY = World::sampleTerrainHeight(0.0f, 2.0f);
            player.setPosition(glm::vec3(0.0f, startY, 2.0f));
            player.setYaw(0.0f);
        } else {
            world.generateTeamDeathmatch();
            renderer.rebuildTerrainMesh(true);
            CharacterProfile* pAct = ProfileManager::get().getActiveProfile();
            Faction pFaction = pAct ? pAct->faction : Faction::FactionA;
            player.setFaction(pFaction);
            frontline.initTeamDeathmatch(world, pFaction, ui.isTdmAgainstAi());
            if (pFaction == Faction::FactionA) {
                player.setPosition(glm::vec3(0.0f, 0.0f, -26.0f));
                player.setYaw(0.0f);
            } else {
                player.setPosition(glm::vec3(0.0f, 0.0f, 26.0f));
                player.setYaw(3.14159f);
            }
        }
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--range") {
                player.setPosition(glm::vec3(20.0f, 0.0f, 0.0f));
                player.setYaw(1.5707963f);
                customYaw = -90.0f;
                customPitch = 16.0f;
                customZoom = 24.0f;
            } else if (a == "--arsenal") {
                player.setPosition(glm::vec3(-42.0f, 0.0f, 0.0f));
                player.setYaw(-1.5707963f);
                customYaw = 65.0f;
                customPitch = 22.0f;
                customZoom = 15.0f;
            } else if (a == "--patrol") {
                player.setPosition(glm::vec3(0.0f, 0.0f, -34.0f));
                player.setYaw(3.14159f);
                customYaw = 0.0f;
                customPitch = 20.0f;
                customZoom = 18.0f;
            } else if (a == "--shoothouse") {
                player.setPosition(glm::vec3(0.0f, 0.0f, -74.0f));
                player.setYaw(3.14159f);
                customYaw = 0.0f;
                customPitch = 32.0f;
                customZoom = 24.0f;
            } else if (std::string(argv[i]) == "--building") {
                player.setPosition(glm::vec3(-12.0f, 0.0f, 35.0f));
                player.setYaw(3.14159f);
                customPitch = 6.0f;
                customYaw = 180.0f;
                customZoom = 12.0f;
            } else if (std::string(argv[i]) == "--pistol") {
                player.switchWeapon(EquipSlot::SecondaryWeapon);
                camera.setZoom(5.5f);
            } else if (std::string(argv[i]) == "--sandbag") {
                player.setPosition(glm::vec3(-2.8f, World::sampleTerrainHeight(-2.8f, -2.1f), -2.1f));
                player.setYaw(0.0f);
                camera.setZoom(8.5f);
            } else if (std::string(argv[i]) == "--drop") {
                if (!player.getInventory().getEquipped(EquipSlot::PrimaryWeapon).has_value()) {
                    player.getInventory().getEquipped(EquipSlot::PrimaryWeapon) = Inventory::createM4A1();
                }
                player.dropCurrentWeapon(world);
            } else if (std::string(argv[i]) == "--turret") {
                MountedTurret* nearTurret = world.getNearbyTurret(glm::vec3(0.0f, 0.0f, -52.0f), 10.0f);
                if (nearTurret) {
                    player.mountTurret(nearTurret);
                }
                camera.setZoom(6.5f);
            } else if (std::string(argv[i]) == "--extract") {
                player.setPosition(glm::vec3(35.0f, World::sampleTerrainHeight(35.0f, -225.0f), -225.0f));
                camera.setZoom(10.0f);
            } else if (std::string(argv[i]) == "--extract-instant") {
                player.setPosition(glm::vec3(35.0f, World::sampleTerrainHeight(35.0f, -225.0f), -225.0f));
                ExtractionZone* ez = world.getActiveExtractZone(player.getPosition());
                if (ez) ez->countdown = 0.05f;
                camera.setZoom(10.0f);
            } else if (std::string(argv[i]) == "--test-sever" || std::string(argv[i]) == "--sever") {
                player.setPosition(glm::vec3(-4.2f, 0.0f, 8.2f));
                player.setYaw(0.0f);
                customPitch = 12.0f;
                customYaw = -165.0f;
                customZoom = 6.2f;
            }
        }
        if (startInGame) {
            camera.resetToThirdPerson();
            if (customPitch > -90.0f) camera.setPitch(customPitch);
            if (customZoom > 0.0f) { camera.setZoom(customZoom); customZoom = -1.0f; }
            if (customYaw > -999.0f) camera.setYaw(customYaw);
            camera.snapToTarget(player.getPosition());
        } else {
            camera.resetToOrbit();
            camera.snapToTarget(player.getPosition() + glm::vec3(0.0f, 1.00f, 0.0f));
        }
    }

    double lastTime = glfwGetTime();
    bool tabPressedLast = false;
    bool pPressedLast = false;

        VoiceChatManager::get().init();
    ChatSystem::get().init();
    if (testChat) {
        ChatSystem::get().addMessage(ChatChannel::Proximity, 101, "GHOST_LEAD", "Holding northern corridor. Enemy squad advancing across Sector Baker.", player.getPosition() + glm::vec3(-12.0f, 0.0f, -4.0f), 12.0f, 0, false);
        ChatSystem::get().addMessage(ChatChannel::Team, 901, "SSG REEVES", "Munitions bunker fully restocked. Grab extra 5.45 magazines before pushing.", glm::vec3(0.0f, 0.0f, -15.0f), 15.0f, 0, false);
        ChatSystem::get().addMessage(ChatChannel::Global, 102, "VIPER_02", "Heavy mortar barrage incoming near central bridge.", glm::vec3(45.0f, 0.0f, -120.0f), 125.0f, 1, false);
        ChatSystem::get().addMessage(ChatChannel::Proximity, 902, "SGT BISHOP", "Firing lanes 1-5 reset. Range is hot.", glm::vec3(10.0f, 0.0f, -14.0f), 14.0f, 0, false);
        ChatSystem::get().openChat();
    }
    if (testVoice) {
        VoiceChatManager::get().setProximityPtt(true);
        VoiceChatManager::get().triggerSimulatedNpcVoice(901, "SSG REEVES", glm::vec3(0.0f, 0.0f, -15.0f), 6.0f);
    }
    std::cout << "[Mercenary Engine] Game loop active. Profile system ready.\n";

    m_isRunning = true;
    return true;
}

void GameApp::run() {
    if (!m_isRunning) return;
    Window& window = m_window;
    Renderer& renderer = m_renderer;
    UIManager& ui = m_ui;
    World& world = m_world;
    Player& player = m_player;
    FrontlineWarfare& frontline = m_frontline;
    MissionSystem& missions = m_missions;
    BallisticsSystem& ballistics = m_ballistics;
    Camera& camera = m_camera;
    ShadowMap& shadowMap = *m_shadowMap;
    PostProcess& postProcess = m_postProcess;
    double lastTime = glfwGetTime();

    bool& startInGame = m_startInGame;
    bool& testAim = m_testAim;
    bool& testRemote = m_testRemote;
    bool& testChat = m_testChat;
    bool& testVoice = m_testVoice;
    bool& testCrouch = m_testCrouch;
    bool& testMove = m_testMove;
    bool& testFire = m_testFire;
    bool& closeupCamera = m_closeupCamera;
    bool& inspectFaceCamera = m_inspectFaceCamera;
    bool& spawnFrontline = m_spawnFrontline;
    bool& testGore = m_testGore;
    bool& testSeverPole = m_testSeverPole;
    std::string& autoCaptureFile = m_autoCaptureFile;
    int& autoCaptureFrames = m_autoCaptureFrames;
    float& customPitch = m_customPitch;
    float& customZoom = m_customZoom;
    float& customYaw = m_customYaw;

    while (!window.shouldClose()) {
        double currentTime = glfwGetTime();
        float dt = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        if (dt > 0.1f) dt = 0.1f; // Cap delta time during hitches

        window.pollEvents();
        SteamIntegration::get().update();

        // Hotkeys: TAB tap (< 0.20s) toggles Inventory; Hold (>= 0.20s) opens GTA 5 Radial Weapon Wheel
        static float s_tabHoldTime = 0.0f;
        static bool s_tabWasDown = false;
        static bool s_wheelActivated = false;

        bool tabIsDown = window.isKeyPressed(GLFW_KEY_TAB);
        bool iIsDown = window.isKeyPressed(GLFW_KEY_I);
        static bool iWasDown = false;
        // Dedicated 'I' key toggles inventory immediately
        if (iIsDown && !iWasDown) {
            ui.toggleInventory();
        }
        iWasDown = iIsDown;

        if (ui.getGameState() == GameState::InGame) {
            if (frontline.isTdmMode()) {
                ui.setTdmScoreboardOpen(tabIsDown);
            }
            if (tabIsDown) {
                s_tabHoldTime += dt;
                s_tabWasDown = true;
                // If held for >= 0.20s, activate GTA 5 style radial weapon wheel!
                if (s_tabHoldTime >= 0.20f && !s_wheelActivated) {
                    s_wheelActivated = true;
                    if (ui.isInventoryOpen()) {
                        ui.toggleInventory();
                    }
                    ui.setWeaponWheelOpen(true);
                }
            } else {
                if (s_tabWasDown) {
                    if (s_wheelActivated) {
                        // Released after holding: equip selected sector and dismiss weapon wheel
                        ui.setWeaponWheelOpen(false, &player, &world);
                        s_wheelActivated = false;
                    } else if (s_tabHoldTime < 0.20f) {
                        // Quick short tap (< 0.20s): toggle tactical inventory
                        ui.toggleInventory();
                    }
                    s_tabHoldTime = 0.0f;
                    s_tabWasDown = false;
                }
            }
        } else {
            // Outside of active raid (Lobby / Hideout / Customizer)
            if (tabIsDown && !s_tabWasDown) {
                ui.toggleInventory();
            }
            s_tabWasDown = tabIsDown;
            s_tabHoldTime = 0.0f;
            s_wheelActivated = false;
        }

        static bool mPressedLast = false;
        bool mPressed = window.isKeyPressed(GLFW_KEY_M);
        if (mPressed && !mPressedLast) {
            ui.toggleMap();
        }
        mPressedLast = mPressed;

        static bool bPressedLast = false;
        bool bPressed = window.isKeyPressed(GLFW_KEY_B);
        if (bPressed && !bPressedLast && ui.getGameState() == GameState::InGame) {
            ui.toggleConstructionMenu();
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        bPressedLast = bPressed;

        // F11 or Alt+Enter: Seamless Fullscreen / Windowed Toggle
        static bool f11PressedLast = false;
        bool altEnter = (window.isKeyPressed(GLFW_KEY_LEFT_ALT) || window.isKeyPressed(GLFW_KEY_RIGHT_ALT)) && window.isKeyPressed(GLFW_KEY_ENTER);
        bool f11Pressed = window.isKeyPressed(GLFW_KEY_F11) || altEnter;
        if (f11Pressed && !f11PressedLast) {
            window.toggleFullscreen();
            ui.setWindowMode(window.getDisplayMode() == WindowDisplayMode::ExclusiveFullscreen ? 1 : 0);
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        f11PressedLast = f11Pressed;

        // Enter Key: Toggle Tactical Text Chat
        static bool enterPressedLast = false;
        static bool yPressedLast = false;
        static bool uPressedLast = false;
        bool enterPressed = window.isKeyPressed(GLFW_KEY_ENTER);
        bool yPressed = window.isKeyPressed(GLFW_KEY_Y);
        bool uPressed = window.isKeyPressed(GLFW_KEY_U);
        bool chatAlreadyOpen = ChatSystem::get().isChatOpen();
        if (!altEnter && ui.getGameState() == GameState::InGame && !ui.isOptionsOpen() && !ui.isEscapeMenuOpen() && !ui.isInventoryOpen()) {
            if (!chatAlreadyOpen) {
                if (enterPressed && !enterPressedLast) {
                    ChatSystem::get().openChat();
                } else if (yPressed && !yPressedLast) {
                    ChatSystem::get().setActiveChannel(ChatChannel::Global);
                    ChatSystem::get().openChat();
                } else if (uPressed && !uPressedLast) {
                    ChatSystem::get().setActiveChannel(ChatChannel::Team);
                    ChatSystem::get().openChat();
                }
            }
        }
        enterPressedLast = enterPressed;
        yPressedLast = yPressed;
        uPressedLast = uPressed;


        // ESC Key: In-game pause modal / menu navigation
        static bool escPressedLast = false;
        bool escPressed = window.isKeyPressed(GLFW_KEY_ESCAPE);
        if (escPressed && !escPressedLast) {
            if (ui.getGameState() == GameState::InGame) {
                if (ChatSystem::get().isChatOpen()) {
                    ChatSystem::get().closeChat();
                } else if (ui.isWeaponWheelOpen()) {
                    ui.setWeaponWheelOpen(false);
                } else if (ui.isOptionsOpen()) {
                    ui.toggleOptions();
                } else if (ui.isMapOpen()) {
                    ui.toggleMap();
                } else if (ui.isInventoryOpen()) {
                    ui.toggleInventory();
                } else {
                    ui.toggleEscapeMenu();
                }
            } else if (ui.getGameState() == GameState::MainMenu) {
                if (ui.getMenuTab() != MainMenuTab::Overview) {
                    ui.setMenuTab(MainMenuTab::Overview);
                } else {
                    ui.setGameState(GameState::CharacterSelect);
                }
            }
        }
        escPressedLast = escPressed;

        // Active profile
        CharacterProfile* activeProfile = ProfileManager::get().getActiveProfile();
        Faction playerFaction = activeProfile ? activeProfile->faction : Faction::FactionA;
        player.setFaction(playerFaction);

        // Dynamic Real-time Gold Commodity & Crypto Exchange Update
        ExchangeMarket::get().update(dt);

        glm::vec2 mousePos = window.getMousePos();
        glm::vec3 mouseWorldPos = camera.screenToWorldGround(mousePos, window.getWidth(), window.getHeight(), 0.0f);

        // Camera Update
        static GameState s_lastCamGameState = GameState::CharacterSelect;
        if (ui.getGameState() == GameState::InGame && s_lastCamGameState != GameState::InGame) {
            if (customZoom <= 0.0f) camera.setZoom(2.4f);
            else { camera.setZoom(customZoom); customZoom = -1.0f; }
            camera.resetToThirdPerson();
            camera.snapToTarget(player.getPosition());
            renderer.rebuildTerrainMesh(world.getMapType() == MapType::TrainingBase);
            if (world.getMapType() == MapType::TrainingBase || ui.isTrainingMode()) {
                frontline.initTrainingGarrison(world, playerFaction);
            }
        } else if (ui.getGameState() != GameState::InGame && s_lastCamGameState == GameState::InGame) {
            camera.setMode(CameraMode::Orbit);
            camera.setZoom(2.85f);
            camera.setPitch(5.0f);
            camera.setYaw(0.0f);
        }
        s_lastCamGameState = ui.getGameState();

        if (ui.getGameState() == GameState::CharacterSelect || ui.getGameState() == GameState::MainMenu) {
            camera.setMode(CameraMode::Orbit);
            bool inCustomize = (ui.getMenuTab() == MainMenuTab::Customize);

            static MainMenuTab s_lastMenuTab = MainMenuTab::Overview;
            if (ui.getMenuTab() != s_lastMenuTab) {
                if (ui.getMenuTab() == MainMenuTab::Customize) {
                    float z = ui.isCustomizeFaceZoom() ? 1.25f : 2.80f;
                    float p = ui.isCustomizeFaceZoom() ? 0.0f : 2.0f;
                    float y = ui.isCustomizeFaceZoom() ? 1.55f : 1.00f;
                    float x = -0.35f;
                    camera.setZoom(z);
                    camera.setPitch(p);
                    camera.setYaw(0.0f);
                    camera.snapToTarget(player.getPosition() + glm::vec3(x, y, 0.0f));
                }
                s_lastMenuTab = ui.getMenuTab();
            }

            // Interactive 3D viewport mouse drag & zoom to orbit operator (MMO Armory Turntable)
            static glm::dvec2 s_lastLobbyMouse(0.0);
            static bool s_isLobbyDragging = false;
            static float s_lobbyZoom = 2.85f;
            static float s_lobbyPitch = 5.0f;

            if (!ImGui::GetIO().WantCaptureMouse) {
                // Mouse wheel zoom in / out
                float scroll = window.getScrollDelta();
                if (std::abs(scroll) > 0.01f) {
                    float factor = ui.isInvertScroll() ? -0.25f : 0.25f;
                    s_lobbyZoom = std::clamp(s_lobbyZoom - scroll * factor, 1.20f, 4.20f);
                }

                // Left or Right click drag to rotate operative 360 degrees & tilt pitch
                if (window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT) || window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
                    glm::dvec2 curM = window.getMousePos();
                    if (s_isLobbyDragging) {
                        float dx = (float)(curM.x - s_lastLobbyMouse.x);
                        float dy = (float)(curM.y - s_lastLobbyMouse.y);
                        ui.addOperatorTurntable(dx * 0.016f);
                        s_lobbyPitch = std::clamp(s_lobbyPitch + dy * 0.12f, -8.0f, 20.0f);
                    }
                    s_lastLobbyMouse = curM;
                    s_isLobbyDragging = true;
                } else {
                    s_isLobbyDragging = false;
                }
            } else {
                s_isLobbyDragging = false;
            }

            player.setLobbyTurntableOffset(ui.getOperatorTurntable());

            if (inCustomize) {
                camera.setTargetYaw(0.0f);
                if (ui.isCustomizeFaceZoom()) {
                    camera.setTargetZoom(1.25f);
                    camera.setTargetPitch(0.0f);
                    glm::vec3 hideoutTarget = player.getPosition() + glm::vec3(-0.35f, 1.55f, 0.0f);
                    camera.update(dt, hideoutTarget, glm::vec2(window.getWidth() * 0.5f, window.getHeight() * 0.5f), window.getWidth(), window.getHeight(), false);
                } else {
                    camera.setTargetZoom(2.80f);
                    camera.setTargetPitch(2.0f);
                    glm::vec3 hideoutTarget = player.getPosition() + glm::vec3(-0.35f, 1.00f, 0.0f);
                    camera.update(dt, hideoutTarget, glm::vec2(window.getWidth() * 0.5f, window.getHeight() * 0.5f), window.getWidth(), window.getHeight(), false);
                }
            } else if (ui.getGameState() == GameState::CharacterSelect) {
                camera.setTargetZoom(2.85f);
                camera.setTargetPitch(s_lobbyPitch);
                camera.setTargetYaw(0.0f);
                glm::vec3 hideoutTarget = player.getPosition() + glm::vec3(0.0f, 1.00f, 0.0f);
                camera.update(dt, hideoutTarget, glm::vec2(window.getWidth() * 0.5f, window.getHeight() * 0.5f), window.getWidth(), window.getHeight(), false);
            } else {
                camera.setTargetZoom(s_lobbyZoom);
                camera.setTargetPitch(s_lobbyPitch);
                camera.setTargetYaw(0.0f);
                glm::vec3 hideoutTarget = player.getPosition() + glm::vec3(-0.35f, 1.00f, 0.0f);
                camera.update(dt, hideoutTarget, glm::vec2(window.getWidth() * 0.5f, window.getHeight() * 0.5f), window.getWidth(), window.getHeight(), false);
            }
        } else if (closeupCamera) {
            camera.setMode(CameraMode::Orbit);
            if (inspectFaceCamera) {
                camera.setZoom(1.5f);
                camera.setPitch(4.0f);
                camera.setYaw(player.getYaw() * (180.0f / 3.14159f) + 180.0f); // Look directly into face
                camera.snapToTarget(player.getPosition() + glm::vec3(0.0f, 1.55f, 0.0f));
                camera.update(dt, player.getPosition() + glm::vec3(0.0f, 1.55f, 0.0f), glm::vec2(window.getWidth() * 0.5f, window.getHeight() * 0.5f), window.getWidth(), window.getHeight(), false);
            } else {
                camera.setZoom(2.2f);
                camera.setPitch(10.0f);
                camera.setYaw(player.getYaw() * (180.0f / 3.14159f) - 30.0f);
                camera.snapToTarget(player.getPosition() + glm::vec3(0.0f, 1.05f, 0.0f));
                camera.update(dt, player.getPosition() + glm::vec3(0.0f, 1.05f, 0.0f), glm::vec2(window.getWidth() * 0.5f, window.getHeight() * 0.5f), window.getWidth(), window.getHeight(), false);
            }
        } else {
            camera.setMode(CameraMode::ThirdPerson);
            // =========================================================================
            // THIRD-PERSON OVER-THE-SHOULDER TACTICAL CONTROLS & CAMERA
            // =========================================================================
            bool inMenu = ui.isOptionsOpen() || ui.isEscapeMenuOpen() || ui.isInventoryOpen() || 
                          ui.isMapOpen() || ui.isWeaponWheelOpen() || ChatSystem::get().isChatOpen() ||
                          (ui.getGameState() != GameState::InGame);

            if (inMenu || ui.isAlertWheelOpen() || ui.isWeaponWheelOpen()) {
                window.setCursorCaptured(false);
            } else {
                window.setCursorCaptured(true);
            }

            if (window.isCursorCaptured()) {
                glm::vec2 mDelta = window.getMouseDelta();
                float lookSens = player.isAiming() ? (0.12f * ui.getAdsSensitivity()) : (0.18f * ui.getMouseSensitivity());
                float pitchFactor = ui.isInvertY() ? -1.0f : 1.0f;

                camera.rotateYaw(-mDelta.x * lookSens);
                camera.rotatePitch(mDelta.y * pitchFactor * lookSens);

                // Tactical Shoulder Swap (Dedicated Rebound Key, e.g. T / Alt)
                static bool s_shoulderKeyLast = false;
                int shoulderKey = InputConfig::get().getKey(GameAction::ShoulderSwap);
                bool curShoulderKey = (shoulderKey != GLFW_KEY_UNKNOWN && window.isKeyPressed(shoulderKey));
                if (curShoulderKey && !s_shoulderKeyLast) {
                    camera.toggleShoulder();
                    ui.setCameraPreset(camera.isShoulderRight() ? 0 : 1);
                    AudioEngine::get().playSound(SoundType::TacticalClick, 0.45f, 1.25f);
                }
                s_shoulderKeyLast = curShoulderKey;

                // Mouse Scroll: Adjust third-person follow distance
                camera.handleScroll(window.getScrollDelta(), ui.isInvertScroll());
            }

            // Home key: Reset to default third-person over-the-shoulder angle
            static bool s_lastHome = false;
            bool curHome = window.isKeyPressed(GLFW_KEY_HOME);
            if (curHome && !s_lastHome) {
                camera.resetToThirdPerson();
                camera.snapToTarget(player.getPosition());
            }
            s_lastHome = curHome;

            camera.setFOV(78.0f); // Fixed standard tactical TPS field of view
            static int s_lastAppliedCameraPreset = -1;
            int curCamPreset = ui.getCameraPreset();
            if (curCamPreset != s_lastAppliedCameraPreset) {
                if (curCamPreset == 1) camera.setShoulderRight(false);
                else if (curCamPreset == 0) camera.setShoulderRight(true);
                s_lastAppliedCameraPreset = curCamPreset;
            }

            glm::vec2 effMouse(window.getWidth() * 0.5f, window.getHeight() * 0.5f);
            float strafeSpeed = 0.0f;
            if (window.isCursorCaptured()) {
                float rad = glm::radians(camera.getYaw());
                glm::vec3 rightDir(std::cos(rad), 0.0f, -std::sin(rad));
                strafeSpeed = glm::dot(player.getVelocity(), rightDir);
            }
            camera.update(dt, player.getPosition(), effMouse, window.getWidth(), window.getHeight(), 
                          player.isAiming(), player.getAdsZoomLevel(), player.isSprinting(), glm::length(player.getVelocity()), &world,
                          player.getEyeHeight(), strafeSpeed);
        }

        // Apply Real-time Graphics & Atmosphere Toggles
        postProcess.setSSAOEnabled(ui.isAmbientOcclusionEnabled());
        postProcess.setMotionBlurEnabled(ui.isMotionBlurEnabled());
        world.setWeatherParticlesEnabled(ui.isWeatherParticlesEnabled());

        // Step Jolt Physics simulation (Multi-threaded Job System & Rigid Bodies)
        PhysicsEngine::get().update(dt);

        // Update World & Dynamic Day/Night & Weather
        world.update(dt, renderer, camera.getPosition());

        // Update Voice Comms & Chat System
        CharacterProfile* activeProf = ProfileManager::get().getActiveProfile();
        VoiceChatManager::get().setLocalPlayerContext(
            NetworkEngine::get().getLocalPlayerId(),
            activeProf ? activeProf->callsign : "OPERATIVE",
            player.getPosition(),
            activeProf ? (activeProf->faction == Faction::FactionB ? 1 : 0) : 0
        );

        if (ui.getGameState() == GameState::InGame && !ChatSystem::get().isChatOpen() && !ui.isOptionsOpen() && !ui.isEscapeMenuOpen()) {
            InputConfig& inCfg = InputConfig::get();
            int proxKey = inCfg.getKey(GameAction::VoiceProximity);
            int squadKey = inCfg.getKey(GameAction::VoiceSquad);
            bool proxActive = (proxKey != GLFW_KEY_UNKNOWN) ? window.isKeyPressed(proxKey) : false;
            bool squadActive = (squadKey != GLFW_KEY_UNKNOWN) ? window.isKeyPressed(squadKey) : false;

            VoiceChatManager::get().setProximityPtt(proxActive);
            VoiceChatManager::get().setSquadPtt(squadActive);
        } else {
            VoiceChatManager::get().setProximityPtt(false);
            VoiceChatManager::get().setSquadPtt(false);
        }
        VoiceChatManager::get().update(dt, player.getPosition(), glm::radians(camera.getYaw()));
        ChatSystem::get().update(dt);

        // Update Debug Audit Engine

        // Gameplay Updates
        if (ui.getGameState() == GameState::InGame) {
            if (testRemote) {
                auto& remotes = NetworkEngine::get().getRemotePlayers();
                // 1. Standing Remote Operative with M4A1
                {
                    auto& r1 = remotes[101];
                    r1.active = true;
                    r1.currentState.playerId = 101;
                    r1.currentState.position = player.getPosition() + glm::vec3(-2.8f, 0.0f, -0.4f);
                    r1.currentState.yaw = player.getYaw() + 0.35f;
                    r1.currentState.stance = 0; // Standing
                    r1.currentState.weaponId = 1; // M4A1
                    r1.currentState.isAiming = 1;
                    r1.currentState.faction = 0; // Blue
                    r1.currentState.helmetId = 0;
                    r1.currentState.vestId = 0;
                    std::strcpy(r1.currentState.callsign, "GHOST_LEAD");
                }
                // 2. Crouching Remote Operative with AK-74M
                {
                    auto& r2 = remotes[102];
                    r2.active = true;
                    r2.currentState.playerId = 102;
                    r2.currentState.position = player.getPosition() + glm::vec3(2.8f, 0.0f, -0.4f);
                    r2.currentState.yaw = player.getYaw() - 0.35f;
                    r2.currentState.stance = 1; // Crouching
                    r2.currentState.weaponId = 2; // AK-74M
                    r2.currentState.isAiming = 0;
                    r2.currentState.faction = 1; // Red
                    r2.currentState.helmetId = 1;
                    r2.currentState.vestId = 1;
                    std::strcpy(r2.currentState.callsign, "VIPER_02");
                }
                // 3. Prone Remote Operative with Mosin-Nagant
                {
                    auto& r3 = remotes[103];
                    r3.active = true;
                    r3.currentState.playerId = 103;
                    r3.currentState.position = player.getPosition() + glm::vec3(0.0f, 0.0f, -2.2f);
                    r3.currentState.yaw = player.getYaw();
                    r3.currentState.stance = 2; // Prone
                    r3.currentState.weaponId = 3; // Mosin
                    r3.currentState.isAiming = 1;
                    r3.currentState.faction = 0; // Blue
                    r3.currentState.helmetId = 2;
                    r3.currentState.vestId = 2;
                    std::strcpy(r3.currentState.callsign, "HAWK_SNIPER");
                }
                // 4. Athletic Sprinting Remote Operative with M4A1 (Tactical Carry)
                {
                    auto& r4 = remotes[104];
                    r4.active = true;
                    r4.currentState.playerId = 104;
                    r4.currentState.position = player.getPosition() + glm::vec3(-4.5f, 0.0f, -0.6f);
                    r4.currentState.velocity = glm::vec3(0.0f, 0.0f, -3.6f);
                    r4.currentState.yaw = 3.14159f;
                    r4.currentState.stance = 0;
                    r4.currentState.isSprinting = 1;
                    r4.currentState.weaponId = 1;
                    r4.currentState.isAiming = 0;
                    r4.currentState.faction = 0;
                    r4.currentState.helmetId = 0;
                    r4.currentState.vestId = 0;
                    std::strcpy(r4.currentState.callsign, "STALKER_04");
                }

                // (Remote gunfire test injection removed to prevent phantom bullet damage)
            }
            // Authentic GTA 5 weapon wheel slow-motion time dilation
            float gameDt = ui.isWeaponWheelOpen() ? (dt * 0.18f) : dt;

            // Check player casualty
            if (player.getStats().health <= 0.0f) {
                if (frontline.isTdmMode()) {
                    if (ui.getTdmRespawnTimer() <= 0.0f) {
                        ui.setTdmRespawnTimer(4.0f);
                        Faction enemyFac = (player.getFaction() == Faction::FactionA) ? Faction::FactionB : Faction::FactionA;
                        frontline.recordTdmKill(enemyFac);
                        if (!player.getRig().isDead()) {
                            player.getRig().triggerDeath(DeathType::CollapseBackward, glm::vec3(0.0f), true, DISMEMBER_NONE);
                            GoreSystem::get().spawnBloodBurst(player.getPosition() + glm::vec3(0, 1.2f, 0), glm::vec3(0, 1, 0), 45, 1.2f);
                        }
                    }
                } else {
                    if (!player.getRig().isDead()) {
                        player.getRig().triggerDeath(DeathType::CollapseBackward, glm::vec3(0.0f), true, DISMEMBER_NONE);
                        GoreSystem::get().spawnBloodBurst(player.getPosition() + glm::vec3(0, 1.2f, 0), glm::vec3(0, 1, 0), 65, 1.2f);
                        GoreSystem::get().addBloodPool(player.getPosition(), 0.26f, 1.10f, 0.18f);
                    }
                    ui.setGameState(GameState::GameOver);
                    AudioEngine::get().playSound(SoundType::Impact, 1.0f, 0.7f);
                }
            }

            // TDM Player Respawn Countdown & Revive with Loadout Replenishment & Safe Spawning
            if (frontline.isTdmMode() && ui.getTdmRespawnTimer() > 0.0f) {
                float newTimer = ui.getTdmRespawnTimer() - dt;
                ui.setTdmRespawnTimer(newTimer);
                if (newTimer <= 0.0f) {
                    player.replenishTdmLoadout();

                    // Tactical Safe Spawn Selection: evaluate 3 covered spawn points
                    Faction pFaction = player.getFaction();
                    std::vector<glm::vec3> spawnAnchors;
                    if (pFaction == Faction::FactionA) {
                        spawnAnchors = {
                            glm::vec3(-8.0f, 0.0f, -27.0f), // Left container flank
                            glm::vec3( 0.0f, 0.0f, -26.0f), // Center sandbag bunker
                            glm::vec3( 8.0f, 0.0f, -27.0f)  // Right barrier flank
                        };
                    } else {
                        spawnAnchors = {
                            glm::vec3( 8.0f, 0.0f, 27.0f),
                            glm::vec3( 0.0f, 0.0f, 26.0f),
                            glm::vec3(-8.0f, 0.0f, 27.0f)
                        };
                    }

                    // Pick the anchor furthest from any active enemy bot
                    glm::vec3 bestSpawn = spawnAnchors[1];
                    float maxMinDist = -1.0f;
                    for (const auto& anchor : spawnAnchors) {
                        float minDistToEnemy = 999.0f;
                        for (const auto& bot : frontline.getBots()) {
                            if (bot.state != BotState::Dead && bot.faction != pFaction) {
                                float d = glm::distance(anchor, bot.position);
                                if (d < minDistToEnemy) minDistToEnemy = d;
                            }
                        }
                        if (minDistToEnemy > maxMinDist) {
                            maxMinDist = minDistToEnemy;
                            bestSpawn = anchor;
                        }
                    }

                    bestSpawn.y = World::sampleTerrainHeight(bestSpawn.x, bestSpawn.z);
                    player.setPosition(bestSpawn);
                    player.setYaw((pFaction == Faction::FactionA) ? 0.0f : 3.14159265f);
                    camera.snapToTarget(player.getPosition());
                    AudioEngine::get().playSound(SoundType::DeployHorn);
                    ui.showTargetHitBanner("OPERATIVE DEPLOYED // TACTICAL SPAWN SHIELD ACTIVE (4.0s)");
                }
            }

            // Central Camera Ray for Third-Person Aim Reticle
            glm::vec2 screenCenter = glm::vec2(window.getWidth() * 0.5f, window.getHeight() * 0.5f);
            if (!autoCaptureFile.empty() && testRemote) {
                glm::vec3 targetHead = player.getPosition() + glm::vec3(2.8f, 1.35f, -0.4f);
                float aspect = (float)window.getWidth() / (float)window.getHeight();
                glm::vec2 screenPt;
                if (camera.worldToScreen(targetHead, aspect, (float)window.getWidth(), (float)window.getHeight(), screenPt)) {
                    screenCenter = screenPt;
                }
            }

            glm::vec3 rayOrigin, rayDir;
            camera.getRay(screenCenter, window.getWidth(), window.getHeight(), rayOrigin, rayDir);
            mouseWorldPos = rayOrigin + rayDir * 120.0f;

            // Precision 3D Target Micro-Aiming & Anatomical Raycast
            AimTargetInfo aimInfo;
            float closestTargetDist = 120.0f;

            // 1. Raycast vs Frontline AI Bots
            for (const auto& bot : frontline.getBots()) {
                if (bot.state == BotState::Dead) continue;
                float distToPlayer = glm::distance(player.getPosition(), bot.position);
                if (distToPlayer > 85.0f) continue;

                float botH = (bot.state == BotState::WoundedDown) ? 0.40f : (bot.isCrouched ? 1.45f : 1.88f);
                float halfW = (bot.state == BotState::WoundedDown) ? 0.65f : 0.42f;

                // Camera-facing target billboard plane
                glm::vec3 toCam = glm::vec3(rayOrigin.x - bot.position.x, 0.0f, rayOrigin.z - bot.position.z);
                if (glm::length(toCam) < 0.01f) toCam = glm::vec3(0, 0, 1);
                glm::vec3 planeNormal = glm::normalize(toCam);
                glm::vec3 planeRight = glm::normalize(glm::cross(glm::vec3(0, 1, 0), planeNormal));

                float denom = glm::dot(planeNormal, rayDir);
                if (denom < -0.001f) {
                    float t = glm::dot(bot.position - rayOrigin, planeNormal) / denom;
                    if (t > 0.0f && t < closestTargetDist) {
                        glm::vec3 p = rayOrigin + rayDir * t;
                        float localY = p.y - bot.position.y;
                        float localH = glm::dot(p - bot.position, planeRight);

                        if (localY >= -0.08f && localY <= botH + 0.15f && std::abs(localH) <= halfW + 0.12f) {
                            closestTargetDist = t;
                            aimInfo.hasTarget = true;
                            aimInfo.worldPos = p;
                            aimInfo.bodyHeight = localY;
                            aimInfo.bodyHoriz = localH;
                            aimInfo.targetId = (int)bot.id;
                            aimInfo.isObstacle = false;

                            // Precision Headshot Detection
                            float headThreshold = (bot.state == BotState::WoundedDown) ? 0.28f : (bot.isCrouched ? 1.18f : 1.52f);
                            aimInfo.isHeadshot = (localY >= headThreshold && std::abs(localH) <= 0.22f);
                        }
                    }
                }
            }

            // 1b. Raycast vs Shooting Range Reactive Targets
            if (!aimInfo.hasTarget) {
                for (const auto& target : world.getTargets()) {
                    if (!target.isAlive) continue;
                    float distToPlayer = glm::distance(player.getPosition(), target.position);
                    if (distToPlayer > 260.0f) continue;

                    glm::vec3 toCam = glm::vec3(rayOrigin.x - target.position.x, 0.0f, rayOrigin.z - target.position.z);
                    if (glm::length(toCam) < 0.01f) toCam = glm::vec3(0, 0, 1);
                    glm::vec3 planeNormal = glm::normalize(toCam);
                    glm::vec3 planeRight = glm::normalize(glm::cross(glm::vec3(0, 1, 0), planeNormal));

                    float denom = glm::dot(planeNormal, rayDir);
                    if (denom < -0.001f) {
                        float t = glm::dot(target.position - rayOrigin, planeNormal) / denom;
                        if (t > 0.0f && t < closestTargetDist) {
                            glm::vec3 p = rayOrigin + rayDir * t;
                            float localY = p.y - target.position.y;
                            float localH = glm::dot(p - target.position, planeRight);

                            if (localY >= 0.20f && localY <= 1.55f && std::abs(localH) <= 0.42f) {
                                closestTargetDist = t;
                                aimInfo.hasTarget = true;
                                aimInfo.worldPos = p;
                                aimInfo.bodyHeight = localY;
                                aimInfo.bodyHoriz = localH;
                                aimInfo.targetId = 50000;
                                aimInfo.isObstacle = false;
                                aimInfo.isHeadshot = (localY >= 1.20f && std::abs(localH) <= 0.22f);
                            }
                        }
                    }
                }
            }

            // 2. Raycast vs Remote MMO Players
            NetworkEngine& netEngine = NetworkEngine::get();
            for (const auto& [rId, remote] : netEngine.getRemotePlayers()) {
                if (!remote.active || remote.currentState.health <= 0.0f) continue;
                glm::vec3 rPos = remote.currentState.position;
                float distToPlayer = glm::distance(player.getPosition(), rPos);
                if (distToPlayer > 85.0f) continue;

                float rH = (remote.currentState.stance == 2) ? 0.45f : ((remote.currentState.stance == 1) ? 1.45f : 1.90f);
                float halfW = (remote.currentState.stance == 2) ? 0.65f : 0.42f;

                glm::vec3 toCam = glm::vec3(rayOrigin.x - rPos.x, 0.0f, rayOrigin.z - rPos.z);
                if (glm::length(toCam) < 0.01f) toCam = glm::vec3(0, 0, 1);
                glm::vec3 planeNormal = glm::normalize(toCam);
                glm::vec3 planeRight = glm::normalize(glm::cross(glm::vec3(0, 1, 0), planeNormal));

                float denom = glm::dot(planeNormal, rayDir);
                if (denom < -0.001f) {
                    float t = glm::dot(rPos - rayOrigin, planeNormal) / denom;
                    if (t > 0.0f && t < closestTargetDist) {
                        glm::vec3 p = rayOrigin + rayDir * t;
                        float localY = p.y - rPos.y;
                        float localH = glm::dot(p - rPos, planeRight);

                        if (localY >= -0.08f && localY <= rH + 0.15f && std::abs(localH) <= halfW + 0.12f) {
                            closestTargetDist = t;
                            aimInfo.hasTarget = true;
                            aimInfo.worldPos = p;
                            aimInfo.bodyHeight = localY;
                            aimInfo.bodyHoriz = localH;
                            aimInfo.targetId = (int)rId + 10000;
                            aimInfo.isObstacle = false;

                            float headThreshold = (remote.currentState.stance == 2) ? 0.30f : ((remote.currentState.stance == 1) ? 1.18f : 1.54f);
                            aimInfo.isHeadshot = (localY >= headThreshold && std::abs(localH) <= 0.22f);
                        }
                    }
                }
            }

            // 3. Raycast vs World Obstacles
            if (!aimInfo.hasTarget) {
                glm::vec3 obsHit, obsNorm;
                WorldObstacle* hitObs = nullptr;
                if (world.raycast(rayOrigin, rayOrigin + rayDir * 80.0f, obsHit, obsNorm, hitObs)) {
                    aimInfo.hasTarget = true;
                    aimInfo.worldPos = obsHit;
                    aimInfo.bodyHeight = obsHit.y;
                    aimInfo.bodyHoriz = 0.0f;
                    aimInfo.isHeadshot = false;
                    aimInfo.isObstacle = true;
                }
            }

            glm::vec3 camHorizFwd = glm::normalize(glm::vec3(rayDir.x, 0.0f, rayDir.z));

            // Validate aimInfo is in front of the player (never behind)
            if (aimInfo.hasTarget) {
                float targetProj = glm::dot(aimInfo.worldPos - player.getPosition(), camHorizFwd);
                if (targetProj < 1.0f) {
                    aimInfo.hasTarget = false;
                }
            }

            if (aimInfo.hasTarget) {
                mouseWorldPos = aimInfo.worldPos;
            } else {
                // Raycast against physical world obstacles down the reticle line-of-sight starting in front of player
                glm::vec3 hitPoint, hitNormal;
                WorldObstacle* hitObs = nullptr;
                glm::vec3 rayStart = player.getPosition() + camHorizFwd * 1.0f + glm::vec3(0.0f, 1.4f, 0.0f);
                bool hitObstacle = world.raycast(rayStart, rayStart + rayDir * 150.0f, hitPoint, hitNormal, hitObs);

                // Precision terrain heightfield ray intersection
                bool hitGround = false;
                glm::vec3 groundHitPos = rayOrigin;
                if (rayDir.y < -0.01f) {
                    for (float t = 1.0f; t <= 180.0f; t += 0.8f) {
                        glm::vec3 testP = rayOrigin + rayDir * t;
                        float terrH = World::sampleTerrainHeight(testP.x, testP.z);
                        if (testP.y <= terrH) {
                            float t0 = t - 0.8f, t1 = t;
                            for (int b = 0; b < 4; ++b) {
                                float tm = (t0 + t1) * 0.5f;
                                glm::vec3 pm = rayOrigin + rayDir * tm;
                                if (pm.y <= World::sampleTerrainHeight(pm.x, pm.z)) t1 = tm;
                                else t0 = tm;
                            }
                            groundHitPos = rayOrigin + rayDir * ((t0 + t1) * 0.5f);
                            hitGround = true;
                            break;
                        }
                    }
                }

                if (hitObstacle && glm::dot(hitPoint - player.getPosition(), camHorizFwd) >= 1.0f) {
                    if (hitGround && glm::distance(rayOrigin, groundHitPos) < glm::distance(rayOrigin, hitPoint)) {
                        mouseWorldPos = groundHitPos;
                    } else {
                        mouseWorldPos = hitPoint;
                    }
                } else if (hitGround && glm::dot(groundHitPos - player.getPosition(), camHorizFwd) >= 1.0f) {
                    mouseWorldPos = groundHitPos;
                } else {
                    mouseWorldPos = rayOrigin + rayDir * 80.0f;
                }
            }

            // Safety guarantee: Aim target is ALWAYS projected ahead of the player in horizontal camera forward
            float projDist = glm::dot(mouseWorldPos - player.getPosition(), camHorizFwd);
            if (projDist < 3.5f) {
                mouseWorldPos = player.getPosition() + camHorizFwd * 35.0f + glm::vec3(0.0f, 1.5f, 0.0f);
            }

            // Update Player Controller with Camera-Relative Direction and Precision 3D Aim Target
            player.update(gameDt, window, mouseWorldPos, world, ballistics, camera.getYaw(), &camera, &aimInfo);
            if (testAim) player.setAiming(true);
            if (testCrouch) player.getStats().stance = PlayerStance::Crouching;
            if (testMove) {
                glm::vec3 fwd(std::sin(player.getYaw()), 0.0f, std::cos(player.getYaw()));
                player.setVelocity(fwd * 3.8f);
            }
            static int s_testFrame = 0;
            s_testFrame++;
            if (testFire && s_testFrame == 25) {
                glm::vec3 muz = player.getRig().getMuzzlePosition();
                if (glm::length(muz) < 0.1f) muz = player.getPosition() + glm::vec3(0.0f, 1.2f, 0.0f) + glm::vec3(std::sin(player.getYaw()), 0.0f, std::cos(player.getYaw())) * 0.6f;
                glm::vec3 dir = glm::vec3(std::sin(player.getYaw()), 0.0f, std::cos(player.getYaw()));
                if (testFire && world.getMapType() == MapType::TrainingBase && player.getPosition().x > 10.0f) {
                    dir = glm::normalize(glm::vec3(70.0f, 0.85f, 0.0f) - muz);
                } else if (glm::length(player.getAimTarget() - muz) > 0.05f && glm::length(player.getAimTarget()) > 0.1f) {
                    dir = glm::normalize(player.getAimTarget() - muz);
                }
                glm::vec3 r = glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0)));
                glm::vec3 u = glm::normalize(glm::cross(r, dir));
                ballistics.spawnMuzzleFlash(muz, dir, 1.25f, 0);
                ballistics.spawnMuzzleSmoke(muz, dir, 1.0f);
                ballistics.spawnShellCasing(muz - dir * 0.22f + r * 0.10f + u * 0.04f, r, u, dir, 0);
                ballistics.fireBullet(muz, dir, 750.0f, 65.0f, 2.5f, true, playerFaction, -1, 350.0f, 0);
                camera.addTrauma(0.12f);
            }

            if (testSeverPole && s_testFrame == 8) {
                glm::vec3 muz = glm::vec3(-4.2f, 1.4f, 8.2f);
                glm::vec3 target = glm::vec3(-4.2f, 2.2f, 12.0f);
                glm::vec3 dir = glm::normalize(target - muz);
                ballistics.spawnMuzzleFlash(muz, dir, 1.25f, 0);
                ballistics.fireBullet(muz, dir, 700.0f, 75.0f, 2.5f, true, playerFaction, -1, 100.0f, 0);
            }

            // Interactive Tactical Squad Ping & Alert Radial Menu (Hold Middle Mouse Button / Ping Key)
            static float s_midHoldTimer = 0.0f;
            static bool s_midDownLast = false;
            bool inMenu = ui.isOptionsOpen() || ui.isEscapeMenuOpen() || ui.isInventoryOpen() || 
                          ui.isMapOpen() || ChatSystem::get().isChatOpen() ||
                          (ui.getGameState() != GameState::InGame);
            bool curMidDown = window.isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE) && !inMenu;

            if (curMidDown) {
                s_midHoldTimer += dt;
                if (s_midHoldTimer >= 0.18f && !ui.isAlertWheelOpen()) {
                    ui.setAlertWheelOpen(true);
                }
            } else if (s_midDownLast) {
                // Released!
                PingType chosenType = (aimInfo.hasTarget && !aimInfo.isObstacle) ? PingType::HostileContact : PingType::TacticalMove;
                if (ui.isAlertWheelOpen()) {
                    int selAlert = ui.getSelectedAlertIndex();
                    ui.setAlertWheelOpen(false);

                    if (selAlert >= 0 && selAlert < 5) {
                        static const PingType s_types[5] = {
                            PingType::HostileContact,
                            PingType::TacticalMove,
                            PingType::Danger,
                            PingType::UrgentAssist,
                            PingType::Supplies
                        };
                        chosenType = s_types[selAlert];
                    }
                }

                if (s_midHoldTimer > 0.001f) {
                    glm::vec3 pingPos = (chosenType == PingType::HostileContact && aimInfo.hasTarget && !aimInfo.isObstacle)
                        ? aimInfo.worldPos : mouseWorldPos;

                    CharacterProfile* activeProf = ProfileManager::get().getActiveProfile();
                    std::string sName = activeProf ? activeProf->callsign : "OPERATOR";
                    ui.addTacticalPing(chosenType, pingPos, sName);
                    AudioEngine::get().playSound(SoundType::RadioBeepDatalink, 0.65f, 1.25f);
                    float pDist = glm::distance(player.getPosition(), pingPos);
                    const char* pName = (chosenType == PingType::HostileContact) ? "ENEMY SPOTTED" :
                                        ((chosenType == PingType::TacticalMove) ? "MOVE / RECON" :
                                        ((chosenType == PingType::Danger) ? "DANGER" :
                                        ((chosenType == PingType::UrgentAssist) ? "NEED BACKUP" : "SUPPLIES FOUND")));
                    char dBuf[128];
                    std::snprintf(dBuf, sizeof(dBuf), "[DATALINK] %s: %s (%.0fm)", sName.c_str(), pName, pDist);
                    ChatSystem::get().addSystemMessage(dBuf);
                }
                s_midHoldTimer = 0.0f;
            }
            s_midDownLast = curMidDown;

            // Tactical Garrison NPC [F] Interaction (SSG Reeves & SGT Bishop)
            static bool s_fKeyLast = false;
            bool s_fKey = window.isKeyPressed(GLFW_KEY_F);
            if (s_fKey && !s_fKeyLast && world.getMapType() == MapType::TrainingBase) {
                for (const auto& bot : frontline.getBots()) {
                    if (bot.state == BotState::Dead) continue;
                    float dist = glm::distance(player.getPosition(), bot.position);
                    if (dist <= 3.5f) {
                        if (bot.callsign.find("Reeves") != std::string::npos) {
                            // SSG Reeves: Full ammunition restock and medical treatment
                            player.resetFullHealthAndVitals();
                            player.getInventory().addItem(Inventory::create556Ammo(180));
                            player.getInventory().addItem(Inventory::create545Ammo(180));
                            player.getInventory().addItem(Inventory::create762Ammo(60));
                            player.getInventory().addItem(Inventory::create9mmAmmo(90));
                            AudioEngine::get().playSound(SoundType::ItemEquip, 0.75f, 1.0f);
                            ui.showTargetHitBanner("SSG REEVES: MUNITIONS TOPPED OFF // ARMOR INSPECTED");
                            break;
                        } else if (bot.callsign.find("Bishop") != std::string::npos) {
                            // SGT Bishop: Reset all firing lane targets
                            for (auto& tgt : world.getTargets()) {
                                tgt.isAlive = true;
                                tgt.health = tgt.maxHealth;
                                tgt.knockdownAngle = 0.0f;
                            }
                            AudioEngine::get().playSound(SoundType::ChamberRound, 0.85f, 1.0f);
                            ui.showTargetHitBanner("SGT BISHOP: TARGET LANES RESET // RANGE ALL GREEN");
                            break;
                        }
                    }
                }
            }
            s_fKeyLast = s_fKey;

            // TDM Match End Flow & Clean Return to Hideout
            if (frontline.isTdmMode() && frontline.isTdmMatchOver()) {
                if (ui.getTdmEndTimer() <= 0.0f) {
                    ui.setTdmEndTimer(7.0f);
                    Faction winner = frontline.getTdmWinner();
                    if (winner == player.getFaction()) {
                        AudioEngine::get().playSound(SoundType::DeployHorn, 0.90f);
                    } else {
                        AudioEngine::get().playSound(SoundType::Impact, 0.90f, 0.60f);
                    }
                }
                float newEndTimer = ui.getTdmEndTimer() - dt;
                ui.setTdmEndTimer(newEndTimer);
                bool spaceOrEsc = window.isKeyPressed(GLFW_KEY_SPACE) || window.isKeyPressed(GLFW_KEY_ESCAPE);
                if (newEndTimer <= 0.0f || (spaceOrEsc && newEndTimer < 5.5f)) {
                    ui.setTdmEndTimer(0.0f);
                    frontline.resetTdmMatch();
                    ui.setGameState(GameState::MainMenu);
                    camera.resetToOrbit();
                    window.setCursorCaptured(false);
                }
            } else {
                ui.setTdmEndTimer(0.0f);
            }

            // Update Active Frontline AI Battle (Team A bots vs Team B bots)
            bool isPvPOnlyTdm = frontline.isTdmMode() && !ui.isTdmAgainstAi() && (NetworkEngine::get().isClientConnected() || ui.isOnlineMultiplayer());
            if (isPvPOnlyTdm) {
                // Completely purge local AI bots only in explicit online PvP-only matches
                if (!frontline.getBots().empty()) {
                    frontline.getBots().clear();
                }
            } else if (activeProfile) {
                // Simulate local tactical AI bots in Against AI mode, training, offline warzone, and PVE raids
                frontline.update(gameDt, player.getPosition(), player.getYaw(), playerFaction, world, ballistics, *activeProfile, glm::radians(camera.getYaw()), player.hasSpawnProtection());
            }
            if (activeProfile) {
                bool isTrainingActive = (world.getMapType() == MapType::TrainingBase) || ui.isTrainingMode();
                missions.update(gameDt, player.getPosition(), *activeProfile, isTrainingActive);
                GoreSystem::get().update(gameDt, &world);
            }

            // Update Foxhole Strategic Frontline Sectors
            world.updateSectors(gameDt, player.getPosition(), playerFaction, &frontline.getBots());

            // Update Dynamic Frontline Artillery Strikes (Warzone Mortars only - disabled in TDM, Training, and Online PvP)
            if (!frontline.isTdmMode() && !ui.isTrainingMode() && !NetworkEngine::get().isClientConnected() && !ui.isOnlineMultiplayer()) {
                float artilleryDamage = 0.0f;
                world.updateArtillery(gameDt, player.getPosition(), artilleryDamage, glm::radians(camera.getYaw()));
                if (artilleryDamage > 0.0f && !player.hasSpawnProtection()) {
                    player.getStats().takeDamage(artilleryDamage, false);
                    camera.addTrauma(0.70f);
                    AudioEngine::get().playSound(SoundType::Impact, 1.0f, 0.65f);
                }
            }

            // Update Ballistics (bullet physics stepping, drop, velocity, 3D spatialized shell casings)
            ballistics.update(gameDt, camera.getPosition(), glm::radians(camera.getYaw()), &world);
            DecalSystem::get().update(gameDt);
            PunctureSystem::get().update(gameDt);

            // Process Tactical Ordnance Explosions (Grenades & Blast Waves)
            for (const auto& exp : ballistics.getPendingExplosions()) {
                if (exp.isSmoke) continue;

                // 3D Spatial Audio & Acoustic Environment Reverb
                AcousticEnvironment expEnv = world.detectAcousticEnvironment(exp.position);
                float expOcc = world.evaluateAcousticOcclusion(exp.position, player.getPosition());
                AudioEngine::get().playSound3D(SoundType::GrenadeExplosion, exp.position, player.getPosition(), glm::radians(camera.getYaw()), 1.0f, 160.0f, expEnv, expOcc);

                // Blast Wave on Player
                float distToPlayer = glm::distance(exp.position, player.getPosition() + glm::vec3(0, 0.9f, 0));
                if (distToPlayer <= exp.blastRadius) {
                    float blastFraction = 1.0f - (distToPlayer / exp.blastRadius);
                    float dmg = exp.damage * blastFraction;
                    CoverType cov = world.evaluateCoverAt(player.getPosition(), exp.position);
                    if (cov == CoverType::Heavy) dmg *= 0.25f;
                    else if (cov == CoverType::Light) dmg *= 0.60f;

                    if (dmg > 1.0f && !player.hasSpawnProtection()) {
                        player.getStats().takeDamage(dmg, true);
                        camera.addTrauma(std::min(1.0f, blastFraction * 1.2f));
                        player.getStats().addSuppression(blastFraction * 65.0f);
                        glm::vec3 blastDir = player.getPosition() - exp.position;
                        if (glm::length(blastDir) > 0.01f) {
                            blastDir = glm::normalize(blastDir);
                            player.setVelocity(player.getVelocity() + blastDir * (blastFraction * 6.5f));
                        }
                    }
                } else if (distToPlayer <= exp.blastRadius * 2.2f) {
                    float shockFraction = 1.0f - (distToPlayer / (exp.blastRadius * 2.2f));
                    camera.addTrauma(shockFraction * 0.45f);
                    player.getStats().addSuppression(shockFraction * 35.0f);
                }

                // Blast Wave on Frontline AI Bots
                for (auto& bot : frontline.getBots()) {
                    if (bot.state == BotState::Dead) continue;
                    float distToBot = glm::distance(exp.position, bot.position + glm::vec3(0, 0.9f, 0));
                    if (distToBot <= exp.blastRadius) {
                        float blastFraction = 1.0f - (distToBot / exp.blastRadius);
                        float dmg = exp.damage * blastFraction;
                        CoverType botCov = world.evaluateCoverAt(bot.position, exp.position);
                        if (botCov == CoverType::Heavy) dmg *= 0.30f;
                        else if (botCov == CoverType::Light) dmg *= 0.65f;

                        bot.health -= dmg;
                        bot.suppression = 100.0f;
                        GoreSystem::get().spawnBloodBurst(bot.position + glm::vec3(0, 0.8f, 0), (bot.position - exp.position) * 3.0f, 40, 1.2f);

                        if (exp.isPlayer) {
                            ui.triggerHitMarker(bot.health <= 0.0f);
                        }

                        if (bot.health <= 0.0f) {
                            if (frontline.isTdmMode()) {
                                Faction killerFac = (exp.faction != Faction::None) ? exp.faction : (exp.isPlayer ? player.getFaction() : Faction::FactionA);
                                frontline.recordTdmKill(killerFac);
                            }
                            bot.state = BotState::Dead;
                            CharacterCustomization botCustom;
                            if (bot.faction == Faction::FactionB) {
                                botCustom.camoId = 2;
                                botCustom.armbandId = 1;
                            } else {
                                botCustom.camoId = 0;
                                botCustom.armbandId = 0;
                            }
                            std::shared_ptr<CharacterRig> corpseRig = std::make_shared<CharacterRig>();
                            corpseRig->snapYaw(bot.yaw);
                            glm::vec3 blastImpulse = (bot.position - exp.position) * 1.5f;
                            GoreSystem::get().addCorpse(corpseRig, botCustom, bot.position, bot.yaw, DeathType::CollapseBackward, blastImpulse, false, DISMEMBER_HEAD, "None");
                            GoreSystem::get().spawnGib(GibType::ArmRight, bot.position + glm::vec3(0, 1.0f, 0), blastImpulse + glm::vec3(0, 2.5f, 0), botCustom);

                            if (exp.isPlayer && activeProfile) {
                                activeProfile->combatKills++;
                                activeProfile->cash += 150;
                                ProfileManager::get().saveProfiles();
                                SteamIntegration::get().unlockAchievement("ACH_FIRST_KILL");
                            }
                        }
                    }
                }
            }
            ballistics.clearPendingExplosions();

            // ================= MMO NETWORK UPDATES =================
            NetworkEngine& net = NetworkEngine::get();

            CharacterCustomization customAppearance = activeProfile ? activeProfile->customization : CharacterCustomization{};

            PlayerNetworkState localNetState;
            localNetState.position = player.getPosition();
            localNetState.velocity = player.getVelocity();
            localNetState.yaw = player.getYaw();
            localNetState.stance = static_cast<uint8_t>(player.getStats().stance);
            localNetState.isSprinting = player.getStats().isSprinting ? 1 : 0;
            localNetState.health = player.getStats().health;
            localNetState.blood = player.getStats().blood;
            localNetState.bleedCuts = static_cast<uint8_t>(player.getStats().bleedCuts);
            localNetState.faction = (playerFaction == Faction::FactionB) ? 1 : 0;
            localNetState.helmetId = customAppearance.helmetId;
            localNetState.vestId = customAppearance.vestId;
            localNetState.camoId = customAppearance.camoId;
            localNetState.faceId = customAppearance.faceId;
            localNetState.skinToneId = customAppearance.skinToneId;
            localNetState.hairId = customAppearance.hairId;
            localNetState.beardId = customAppearance.beardId;
            localNetState.backpackId = customAppearance.backpackId;
            localNetState.gloveId = customAppearance.gloveId;
            localNetState.armbandId = customAppearance.armbandId;
            localNetState.isAiming = player.isAiming() ? 1 : 0;
            localNetState.isMounted = player.isMounted() ? 1 : 0;
            auto& activeEqW = player.getInventory().getEquipped(player.getActiveWeaponSlot());
            if (activeEqW.has_value()) {
                if (activeEqW->name.find("M4A1") != std::string::npos) localNetState.weaponId = 1;
                else if (activeEqW->name.find("AK") != std::string::npos) localNetState.weaponId = 2;
                else if (activeEqW->name.find("Mosin") != std::string::npos) localNetState.weaponId = 3;
                else if (activeEqW->name.find("M9") != std::string::npos) localNetState.weaponId = 4;
                else localNetState.weaponId = 1;
            } else {
                localNetState.weaponId = 0;
            }
            if (activeProfile) {
                if (net.getLocalPlayerId() > 1) {
                    std::string pName = activeProfile->callsign + " [P" + std::to_string(net.getLocalPlayerId()) + "]";
                    std::strncpy(localNetState.callsign, pName.c_str(), sizeof(localNetState.callsign) - 1);
                } else {
                    std::strncpy(localNetState.callsign, activeProfile->callsign.c_str(), sizeof(localNetState.callsign) - 1);
                }
            }

            net.updateClient(dt, localNetState, glm::radians(camera.getYaw()));

            // Synchronize authoritative match state (scores, round timer, victory/defeat)
            if (net.isClientConnected() && net.hasMatchState()) {
                const auto& ms = net.getMatchState();
                if (ms.sessionId == net.getCurrentSessionId() || net.getCurrentSessionId() == 0) {
                    frontline.syncServerState(ms.coalitionScore, ms.crimsonScore, ms.roundTimer, ms.matchStatus);
                }
            }

            // Seamless sector handoff dynamic map streaming
            if (net.hasPendingHandoffSwitch()) {
                std::string targetHex = net.getCurrentMapName();
                if (!targetHex.empty()) {
                    std::string mPath = "assets/maps/" + targetHex + ".vmap";
                    if (world.loadMap(mPath) || world.loadMap("ASSETS/maps/" + targetHex + ".vmap")) {
                        renderer.rebuildTerrainMesh(false);
                        std::cout << "[Client Handoff] Loaded new sector map: " << mPath << "\n" << std::flush;
                    }
                }
            }

            // Sync incoming persistent structures from server
            if (!net.getPendingStructures().empty()) {
                for (const auto& sp : net.getPendingStructures()) {
                    WorldObstacle obs;
                    obs.position = sp.position;
                    obs.yaw = sp.rotYaw;
                    obs.health = sp.health;
                    obs.destructible = true;
                    obs.blocksLineOfSight = true;
                    obs.size = (sp.structureType == 2) ? glm::vec3(4.0f, 1.1f, 0.9f) :
                               ((sp.structureType == 1) ? glm::vec3(6.0f, 3.5f, 5.0f) :
                                glm::vec3(2.5f, 2.5f, 2.5f));
                    obs.type = (sp.structureType == 2) ? ObstacleType::Sandbag :
                               ((sp.structureType == 1) ? ObstacleType::ConcreteBunker : ObstacleType::ConcreteBarrier);
                    obs.color = (sp.faction == 0) ? glm::vec4(0.4f, 0.6f, 0.9f, 1.0f) : glm::vec4(0.9f, 0.4f, 0.4f, 1.0f);
                    world.getObstacles().push_back(obs);
                }
                net.clearPendingStructures();
            }

            // Continental coordinate border monitor (every 1.0s)
            static float s_borderCheckTimer = 0.0f;
            s_borderCheckTimer += dt;
            if (s_borderCheckTimer >= 1.0f && net.isClientConnected() && net.getCurrentSessionId() >= 101 && net.getCurrentSessionId() <= 106) {
                s_borderCheckTimer = 0.0f;
                glm::vec3 pPos = player.getPosition();
                std::string targetRegion;
                if (pPos.z > 60.0f) {
                    targetRegion = (pPos.x < 0.0f) ? "sumy" : "kharkiv";
                } else if (pPos.z >= -60.0f) {
                    targetRegion = (pPos.x < 0.0f) ? "donetsk" : "luhansk";
                } else {
                    targetRegion = (pPos.x < 0.0f) ? "zaporizhzhia" : "kherson";
                }

                if (!targetRegion.empty() && targetRegion != net.getCurrentMapName()) {
                    std::cout << "[Continental Handoff] Border crossing detected: " << net.getCurrentMapName() << " -> " << targetRegion << " at (" << pPos.x << ", " << pPos.z << ")\n" << std::flush;
                    net.requestSectorHandoff(targetRegion, pPos);
                }
            }

            // Replicate incoming gunfire from remote players in Area of Interest
            for (const auto& rBullet : net.getPendingBullets()) {
                SoundType snd = (rBullet.weaponType == 3) ? SoundType::HeavyMGShot :
                                ((rBullet.weaponType == 1) ? SoundType::SniperShot :
                                ((rBullet.weaponType == 2) ? SoundType::PistolShot : SoundType::RifleShot));
                AcousticEnvironment rEnv = world.detectAcousticEnvironment(rBullet.origin);
                float rOcc = world.evaluateAcousticOcclusion(rBullet.origin, player.getPosition());
                AudioEngine::get().playSound3D(snd, rBullet.origin, player.getPosition(), glm::radians(camera.getYaw()), 0.85f, 180.0f, rEnv, rOcc);

                float flashScale = (rBullet.weaponType == 3) ? 1.95f : ((rBullet.weaponType == 1) ? 1.55f : ((rBullet.weaponType == 2) ? 0.75f : 1.0f));
                float smokeScale = (rBullet.weaponType == 3) ? 2.2f : ((rBullet.weaponType == 1) ? 1.6f : 1.0f);
                ballistics.spawnMuzzleFlash(rBullet.origin, rBullet.direction, flashScale, rBullet.weaponType);
                ballistics.spawnMuzzleSmoke(rBullet.origin, rBullet.direction, smokeScale);

                // Compute physical ejection coordinate basis for remote shooter's empty brass casing
                glm::vec3 rFwd = glm::normalize(rBullet.direction);
                glm::vec3 rRight = glm::cross(rFwd, glm::vec3(0.0f, 1.0f, 0.0f));
                if (glm::length(rRight) < 0.001f) rRight = glm::vec3(1.0f, 0.0f, 0.0f);
                else rRight = glm::normalize(rRight);
                glm::vec3 rUp = glm::cross(rRight, rFwd);
                glm::vec3 casingOrigin = rBullet.origin - rFwd * 0.22f + rRight * 0.10f + rUp * 0.04f;
                ballistics.spawnShellCasing(casingOrigin, rRight, rUp, rFwd, rBullet.weaponType);

                float netRange = (rBullet.weaponType == 3) ? 140.0f : ((rBullet.weaponType == 1) ? 240.0f : ((rBullet.weaponType == 2) ? 55.0f : 150.0f));

                ballistics.fireBullet(
                    rBullet.origin,
                    rBullet.direction,
                    rBullet.speed,
                    rBullet.damage,
                    0.85f,
                    false, // Remote player bullet
                    static_cast<Faction>(rBullet.faction),
                    rBullet.shooterId,
                    netRange,
                    rBullet.weaponType
                );

                // Trigger weapon recoil kick on the remote shooter's rig
                for (const auto& [rId, rPlayer] : net.getRemotePlayers()) {
                    if (rId == rBullet.shooterId && rPlayer.rig) {
                        rPlayer.rig->triggerMuzzleFlash(0.08f);
                        break;
                    }
                }
            }
            net.clearPendingBullets();
            // Process Networked Hit Confirmations
            for (const auto& hit : net.getPendingHits()) {
                if (hit.victimId == net.getLocalPlayerId()) {
                    // Local player took damage from remote operative
                    player.getStats().takeDamage(hit.damage, true);
                    if (hit.isFatal) {
                        player.getStats().health = 0.0f;
                    }
                    camera.addTrauma(0.40f);
                    ballistics.spawnImpactFX(hit.hitPoint, glm::vec3(0, 1, 0), 2);
                    GoreSystem::get().spawnBloodBurst(hit.hitPoint, glm::vec3(0, 1, 0), 40, 1.2f);
                    AudioEngine::get().playSound(SoundType::Impact, 0.85f, 0.90f);
                } else if (hit.shooterId == net.getLocalPlayerId()) {
                    // Confirmation of our hit on another player
                    ui.triggerHitMarker(hit.isFatal != 0);
                } else {
                    // Remote player hit another remote player
                    auto rIt = net.getRemotePlayers().find(hit.victimId);
                    if (rIt != net.getRemotePlayers().end()) {
                        rIt->second.currentState.health = std::max(0.0f, rIt->second.currentState.health - hit.damage);
                    }
                    GoreSystem::get().spawnBloodBurst(hit.hitPoint, glm::vec3(0, 1, 0), 30, 1.0f);
                }
            }
            net.clearPendingHits();


            // Test Bullet Collisions against World, Targets, Bots, and Player (Continuous Swept-Segment Hitreg)
            for (auto& bullet : ballistics.getBullets()) {
                if (!bullet.alive) continue;

                // 1. Raycast vs World Obstacles (with thin CQB wall penetration)
                glm::vec3 hitPoint, hitNormal;
                WorldObstacle* hitObs = nullptr;
                if (world.raycast(bullet.prevPosition, bullet.position, hitPoint, hitNormal, hitObs)) {
                    bool penetrated = false;
                    bool severed = false;
                    if (hitObs) {
                        // Procedural Jolt Physics Severance & Damage
                        if (hitObs->destructible) {
                            hitObs->health -= bullet.damage;
                            if (hitObs->health <= 0.0f) {
                                std::cout << "[Ballistics] Obstacle SEVERED by bullet hit! Type: " << (int)hitObs->type << " at (" << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z << ")\n";
                                world.severObstacle(*hitObs, hitPoint, bullet.velocity);
                                ballistics.spawnImpactFX(hitPoint, hitNormal, 3);
                                severed = true;
                            }
                        }
                        if (!severed) {
                            DecalSurface surf = DecalSurface::Concrete;
                            int pMat = 0;
                            bool isThrough = false;

                            if (hitObs->type == ObstacleType::StreetLamp || hitObs->type == ObstacleType::CommunicationTower || hitObs->type == ObstacleType::RefineryTank) {
                                surf = DecalSurface::Metal;
                                pMat = 1;
                                isThrough = (hitObs->type != ObstacleType::RefineryTank);
                            } else if (hitObs->type == ObstacleType::WoodenCrate) {
                                surf = DecalSurface::Wood;
                                pMat = 2;
                                isThrough = true;
                            } else if (hitObs->type == ObstacleType::Sandbag) {
                                surf = DecalSurface::Dirt;
                                pMat = 3;
                                isThrough = false; // Sandbags are immutable! Physical crater only, no through-holes!
                            } else if (hitObs->type == ObstacleType::GuardBooth && hitPoint.y >= 1.1f && hitPoint.y <= 2.1f) {
                                surf = DecalSurface::Glass;
                                pMat = 1;
                                isThrough = true;
                            } else if (hitObs->type == ObstacleType::UrbanApartment && (std::fmod(hitPoint.y, 3.0f) >= 1.0f && std::fmod(hitPoint.y, 3.0f) <= 2.2f)) {
                                surf = DecalSurface::Glass;
                                pMat = 0;
                                isThrough = true;
                            } else if (hitObs->type == ObstacleType::PerimeterFence || hitObs->type == ObstacleType::ObstacleWall || hitObs->type == ObstacleType::GuardBooth) {
                                surf = DecalSurface::Concrete;
                                pMat = 0;
                                isThrough = true;
                            } else if (hitObs->type == ObstacleType::DamagedBuilding || hitObs->type == ObstacleType::ConcreteBarrier) {
                                surf = DecalSurface::Concrete;
                                pMat = 0;
                                isThrough = false;
                            } else {
                                surf = DecalSurface::Concrete;
                                pMat = 0;
                                isThrough = (hitObs->size.x <= 0.6f || hitObs->size.z <= 0.6f);
                            }

                            DecalSystem::get().addBulletHole(hitPoint, hitNormal, surf, bullet.caliberSize, bullet.weaponType);
                            PunctureSystem::get().addPuncture(hitPoint, hitNormal, bullet.velocity, pMat, isThrough, bullet.caliberSize, bullet.weaponType);
                        }
                        if (bullet.penetrationPower > 1.4f) {
                            if (hitObs->type == ObstacleType::WoodenCrate || hitObs->destructible || (hitObs->type == ObstacleType::Sandbag && bullet.penetrationPower > 3.0f)) {
                                penetrated = true;
                                bullet.penetrationPower -= 1.1f;
                                bullet.damage *= 0.78f;
                                bullet.velocity *= 0.88f;
                                ballistics.spawnImpactFX(hitPoint, hitNormal, 1);
                                AudioEngine::get().playSound3D(SoundType::Impact, hitPoint, player.getPosition(), glm::radians(camera.getYaw()), 0.45f, 28.0f);
                                AudioEngine::get().playSound3D(SoundType::PenetrationCrack, hitPoint, player.getPosition(), glm::radians(camera.getYaw()), 0.95f, 52.0f);
                            }
                        }
                    }
                    if (!penetrated) {
                        bullet.alive = false;
                        ballistics.spawnImpactFX(hitPoint, hitNormal, 1);
                        AudioEngine::get().playSound3D(SoundType::Impact, hitPoint, player.getPosition(), glm::radians(camera.getYaw()), 0.55f, 32.0f);
                        continue;
                    }
                }

                // 1b. Test vs Shooting Range Reactive Targets (Continuous Swept Segment - Zero Ghosting)
                bool hitTarget = false;
                for (auto& target : world.getTargets()) {
                    if (!target.isAlive) continue;
                    glm::vec3 tgtCenter = target.position + glm::vec3(0.0f, 0.85f, 0.0f);
                    glm::vec3 closestPt;
                    float distSq = distSqPointToSegment(tgtCenter, bullet.prevPosition, bullet.position, closestPt);

                    float horizDist = glm::distance(glm::vec2(closestPt.x, closestPt.z), glm::vec2(target.position.x, target.position.z));
                    float hitY = closestPt.y - target.position.y;

                    if (horizDist <= 0.48f && hitY >= 0.25f && hitY <= 1.55f) {
                        bullet.alive = false;
                        hitTarget = true;
                        target.health -= bullet.damage;
                        target.hitFlashTimer = 0.24f;
                        ballistics.spawnImpactFX(closestPt, glm::vec3(-1, 0, 0), 3); // Bright metallic sparks

                        // Resonant metallic steel target strike
                        AudioEngine::get().playSound3D(SoundType::Impact, target.position, player.getPosition(), glm::radians(camera.getYaw()), 0.90f, 95.0f);
                        AudioEngine::get().playSound3D(SoundType::TacticalClick, target.position, player.getPosition(), glm::radians(camera.getYaw()), 1.0f, 95.0f);

                        bool knockedDown = (target.health <= 0.0f);
                        ui.triggerHitMarker(knockedDown);

                        // Range Distance & Damage Confirmation Banner
                        float shotRangeMeters = glm::distance(player.getPosition(), target.position);
                        char hitMsg[128];
                        std::snprintf(hitMsg, sizeof(hitMsg), "TARGET HIT // %s (%.0fm) - %.0f DMG", target.laneLabel.c_str(), shotRangeMeters, bullet.damage);
                        ui.showTargetHitBanner(hitMsg);

                        if (knockedDown) {
                            target.isAlive = false;
                            target.respawnTimer = 0.0f;
                            AudioEngine::get().playSound3D(SoundType::ItemEquip, target.position, player.getPosition(), glm::radians(camera.getYaw()), 0.85f, 80.0f);
                        }
                        break;
                    }
                }
                if (hitTarget) continue;

                // 2. Test vs Frontline AI Bots (Continuous Swept Segment - Zero Ghosting)
                for (auto& bot : frontline.getBots()) {
                    if (bot.state == BotState::Dead) continue;
                    if (!bullet.isPlayer && bullet.shooterId == (int)bot.id) continue;
                    if (bullet.faction != Faction::None && bullet.faction == bot.faction) continue;

                    glm::vec3 botCenter = bot.position + glm::vec3(0.0f, (bot.state == BotState::WoundedDown ? 0.3f : (bot.isCrouched ? 0.7f : 0.95f)), 0.0f);
                    glm::vec3 closestPt;
                    float distSq = distSqPointToSegment(botCenter, bullet.prevPosition, bullet.position, closestPt);

                    float horizDist = glm::distance(glm::vec2(closestPt.x, closestPt.z), glm::vec2(bot.position.x, bot.position.z));
                    float localHitY = closestPt.y - bot.position.y;
                    float maxHitH = (bot.state == BotState::WoundedDown) ? 0.55f : (bot.isCrouched ? 1.45f : 1.95f);
                    float hitRadius = (bot.state == BotState::WoundedDown) ? 0.65f : 0.48f;

                    if (horizDist <= hitRadius && localHitY >= -0.10f && localHitY <= maxHitH) {
                        bullet.alive = false;

                        // Ballistic kinetic energy dropoff over distance
                        float maxR = std::max(10.0f, bullet.maxRange);
                        float rangeFraction = bullet.distanceTraveled / maxR;
                        float falloffMult = 1.0f - std::clamp(rangeFraction - 0.40f, 0.0f, 0.60f) * 0.55f;
                        float effectiveDamage = bullet.damage * falloffMult;
                        float effectivePen = bullet.penetrationPower * falloffMult;

                        // Anatomical Organ Raycast Tracing
                        float hitAngle = std::atan2(bullet.velocity.x, bullet.velocity.z) - bot.yaw;
                        WoundEffect wound = bot.body.traceHit(localHitY, hitAngle, effectiveDamage, effectivePen);

                        float damageDealt = wound.damageDealt;
                        if (bot.isRallied) {
                            damageDealt *= 0.80f;
                        }
                        CoverType botCover = world.evaluateCoverAt(bot.position, bullet.prevPosition);
                        if (botCover == CoverType::Heavy) damageDealt *= 0.45f;
                        else if (botCover == CoverType::Light) damageDealt *= 0.70f;

                        if (wound.instantKill) {
                            bot.health = 0.0f;
                        } else {
                            bot.health -= damageDealt;
                        }

                        bot.suppression = std::min(100.0f, bot.suppression + 22.0f);
                        ballistics.spawnImpactFX(closestPt, glm::vec3(0, 1, 0), 2);
                        if (!wound.armorPenetrated) {
                            if (wound.hitRegion == BodyRegion::Head) {
                                AudioEngine::get().playSound3D(SoundType::HelmetPing, bot.position, player.getPosition(), glm::radians(camera.getYaw()), 0.90f, 60.0f);
                            } else {
                                AudioEngine::get().playSound3D(SoundType::ArmorDeflect, bot.position, player.getPosition(), glm::radians(camera.getYaw()), 0.85f, 60.0f);
                            }
                        } else {
                            AudioEngine::get().playSound3D(SoundType::FleshImpact, bot.position, player.getPosition(), glm::radians(camera.getYaw()), 0.85f, 50.0f);
                        }

                        // Spawn high-velocity arterial blood burst on every hit
                        GoreSystem::get().spawnBloodBurst(closestPt, bullet.velocity, 35, 1.0f);

                        if (bullet.isPlayer) {
                            ui.triggerHitMarker(wound.instantKill || (bot.health - damageDealt) <= 0.0f);
                            bot.hasNoticedPlayer = true;
                            bot.recentDamageTakenTimer = 6.0f;
                            for (auto& ally : frontline.getBots()) {
                                if (ally.faction == bot.faction && glm::distance(ally.position, bot.position) < 28.0f) {
                                    ally.hasNoticedPlayer = true;
                                    ally.recentDamageTakenTimer = 4.0f;
                                }
                            }
                        }

                        if (bot.health <= 0.0f) {
                            if (frontline.isTdmMode()) {
                                Faction killerFac = (bullet.faction != Faction::None) ? bullet.faction : (bullet.isPlayer ? player.getFaction() : Faction::FactionA);
                                frontline.recordTdmKill(killerFac);
                            }
                            if (bot.state != BotState::WoundedDown && !wound.instantKill && !bot.isCqbAggressor && !frontline.isTdmMode()) {
                                bot.state = BotState::WoundedDown;
                                bot.dbnoTimer = 22.0f;
                                bot.health = 0.0f;
                                AudioEngine::get().playSound3D(SoundType::SoldierBarkWounded, bot.position, player.getPosition(), glm::radians(camera.getYaw()), 0.65f, 40.0f);
                                GoreSystem::get().addBloodPool(bot.position, 0.16f, 0.65f, 0.15f);
                            } else {
                                bot.state = BotState::Dead;

                                // Dismemberment & Gibs Evaluation
                                uint32_t dismemberMask = DISMEMBER_NONE;
                                CharacterCustomization botCustom;
                                if (bot.faction == Faction::FactionB) {
                                    botCustom.camoId = 2; // Crimson Charcoal
                                    botCustom.armbandId = 1; // Red
                                } else {
                                    botCustom.camoId = 0; // MultiCam
                                    botCustom.armbandId = 0; // Blue
                                }

                                if (wound.hitRegion == BodyRegion::Head && (wound.instantKill || bullet.damage > 40.0f)) {
                                    dismemberMask |= DISMEMBER_HEAD;
                                    GoreSystem::get().spawnGib(GibType::Head, closestPt + glm::vec3(0, 0.15f, 0), glm::normalize(bullet.velocity) * 4.2f + glm::vec3(0, 3.2f, 0), botCustom);
                                } else if (wound.hitRegion == BodyRegion::LeftArm && bullet.damage > 35.0f) {
                                    dismemberMask |= DISMEMBER_L_ARM;
                                    GoreSystem::get().spawnGib(GibType::ArmLeft, closestPt, glm::normalize(bullet.velocity) * 3.6f + glm::vec3(0, 2.2f, 0), botCustom);
                                } else if (wound.hitRegion == BodyRegion::RightArm && bullet.damage > 35.0f) {
                                    dismemberMask |= DISMEMBER_R_ARM;
                                    GoreSystem::get().spawnGib(GibType::ArmRight, closestPt, glm::normalize(bullet.velocity) * 3.6f + glm::vec3(0, 2.2f, 0), botCustom);
                                } else if (wound.hitRegion == BodyRegion::LeftLeg && bullet.damage > 40.0f) {
                                    dismemberMask |= DISMEMBER_L_LEG;
                                    GoreSystem::get().spawnGib(GibType::LegLeft, closestPt, glm::normalize(bullet.velocity) * 3.0f + glm::vec3(0, 1.8f, 0), botCustom);
                                } else if (wound.hitRegion == BodyRegion::RightLeg && bullet.damage > 40.0f) {
                                    dismemberMask |= DISMEMBER_R_LEG;
                                    GoreSystem::get().spawnGib(GibType::LegRight, closestPt, glm::normalize(bullet.velocity) * 3.0f + glm::vec3(0, 1.8f, 0), botCustom);
                                }

                                // 50% Chance for Death Grip on weapon (if right arm intact)
                                bool deathGrip = (std::rand() % 2 == 0);
                                if (dismemberMask & DISMEMBER_R_ARM) deathGrip = false;

                                // Impact kinematic trajectory determines collapse type
                                glm::vec3 botFwd(std::sin(bot.yaw), 0, std::cos(bot.yaw));
                                float bulletDotFwd = glm::dot(glm::normalize(bullet.velocity), botFwd);
                                DeathType dType = DeathType::CollapseForward;
                                if (wound.hitRegion == BodyRegion::LeftLeg || wound.hitRegion == BodyRegion::RightLeg || wound.hitRegion == BodyRegion::LeftArm) {
                                    dType = DeathType::CollapseCrumple;
                                } else if (bulletDotFwd < -0.2f) {
                                    dType = DeathType::CollapseBackward;
                                } else {
                                    dType = DeathType::CollapseForward;
                                }

                                std::string weaponName = (bot.faction == Faction::FactionA) ? "M4A1" : "AK-74M";

                                // Add Persistent Battlefield Corpse (NEVER disappears from battlefield!)
                                std::shared_ptr<CharacterRig> corpseRig = std::make_shared<CharacterRig>();
                                corpseRig->snapYaw(bot.yaw);
                                glm::vec3 hitImpulse = (glm::length(bullet.velocity) > 0.1f)
                                    ? (glm::normalize(bullet.velocity) * std::min(2.4f, glm::length(bullet.velocity) * 0.003f) + glm::vec3(0.0f, 0.12f, 0.0f))
                                    : glm::vec3(0.0f);
                                GoreSystem::get().addCorpse(corpseRig, botCustom, bot.position, bot.yaw, dType, hitImpulse, deathGrip, dismemberMask, weaponName);
                                GoreSystem::get().spawnBloodBurst(closestPt, bullet.velocity, 50, 1.25f);

                                // Drop bot's weapon ONLY in Warzone
                                if (world.getMapType() == MapType::Warzone) {
                                    SupplyCrate botLoot;
                                    botLoot.position = bot.position + glm::vec3(0.0f, 0.05f, 0.0f);
                                    botLoot.name = bot.rank + " " + bot.callsign + "'s Weapon";
                                    if (bot.faction == Faction::FactionA) {
                                        botLoot.contents.push_back(Inventory::createM4A1());
                                        botLoot.contents.push_back(Inventory::create556Ammo(30));
                                    } else {
                                        botLoot.contents.push_back(Inventory::createAK74());
                                        botLoot.contents.push_back(Inventory::create545Ammo(30));
                                    }
                                    botLoot.looted = false;
                                    botLoot.isGroundItem = true;
                                    botLoot.yaw = bot.yaw + 1.5707f;
                                    world.getCrates().push_back(botLoot);
                                }

                                if (bullet.isPlayer && activeProfile) {
                                    activeProfile->combatKills++;
                                    activeProfile->cash += 150;
                                    ProfileManager::get().saveProfiles();
                                    SteamIntegration::get().unlockAchievement("ACH_FIRST_KILL");
                                    if (activeProfile->combatKills >= 10) {
                                        SteamIntegration::get().unlockAchievement("ACH_WAR_HERO");
                                    }
                                }
                            }
                        }
                        break;
                    }
                }

                // 3. Test vs Player Soldier (Continuous Swept Segment - Zero Ghosting)
                if (!bullet.isPlayer) {
                    if (bullet.faction != Faction::None && bullet.faction == playerFaction) continue;
                    if (player.hasSpawnProtection()) continue;
                    // In online multiplayer, human player vs human player hitreg is handled authoritatively via HitConfirmEvent
                    if (NetworkEngine::get().isClientConnected() && bullet.shooterId > 0 &&
                        NetworkEngine::get().getRemotePlayers().find(bullet.shooterId) != NetworkEngine::get().getRemotePlayers().end()) {
                        continue;
                    }

                    glm::vec3 playerPos = player.getPosition();
                    glm::vec3 playerCenter = playerPos + glm::vec3(0.0f, (player.getStats().stance == PlayerStance::Prone ? 0.3f : (player.getStats().stance == PlayerStance::Crouching ? 0.7f : 0.95f)), 0.0f);
                    glm::vec3 closestPt;
                    float distSq = distSqPointToSegment(playerCenter, bullet.prevPosition, bullet.position, closestPt);

                    float horizDist = glm::distance(glm::vec2(closestPt.x, closestPt.z), glm::vec2(playerPos.x, playerPos.z));
                    float localHitY = closestPt.y - playerPos.y;
                    float maxHitH = (player.getStats().stance == PlayerStance::Prone) ? 0.55f : (player.getStats().stance == PlayerStance::Crouching ? 1.45f : 1.95f);
                    float hitRadius = (player.getStats().stance == PlayerStance::Prone) ? 0.65f : 0.48f;

                    if (horizDist <= hitRadius && localHitY >= -0.10f && localHitY <= maxHitH) {
                        bullet.alive = false;

                        float maxR = std::max(10.0f, bullet.maxRange);
                        float rangeFraction = bullet.distanceTraveled / maxR;
                        float falloffMult = 1.0f - std::clamp(rangeFraction - 0.40f, 0.0f, 0.60f) * 0.55f;
                        float effectiveDamage = bullet.damage * falloffMult;
                        float effectivePen = bullet.penetrationPower * falloffMult;

                        float hitAngle = std::atan2(bullet.velocity.x, bullet.velocity.z) - player.getYaw();
                        WoundEffect wound = player.getBody().traceHit(localHitY, hitAngle, effectiveDamage, effectivePen);

                        // Prevent PvP double-damage: remote human player bullets deal authoritative damage upon verified server HitConfirm
                        bool isFromRemotePlayer = (bullet.shooterId > 0 && net.getRemotePlayers().count(bullet.shooterId) > 0);
                        if (!isFromRemotePlayer) {
                            player.getStats().takeDamage(wound.damageDealt, wound.bleedAdded > 0.0f);
                            if (wound.instantKill) {
                                player.getStats().health = 0.0f;
                            }
                        }

                        camera.addTrauma(0.40f);
                        ballistics.spawnImpactFX(closestPt, glm::vec3(0, 1, 0), 2);
                        if (!wound.armorPenetrated) {
                            if (wound.hitRegion == BodyRegion::Head) {
                                AudioEngine::get().playSound(SoundType::HelmetPing, 0.95f);
                            } else {
                                AudioEngine::get().playSound(SoundType::ArmorDeflect, 0.90f);
                            }
                        } else {
                            AudioEngine::get().playSound(SoundType::FleshImpact, 0.90f);
                        }
                        GoreSystem::get().spawnBloodBurst(closestPt, bullet.velocity, 40, 1.1f);
                    } else if (distSq < (2.8f * 2.8f) && !bullet.flybyTriggered) {
                        bullet.flybyTriggered = true;
                        AudioEngine::get().playSound3D(SoundType::BulletFlyby, closestPt, playerPos, glm::radians(camera.getYaw()), 0.70f, 16.0f);
                        camera.addTrauma(0.18f);
                        player.getStats().addSuppression(14.0f);
                    }
                }

                // 4. Test vs Remote Players (Continuous Swept Segment with Damage & Network Sync)
                if (bullet.isPlayer) {
                    for (auto& [rId, remote] : net.getRemotePlayers()) {
                        if (!remote.active) continue;
                        if (bullet.faction != Faction::None && bullet.faction == (remote.currentState.faction == 1 ? Faction::FactionB : Faction::FactionA)) continue;

                        glm::vec3 rCenter = remote.currentState.position + glm::vec3(0, 0.9f, 0);
                        glm::vec3 closestPt;
                        float distSq = distSqPointToSegment(rCenter, bullet.prevPosition, bullet.position, closestPt);

                        float horizDist = glm::distance(glm::vec2(closestPt.x, closestPt.z), glm::vec2(remote.currentState.position.x, remote.currentState.position.z));
                        float localHitY = closestPt.y - remote.currentState.position.y;
                        if (horizDist <= 0.48f && localHitY >= -0.10f && localHitY <= 1.95f) {
                            bullet.alive = false;
                            bool isHead = (localHitY >= 1.45f);
                            float dealtDmg = isHead ? (bullet.damage * 2.2f) : bullet.damage;
                            remote.currentState.health = std::max(0.0f, remote.currentState.health - dealtDmg);
                            bool isFatal = (remote.currentState.health <= 0.0f);

                            ui.triggerHitMarker(isFatal);
                            ballistics.spawnImpactFX(closestPt, glm::vec3(0, 1, 0), 2);
                            GoreSystem::get().spawnBloodBurst(closestPt, bullet.velocity, 45, 1.2f);
                            AudioEngine::get().playSound3D(SoundType::Impact, remote.currentState.position, player.getPosition(), glm::radians(camera.getYaw()), 0.75f, 32.0f);

                            HitConfirmEvent hitEvt;
                            hitEvt.shooterId = net.getLocalPlayerId();
                            hitEvt.victimId = rId;
                            hitEvt.damage = dealtDmg;
                            hitEvt.hitRegion = isHead ? 1 : 0;
                            hitEvt.isFatal = isFatal ? 1 : 0;
                            hitEvt.hitPoint = closestPt;
                            net.broadcastHitConfirm(hitEvt);
                            break;
                        }
                    }
                }
            }
        } else {
            // Lobby / Character Creation: update operator idle breathing & pose
            player.updateLobby(dt, camera.getPosition());
            NetworkEngine::get().probeShards(dt);
        }

        // ================= PASS 1: REAL-TIME DIRECTIONAL SHADOW PASS =================
        bool isHideout = (ui.getGameState() == GameState::CharacterSelect || ui.getGameState() == GameState::MainMenu);
        if (isHideout) {
            // Warm studio tungsten key lamp shining down from front-left across operative and back armory wall
            renderer.getLight().color = glm::vec3(1.0f, 0.95f, 0.86f);
            renderer.getLight().direction = glm::normalize(glm::vec3(0.18f, -0.68f, -0.68f));
            renderer.getLight().ambientIntensity = 0.50f;
            renderer.getLight().diffuseIntensity = 1.35f;
        } else {
            // [ 07:18 DAWN // CLEAR ] - Gritty Frontline Morning (Company of Heroes / Foxhole Aesthetic)
            renderer.getLight().color = glm::vec3(1.0f, 0.92f, 0.78f); // Warm low-angle morning sunlight
            renderer.getLight().direction = glm::normalize(glm::vec3(0.55f, -0.65f, -0.52f)); // Low-angle sun casting sharp dramatic shadows
            renderer.getLight().ambientIntensity = 0.35f; // Moody, high-contrast ambient fill (not washed out!)
            renderer.getLight().diffuseIntensity = 1.15f; // Crisp morning directional light
        }

        // Dynamic G-Buffer Resize (adapts instantly when window is maximized or resized, eliminating black borders)
        int curW = window.getWidth();
        int curH = window.getHeight();
        if (curW > 0 && curH > 0 && (curW != postProcess.getWidth() || curH != postProcess.getHeight())) {
            postProcess.resize(curW, curH);
        }

        float shadowOrtho = isHideout ? 8.5f : 28.0f;
        glm::vec3 shadowFocus = isHideout ? (player.getPosition() + glm::vec3(0.0f, 0.95f, 0.0f)) : player.getPosition();
        shadowMap.bindForWriting(renderer.getLight().direction, shadowFocus, shadowOrtho);
        renderer.renderShadowPass(shadowMap.getShadowShader(), world, player, &frontline);
        shadowMap.unbindWriting(window.getWidth(), window.getHeight());

        // ================= PASS 2: HDR PBR SCENE PASS =================
        postProcess.beginRender();
        renderer.beginFrame(camera, window.getAspectRatio(), shadowMap.getLightSpaceMatrix(), shadowMap.getDepthTexture());

        if (ui.getGameState() == GameState::CharacterSelect || ui.getGameState() == GameState::MainMenu) {
            // Render Atmospheric Concrete PMC Staging Hideout
            renderer.drawBunkerHideout(player.getPosition());
        } else {
            world.render(renderer, camera.getPosition());
            frontline.render(renderer, camera.getPosition(), player.getPosition(), player.getYaw(), playerFaction, player.getStats().hasRadioComms);
            GoreSystem::get().render(renderer, camera.getPosition());
        }

        // Render Local Mercenary Operator (with modular customized appearance & dynamic equipment sync)
        CharacterCustomization customAppearance = activeProfile ? activeProfile->customization : CharacterCustomization{};
        bool isCustomizing = (ui.getGameState() == GameState::CharacterSelect) ||
                             (ui.getGameState() == GameState::MainMenu && ui.getMenuTab() == MainMenuTab::Customize);
        if (!isCustomizing) {
            // Equipment is dynamically driven by player's equipped inventory items:
            auto& eqVest = player.getInventory().getEquipped(EquipSlot::Vest);
            if (eqVest.has_value()) {
                if (eqVest->name.find("6B45") != std::string::npos) customAppearance.vestId = 1;
                else if (eqVest->name.find("JPC") != std::string::npos) customAppearance.vestId = 0;
                else customAppearance.vestId = 2; // Chest Rig
            } else {
                customAppearance.vestId = 3; // None: Default Combat Uniform Shirt
            }

            auto& eqHelm = player.getInventory().getEquipped(EquipSlot::Helmet);
            if (eqHelm.has_value()) {
                if (eqHelm->name.find("6B47") != std::string::npos) customAppearance.helmetId = 1;
                else if (eqHelm->name.find("Boonie") != std::string::npos) customAppearance.helmetId = 2;
                else if (eqHelm->name.find("FAST") != std::string::npos) customAppearance.helmetId = 0;
                else customAppearance.helmetId = 3; // Balaclava
            } else {
                customAppearance.helmetId = 4; // None: Full face and hair visible!
            }

            auto& eqPack = player.getInventory().getEquipped(EquipSlot::Backpack);
            if (eqPack.has_value()) {
                if (eqPack->name.find("3-Day") != std::string::npos) customAppearance.backpackId = 0;
                else customAppearance.backpackId = 1;
            } else {
                customAppearance.backpackId = 2; // None: Slick
            }
        } else {
            // In Character Creation & Customization Tab: Show face, hair & uniform clearly
            if (ui.isPreviewCombatGear()) {
                customAppearance.helmetId = 0; // FAST MT Ballistic Helmet
                customAppearance.vestId = 0;   // Crye Precision JPC 2.0 Plate Carrier
                customAppearance.backpackId = 0;
            } else {
                customAppearance.helmetId = 4; // None: Bare Head & selected Hair clearly visible!
                customAppearance.vestId = 3;   // None: Clean Base Fatigues
                customAppearance.backpackId = 2; // Slick
            }
        }
        player.render(renderer, customAppearance, ui.getGameState() == GameState::InGame);

        // Render remote MMO players with full articulated skeletal animation, IK & single attached weapon
        NetworkEngine& net = NetworkEngine::get();
        for (const auto& [id, remote] : net.getRemotePlayers()) {
            if (!remote.active) continue;
            if (!remote.rig) {
                const_cast<RemotePlayer&>(remote).rig = std::make_shared<CharacterRig>();
                const_cast<RemotePlayer&>(remote).rig->snapYaw(remote.currentState.yaw);
            }

            glm::vec3 rPos = remote.currentState.position;
            glm::vec3 rVel = remote.currentState.velocity;
            // Deduce movement velocity from delta position if velocity was not streamed
            float moveDist = glm::distance(remote.targetState.position, remote.currentState.position);
            if (glm::length(rVel) < 0.05f && moveDist > 0.01f) {
                rVel = (remote.targetState.position - remote.currentState.position) * 12.0f;
            }

            float rYaw = remote.currentState.yaw;
            int rStance = std::clamp(static_cast<int>(remote.currentState.stance), 0, 2);
            bool rMoving = glm::length(glm::vec2(rVel.x, rVel.z)) > 0.06f;
            bool rSprinting = (remote.currentState.isSprinting != 0) && rMoving;
            bool rAiming = (remote.currentState.isAiming != 0);
            bool rFiring = false;

            std::string rWeapon = "None";
            if (remote.currentState.weaponId == 1) rWeapon = "M4A1";
            else if (remote.currentState.weaponId == 2) rWeapon = "AK-74M";
            else if (remote.currentState.weaponId == 3) rWeapon = "Mosin-Nagant";
            else if (remote.currentState.weaponId == 4) rWeapon = "Beretta M9";

            if (remote.currentState.isMounted) {
                rWeapon = "None";
            }

            float aimH = (rStance == 1 ? 0.95f : (rStance == 2 ? 0.35f : 1.40f));
            glm::vec3 aimTarget = rPos + glm::vec3(0.0f, aimH, 0.0f) + glm::vec3(std::sin(rYaw), 0.0f, std::cos(rYaw)) * 10.0f;

            remote.rig->setActiveWeaponName(rWeapon);
            remote.rig->update9D(dt, rPos, rYaw, aimTarget, rVel, rMoving, rSprinting, rStance, rFiring, rAiming, (rWeapon != "None"));

            CharacterCustomization remoteCustom;
            remoteCustom.skinToneId = remote.currentState.skinToneId;
            remoteCustom.faceId = remote.currentState.faceId;
            remoteCustom.hairId = remote.currentState.hairId;
            remoteCustom.beardId = remote.currentState.beardId;
            remoteCustom.backpackId = remote.currentState.backpackId;
            remoteCustom.gloveId = remote.currentState.gloveId;
            remoteCustom.helmetId = remote.currentState.helmetId;
            remoteCustom.vestId = remote.currentState.vestId;

            if (remote.currentState.faction == 1) {
                remoteCustom.camoId = (remote.currentState.camoId > 0) ? remote.currentState.camoId : 2;
                remoteCustom.armbandId = (remote.currentState.armbandId > 0) ? remote.currentState.armbandId : 1;
            } else {
                remoteCustom.camoId = (remote.currentState.camoId > 0) ? remote.currentState.camoId : 1;
                remoteCustom.armbandId = (remote.currentState.armbandId > 0) ? remote.currentState.armbandId : 0;
            }

            remote.rig->render(renderer, remoteCustom);
        }

        glm::mat4 decalVP = camera.getProjectionMatrix(window.getAspectRatio()) * camera.getViewMatrix();
        DecalSystem::get().render(decalVP, camera.getPosition(), renderer.getLight().direction, renderer.getLight().color);
        ballistics.render(renderer);
        renderer.endFrame();

        // ================= PASS 3: NEXT-GEN SSAO, SSGI BOUNCE & ACES TONEMAPPING =================
        postProcess.endRenderAndApply(camera.getViewMatrix(), camera.getProjectionMatrix(window.getAspectRatio()), player.getStats().blood, 0.0f);

        // ================= PASS 4: IMGUI TACTICAL HUD PASS =================
        ui.newFrame();
        ui.render(player, world, missions, &camera, window.getAspectRatio(), &frontline, &renderer);
        ui.endFrame();

        window.swapBuffers();

        if (autoCaptureFrames > 0) {
            autoCaptureFrames--;
            if (autoCaptureFrames == 0) {
                int w = window.getWidth();
                int h = window.getHeight();
                std::vector<uint8_t> pixels(w * h * 4);
                glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
                std::ofstream out(autoCaptureFile, std::ios::binary);
                uint8_t header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                       (uint8_t)(w & 0xFF), (uint8_t)((w >> 8) & 0xFF),
                                       (uint8_t)(h & 0xFF), (uint8_t)((h >> 8) & 0xFF),
                                       32, 0 };
                out.write(reinterpret_cast<char*>(header), 18);
                out.write(reinterpret_cast<char*>(pixels.data()), pixels.size());
                out.close();
                std::cout << "[Screen] Auto-captured " << autoCaptureFile << " (" << w << "x" << h 
                          << ") Cam: pitch=" << camera.getPitch() << " yaw=" << camera.getYaw() 
                          << " zoom=" << camera.getZoom() << " pos=(" << camera.getPosition().x 
                          << ", " << camera.getPosition().y << ", " << camera.getPosition().z << ")\n" << std::flush;
                glfwSetWindowShouldClose(window.getNativeWindow(), GLFW_TRUE);
            }
        }

        // Frame Rate Limiter (Real Engine Setting)
        int maxFps = ui.getMaxFps();
        if (maxFps < 3) {
            double targetSec = (maxFps == 0) ? (1.0 / 60.0) : ((maxFps == 1) ? (1.0 / 120.0) : (1.0 / 144.0));
            double curSec = glfwGetTime() - lastTime;
            while (curSec < targetSec) {
                #ifdef _WIN32
                Sleep(0);
                #endif
                curSec = glfwGetTime() - lastTime;
            }
        }

        // F12 Screenshot Hotkey
        static bool s_lastF12 = false;
        bool curF12 = (glfwGetKey(window.getNativeWindow(), GLFW_KEY_F12) == GLFW_PRESS);
        if (curF12 && !s_lastF12) {
            int w = window.getWidth();
            int h = window.getHeight();
            std::vector<uint8_t> pixels(w * h * 4);
            glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
            std::ofstream out("logs/game_frame.tga", std::ios::binary);
            uint8_t header[18] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   (uint8_t)(w & 0xFF), (uint8_t)((w >> 8) & 0xFF),
                                   (uint8_t)(h & 0xFF), (uint8_t)((h >> 8) & 0xFF),
                                   32, 0 };
            out.write(reinterpret_cast<char*>(header), 18);
            out.write(reinterpret_cast<char*>(pixels.data()), pixels.size());
            out.close();
            std::cout << "[Screen] Captured logs/game_frame.tga (" << w << "x" << h << ")\n" << std::flush;
        }
        s_lastF12 = curF12;
    }
}

void GameApp::shutdown() {
    std::cout << "[Mercenary Engine] Exiting cleanly.\n";
    VoiceChatManager::get().shutdown();
    NetworkEngine::get().shutdown();
    NavMeshManager::instance().shutdown();
    PhysicsEngine::get().shutdown();
    SteamIntegration::get().shutdown();
    AudioEngine::get().shutdown();
    m_isRunning = false;
}

} // namespace Mercenary

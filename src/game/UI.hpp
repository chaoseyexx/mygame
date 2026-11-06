#pragma once
#include <imgui.h>
#include "engine/Types.hpp"
#include "engine/Audio.hpp"
#include "Player.hpp"
#include "World.hpp"
#include "CharacterProfile.hpp"
#include "MissionSystem.hpp"
#include "ArmoryShop.hpp"
#include <string>
#include <unordered_map>

namespace Mercenary {

class Camera;
class FrontlineWarfare;
class Renderer;

enum class GameState {
    CharacterSelect, // First screen on game launch: Select or create operative
    MainMenu,        // Main menu game lobby (Tarkov style hideout with 3D character, matchmaking)
    InGame,          // Active warzone operations
    GameOver,        // Casualty report
    Extracted        // Raid debriefing: Survived & Extracted with all loot!
};

enum class MainMenuTab {
    Overview,       // Operator Dossier & Deployment Summary
    Deployment,     // Unified Pre-Raid Loadout & Armory Staging (No Stash / Direct Equip / Infiltrate)
    ExchangeMarket, // Real-time Gold Bullion & USD Dynamic Crypto/Commodities Exchange
    TacticalMap,    // Frontline Strategic Sector Map
    Customize       // Operative Appearance (Face, Skin, Hair, Camo, Helmet, Armor)
};

enum class PingType : uint8_t {
    HostileContact = 0,
    TacticalMove,
    Danger,
    UrgentAssist,
    Supplies
};

struct TacticalPing {
    PingType type = PingType::HostileContact;
    glm::vec3 worldPos{0.0f};
    std::string senderCallsign = "OPERATOR";
    float life = 9.0f;
    float maxLife = 9.0f;
    float pulseTime = 0.0f;
};

class UIManager {
public:
    UIManager();
    ~UIManager();

    void init(GLFWwindow* window);
    void newFrame();
    void render(Player& player, World& world, MissionSystem& missions, const Camera* camera = nullptr, float aspectRatio = 1.777f, FrontlineWarfare* frontline = nullptr, Renderer* renderer = nullptr);
    void renderOverheadNameplates(const Camera& camera, float aspectRatio, Player& player, FrontlineWarfare* frontline = nullptr);
    void endFrame();

    enum class SelectedGameMode {
        TeamDeathmatch = 0,
        Infiltration = 1,
        OpenWorld = 2
    };

    bool isTrainingMode() const { return m_isTrainingMode; }
    void setTrainingMode(bool training) { m_isTrainingMode = training; }
    SelectedGameMode getSelectedGameMode() const { return m_selectedGameMode; }
    void setSelectedGameMode(SelectedGameMode mode) { m_selectedGameMode = mode; }
    void setTdmScoreboardOpen(bool open) { m_showTdmScoreboard = open; }
    bool isTdmScoreboardOpen() const { return m_showTdmScoreboard; }
    float getTdmRespawnTimer() const { return m_tdmRespawnTimer; }
    bool isTdmAgainstAi() const { return m_tdmAgainstAi; }
    void setTdmAgainstAi(bool againstAi) { m_tdmAgainstAi = againstAi; }
    void setTdmRespawnTimer(float timer) { m_tdmRespawnTimer = timer; }
    float getTdmEndTimer() const { return m_tdmEndTimer; }
    void setTdmEndTimer(float timer) { m_tdmEndTimer = timer; }
    bool isOnlineMultiplayer() const { return m_isOnlineMultiplayer; }
    void setOnlineMultiplayer(bool online) { m_isOnlineMultiplayer = online; }
    bool isDeploying() const { return m_isDeploying; }
    void setDeploying(bool deploying) { m_isDeploying = deploying; m_deploymentTimer = 0.8f; }
    void setServerOfflineModalOpen(bool open, const std::string& msg = "") { m_serverOfflineModalOpen = open; m_serverOfflineModalMsg = msg; }
    void setEscapeMenuOpen(bool open) { m_escapeMenuOpen = open; }

    bool isChatOpen() const;
    void toggleChat();
    void openChat();
    void closeChat();

    bool isInventoryOpen() const { return m_inventoryOpen; }
    void toggleInventory() { m_inventoryOpen = !m_inventoryOpen; }

    bool isConstructionMenuOpen() const { return m_constructionMenuOpen; }
    void toggleConstructionMenu() { m_constructionMenuOpen = !m_constructionMenuOpen; }
    void setConstructionMenuOpen(bool open) { m_constructionMenuOpen = open; }

    bool isWeaponWheelOpen() const { return m_weaponWheelOpen; }
    void setWeaponWheelOpen(bool open, Player* player = nullptr, World* world = nullptr);

    bool isAlertWheelOpen() const { return m_alertWheelOpen; }
    void setAlertWheelOpen(bool open) {
        if (open && !m_alertWheelOpen) m_alertWheelAnimTime = 0.0f;
        m_alertWheelOpen = open;
    }
    int getSelectedAlertIndex() const { return m_selectedAlertIndex; }

    // Tactical Datalink Pings (Enemy Spotted, Waypoint, Danger, Backup, Supplies)
    void addTacticalPing(PingType type, const glm::vec3& worldPos, const std::string& senderCallsign = "OPERATOR");
    void updateTacticalPings(float dt);
    void renderTacticalPings(const Camera& camera, float aspectRatio, const Player& player);
    const std::vector<TacticalPing>& getTacticalPings() const { return m_tacticalPings; }

    // F3 Real-Time Acoustic & Sensor Debug Visualizer
    bool m_audioDebugVisualizerEnabled = false;
    void toggleAudioDebugVisualizer() { m_audioDebugVisualizerEnabled = !m_audioDebugVisualizerEnabled; }
    bool isAudioDebugVisualizerEnabled() const { return m_audioDebugVisualizerEnabled; }
    bool m_cursorFreed = false;
    void setCursorFreed(bool freed) { m_cursorFreed = freed; }
    bool isCursorFreed() const { return m_cursorFreed; }
    void setAudioDebugVisualizerEnabled(bool val) { m_audioDebugVisualizerEnabled = val; }
    void renderAcousticDebugVisualizer(const Player& player, const Camera& camera, float aspectRatio, float screenW, float screenH);
    void clearTacticalPings() { m_tacticalPings.clear(); }



    bool isMapOpen() const { return m_mapOpen; }
    void toggleMap() { m_mapOpen = !m_mapOpen; }

    bool isEscapeMenuOpen() const { return m_escapeMenuOpen; }
    void toggleEscapeMenu() { m_escapeMenuOpen = !m_escapeMenuOpen; }
    void closeEscapeMenu() { m_escapeMenuOpen = false; }

    bool isOptionsOpen() const { return m_optionsOpen; }
    bool isInvertScroll() const { return m_invertScroll; }
    void setInvertScroll(bool inv) { m_invertScroll = inv; }
    void openRecruitModal(int slotIdx) {
        m_showRecruitModal = true;
        m_recruitSlotIndex = slotIdx;
        m_recruitFaction = 1;
        m_recruitNameBuf[0] = '\0';
    }
    void toggleOptions() { m_optionsOpen = !m_optionsOpen; }


    GameState getGameState() const { return m_gameState; }
    void setGameState(GameState state) { m_gameState = state; }

    void triggerHitMarker(bool isCritical = false) {
        m_hitMarkerTimer = 0.16f;
        m_hitMarkerCritical = isCritical;
        AudioEngine::get().playSound(SoundType::TacticalClick, 0.45f, isCritical ? 1.45f : 1.20f);
    }

    void showTargetHitBanner(const std::string& text) {
        m_targetHitBannerTimer = 2.4f;
        m_targetHitBannerText = text;
    }

    MainMenuTab getMenuTab() const { return m_currentTab; }
    void setMenuTab(MainMenuTab tab) { m_currentTab = tab; }

    ArmoryShop& getArmory() { return m_armory; }

    bool isSquadMode() const { return m_squadMode; }

    void inspectWeapon(const Item& item);

    float getOperatorTurntable() const { return m_operatorTurntableYaw; }
    void setOperatorTurntable(float y) { m_operatorTurntableYaw = y; }
    void addOperatorTurntable(float d) { m_operatorTurntableYaw += d; }
    bool isCustomizeFaceZoom() const { return m_customizeFaceZoom; }
    void setCustomizeFaceZoom(bool z) { m_customizeFaceZoom = z; }
    void setCustomizeTab(int tab) { m_customizeTab = tab; }
    void setSettingsTab(int tab) { m_settingsTab = tab; }
    bool isPreviewCombatGear() const { return m_previewCombatGearInCustomize; }

    void setFullscreenCallbacks(std::function<void()> toggleCb, std::function<bool()> queryCb) {
        m_fullscreenToggleCb = toggleCb;
        m_isFullscreenQuery = queryCb;
    }
    void setWindowModeCallback(std::function<void(int)> cb) { m_windowModeCb = cb; }
    void setWindowMode(int mode) { m_optWindowMode = mode; }
    int getWindowMode() const { return m_optWindowMode; }
    void setResolutionCallback(std::function<void(int)> cb) { m_resolutionCb = cb; }
    int getResolutionIndex() const { return m_optResolution; }
    void setResolutionIndex(int idx) { m_optResolution = idx; }
    void setVsyncCallback(std::function<void(bool)> cb) { m_vsyncCb = cb; }

    void loadSettings();
    void saveSettings();

    bool isAmbientOcclusionEnabled() const { return m_optAmbientOcclusion; }
    bool isMotionBlurEnabled() const { return m_optMotionBlur; }
    bool isWeatherParticlesEnabled() const { return m_weatherParticles; }
    bool isShadowQualityHigh() const { return m_shadowQualityHigh; }
    float getFov() const { return 78.0f; }
    int getCameraPreset() const { return m_cameraPreset; }
    void setCameraPreset(int p) { m_cameraPreset = p; }
    int getMaxFps() const { return m_optMaxFps; }
    bool getVsync() const { return m_vsync; }

    float getMasterVolume() const { return m_masterVolume; }
    float getSfxVolume() const { return m_sfxVolume; }
    float getAmbientVolume() const { return m_ambientVolume; }
    float getVoiceVolume() const { return m_voiceVolume; }
    void setMasterVolume(float v) { m_masterVolume = v; }
    void setSfxVolume(float v) { m_sfxVolume = v; }
    void setAmbientVolume(float v) { m_ambientVolume = v; }
    void setVoiceVolume(float v) { m_voiceVolume = v; }

    float getMouseSensitivity() const { return m_mouseSensitivity; }
    float getAdsSensitivity() const { return m_adsSensitivity; }
    bool isInvertY() const { return m_invertY; }

private:
    float m_hitMarkerTimer = 0.0f;
    bool m_hitMarkerCritical = false;
    float m_targetHitBannerTimer = 0.0f;
    std::string m_targetHitBannerText = "";
    GameState m_gameState = GameState::CharacterSelect; // Startup: select or create character first!
    MainMenuTab m_currentTab = MainMenuTab::Overview;

    bool m_inventoryOpen = false;
    bool m_constructionMenuOpen = false;
    int m_selectedStructureType = 1;
    bool m_weaponWheelOpen = false;
    int m_selectedWheelSector = 0;
    float m_wheelAnimTime = 0.0f;

    bool m_alertWheelOpen = false;
    float m_alertWheelAnimTime = 0.0f;
    int m_selectedAlertIndex = -1;
    std::vector<TacticalPing> m_tacticalPings;


    bool m_mapOpen = false;
    bool m_escapeMenuOpen = false;
    bool m_optionsOpen = false;

    // MMO Real-time Operator Customization & Turntable
    float m_operatorTurntableYaw = 0.0f;
    bool m_customizeFaceZoom = true; // Zoom in to face/hair by default in customization!
    bool m_previewCombatGearInCustomize = false; // Toggle helmet and vest preview

    // Tactical Deployment Sequence & Offline Training System
    SelectedGameMode m_selectedGameMode = SelectedGameMode::TeamDeathmatch;
    bool m_showTdmScoreboard = false;
    float m_tdmRespawnTimer = 0.0f;
    float m_tdmEndTimer = 0.0f;
    bool m_isTrainingMode = false;              // Offline combat simulation & firing range mode
    bool m_isOnlineMultiplayer = false;         // Online dedicated shard session vs offline solo
    bool m_isDeploying = false;                 // Fullscreen tactical deployment briefing active
    float m_deploymentTimer = 0.0f;             // Deployment briefing & asset warming timer
    int m_currentTacticalTip = 0;               // Rotating operational tips index
    float m_tipRotateTimer = 0.0f;              // Timer to cycle field tips
    bool m_serverOfflineModalOpen = false;      // Dedicated server offline warning modal
    std::string m_serverOfflineModalMsg = "";   // Detailed reason for unreachable shard

    // Matchmaking system (Tarkov style)
    bool m_squadMode = false;       // Toggle: Solo vs Squad
    bool m_tdmAgainstAi = true;     // TDM 5v5 Opponent: true = Against AI (Bots), false = Players Only (PvP)
    bool m_isMatchmaking = false;   // Finding match queue
    bool m_connectionError = false; // Server unreachable error state
    float m_matchTimer = 0.0f;      // Queue countdown
    int m_matchmakingPlaylist = 0;  // 0=5v5 TDM, 1=Tactical Infiltration, 2=Persistent MMO Warzone
    bool m_matchmakingTimeout = false;
    bool m_deployUseRubles = false; // Currency toggle for deployment shop
    int m_deployShopCategory = 0;   // 0=All, 1=Weapons, 2=Ammo, 3=Meds, 4=Gear

    // Exchange Market Terminal State
    int m_exchangeQuantity = 1;
    bool m_exchangeIsBuyTab = true;

    // Traders State
    size_t m_selectedTrader = 0;
    int m_traderCategoryFilter = 0;
    int m_selectedShopItemIndex = 0; // Default first item inspected (Wardogs style)
    int m_selectedLoadoutSlot = 0;   // 0=Primary, 1=Secondary, 2=Armor, 3=Helmet

    // Weapon Inspection Modal
    bool m_showWeaponInspect = false;
    Item m_inspectedItem;

    // Comprehensive Settings State (100% Functional & Verified Engine Controls)
    int m_settingsTab = 0; // 0=Display, 1=Graphics, 2=Sound, 3=Controls, 4=Key Bindings
    int m_rebindingAction = -1; // -1 = None, >= 0 = Index of action awaiting key press
    int m_hoveredSetting = 0;

    // Display
    int m_optWindowMode = 0; // 0=Borderless Windowed, 1=Exclusive Fullscreen
    int m_optResolution = 1; // 0=1600x900, 1=1920x1080, 2=2560x1440
    int m_optMaxFps = 3;     // 0=60 FPS, 1=120 FPS, 2=144 FPS, 3=Uncapped
    bool m_vsync = true;

    // Graphics
    bool m_optAmbientOcclusion = true; // SSAO Pass
    bool m_optMotionBlur = false;
    bool m_weatherParticles = true;
    bool m_shadowQualityHigh = true;
    int m_cameraPreset = 0;  // 0=Tactical 22.5, 1=Dynamic 30, 2=Isometric 45

    // Sound
    float m_masterVolume = 1.0f;
    float m_sfxVolume = 0.85f;
    float m_ambientVolume = 0.70f;
    float m_voiceVolume = 0.90f;

    // Controls
    float m_mouseSensitivity = 1.0f;
    float m_adsSensitivity = 0.80f;
    bool m_invertY = false;
    bool m_invertScroll = false;

    // Visual Customization Workflow Sub-Tabs
    int m_customizeTab = 0; // 0=Appearance/Hair, 1=Uniform/Camo, 2=Loadout Presets, 3=Turntable/View

    ArmoryShop m_armory;

    // High-Resolution Modern Typography (Segoe UI TrueType)
    ImFont* m_fontRegular = nullptr;
    ImFont* m_fontBold = nullptr;
    ImFont* m_fontTitle = nullptr;
    ImFont* m_fontHeading = nullptr;
    ImFont* m_fontSmall = nullptr;

    // Screens & Modals
    bool m_showRecruitModal = false;
    int m_recruitSlotIndex = 0;
    int m_recruitFaction = 1;
    char m_recruitNameBuf[64] = "";
    void renderRecruitOperativeModal(Player& player);
    void renderInGameAuthScreen(Player& player);

    // In-Game Central Server Authentication State
    int m_authTab = 0; // 0: Login, 1: Register
    char m_authUsernameBuf[32] = "";
    char m_authPasswordBuf[32] = "";
    char m_regUsernameBuf[32] = "";
    char m_regPasswordBuf[32] = "";
    char m_regCallsignBuf[32] = "";
    int m_regFaction = 0; // 0: Coalition, 1: Crimson
    bool m_steamAuthInitiated = false;
    void renderCharacterSelectScreen(Player& player);
    void renderLobbyMainMenu(Player& player, const World& world, const MissionSystem& missions);
    void renderDeploymentStaging(Player& player, CharacterProfile* active, const World& world, const MissionSystem& missions);
    void renderTopNavigationBar(Player& player, CharacterProfile* active);
    void renderMinimalistHUD(Player& player, const World& world, const MissionSystem& missions);
    void renderVoiceChatHUD(const Player& player, float screenW, float screenH);
    void renderTacticalMap(const Player& player, const World& world, const MissionSystem& missions);
    void renderInventoryWindow(Player& player);
    void renderDeathScreen(Player& player);
    void renderExtractionScreen(Player& player);
    void renderInteractionPrompt(Player& player, World& world, const Camera* camera = nullptr, float aspectRatio = 1.777f, FrontlineWarfare* frontline = nullptr);
    World* m_currentWorld = nullptr;
    FrontlineWarfare* m_currentFrontline = nullptr;
    void renderInGameEscapePopup(Player& player, World* world = nullptr, FrontlineWarfare* frontline = nullptr);
    void renderOptionsModal();
    void renderWeaponInspectModal();
    void renderWeaponWheel(Player& player, World& world);
    void renderConstructionHUD(Player& player, World& world);
    void renderAlertRadialMenu(Player& player, const glm::vec3& mouseWorldPos, const AimTargetInfo* aimInfo = nullptr);

    void renderCustomizationMenu(CharacterProfile* active, Player* player = nullptr);

    // Arena Breakout: Infinite Visual Inventory & Paperdoll Components
    void drawProceduralItemIcon(ImDrawList* dl, const Item& item, ImVec2 center, float width, float height);
    void drawSlotWatermarkIcon(ImDrawList* dl, const char* slotName, ImVec2 center, float size);
    void drawVisualItemSlot(Item& item, size_t index, bool isEquipped, EquipSlot slot, float width = 72.0f, float height = 72.0f, Player* player = nullptr);
    void drawEmptyVisualSlot(const char* slotName, const char* iconText, float width = 72.0f, float height = 72.0f);
    void drawArenaBreakoutPaperdoll(Player& player, CharacterProfile* active, bool isInGame = false);
    void drawArenaBreakoutGridStash(Player& player, CharacterProfile* active, bool isInGame = false);
    void renderExchangeMarket(Player& player, CharacterProfile* active);
    void renderBottomRightMatchmakingDock(Player& player, CharacterProfile* active);
    void renderTacticalDeploymentScreen(Player& player, CharacterProfile* active, World* world = nullptr, FrontlineWarfare* frontline = nullptr, Renderer* renderer = nullptr);
    void renderTdmMatchHUD(const FrontlineWarfare* frontline, float screenW, float screenH);
    void renderTdmScoreboard(const FrontlineWarfare* frontline, const Player& player);
    void renderTdmRespawnOverlay(float screenW, float screenH);
    void renderServerOfflineModal(Player& player, CharacterProfile* active);
    bool renderCompetitorButton(const char* label, const ImVec2& size = ImVec2(0, 0), bool isSelected = false, bool isHighlighted = false, ImDrawFlags roundFlags = ImDrawFlags_RoundCornersAll);

    std::function<void()> m_fullscreenToggleCb = nullptr;
    std::function<bool()> m_isFullscreenQuery = nullptr;
    std::function<void(int)> m_windowModeCb = nullptr;
    std::function<void(int)> m_resolutionCb = nullptr;
    std::function<void(bool)> m_vsyncCb = nullptr;

    int m_inventoryFilter = 0; // 0=All, 1=Weapons, 2=Ammo, 3=Meds, 4=Gear

    // Tactical Audit & Locomotion Debug Inspector
    bool m_filterLoco = true;
    bool m_filterAudio = true;
    bool m_filterCombat = true;
    bool m_filterWorld = true;
    bool m_filterSystem = true;

    void applySleekMilitaryTheme();

    // Smooth & Light Overhead Nameplate State
    struct SmoothedNameplate {
        glm::vec2 pos{0.0f};
        float alpha = 0.0f;
        bool initialized = false;
    };
    SmoothedNameplate m_playerNameplate;
    std::unordered_map<uint32_t, SmoothedNameplate> m_botNameplates;
};

} // namespace Mercenary

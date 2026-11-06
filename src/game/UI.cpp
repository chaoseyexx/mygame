#include <filesystem>
#include <fstream>
#include <iomanip>
#include "UI.hpp"
#include "engine/InputConfig.hpp"
#include "ChatSystem.hpp"
#include "engine/VoiceChat.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "engine/Audio.hpp"
#include "engine/SteamIntegration.hpp"
#include "engine/Network.hpp"
#include "engine/Camera.hpp"
#include "FrontlineAI.hpp"
#include "ExchangeMarket.hpp"
#include <cmath>
#include <algorithm>

namespace Mercenary {


bool UIManager::isChatOpen() const {
    return ChatSystem::get().isChatOpen();
}
void UIManager::toggleChat() {
    ChatSystem::get().toggleChat();
}
void UIManager::openChat() {
    ChatSystem::get().openChat();
}
void UIManager::closeChat() {
    ChatSystem::get().closeChat();
}

UIManager::UIManager() {}

UIManager::~UIManager() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UIManager::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load High-Resolution Modern Typography (Segoe UI TrueType Fonts - Matching Launcher)
    namespace fs = std::filesystem;
    if (fs::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
        m_fontRegular = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 15.0f);
        m_fontSmall   = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 12.0f);
    }
    if (fs::exists("C:\\Windows\\Fonts\\segoeuib.ttf")) {
        m_fontBold    = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 15.0f);
        m_fontHeading = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 20.0f);
        m_fontTitle   = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 30.0f);
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    applySleekMilitaryTheme();
}

// =========================================================================
// 0. AUTHENTIC TARKOV & WARDOGS DARK GUNMETAL & AMBER THEME
// =========================================================================
void UIManager::applySleekMilitaryTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Modern AAA Obsidian Glass & Radiant Blue Aesthetic (Matching Desktop Launcher)
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 5.0f;

    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.ChildBorderSize   = 1.0f;

    style.WindowPadding     = ImVec2(16, 14);
    style.FramePadding      = ImVec2(12, 7);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(6, 6);

    ImVec4* colors = style.Colors;

    // Obsidian Dark Glass with Radiant Battle.net Blue Accents
    colors[ImGuiCol_Text]                  = ImVec4(0.94f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.55f, 0.62f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.06f, 0.07f, 0.09f, 0.94f); // Deep matte obsidian
    colors[ImGuiCol_ChildBg]               = ImVec4(0.09f, 0.11f, 0.15f, 0.85f); // Frosted dark slate
    colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.10f, 0.14f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.18f, 0.22f, 0.28f, 0.70f); // Sleek hairline slate
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.12f, 0.15f, 0.20f, 0.90f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.18f, 0.23f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.22f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.06f, 0.07f, 0.08f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.06f, 0.07f, 0.09f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.28f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.00f, 0.47f, 0.95f, 1.00f); // Radiant blue
    colors[ImGuiCol_CheckMark]             = ImVec4(0.00f, 0.55f, 0.98f, 1.00f); // Blue checkmark
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.00f, 0.47f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.15f, 0.60f, 1.00f, 1.00f);

    // Tactical Buttons (Radiant Blue hover/active)
    colors[ImGuiCol_Button]                = ImVec4(0.14f, 0.18f, 0.24f, 0.95f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.20f, 0.26f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.00f, 0.47f, 0.95f, 1.00f);

    // Headers & Tabs
    colors[ImGuiCol_Header]                = ImVec4(0.16f, 0.20f, 0.28f, 0.85f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.28f, 0.38f, 0.95f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.47f, 0.95f, 0.95f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.10f, 0.12f, 0.16f, 0.90f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.18f, 0.24f, 0.32f, 1.00f);
    colors[ImGuiCol_TabSelected]           = ImVec4(0.00f, 0.47f, 0.95f, 0.95f);
    colors[ImGuiCol_TabDimmed]             = ImVec4(0.08f, 0.10f, 0.14f, 0.90f);
    colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.14f, 0.18f, 0.24f, 1.00f);

    colors[ImGuiCol_PlotHistogram]         = ImVec4(0.00f, 0.55f, 0.98f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.20f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.25f, 0.32f, 0.60f);
}

void UIManager::newFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::render(Player& player, World& world, MissionSystem& missions, const Camera* camera, float aspectRatio, FrontlineWarfare* frontline, Renderer* renderer) {
    // If Tactical Deployment & Loading Briefing is active, render full-screen over everything
    if (m_isDeploying) {
        renderTacticalDeploymentScreen(player, ProfileManager::get().getActiveProfile(), &world, frontline, renderer);
        if (m_audioDebugVisualizerEnabled && camera) {
            ImGuiIO& io = ImGui::GetIO();
            renderAcousticDebugVisualizer(player, *camera, aspectRatio, io.DisplaySize.x, io.DisplaySize.y);
        }
        return;
    }

    // If Options/Settings is open, render Settings modal exclusively over the cinematic 3D viewport
    if (m_optionsOpen) {
        renderOptionsModal();
        return;
    }

    if (m_gameState == GameState::CharacterSelect) {
        renderCharacterSelectScreen(player);
        if (m_audioDebugVisualizerEnabled && camera) {
            ImGuiIO& io = ImGui::GetIO();
            renderAcousticDebugVisualizer(player, *camera, aspectRatio, io.DisplaySize.x, io.DisplaySize.y);
        }
        return;
    }

    if (m_gameState == GameState::MainMenu) {
        renderLobbyMainMenu(player, world, missions);
        if (m_serverOfflineModalOpen) {
            renderServerOfflineModal(player, ProfileManager::get().getActiveProfile());
        }
        if (m_audioDebugVisualizerEnabled && camera) {
            ImGuiIO& io = ImGui::GetIO();
            renderAcousticDebugVisualizer(player, *camera, aspectRatio, io.DisplaySize.x, io.DisplaySize.y);
        }
        return;
    }

    if (m_gameState == GameState::GameOver) {
        renderDeathScreen(player);
        return;
    }

    if (m_gameState == GameState::Extracted) {
        renderExtractionScreen(player);
        return;
    }

    // In-Game Clean HUD, Overhead Nameplates & Modals
    m_currentWorld = &world;
    m_currentFrontline = frontline;
    renderMinimalistHUD(player, world, missions);
    if (!m_isTrainingMode && frontline) {
        ImGuiIO& io = ImGui::GetIO();
        renderTdmMatchHUD(frontline, io.DisplaySize.x, io.DisplaySize.y);
        if (m_showTdmScoreboard) {
            renderTdmScoreboard(frontline, player);
        }
    }
    if (m_tdmRespawnTimer > 0.0f) {
        ImGuiIO& io = ImGui::GetIO();
        renderTdmRespawnOverlay(io.DisplaySize.x, io.DisplaySize.y);
    }
    if (camera) {
        renderOverheadNameplates(*camera, aspectRatio, player, frontline);
        renderTacticalPings(*camera, aspectRatio, player);
        if (m_audioDebugVisualizerEnabled) {
            ImGuiIO& io = ImGui::GetIO();
            renderAcousticDebugVisualizer(player, *camera, aspectRatio, io.DisplaySize.x, io.DisplaySize.y);
        }
    }
    renderInteractionPrompt(player, world, camera, aspectRatio, frontline);
    ChatSystem::get().renderChatInputWindow(player, m_currentWorld, frontline);
    renderInventoryWindow(player);
    renderTacticalMap(player, world, missions);
    renderInGameEscapePopup(player, &world, frontline);
    if (m_weaponWheelOpen) {
        renderWeaponWheel(player, world);
    }
    if (m_constructionMenuOpen) {
        renderConstructionHUD(player, world);
    }
    if (m_alertWheelOpen) {
        renderAlertRadialMenu(player, player.getAimTarget());
    }

    // Live Systems Audit & Telemetry Inspector (F1 / F3)
}

void UIManager::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace Mercenary

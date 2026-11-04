#define WIN32_LEAN_AND_MEAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include "ServerGUI.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace Mercenary {

ServerGUI::ServerGUI() {}

ServerGUI::~ServerGUI() {
    shutdown();
}

void ServerGUI::applyDarkTacticalTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.11f, 0.13f, 0.16f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.10f, 0.12f, 0.15f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.24f, 0.30f, 0.80f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.14f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.18f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.22f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.12f, 0.18f, 0.25f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.06f, 0.07f, 0.09f, 0.60f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.09f, 0.11f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.26f, 0.32f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.28f, 0.34f, 0.42f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.35f, 0.42f, 0.52f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.35f, 0.85f, 0.55f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.25f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.35f, 0.68f, 0.98f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.16f, 0.22f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.22f, 0.32f, 0.42f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.28f, 0.40f, 0.54f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.16f, 0.22f, 0.29f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.26f, 0.36f, 0.48f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.24f, 0.30f, 0.80f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.28f, 0.36f, 0.46f, 1.00f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.35f, 0.45f, 0.58f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.20f, 0.25f, 0.32f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.30f, 0.40f, 0.52f, 0.80f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.12f, 0.15f, 0.19f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.32f, 0.44f, 1.00f);
    colors[ImGuiCol_TabSelected]           = ImVec4(0.18f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_TabDimmed]             = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.14f, 0.19f, 0.25f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.12f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = ImVec4(0.22f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_TableBorderLight]      = ImVec4(0.16f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
}

bool ServerGUI::init(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;

    if (!glfwInit()) {
        std::cerr << "[ServerGUI Error] Failed to initialize GLFW.\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "MERCENARY: WARZONE // CONTINENTAL SERVER COMMAND CENTER", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "[ServerGUI Error] Failed to create GLFW window.\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable VSync

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "[ServerGUI Error] Failed to initialize GLAD.\n";
        return false;
    }

    // Windows 11 Dark Mica / Acrylic Border Window Attribute
    HWND hwnd = glfwGetWin32Window(m_window);
    if (hwnd) {
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, 20, &useDarkMode, sizeof(useDarkMode));
    }

    // ImGui Initialization
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    applyDarkTacticalTheme();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    m_server = std::make_unique<HubServer>();
    return true;
}

void ServerGUI::startServerThread() {
    if (m_isServerRunning.load()) return;

    if (!m_server->init(static_cast<uint16_t>(m_serverPort))) {
        m_serverStatusText = "ERROR: FAILED TO BIND UDP PORT";
        return;
    }

    m_isServerRunning = true;
    m_serverStatusText = "ONLINE - CONTINENTAL MESH ACTIVE";
    m_serverThread = std::thread([this]() {
        m_server->run();
        m_isServerRunning = false;
        m_serverStatusText = "STANDBY / OFFLINE";
    });
}

void ServerGUI::stopServerThread() {
    if (!m_isServerRunning.load()) return;

    m_server->stop();
    if (m_serverThread.joinable()) {
        m_serverThread.join();
    }
    m_isServerRunning = false;
    m_serverStatusText = "STANDBY / OFFLINE";
}

void ServerGUI::run() {
    // Auto-start server on GUI launch
    startServerThread();

    auto lastTime = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        if (m_isServerRunning.load()) {
            m_uptimeSeconds += dt;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int displayW, displayH;
        glfwGetFramebufferSize(m_window, &displayW, &displayH);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(displayW), static_cast<float>(displayH)));
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus;

        if (ImGui::Begin("MasterServerControl", nullptr, windowFlags)) {
            renderHeaderBar();
            renderTabNavigation();

            ImGui::Separator();
            ImGui::Spacing();

            switch (m_activeTab) {
                case 0: renderTabSessions(); break;
                case 1: renderTabHexWarzone(); break;
                case 2: renderTabPlayers(); break;
                case 3: renderTabSettings(); break;
                case 4: renderTabConsole(); break;
                default: renderTabSessions(); break;
            }
        }
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }

    stopServerThread();
}

void ServerGUI::shutdown() {
    stopServerThread();

    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
    }
}

void ServerGUI::renderHeaderBar() {
    float width = ImGui::GetContentRegionAvail().x;

    // Tactical Banner
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "MERCENARY: WARZONE");
    ImGui::SameLine();
    ImGui::TextDisabled("// CONTINENTAL COMMAND & OPERATIONS SUITE v2.0");

    ImGui::SameLine(width - 340.0f);

    // Status Pill
    bool running = m_isServerRunning.load();
    ImVec4 statusBg = running ? ImVec4(0.12f, 0.35f, 0.20f, 1.0f) : ImVec4(0.35f, 0.15f, 0.15f, 1.0f);
    ImVec4 statusText = running ? ImVec4(0.35f, 0.95f, 0.50f, 1.0f) : ImVec4(0.95f, 0.40f, 0.40f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, statusBg);
    ImGui::PushStyleColor(ImGuiCol_Text, statusText);
    std::string statusStr = running ? "[ONLINE: 64 Hz]" : "[OFFLINE / STOPPED]";
    ImGui::Button(statusStr.c_str(), ImVec2(140, 26));
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    // Start / Stop Toggle Button
    if (running) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.50f, 0.18f, 0.18f, 1.0f));
        if (ImGui::Button("STOP SERVER", ImVec2(100, 26))) {
            stopServerThread();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.50f, 0.28f, 1.0f));
        if (ImGui::Button("START SERVER", ImVec2(100, 26))) {
            startServerThread();
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button("RESTART", ImVec2(75, 26))) {
        stopServerThread();
        startServerThread();
    }

    ImGui::Spacing();
}

void ServerGUI::renderTabNavigation() {
    const char* tabs[] = {
        "LIVE SESSIONS & ROOMS",
        "6-HEX WARZONE THEATER",
        "OPERATIVES ROSTER",
        "SERVER SETTINGS",
        "TACTICAL CONSOLE & LOGS"
    };

    for (int i = 0; i < 5; ++i) {
        if (i > 0) ImGui::SameLine(0, 8);
        bool isSel = (m_activeTab == i);
        if (isSel) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.48f, 0.75f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.13f, 0.17f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.70f, 0.78f, 1.0f));
        }
        if (ImGui::Button(tabs[i], ImVec2(200, 32))) {
            m_activeTab = i;
        }
        ImGui::PopStyleColor(2);
    }
}

void ServerGUI::renderTabSessions() {
    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = (availW - 30.0f) / 4.0f;
    float cardH = 80.0f;

    auto sessions = m_server ? m_server->getSessionsSnapshot() : std::vector<SessionSummary>{};
    size_t totalPlayers = m_server ? m_server->getConnectedClientCount() : 0;

    // Card 1: Active Micro-Instances
    ImGui::BeginChild("Card1", ImVec2(cardW, cardH), true);
    ImGui::TextDisabled("ACTIVE ROOMS & SESSIONS");
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%zu Rooms Active", sessions.size());
    ImGui::TextDisabled("1 Persistent | %zu On-Demand", sessions.size() > 1 ? sessions.size() - 1 : 0);
    ImGui::EndChild();

    ImGui::SameLine(0, 10);

    // Card 2: Total Connected Operatives
    ImGui::BeginChild("Card2", ImVec2(cardW, cardH), true);
    ImGui::TextDisabled("GLOBAL OPERATIVES");
    ImGui::TextColored(ImVec4(0.40f, 0.95f, 0.45f, 1.0f), "%zu / %d", totalPlayers, m_maxPlayers);
    ImGui::TextDisabled("Fleet Cap: %d Operatives", m_maxPlayers);
    ImGui::EndChild();

    ImGui::SameLine(0, 10);

    // Card 3: Simulation Tickrate & Uptime
    ImGui::BeginChild("Card3", ImVec2(cardW, cardH), true);
    ImGui::TextDisabled("SIMULATION TICKRATE");
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "64.0 Hz [STABLE]");
    int hrs = static_cast<int>(m_uptimeSeconds) / 3600;
    int mins = (static_cast<int>(m_uptimeSeconds) % 3600) / 60;
    int secs = static_cast<int>(m_uptimeSeconds) % 60;
    char upBuf[32];
    std::snprintf(upBuf, sizeof(upBuf), "Uptime: %02d:%02d:%02d", hrs, mins, secs);
    ImGui::TextDisabled("%s", upBuf);
    ImGui::EndChild();

    ImGui::SameLine(0, 10);

    // Card 4: Fleet Ingress & Discovery
    ImGui::BeginChild("Card4", ImVec2(cardW, cardH), true);
    ImGui::TextDisabled("FLEET INGRESS / PUBLIC IP");
    std::string pubIp = m_server ? m_server->getPublicIP() : "127.0.0.1";
    ImGui::TextColored(ImVec4(0.80f, 0.50f, 1.0f, 1.0f), "%s:%d", pubIp.c_str(), m_serverPort);
    ImGui::TextDisabled("Cloud Discovery: Active");
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // System Broadcast Input
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "SYSTEM BROADCAST TO ALL ONLINE OPERATIVES:");
    ImGui::PushItemWidth(availW - 190.0f);
    ImGui::InputText("##BroadcastInput", m_broadcastMsg, sizeof(m_broadcastMsg));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("SEND ANNOUNCEMENT", ImVec2(175, 26))) {
        if (m_server && m_broadcastMsg[0] != '\0') {
            m_server->broadcastSystemMessage(std::string("[COMMAND] ") + m_broadcastMsg);
            m_broadcastMsg[0] = '\0';
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Session Management Controls
    ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.95f, 1.0f), "LIVE GAME SESSIONS MATRIX (ON-DEMAND MICRO-SERVERS)");
    ImGui::SameLine(availW - 460.0f);
    if (ImGui::Button("+ SPAWN TDM (KILLHOUSE)", ImVec2(170, 24))) {
        if (m_server) m_server->spawnCustomTdm("killhouse");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ SPAWN TDM (INDUSTRIAL)", ImVec2(170, 24))) {
        if (m_server) m_server->spawnCustomTdm("industrial");
    }
    ImGui::SameLine();
    if (ImGui::Button("+ SPAWN RAID (40M)", ImVec2(110, 24))) {
        if (m_server) m_server->spawnCustomInfiltration("fob_vanguard");
    }

    ImGui::Spacing();

    // Sessions Table
    if (ImGui::BeginTable("SessionsTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Game Mode", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Map / Theater", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Operatives", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Match Status & Timers", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Lifecycle", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& sess : sessions) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("#%u", sess.id);

            ImGui::TableSetColumnIndex(1);
            if (sess.mode == GameMode::OpenWorld) {
                ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "OPEN WORLD MMO");
            } else if (sess.mode == GameMode::TeamDeathmatch) {
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.25f, 1.0f), "TEAM DEATHMATCH");
            } else {
                ImGui::TextColored(ImVec4(0.85f, 0.45f, 1.0f, 1.0f), "EXTRACTION RAID");
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", sess.mapName.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u / %u", sess.currentPlayers, sess.maxPlayers);

            ImGui::TableSetColumnIndex(4);
            if (sess.mode == GameMode::OpenWorld) {
                int sHrs = static_cast<int>(sess.sessionAge) / 3600;
                int sMins = (static_cast<int>(sess.sessionAge) % 3600) / 60;
                ImGui::TextColored(ImVec4(0.40f, 0.95f, 0.45f, 1.0f), "6 Hexes Active (%02dh %02dm)", sHrs, sMins);
            } else if (sess.mode == GameMode::TeamDeathmatch) {
                int rMins = static_cast<int>(sess.roundTimer) / 60;
                int rSecs = static_cast<int>(sess.roundTimer) % 60;
                ImGui::Text("Tickets: %d-%d | %02d:%02d", sess.coalitionTickets, sess.crimsonTickets, rMins, rSecs);
            } else {
                int rMins = static_cast<int>(sess.raidTimer) / 60;
                int rSecs = static_cast<int>(sess.raidTimer) % 60;
                ImGui::Text("Raid Time: %02d:%02d | Exfil: %s", rMins, rSecs, sess.exfilActive ? "OPEN" : "WAIT");
            }

            ImGui::TableSetColumnIndex(5);
            if (sess.lifecycle == SessionLifecycle::PersistentMMO) {
                ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.55f, 1.0f), "24/7 Persistent");
            } else {
                ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.85f, 1.0f), "On-Demand (Auto-Destruct)");
            }

            ImGui::TableSetColumnIndex(6);
            if (sess.lifecycle != SessionLifecycle::PersistentMMO) {
                std::string btnLabel = "END MATCH##" + std::to_string(sess.id);
                if (ImGui::Button(btnLabel.c_str(), ImVec2(90, 20))) {
                    if (m_server) m_server->removeSession(sess.id);
                }
            } else {
                ImGui::TextDisabled("Main Shard Active");
            }
        }
        ImGui::EndTable();
    }
}

void ServerGUI::renderTabHexWarzone() {
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "CONTINENTAL WARZONE: THE 6 FRONTLINE CHILD HEX MICROSERVERS (96 km²)");
    ImGui::Separator();
    ImGui::Spacing();

    auto sessions = m_server ? m_server->getSessionsSnapshot() : std::vector<SessionSummary>{};
    auto getHexPlayers = [&](uint32_t hexId) -> int {
        for (const auto& s : sessions) {
            if (s.id == hexId) return static_cast<int>(s.currentPlayers);
        }
        return 0;
    };

    struct HexStatus {
        uint32_t id;
        const char* name;
        const char* biome;
        const char* faction;
        ImVec4 facCol;
        int fobsBuilt;
        const char* tacticalRole;
    };

    HexStatus hexes[] = {
        { 101, "CHILD #101: SUMY REGION",         "Dense Pine Forests, Borderland Tracks",    "COALITION (90%)", ImVec4(0.35f, 0.75f, 1.0f, 1.0f), 2, "Coalition Airbase & Woodland Recon" },
        { 102, "CHILD #102: KHARKIV REGION",      "Villages, Highways, Agricultural Fields",  "COALITION (65%)", ImVec4(0.35f, 0.75f, 1.0f, 1.0f), 4, "Mechanized Staging & Convoy Artery" },
        { 103, "CHILD #103: DONETSK REGION",      "Dense Urban Ruins, Coking Metallurgy",     "HEAVY COMBAT",    ImVec4(1.0f, 0.35f, 0.35f, 1.0f), 7, "Urban Fortress Meat-Grinder" },
        { 104, "CHILD #104: LUHANSK REGION",      "Chalk Ridges, River Crossings, Forests",   "CONTESTED (50%)", ImVec4(1.0f, 0.85f, 0.35f, 1.0f), 3, "Long-Range Artillery & Ridge Observation" },
        { 105, "CHILD #105: ZAPORIZHZHIA REGION", "Flat Open Steppe, Dragon's Teeth Ditches", "CONTESTED (50%)", ImVec4(1.0f, 0.85f, 0.35f, 1.0f), 5, "Armored Tank Maneuvers & Minefields" },
        { 106, "CHILD #106: KHERSON REGION",      "Wide Dnipro River Delta, Pontoon Crossings","CRIMSON (80%)",   ImVec4(1.0f, 0.40f, 0.35f, 1.0f), 2, "Crimson Airbase & Delta Naval Port" }
    };

    float cardW = (availW - 15.0f) / 2.0f;
    for (int i = 0; i < 6; ++i) {
        if (i % 2 == 1) ImGui::SameLine(0, 15);
        ImGui::BeginChild((std::string("HexCard_") + std::to_string(i)).c_str(), ImVec2(cardW, 125), true);
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s", hexes[i].name);
        ImGui::TextDisabled("Biome: %s", hexes[i].biome);
        ImGui::TextDisabled("Role:  %s", hexes[i].tacticalRole);
        ImGui::TextColored(hexes[i].facCol, "Dominance: %s", hexes[i].faction);
        int liveCount = getHexPlayers(hexes[i].id);
        ImGui::TextColored(ImVec4(0.4f, 0.95f, 0.5f, 1.0f), "Live Operatives: %d | Persistent FOBs: %d", liveCount, hexes[i].fobsBuilt);
        ImGui::EndChild();
        if (i % 2 == 1) ImGui::Spacing();
    }
}

void ServerGUI::renderTabPlayers() {
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "OPERATIVES DIRECTORY & PLAYER MANAGEMENT");
    ImGui::Separator();
    ImGui::Spacing();

    auto clients = m_server ? m_server->getClientsSnapshot() : std::vector<ClientSummary>{};

    if (clients.empty()) {
        ImGui::TextDisabled("No active operatives currently connected to this shard.");
        return;
    }

    // Filter bar
    ImGui::Text("Filter Callsign / ID:");
    ImGui::SameLine();
    ImGui::PushItemWidth(250.0f);
    ImGui::InputText("##PlayerFilter", m_playerSearch, sizeof(m_playerSearch));
    ImGui::PopItemWidth();
    ImGui::Spacing();

    if (ImGui::BeginTable("PlayersTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("IP & Port", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Current Session", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Faction", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Position (X, Z)", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& c : clients) {
            std::string idStr = std::to_string(c.id);
            if (m_playerSearch[0] != '\0' && idStr.find(m_playerSearch) == std::string::npos) {
                continue;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("#%u", c.id);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s:%u", c.ipStr.c_str(), c.port);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("Session #%u", c.sessionId);

            ImGui::TableSetColumnIndex(3);
            if (c.faction == 0) {
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "COALITION");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.35f, 1.0f), "CRIMSON");
            }

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.0f, %.0f", c.position.x, c.position.z);

            ImGui::TableSetColumnIndex(5);
            ImVec4 pingCol = c.pingMs < 60 ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) :
                            (c.pingMs < 120 ? ImVec4(1.0f, 0.85f, 0.35f, 1.0f) : ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
            ImGui::TextColored(pingCol, "%d ms", c.pingMs);

            ImGui::TableSetColumnIndex(6);
            std::string kickBtn = "KICK OPERATIVE##" + std::to_string(c.id);
            if (ImGui::Button(kickBtn.c_str(), ImVec2(120, 20))) {
                if (m_server) m_server->kickPlayer(c.id);
            }
        }
        ImGui::EndTable();
    }
}

void ServerGUI::renderTabSettings() {
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "SERVER CONFIGURATION & TOPOLOGY SETTINGS");
    ImGui::Separator();
    ImGui::Spacing();

    // Operational Role Selection
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "SERVER OPERATIONAL ROLE:");
    int r = static_cast<int>(m_role);
    if (ImGui::RadioButton("Continental Hex Master Server (Authoritative 6-Hex Warzone)", r == 0)) {
        m_role = ServerRole::ContinentalHexMaster;
    }
    if (ImGui::RadioButton("Edge Ingress Relay (Lightweight Traffic Accelerator)", r == 1)) {
        m_role = ServerRole::VercelEdgeRelay;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Network & Capacity
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "NETWORK & SIMULATION CAPACITY:");
    ImGui::InputInt("Ingress Port (UDP)", &m_serverPort);
    ImGui::InputInt("Max Player Capacity", &m_maxPlayers);
    ImGui::Checkbox("Auto-Restart on Crash (Watchdog Supervisor)", &m_autoRestartOnCrash);

    if (m_role == ServerRole::VercelEdgeRelay) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "EDGE RELAY UPSTREAM TUNNEL CONFIG:");
        ImGui::InputText("Upstream Host / IP", m_upstreamHost, sizeof(m_upstreamHost));
        ImGui::InputInt("Upstream Port", &m_upstreamPort);
        ImGui::InputText("Region Identifier", m_regionName, sizeof(m_regionName));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Database Status
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "DATABASE & STORAGE ENGINE STATUS:");
    ImGui::BulletText("Authoritative SQLite Database: mercenary_master.db");
    ImGui::BulletText("Storage Mode: WAL (Write-Ahead Logging)");
    ImGui::BulletText("Persistent Construction FOBs Table: world_structures (Active)");
    ImGui::BulletText("Player Accounts Table: profiles (Active)");
}

void ServerGUI::renderTabConsole() {
    float availW = ImGui::GetContentRegionAvail().x;
    float availH = ImGui::GetContentRegionAvail().y;

    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "REAL-TIME AUTHORITATIVE CONSOLE TERMINAL");
    ImGui::SameLine(availW - 120.0f);
    static bool autoScroll = true;
    ImGui::Checkbox("Auto-Scroll", &autoScroll);

    ImGui::BeginChild("ConsoleScrollArea", ImVec2(availW, availH - 45.0f), true);
    if (m_server) {
        auto logs = m_server->getLogMessages();
        for (const auto& msg : logs) {
            ImGui::TextUnformatted(msg.c_str());
        }
        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();

    // Command input line
    static char cmdBuf[128] = "";
    ImGui::PushItemWidth(availW - 90.0f);
    if (ImGui::InputText("##ConsoleCmd", cmdBuf, sizeof(cmdBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (cmdBuf[0] != '\0' && m_server) {
            m_server->appendLog(std::string("> ") + cmdBuf);
            cmdBuf[0] = '\0';
        }
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("SUBMIT", ImVec2(80, 24))) {
        if (cmdBuf[0] != '\0' && m_server) {
            m_server->appendLog(std::string("> ") + cmdBuf);
            cmdBuf[0] = '\0';
        }
    }
}

} // namespace Mercenary

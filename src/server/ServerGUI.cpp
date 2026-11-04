#include "ServerGUI.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <dwmapi.h>
#endif

namespace Mercenary {

ServerGUI::ServerGUI() {
}

ServerGUI::~ServerGUI() {
    shutdown();
}

void ServerGUI::applyDarkTacticalTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(16, 16);
    style.ItemSpacing = ImVec2(10, 8);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.07f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.24f, 0.30f, 0.60f);
    colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.50f, 0.58f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.14f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.20f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.18f, 0.38f, 0.58f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.13f, 0.17f, 0.23f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.32f, 0.45f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.18f, 0.45f, 0.70f, 1.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.16f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.26f, 0.34f, 1.00f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.13f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.22f, 0.35f, 0.50f, 1.00f);
    colors[ImGuiCol_TabSelected]          = ImVec4(0.16f, 0.38f, 0.62f, 1.00f);
    colors[ImGuiCol_TabDimmed]            = ImVec4(0.08f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected]    = ImVec4(0.12f, 0.20f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.07f, 0.09f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.36f, 0.45f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.35f, 0.48f, 0.65f, 1.00f);
}

bool ServerGUI::init(int width, int height) {
    m_windowWidth = width;
    m_windowHeight = height;

    if (!glfwInit()) {
        std::cerr << "[ServerGUI] Failed to initialize GLFW.\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, 
                                "MERCENARY: WARZONE // DEDICATED SERVER CONTROL CENTER", 
                                nullptr, nullptr);
    if (!m_window) {
        std::cerr << "[ServerGUI] Failed to create GLFW window.\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // 60 FPS V-Sync for GUI

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "[ServerGUI] Failed to initialize GLAD OpenGL loader.\n";
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(m_window);
    if (hwnd) {
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    }
#endif

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
        m_serverStatusText = "ERROR: FAILED TO BIND PORT";
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
        } else {
            m_uptimeSeconds = 0.0f;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int displayW, displayH;
        glfwGetFramebufferSize(m_window, &displayW, &displayH);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(displayW), static_cast<float>(displayH)));
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

        if (ImGui::Begin("##ServerControlCenter", nullptr, flags)) {
            renderHeaderBar();
            renderTabNavigation();

            ImGui::Separator();
            ImGui::Spacing();

            switch (m_activeTab) {
                case 0: renderTabOverview(); break;
                case 1: renderTabPlayers(); break;
                case 2: renderTabHexWarzone(); break;
                case 3: renderTabConsole(); break;
                case 4: renderTabSettings(); break;
                default: renderTabOverview(); break;
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
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;

    // Tactical Banner
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "MERCENARY: WARZONE");
    ImGui::SameLine();
    ImGui::TextDisabled("// DEDICATED CONTINENTAL SERVER CONTROL CENTER v1.2");

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
        "MONITORING DASHBOARD",
        "OPERATIVES ROSTER",
        "6-HEX WARZONE SECTORS",
        "LIVE SERVER CONSOLE",
        "CONFIG & WATCHDOG"
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
        if (ImGui::Button(tabs[i], ImVec2(185, 32))) {
            m_activeTab = i;
        }
        ImGui::PopStyleColor(2);
    }
}

void ServerGUI::renderTabOverview() {
    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = (availW - 30.0f) / 4.0f;
    float cardH = 85.0f;

    // Card 1: Connected Operatives
    ImGui::BeginChild("Card1", ImVec2(cardW, cardH), true);
    ImGui::TextDisabled("ACTIVE OPERATIVES");
    size_t clientCount = m_server ? m_server->getConnectedClientCount() : 0;
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%zu / %d", clientCount, m_maxPlayers);
    ImGui::TextDisabled("Peak: %zu | Cap: %d", clientCount, m_maxPlayers);
    ImGui::EndChild();

    ImGui::SameLine(0, 10);

    // Card 2: Tickrate & Uptime
    ImGui::BeginChild("Card2", ImVec2(cardW, cardH), true);
    ImGui::TextDisabled("SIMULATION TICKRATE");
    ImGui::TextColored(ImVec4(0.40f, 0.95f, 0.45f, 1.0f), "64.0 Hz [STABLE]");
    int hrs = static_cast<int>(m_uptimeSeconds) / 3600;
    int mins = (static_cast<int>(m_uptimeSeconds) % 3600) / 60;
    int secs = static_cast<int>(m_uptimeSeconds) % 60;
    char upBuf[32];
    std::snprintf(upBuf, sizeof(upBuf), "Uptime: %02d:%02d:%02d", hrs, mins, secs);
    ImGui::TextDisabled("%s", upBuf);
    ImGui::EndChild();

    ImGui::SameLine(0, 10);

    // Card 3: Ingress & Fleet Public IP
    ImGui::BeginChild("Card3", ImVec2(cardW, cardH), true);
    ImGui::TextDisabled("FLEET INGRESS / PUBLIC IP");
    std::string pubIp = m_server ? m_server->getPublicIP() : "127.0.0.1";
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "%s:%d", pubIp.c_str(), m_serverPort);
    ImGui::TextDisabled("Discovery: Cloud Reg Active");
    ImGui::EndChild();

    ImGui::SameLine(0, 10);

    // Card 4: Continental Master State
    ImGui::BeginChild("Card4", ImVec2(cardW, cardH), true);
    ImGui::TextDisabled("CONTINENTAL THEATER");
    ImGui::TextColored(ImVec4(0.80f, 0.50f, 1.0f, 1.0f), "6 HEXES (96 km²)");
    ImGui::TextDisabled("Frontline: Contested");
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

    // Quick Live Status Log Preview
    ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.88f, 1.0f), "RECENT CONTINENTAL EVENTS & TELEMETRY:");
    ImGui::BeginChild("OverviewLogPreview", ImVec2(availW, 280), true);
    if (m_server) {
        auto logs = m_server->getLogMessages();
        for (const auto& l : logs) {
            ImGui::TextUnformatted(l.c_str());
        }
    }
    ImGui::EndChild();
}

void ServerGUI::renderTabPlayers() {
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "ACTIVE OPERATIVES DIRECTORY");
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_server || m_server->getClients().empty()) {
        ImGui::TextDisabled("No active operatives currently connected to this shard.");
        return;
    }

    if (ImGui::BeginTable("PlayersTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Callsign", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Faction", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Current Session", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn("Position (X, Z)", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& [pId, client] : m_server->getClients()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("#%u", pId);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s", client.state.callsign[0] ? client.state.callsign : "Operative");

            ImGui::TableSetColumnIndex(2);
            if (client.state.faction == 0) {
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "COALITION");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.40f, 0.35f, 1.0f), "CRIMSON");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("Session #%u", client.sessionId);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.0f, %.0f", client.state.position.x, client.state.position.z);

            ImGui::TableSetColumnIndex(5);
            std::string kickBtn = "KICK##" + std::to_string(pId);
            if (ImGui::Button(kickBtn.c_str(), ImVec2(60, 20))) {
                m_server->kickPlayer(pId);
            }
        }
        ImGui::EndTable();
    }
}

void ServerGUI::renderTabHexWarzone() {
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "THE 6 CONTINENTAL HEX SECTORS (UKRAINIAN / RUSSIAN THEATER)");
    ImGui::Separator();
    ImGui::Spacing();

    struct HexStatus {
        const char* name;
        const char* biome;
        const char* faction;
        ImVec4 facCol;
        int activeOperatives;
        const char* tacticalRole;
    };

    HexStatus hexes[] = {
        { "1. KHARKIV REGION",      "Villages, Highways, Agricultural Fields",  "COALITION (65%)", ImVec4(0.35f, 0.75f, 1.0f, 1.0f), 8,  "Mechanized Staging & Convoy Artery" },
        { "2. LUHANSK REGION",      "Chalk Ridges, River Crossings, Forests",   "CONTESTED (50%)", ImVec4(1.0f, 0.85f, 0.35f, 1.0f), 12, "Long-Range Artillery & Ridge Observation" },
        { "3. DONETSK REGION",      "Dense Urban Ruins, Coking Metallurgy",     "HEAVY COMBAT",    ImVec4(1.0f, 0.35f, 0.35f, 1.0f), 24, "Urban Fortress Meat-Grinder" },
        { "4. ZAPORIZHZHIA REGION", "Flat Open Steppe, Dragon's Teeth Ditches", "CONTESTED (50%)", ImVec4(1.0f, 0.85f, 0.35f, 1.0f), 10, "Armored Tank Maneuvers & Minefields" },
        { "5. KHERSON REGION",      "Wide Dnipro River Delta, Pontoon Crossings","CRIMSON (80%)",   ImVec4(1.0f, 0.40f, 0.35f, 1.0f), 6,  "Crimson Primary Airbase & Delta Port" },
        { "6. SUMY REGION",         "Dense Pine Forests, Borderland Tracks",    "COALITION (90%)", ImVec4(0.35f, 0.75f, 1.0f, 1.0f), 4,  "Coalition Primary Airbase & Woodland Recon" }
    };

    float cardW = (availW - 15.0f) / 2.0f;
    for (int i = 0; i < 6; ++i) {
        if (i % 2 == 1) ImGui::SameLine(0, 15);
        ImGui::BeginChild((std::string("HexCard_") + std::to_string(i)).c_str(), ImVec2(cardW, 115), true);
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s", hexes[i].name);
        ImGui::TextDisabled("Biome: %s", hexes[i].biome);
        ImGui::TextDisabled("Tactical: %s", hexes[i].tacticalRole);
        ImGui::TextColored(hexes[i].facCol, "Dominance: %s", hexes[i].faction);
        ImGui::EndChild();
        if (i % 2 == 1) ImGui::Spacing();
    }
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

void ServerGUI::renderTabSettings() {
    float availW = ImGui::GetContentRegionAvail().x;
    ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "SERVER NODE CONFIGURATION & TOPOLOGY");
    ImGui::Separator();
    ImGui::Spacing();

    // Role Selection Radio Buttons
    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "SERVER OPERATIONAL ROLE:");
    int r = static_cast<int>(m_role);
    if (ImGui::RadioButton("Continental Hex Master Server (Authoritative 6-Hex Warzone)", r == 0)) {
        m_role = ServerRole::ContinentalHexMaster;
    }
    if (ImGui::RadioButton("Vercel-Style Edge Ingress Relay (Tokyo / Virginia / Frankfurt)", r == 1)) {
        m_role = ServerRole::VercelEdgeRelay;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::InputInt("Shard Port (UDP)", &m_serverPort);
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

    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "DATABASE ENGINE STATUS:");
    ImGui::BulletText("SQLite Master Database: mercenary_master.db");
    ImGui::BulletText("Journal Mode: WAL (Write-Ahead Logging)");
    ImGui::BulletText("Persistent Structures Table: world_structures (Active)");
    ImGui::BulletText("Account Profiles Table: profiles (Active)");
}

} // namespace Mercenary

#pragma once

#include "HubServer.hpp"
#include "ServerSystemUtils.hpp"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

struct GLFWwindow;

namespace Mercenary {

enum class ServerRole : uint8_t {
    ContinentalHexMaster = 0, // 6-Hex World, TDM Instances, Database
    VercelEdgeRelay      = 1  // Ingress Relay for Tokyo/Virginia/Frankfurt
};

class ServerGUI {
public:
    ServerGUI();
    ~ServerGUI();

    bool init(int width = 1080, int height = 720);
    void run();
    void shutdown();

    void startServerThread();
    void stopServerThread();

private:
    GLFWwindow* m_window = nullptr;
    int m_windowWidth = 1080;
    int m_windowHeight = 720;

    std::unique_ptr<HubServer> m_server;
    std::thread m_serverThread;
    std::atomic<bool> m_isServerRunning{false};

    // Configuration State
    ServerRole m_role = ServerRole::ContinentalHexMaster;
    int m_serverPort = 7777;
    int m_maxPlayers = 3000;
    bool m_autoRestartOnCrash = true;
    char m_upstreamHost[64] = "127.0.0.1";
    int m_upstreamPort = 7777;
    char m_regionName[64] = "EU_CENTRAL_01";
    char m_broadcastMsg[128] = "";

    int m_activeTab = 0;
    float m_uptimeSeconds = 0.0f;
    std::string m_serverStatusText = "STANDBY / OFFLINE";

    // UI State & Actions
    bool m_firewallConfigured = false;
    std::string m_firewallStatusMsg = "Click '1-Click Setup' to configure Windows Firewall for UDP 7777";
    int m_serviceState = 0; // 0: Not Installed, 1: Stopped, 2: Running, 3: Pending
    std::string m_serviceStatusMsg = "Windows Background Service: Not Installed";
    char m_newSessionMap[64] = "killhouse";
    int m_newSessionMode = 1; // 1: TDM, 2: Infiltration
    char m_playerSearch[64] = "";

    // UI Render Passes
    void renderHeaderBar();
    void renderTabNavigation();
    void renderTabSessions();       // Tab 0: Live Sessions & Micro-Instance Matrix
    void renderTabHexWarzone();     // Tab 1: 6-Hex Continental Frontline
    void renderTabPlayers();        // Tab 2: Operatives Management Roster
    void renderTabSetupService();   // Tab 3: 1-Click RDP Setup & 24/7 Windows Service
    void renderTabConsole();        // Tab 4: Live Server Console & Logs

    void applyDarkTacticalTheme();
};

} // namespace Mercenary

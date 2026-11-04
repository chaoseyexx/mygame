#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "HubServer.hpp"
#include "ServerGUI.hpp"
#include "ServerSystemUtils.hpp"
#include <iostream>
#include <csignal>
#include <string>

using namespace Mercenary;

static HubServer* g_serverInstance = nullptr;
static SERVICE_STATUS_HANDLE g_serviceStatusHandle = NULL;
static SERVICE_STATUS g_serviceStatus{};
static uint16_t g_servicePort = 7777;

static void signalHandler(int signum) {
    (void)signum;
    std::cout << "\n[Server] Shutdown signal received. Stopping Hub Server...\n" << std::flush;
    if (g_serverInstance) {
        g_serverInstance->stop();
    }
}

VOID WINAPI ServiceCtrlHandler(DWORD request) {
    switch (request) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            g_serviceStatus.dwWin32ExitCode = 0;
            g_serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
            if (g_serverInstance) {
                g_serverInstance->stop();
            }
            g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
            break;
        default:
            break;
    }
}

VOID WINAPI ServiceMain(DWORD argc, LPSTR* argv) {
    g_serviceStatusHandle = RegisterServiceCtrlHandlerA(ServerSystemUtils::SERVICE_NAME(), ServiceCtrlHandler);
    if (!g_serviceStatusHandle) return;

    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);

    HubServer server;
    g_serverInstance = &server;

    if (!server.init(g_servicePort)) {
        g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
        return;
    }

    g_serviceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);

    server.run();

    g_serviceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_serviceStatusHandle, &g_serviceStatus);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    bool isService = false;
    bool headless = false;
    uint16_t srvPort = 7777;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--service") {
            isService = true;
        } else if (arg == "--headless" || arg == "-h" || arg == "--nogui" || arg == "--cli") {
            headless = true;
        } else if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            try {
                srvPort = static_cast<uint16_t>(std::stoi(argv[++i]));
            } catch (...) {}
        } else if (arg[0] != '-') {
            try {
                srvPort = static_cast<uint16_t>(std::stoi(arg));
            } catch (...) {}
        }
    }

    // 1. Windows Background NT Service Mode
    if (isService) {
        g_servicePort = srvPort;
        SERVICE_TABLE_ENTRYA serviceTable[] = {
            { const_cast<LPSTR>(ServerSystemUtils::SERVICE_NAME()), ServiceMain },
            { NULL, NULL }
        };
        StartServiceCtrlDispatcherA(serviceTable);
        return 0;
    }

    // 2. Visual GUI Server Control Center Mode (Default)
    if (!headless) {
        std::cout << "[ServerMain] Initializing Server Control Center GUI...\n";
        ServerGUI gui;
        if (gui.init(1120, 750)) {
            gui.run();
            gui.shutdown();
            return 0;
        }
        std::cerr << "[ServerMain] GUI initialization failed, falling back to headless console mode...\n";
    }

    // 3. Headless Console Mode
    std::cout << "====================================================================\n";
    std::cout << "   MERCENARY: PERSISTENT CONTINENTAL DEDICATED SERVER (HEADLESS)    \n";
    std::cout << "   Target Ingress Port: " << srvPort << "                           \n";
    std::cout << "====================================================================\n" << std::flush;

    HubServer server;
    g_serverInstance = &server;

    if (!server.init(srvPort)) {
        std::cerr << "[Server Error] Failed to bind port " << srvPort << ".\n";
        return 1;
    }

    server.run();
    return 0;
}

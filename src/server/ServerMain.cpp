#include "HubServer.hpp"
#include "ServerGUI.hpp"
#include <iostream>
#include <csignal>
#include <string>

using namespace Mercenary;

static HubServer* g_serverInstance = nullptr;

static void signalHandler(int signum) {
    (void)signum;
    std::cout << "\n[Server] Shutdown signal received. Stopping Hub Server...\n" << std::flush;
    if (g_serverInstance) {
        g_serverInstance->stop();
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    bool headless = false;
    uint16_t srvPort = 7777;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless" || arg == "-h" || arg == "--nogui" || arg == "--cli") {
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

    // 1. Visual GUI Server Control Center Mode (Default)
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

    // 2. Headless Console Mode
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

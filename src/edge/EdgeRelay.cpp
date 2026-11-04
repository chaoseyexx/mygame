#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include "engine/NetworkDiscovery.hpp"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <csignal>
#include <atomic>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

static std::atomic<bool> g_running{true};

static void signalHandler(int signum) {
    (void)signum;
    std::cout << "\n[Edge Relay] Shutdown signal received. Exiting...\n" << std::flush;
    g_running = false;
}

struct ClientSession {
    sockaddr_in clientAddr{};
    std::chrono::steady_clock::time_point lastActivity;
    uint32_t packetsForwarded = 0;
};

// Key format: "IP:Port"
static std::string addrToString(const sockaddr_in& addr) {
    char ipStr[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));
    return std::string(ipStr) + ":" + std::to_string(ntohs(addr.sin_port));
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    uint16_t edgePort = 7777;
    std::string upstreamHost = "127.0.0.1";
    uint16_t upstreamPort = 7777;
    std::string regionName = "EDGE_NODE_01";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--port" || arg == "-p") && i + 1 < argc) {
            edgePort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "--upstream" || arg == "-u") && i + 1 < argc) {
            upstreamHost = argv[++i];
        } else if ((arg == "--upstream-port") && i + 1 < argc) {
            upstreamPort = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "--region" || arg == "-r") && i + 1 < argc) {
            regionName = argv[++i];
        }
    }

    std::cout << "=================================================================\n";
    std::cout << "  MERCENARY: VERCEL-STYLE EDGE INGRESS MICROSERVER (UDP RELAY)   \n";
    std::cout << "  Region: " << regionName << "  |  Ingress Port: " << edgePort << "\n";
    std::cout << "  Upstream Mesh Target: " << upstreamHost << ":" << upstreamPort << "\n";
    std::cout << "=================================================================\n" << std::flush;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[Fatal Error] WSAStartup failed.\n";
        return 1;
    }

    SOCKET edgeSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (edgeSocket == INVALID_SOCKET) {
        std::cerr << "[Fatal Error] Failed to create edge UDP socket.\n";
        WSACleanup();
        return 1;
    }

    // Set non-blocking mode
    u_long nonBlocking = 1;
    ioctlsocket(edgeSocket, FIONBIO, &nonBlocking);

    // Set high socket buffer sizes
    int bufSize = 4 * 1024 * 1024; // 4MB buffer
    setsockopt(edgeSocket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));
    setsockopt(edgeSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufSize), sizeof(bufSize));

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(edgePort);
    bindAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(edgeSocket, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
        std::cerr << "[Fatal Error] Failed to bind edge port " << edgePort << " (Error: " << WSAGetLastError() << ")\n";
        closesocket(edgeSocket);
        WSACleanup();
        return 1;
    }

    sockaddr_in upstreamAddr{};
    if (!Mercenary::resolveHostToSockaddr(upstreamHost, upstreamPort, upstreamAddr)) {
        std::cerr << "[Edge Relay Warning] Failed to resolve upstream host: " << upstreamHost << " (defaulting to 127.0.0.1)\n";
        Mercenary::resolveHostToSockaddr("127.0.0.1", upstreamPort, upstreamAddr);
    }

    std::cout << "[Edge Relay] Listening for global players on port " << edgePort << "...\n" << std::flush;

    std::unordered_map<std::string, ClientSession> clientSessions;
    std::unordered_map<uint32_t, sockaddr_in> playerToClientAddr;
    std::mutex sessionMutex;

    char buffer[4096];
    sockaddr_in senderAddr{};
    int senderLen = sizeof(senderAddr);

    auto lastCleanup = std::chrono::steady_clock::now();
    uint64_t totalPacketsRouted = 0;

    while (g_running) {
        bool hadActivity = false;

        while (true) {
            senderLen = sizeof(senderAddr);
            int bytesReceived = recvfrom(edgeSocket, buffer, sizeof(buffer), 0,
                                         reinterpret_cast<sockaddr*>(&senderAddr), &senderLen);
            if (bytesReceived <= 0) break;
            hadActivity = true;
            totalPacketsRouted++;

            // Check if packet came from upstream worker or from an external player
            bool isFromUpstream = (senderAddr.sin_addr.s_addr == upstreamAddr.sin_addr.s_addr &&
                                   senderAddr.sin_port == upstreamAddr.sin_port);

            if (isFromUpstream) {
                // Downstream routing: Upstream server returned a packet for players
                // If packet has a client ID or sequence, route to respective client
                // Otherwise broadcast to all active local sessions
                std::lock_guard<std::mutex> lock(sessionMutex);
                for (auto& [key, session] : clientSessions) {
                    sendto(edgeSocket, buffer, bytesReceived, 0,
                           reinterpret_cast<const sockaddr*>(&session.clientAddr), sizeof(session.clientAddr));
                }
            } else {
                // Upstream routing: External player sent input/action to Edge Relay
                std::string clientKey = addrToString(senderAddr);
                {
                    std::lock_guard<std::mutex> lock(sessionMutex);
                    auto& session = clientSessions[clientKey];
                    session.clientAddr = senderAddr;
                    session.lastActivity = std::chrono::steady_clock::now();
                    session.packetsForwarded++;
                }

                // Tunnel packet directly to upstream authoritative hex worker
                sendto(edgeSocket, buffer, bytesReceived, 0,
                       reinterpret_cast<const sockaddr*>(&upstreamAddr), sizeof(upstreamAddr));
            }
        }

        // Periodic session cleanup (every 10s, purge sessions inactive > 30s)
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<float>(now - lastCleanup).count() >= 10.0f) {
            lastCleanup = now;
            std::lock_guard<std::mutex> lock(sessionMutex);
            for (auto it = clientSessions.begin(); it != clientSessions.end();) {
                float idle = std::chrono::duration<float>(now - it->second.lastActivity).count();
                if (idle > 30.0f) {
                    it = clientSessions.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (!hadActivity) {
            Sleep(1); // 1ms sleep to avoid burning CPU when idle
        }
    }

    std::cout << "[Edge Relay] Shutting down. Total packets routed: " << totalPacketsRouted << "\n";
    closesocket(edgeSocket);
    WSACleanup();
    return 0;
}

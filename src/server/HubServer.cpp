#include "HubServer.hpp"
#include "DatabaseEngine.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <algorithm>
#include <cstring>
#include <mmsystem.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

namespace Mercenary {

HubServer::HubServer() {
}

HubServer::~HubServer() {
    stop();
}

bool HubServer::init(uint16_t port) {
    m_port = port;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[HubServer Error] WSAStartup failed.\n";
        return false;
    }

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "[HubServer Error] Failed to create UDP socket.\n";
        WSACleanup();
        return false;
    }

    u_long nonBlocking = 1;
    if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) != 0) {
        std::cerr << "[HubServer Error] Failed to set non-blocking mode.\n";
        closesocket(m_socket);
        WSACleanup();
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[HubServer Error] Failed to bind socket to UDP port " << port << ".\n";
        closesocket(m_socket);
        WSACleanup();
        return false;
    }

    // 0. Initialize Master Authoritative Database Engine (Embedded SQLite WAL)
    DatabaseEngine::get().init("mercenary_master.db");

    // Continental Open World Architecture: 6 Dedicated Child Hex Microservers
    struct HexConfig {
        uint32_t id;
        const char* name;
        const char* mapName;
    };

    HexConfig hexConfigs[] = {
        { 101, "CONTINENTAL HEX #1 // SUMY REGION (NORTH AIRBASE)", "sumy" },
        { 102, "CONTINENTAL HEX #2 // KHARKIV REGION (HIGHWAYS)", "kharkiv" },
        { 103, "CONTINENTAL HEX #3 // DONETSK REGION (URBAN RUINS)", "donetsk" },
        { 104, "CONTINENTAL HEX #4 // LUHANSK REGION (ARTILLERY RIDGES)", "luhansk" },
        { 105, "CONTINENTAL HEX #5 // ZAPORIZHZHIA REGION (STEPPE ARMOR)", "zaporizhzhia" },
        { 106, "CONTINENTAL HEX #6 // KHERSON REGION (DELTA AIRBASE)", "kherson" }
    };

    for (const auto& h : hexConfigs) {
        HubSession sHex;
        sHex.id = h.id;
        sHex.mode = GameMode::OpenWorld;
        sHex.lifecycle = SessionLifecycle::PersistentMMO;
        sHex.name = h.name;
        sHex.mapName = h.mapName;
        sHex.maxPlayers = 500;
        m_sessions[h.id] = sHex;
    }

    // Master Continental Routing Shard
    HubSession sMaster;
    sMaster.id = 1001;
    sMaster.mode = GameMode::OpenWorld;
    sMaster.lifecycle = SessionLifecycle::PersistentMMO;
    sMaster.name = "CONTINENTAL WARZONE // MASTER OVERWORLD ROUTER";
    sMaster.mapName = "continental_master";
    sMaster.maxPlayers = 3000;
    m_sessions[1001] = sMaster;

    std::cout << "[Hub Boot] 6 Continental Hex Child Microservers (#101-#106) Initialized Successfully.\n"
              << "[Hub Boot] Master Continental Router #1001 Online (Total Capacity: 3,000 Operatives).\n"
              << "[Hub Boot] On-Demand Micro-Server Pool Ready: TDM & Infiltration instances spawn on-demand.\n" << std::flush;

    m_startTime = std::chrono::steady_clock::now();
    m_running = true;

    std::cout << "=========================================================================\n";
    std::cout << "   MERCENARY: MULTI-SESSION BACKEND HUB SERVER (v1.1.0)\n";
    std::cout << "   [Open World MMO] + [Team Deathmatch] + [Tactical Infiltration]\n";
    std::cout << "=========================================================================\n";
    std::cout << "[Hub Ready] Listening on 0.0.0.0:" << port << " (UDP Non-Blocking)\n";
    std::cout << "[Hub Ready] Active Sessions Loaded: " << m_sessions.size() << " Game Rooms\n";
    std::cout << "[Hub Ready] Type 'help' for interactive console commands.\n";
    std::cout << "-------------------------------------------------------------------------\n" << std::flush;

    // Auto-detect public IP and register with Cloud Discovery
    std::thread([this, port]() {
        std::string pubIp = CloudDiscoveryCoordinator::detectPublicIP();
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            m_publicIP = pubIp;
            char hostName[128] = {0};
            gethostname(hostName, sizeof(hostName));
            m_shardId = std::string("hex-") + (hostName[0] ? hostName : "node") + "-" + std::to_string(port);
        }
        appendLog("[Cloud Discovery] Detected Public IP: " + pubIp);
        bool regOk = CloudDiscoveryCoordinator::registerServerNode(
            m_shardId, m_shardName, m_regionName, pubIp, port, "HexMaster", 0, 3000
        );
        if (regOk) {
            m_cloudRegistered = true;
            appendLog("[Cloud Discovery] Node registered with Cloud Registry: " + m_shardId);
        } else {
            appendLog("[Cloud Discovery Warning] Cloud registry unreachable; running in standalone mode.");
        }
    }).detach();

    return true;
}

void HubServer::stop() {
    if (!m_running) return;
    m_running = false;

    if (m_cloudRegistered.load()) {
        std::string sId = m_shardId;
        CloudDiscoveryCoordinator::unregisterServerNode(sId);
        m_cloudRegistered = false;
    }

    // Notify all clients of server shutdown
    PacketHeader discPkt;
    discPkt.magic = 0x5647;
    discPkt.type = PacketType::Disconnect;
    discPkt.sequenceNumber = 0;
    discPkt.timestamp = 0.0f;

    for (const auto& [id, client] : m_clients) {
        sendRaw(&discPkt, sizeof(discPkt), client.address);
    }
    m_clients.clear();
    m_sessions.clear();

    DatabaseEngine::get().shutdown();

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    WSACleanup();
}

bool HubServer::sendRaw(const void* data, size_t size, const sockaddr_in& dest) {
    if (m_socket == INVALID_SOCKET) return false;
    int sent = sendto(m_socket, static_cast<const char*>(data), static_cast<int>(size), 0,
                      reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
    return sent > 0;
}

void HubServer::run() {
    timeBeginPeriod(1); // Lock Windows scheduler timer to 1ms for rock-solid 30 Hz ticks

    // Start background asynchronous interactive console thread
    std::thread consoleThread(&HubServer::consoleCommandLoop, this);

    const double targetTickTime = 1.0 / 30.0; // 30.0 Hz tick rate
    auto lastTick = std::chrono::steady_clock::now();
    auto lastTelemetry = lastTick;

    while (m_running) {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;

        update(static_cast<float>(dt));

        // Background telemetry every 10 seconds (silent, non-intrusive)
        double telDt = std::chrono::duration<double>(now - lastTelemetry).count();
        if (telDt >= 10.0) {
            lastTelemetry = now;
            // Background sanity heartbeat
        }

        // Sleep to maintain consistent 30.0 Hz simulation
        auto frameEnd = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(frameEnd - now).count();
        if (elapsed < targetTickTime) {
            int sleepMs = static_cast<int>((targetTickTime - elapsed) * 1000.0);
            if (sleepMs > 0) {
                Sleep(sleepMs);
            }
        }
    }

    if (consoleThread.joinable()) {
        consoleThread.join();
    }

    timeEndPeriod(1);
    std::cout << "[HubServer] Server loop terminated gracefully.\n" << std::flush;
}

void HubServer::update(float dt) {
    m_serverTime += dt;
    m_serverTick++;
    processPackets();

    // Pulse Cloud Registry Heartbeat every 10 seconds
    m_heartbeatCloudTimer += dt;
    if (m_heartbeatCloudTimer >= 10.0f) {
        m_heartbeatCloudTimer = 0.0f;
        if (m_cloudRegistered.load()) {
            int activePlayers = static_cast<int>(m_clients.size());
            std::string sId = m_shardId;
            std::thread([sId, activePlayers]() {
                CloudDiscoveryCoordinator::pulseHeartbeat(sId, activePlayers);
            }).detach();
        }
    }

    // 1. Update Game Sessions logic (Timers, Tickets, Exfil, Destruction)
    for (auto& [sId, session] : m_sessions) {
        session.sessionAge += dt;
        session.syncTimer += dt;

        if (session.lifecycle == SessionLifecycle::PersistentMMO) {
            // Persistent MMO Warzone (Always-on 24/7)
            session.territorySaveTimer += dt;
            if (session.territorySaveTimer >= 30.0f) {
                session.territorySaveTimer = 0.0f;
                DatabaseEngine::get().updateWorldTerritory("Siversky_Frontline", 1, 65.0f);
            }
        } else if (session.lifecycle == SessionLifecycle::EphemeralTDM) {
            // Ephemeral TDM: If empty after 30s grace, auto-destruct
            if (session.playerIds.empty() && session.sessionAge > 30.0f) {
                session.isMarkedForDestruction = true;
            }

            if (session.matchStatus == 0) { // InProgress
                session.roundTimer -= dt;
                if (session.roundTimer <= 0.0f || session.coalitionTickets <= 0 || session.crimsonTickets <= 0) {
                    session.roundTimer = 0.0f;
                    int winner = (session.coalitionTickets > session.crimsonTickets) ? 1 : 2;
                    session.matchStatus = winner;
                    session.postMatchTimer = 10.0f; // 10s victory screen

                    // Authoritatively commit match to master database
                    std::string mId = "TDM_" + std::to_string(sId);
                    DatabaseEngine::get().recordMatch(mId, "TDM", session.sessionAge, winner);
                    std::cout << "[MicroServer Cluster] TDM Session #" << sId << " Finished! Winner: Faction #"
                              << winner << ". Scheduled for destruction in 10s.\n" << std::flush;
                }
            } else {
                session.postMatchTimer -= dt;
                if (session.postMatchTimer <= 0.0f) {
                    session.isMarkedForDestruction = true;
                }
            }
        } else if (session.lifecycle == SessionLifecycle::ExtractionRaid40M) {
            // Strict 40:00 Countdown Timer (Remains active for full 40m even if 0 players)
            session.raidTimer -= dt;

            // Signal flare / Exfil chopper at 10 minutes remaining
            if (session.raidTimer <= 600.0f && !session.exfilActive) {
                session.exfilActive = true;
                std::cout << "[Infiltration Raid #" << sId << "] Extraction window active! Chopper inbound at "
                          << session.exfilZone.x << ", " << session.exfilZone.z << "\n" << std::flush;
            }

            // Exfil timer expired (40:00 reached)
            if (session.raidTimer <= 0.0f) {
                session.raidTimer = 0.0f;
                if (!session.hasCommittedStash) {
                    session.hasCommittedStash = true;
                    session.matchStatus = 4; // Exfil Sealed
                    session.postMatchTimer = 10.0f;

                    std::string mId = "RAID_" + std::to_string(sId);
                    DatabaseEngine::get().recordMatch(mId, "Infiltration", 2400.0f, 0);
                    std::cout << "[Infiltration Raid #" << sId << "] 40:00 Hardware Timer Expired! Extraction sealed. Scheduled for destruction.\n" << std::flush;
                } else {
                    session.postMatchTimer -= dt;
                    if (session.postMatchTimer <= 0.0f) {
                        session.isMarkedForDestruction = true;
                    }
                }
            }
        }

        // Broadcast Match State Sync every 1.0 second to all session operatives
        if (session.syncTimer >= 1.0f) {
            session.syncTimer = 0.0f;

            PacketHeader hdr;
            hdr.magic = 0x5647;
            hdr.type = PacketType::MatchStateSync;
            hdr.sequenceNumber = sId;
            hdr.timestamp = 0.0f;

            MatchStateSyncPacket pkt;
            pkt.sessionId = sId;
            pkt.mode = static_cast<uint8_t>(session.mode);
            pkt.coalitionScore = session.coalitionTickets;
            pkt.crimsonScore = session.crimsonTickets;
            pkt.roundTimer = (session.lifecycle == SessionLifecycle::ExtractionRaid40M) ? session.raidTimer : session.roundTimer;
            pkt.matchStatus = session.matchStatus;

            char sendBuf[sizeof(PacketHeader) + sizeof(MatchStateSyncPacket)];
            std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
            std::memcpy(sendBuf + sizeof(PacketHeader), &pkt, sizeof(MatchStateSyncPacket));

            for (uint32_t pId : session.playerIds) {
                auto it = m_clients.find(pId);
                if (it != m_clients.end()) {
                    sendRaw(sendBuf, sizeof(sendBuf), it->second.address);
                }
            }
        }
    }

    // Dynamic Micro-Instance Teardown: Destroys expired ephemeral instances
    std::vector<uint32_t> toDestroy;
    for (const auto& [sId, sess] : m_sessions) {
        if (sess.isMarkedForDestruction && sess.lifecycle != SessionLifecycle::PersistentMMO) {
            toDestroy.push_back(sId);
        }
    }
    for (uint32_t sId : toDestroy) {
        std::cout << "[MicroServer Cluster] Deallocating expired session #" << sId << " (100% memory/CPU freed).\n" << std::flush;
        m_sessions.erase(sId);
    }

    // 2. Client Heartbeat Timeouts (Disconnect after 8.0s without packets)
    std::vector<uint32_t> timedOut;
    for (auto& [id, client] : m_clients) {
        client.lastHeartbeat += dt;
        if (client.lastHeartbeat >= 8.0f) {
            timedOut.push_back(id);
        }
    }

    for (uint32_t id : timedOut) {
        auto it = m_clients.find(id);
        if (it != m_clients.end()) {
            uint32_t sId = it->second.sessionId;
            auto sIt = m_sessions.find(sId);
            if (sIt != m_sessions.end()) {
                auto& pList = sIt->second.playerIds;
                pList.erase(std::remove(pList.begin(), pList.end(), id), pList.end());
                sIt->second.spatialGrid.removeEntity(id);

                // Authoritative broadcast of Disconnect packet to notify session players immediately
                PacketHeader discHdr;
                discHdr.magic = 0x5647;
                discHdr.type = PacketType::Disconnect;
                discHdr.sequenceNumber = id;
                discHdr.timestamp = 0.0f;
                for (uint32_t remId : pList) {
                    auto remIt = m_clients.find(remId);
                    if (remIt != m_clients.end()) {
                        sendRaw(&discHdr, sizeof(discHdr), remIt->second.address);
                    }
                }
            }
            std::cout << "[Hub] Operative #" << id << " timed out from Session #" << sId << ".\n" << std::flush;
            m_clients.erase(it);
        }
    }
}

void HubServer::processPackets() {
    char buffer[2048];
    sockaddr_in clientAddr{};
    int clientAddrLen = sizeof(clientAddr);

    while (true) {
        int bytesReceived = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                     reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (bytesReceived <= 0) break;
        if (bytesReceived < (int)sizeof(PacketHeader)) continue;

        const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(buffer);
        if (hdr->magic != 0x5647) continue;

        switch (hdr->type) {
            case PacketType::Heartbeat:
                handleHeartbeat(clientAddr, *hdr);
                break;
            case PacketType::ConnectRequest:
                handleConnectRequest(clientAddr, *hdr);
                break;
            case PacketType::SessionListRequest:
                handleSessionListRequest(clientAddr, *hdr);
                break;
            case PacketType::JoinSessionRequest:
                handleJoinSessionRequest(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::EntityState:
                handleEntityState(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::FireWeapon:
                handleFireWeapon(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::HitConfirm:
                handleHitConfirm(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::ChatMessage:
                handleChatMessage(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::VoiceData:
                handleVoiceData(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::MatchmakeRequest:
                handleMatchmakeRequest(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::AuthRequest:
                handleAuthRequest(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::RegisterRequest:
                handleRegisterRequest(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::Disconnect:
                handleDisconnect(clientAddr);
                break;
            case PacketType::SectorHandoffRequest:
                handleSectorHandoff(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::StructureBuildRequest:
                handleStructureBuild(clientAddr, buffer, bytesReceived);
                break;
            case PacketType::StructureDamageEvent:
                handleStructureDamage(clientAddr, buffer, bytesReceived);
                break;
            default:
                break;
        }
    }
}

void HubServer::handleHeartbeat(const sockaddr_in& from, const PacketHeader& hdr) {
    PacketHeader reply;
    reply.magic = 0x5647;
    reply.type = PacketType::Heartbeat;
    reply.sequenceNumber = static_cast<uint32_t>(m_clients.size());
    reply.timestamp = hdr.timestamp;
    sendRaw(&reply, sizeof(reply), from);
}

void HubServer::handleConnectRequest(const sockaddr_in& from, const PacketHeader& hdr) {
    // Check if this operative is already registered (same IP and port)
    for (auto& [existingId, existingClient] : m_clients) {
        if (existingClient.address.sin_addr.s_addr == from.sin_addr.s_addr &&
            existingClient.address.sin_port == from.sin_port) {
            existingClient.lastHeartbeat = 0.0f;
            PacketHeader replyHdr;
            replyHdr.magic = 0x5647;
            replyHdr.type = PacketType::ConnectAccept;
            replyHdr.sequenceNumber = existingId;
            replyHdr.timestamp = hdr.timestamp;
            sendRaw(&replyHdr, sizeof(replyHdr), from);
            return;
        }
    }

    uint32_t newId = m_nextPlayerId++;
    HubClient client;
    client.id = newId;
    client.address = from;
    client.lastHeartbeat = 0.0f;
    client.sessionId = 0; // Starts in PMC Hideout / Lobby (allocated on JoinSession/Matchmake)
    m_clients[newId] = client;

    PacketHeader replyHdr;
    replyHdr.magic = 0x5647;
    replyHdr.type = PacketType::ConnectAccept;
    replyHdr.sequenceNumber = newId;
    replyHdr.timestamp = hdr.timestamp;
    sendRaw(&replyHdr, sizeof(replyHdr), from);

    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, clientIp, sizeof(clientIp));
    std::cout << "[Hub] Operative #" << newId << " connected from " << clientIp 
              << ":" << ntohs(from.sin_port) << " (Total Online: " << m_clients.size() << ")\n" << std::flush;
}

void HubServer::handleSessionListRequest(const sockaddr_in& from, const PacketHeader& hdr) {
    std::vector<SessionInfoItem> items;
    items.reserve(m_sessions.size());

    for (const auto& [sId, sess] : m_sessions) {
        SessionInfoItem item{};
        item.sessionId = sId;
        item.mode = static_cast<uint8_t>(sess.mode);
        strncpy_s(item.name, sess.name.c_str(), sizeof(item.name) - 1);
        strncpy_s(item.mapName, sess.mapName.c_str(), sizeof(item.mapName) - 1);
        item.playerCount = static_cast<uint16_t>(sess.playerIds.size());
        item.maxPlayers = sess.maxPlayers;
        item.coalitionScore = sess.coalitionTickets;
        item.crimsonScore = sess.crimsonTickets;
        item.timeRemaining = (sess.mode == GameMode::Infiltration) ? sess.raidTimer : sess.roundTimer;
        items.push_back(item);
    }

    PacketHeader replyHdr;
    replyHdr.magic = 0x5647;
    replyHdr.type = PacketType::SessionListResponse;
    replyHdr.sequenceNumber = static_cast<uint32_t>(items.size());
    replyHdr.timestamp = hdr.timestamp;

    std::vector<char> sendBuf(sizeof(PacketHeader) + items.size() * sizeof(SessionInfoItem));
    std::memcpy(sendBuf.data(), &replyHdr, sizeof(PacketHeader));
    std::memcpy(sendBuf.data() + sizeof(PacketHeader), items.data(), items.size() * sizeof(SessionInfoItem));

    sendRaw(sendBuf.data(), sendBuf.size(), from);
}

void HubServer::handleJoinSessionRequest(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(JoinSessionRequestPacket))) return;
    const JoinSessionRequestPacket* req = reinterpret_cast<const JoinSessionRequestPacket*>(buffer + sizeof(PacketHeader));

    auto cIt = m_clients.find(req->playerId);
    if (cIt == m_clients.end()) return;

    // Verify caller address
    if (cIt->second.address.sin_addr.s_addr != from.sin_addr.s_addr ||
        cIt->second.address.sin_port != from.sin_port) {
        return; // Spoof attempt rejected
    }

    uint32_t targetSessionId = req->sessionId;
    // If player requested general Open World (#1001), route to appropriate starting hex by faction
    if (targetSessionId == 1001) {
        targetSessionId = (req->faction == 0) ? 101 : 106;
    }

    auto sIt = m_sessions.find(targetSessionId);
    if (sIt == m_sessions.end()) {
        sIt = m_sessions.find(101);
    }
    JoinSessionResponsePacket resp{};
    resp.sessionId = sIt != m_sessions.end() ? sIt->first : req->sessionId;

    if (sIt == m_sessions.end() || sIt->second.playerIds.size() >= sIt->second.maxPlayers) {
        resp.success = 0; // Failed / Full
        PacketHeader hdr;
        hdr.magic = 0x5647;
        hdr.type = PacketType::JoinSessionResponse;
        hdr.sequenceNumber = req->sessionId;

        char sendBuf[sizeof(PacketHeader) + sizeof(JoinSessionResponsePacket)];
        std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
        std::memcpy(sendBuf + sizeof(PacketHeader), &resp, sizeof(JoinSessionResponsePacket));
        sendRaw(sendBuf, sizeof(sendBuf), from);
        return;
    }

    // Remove from previous session
    uint32_t oldSessionId = cIt->second.sessionId;
    auto oldSIt = m_sessions.find(oldSessionId);
    if (oldSIt != m_sessions.end()) {
        auto& pList = oldSIt->second.playerIds;
        pList.erase(std::remove(pList.begin(), pList.end(), req->playerId), pList.end());
        oldSIt->second.spatialGrid.removeEntity(req->playerId);
    }

    // Join target session
    cIt->second.sessionId = sIt->first;
    cIt->second.state.faction = req->faction;
    sIt->second.playerIds.push_back(req->playerId);

    resp.success = 1;
    resp.mode = static_cast<uint8_t>(sIt->second.mode);
    strncpy_s(resp.sessionName, sIt->second.name.c_str(), sizeof(resp.sessionName) - 1);
    strncpy_s(resp.mapName, sIt->second.mapName.c_str(), sizeof(resp.mapName) - 1);

    if (sIt->second.mode == GameMode::TeamDeathmatch) {
        resp.spawnPosition = (req->faction == 1) ? glm::vec3(45.0f, 0.0f, 45.0f) : glm::vec3(-45.0f, 0.0f, -45.0f);
    } else if (sIt->second.mode == GameMode::Infiltration) {
        resp.spawnPosition = glm::vec3(-120.0f, 0.0f, -120.0f);
    } else {
        // Continental Hex microserver starting positions
        if (sIt->first == 101) resp.spawnPosition = glm::vec3(-25.0f, 0.0f, 85.0f);      // Sumy
        else if (sIt->first == 102) resp.spawnPosition = glm::vec3(0.0f, 0.0f, 75.0f);    // Kharkiv
        else if (sIt->first == 103) resp.spawnPosition = glm::vec3(-35.0f, 0.0f, 0.0f);   // Donetsk
        else if (sIt->first == 104) resp.spawnPosition = glm::vec3(35.0f, 0.0f, 0.0f);    // Luhansk
        else if (sIt->first == 105) resp.spawnPosition = glm::vec3(-25.0f, 0.0f, -85.0f); // Zaporizhzhia
        else if (sIt->first == 106) resp.spawnPosition = glm::vec3(25.0f, 0.0f, -85.0f);  // Kherson
        else resp.spawnPosition = (req->faction == 1) ? glm::vec3(0.0f, 0.0f, -75.0f) : glm::vec3(0.0f, 0.0f, 75.0f);
    }
    sIt->second.spatialGrid.updateEntityPosition(req->playerId, resp.spawnPosition);

    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::JoinSessionResponse;
    hdr.sequenceNumber = req->sessionId;

    char sendBuf[sizeof(PacketHeader) + sizeof(JoinSessionResponsePacket)];
    std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), &resp, sizeof(JoinSessionResponsePacket));
    sendRaw(sendBuf, sizeof(sendBuf), from);

    // Sync persistent world structures for this region to the joining client
    if (!sIt->second.mapName.empty()) {
        std::vector<WorldStructure> structs = DatabaseEngine::get().getRegionStructures(sIt->second.mapName);
        for (const auto& ws : structs) {
            StructureBuildPacket sPkt{};
            sPkt.builderId = (uint32_t)ws.builtByAccountId;
            sPkt.structureType = ws.structureType;
            sPkt.faction = ws.faction;
            strncpy_s(sPkt.regionId, ws.regionId.c_str(), sizeof(sPkt.regionId) - 1);
            sPkt.position = ws.position;
            sPkt.rotYaw = ws.rotYaw;
            sPkt.health = ws.health;

            PacketHeader sHdr;
            sHdr.magic = 0x5647;
            sHdr.type = PacketType::StructureBuildConfirm;
            sHdr.sequenceNumber = (uint32_t)ws.id;
            sHdr.timestamp = 0.0f;

            char sBuf[sizeof(PacketHeader) + sizeof(StructureBuildPacket)];
            std::memcpy(sBuf, &sHdr, sizeof(PacketHeader));
            std::memcpy(sBuf + sizeof(PacketHeader), &sPkt, sizeof(StructureBuildPacket));
            sendRaw(sBuf, sizeof(sBuf), from);
        }
    }

    std::cout << "[Hub] Operative #" << req->playerId << " entered Session #" << req->sessionId 
              << " (" << sIt->second.name << ") [Total: " << sIt->second.playerIds.size() << "]\n" << std::flush;
}

void HubServer::handleEntityState(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(PlayerNetworkState))) return;
    const PlayerNetworkState* state = reinterpret_cast<const PlayerNetworkState*>(buffer + sizeof(PacketHeader));

    auto it = m_clients.find(state->playerId);
    if (it == m_clients.end()) return;

    // Security check: IP/Port authentication
    if (it->second.address.sin_addr.s_addr != from.sin_addr.s_addr ||
        it->second.address.sin_port != from.sin_port) {
        return;
    }

    it->second.state = *state;
    it->second.lastHeartbeat = 0.0f;

    // Sub-Tick Rewind Snapshot Recording (Rolling 1000ms buffer)
    PlayerTickSnapshot snap;
    snap.tick = m_serverTick;
    snap.timestamp = m_serverTime;
    snap.position = it->second.state.position;
    snap.velocity = it->second.state.velocity;
    snap.yaw = it->second.state.yaw;
    snap.pitch = 0.0f;

    it->second.history.push_back(snap);
    while (it->second.history.size() > 64) {
        it->second.history.pop_front();
    }

    uint32_t sId = it->second.sessionId;
    auto sIt = m_sessions.find(sId);
    if (sIt == m_sessions.end()) return;

    // Route only to operatives inside the SAME session!
    if (sIt->second.mode == GameMode::OpenWorld && sIt->second.playerIds.size() > 64) {
        // High-concurrency MMO spatial grid routing
        sIt->second.spatialGrid.updateEntityPosition(state->playerId, state->position);
        auto relevancy = sIt->second.spatialGrid.getRelevancyList(state->position);
        for (uint32_t rId : relevancy) {
            if (rId != state->playerId) {
                auto rIt = m_clients.find(rId);
                if (rIt != m_clients.end()) {
                    sendRaw(buffer, len, rIt->second.address);
                }
            }
        }
    } else {
        // TDM / Infiltration / Medium Session: Route to all in this match
        for (uint32_t rId : sIt->second.playerIds) {
            if (rId != state->playerId) {
                auto rIt = m_clients.find(rId);
                if (rIt != m_clients.end()) {
                    sendRaw(buffer, len, rIt->second.address);
                }
            }
        }
    }
}

void HubServer::handleFireWeapon(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(BulletFireEvent))) return;
    const BulletFireEvent* evt = reinterpret_cast<const BulletFireEvent*>(buffer + sizeof(PacketHeader));

    auto it = m_clients.find(evt->shooterId);
    if (it == m_clients.end()) return;
    if (it->second.address.sin_addr.s_addr != from.sin_addr.s_addr ||
        it->second.address.sin_port != from.sin_port) return;

    uint32_t sId = it->second.sessionId;
    auto sIt = m_sessions.find(sId);
    if (sIt == m_sessions.end()) return;

    // Route ballistics only inside this session
    for (uint32_t rId : sIt->second.playerIds) {
        if (rId != evt->shooterId) {
            auto rIt = m_clients.find(rId);
            if (rIt != m_clients.end()) {
                sendRaw(buffer, len, rIt->second.address);
            }
        }
    }
}

void HubServer::handleHitConfirm(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(HitConfirmEvent))) return;
    const HitConfirmEvent* evt = reinterpret_cast<const HitConfirmEvent*>(buffer + sizeof(PacketHeader));

    auto shooterIt = m_clients.find(evt->shooterId);
    if (shooterIt == m_clients.end()) return;
    if (shooterIt->second.address.sin_addr.s_addr != from.sin_addr.s_addr ||
        shooterIt->second.address.sin_port != from.sin_port) return;

    uint32_t sId = shooterIt->second.sessionId;
    auto sIt = m_sessions.find(sId);
    if (sIt == m_sessions.end()) return;

    // Validate victim belongs to SAME session
    auto victimIt = m_clients.find(evt->victimId);
    if (victimIt != m_clients.end() && victimIt->second.sessionId == sId) {
        // Authoritative Sub-Tick Rewind Validation
        float shotTimestamp = m_serverTime - std::clamp(shooterIt->second.estimatedRtt * 0.5f, 0.015f, 0.40f);
        if (!victimIt->second.history.empty()) {
            glm::vec3 rewoundPos = getRewoundPosition(victimIt->second, shotTimestamp);
            float distToRewound = glm::distance(evt->hitPoint, rewoundPos);
            // Allow 4.8m tolerance (cylinder radius 0.6m + height 1.8m + max sprint speed 7.5m/s * 150ms network jitter)
            if (distToRewound > 4.8f) {
                std::cout << "[Hub Sub-Tick Rewind] Rejected ghost bullet: victim #" << evt->victimId
                          << " was " << distToRewound << "m away from hitPoint at T=" << shotTimestamp << "\n" << std::flush;
                return;
            }
        }

        victimIt->second.state.health = std::max(0.0f, victimIt->second.state.health - evt->damage);

        // If TDM, deduct ticket from victim's faction on fatal hit
        if (sIt->second.mode == GameMode::TeamDeathmatch && evt->isFatal) {
            if (victimIt->second.state.faction == 0) { // Coalition died
                sIt->second.coalitionTickets = std::max<int16_t>(0, sIt->second.coalitionTickets - 1);
            } else { // Crimson died
                sIt->second.crimsonTickets = std::max<int16_t>(0, sIt->second.crimsonTickets - 1);
            }
        }
    }

    // Broadcast hit confirm to match operatives
    for (uint32_t rId : sIt->second.playerIds) {
        auto rIt = m_clients.find(rId);
        if (rIt != m_clients.end()) {
            sendRaw(buffer, len, rIt->second.address);
        }
    }
}

void HubServer::handleChatMessage(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(ChatMessagePacket))) return;
    const ChatMessagePacket* chat = reinterpret_cast<const ChatMessagePacket*>(buffer + sizeof(PacketHeader));

    auto senderIt = m_clients.find(chat->senderId);
    if (senderIt == m_clients.end()) return;
    if (senderIt->second.address.sin_addr.s_addr != from.sin_addr.s_addr ||
        senderIt->second.address.sin_port != from.sin_port) return;

    uint32_t sId = senderIt->second.sessionId;
    auto sIt = m_sessions.find(sId);
    bool inOpenWorld = (sIt != m_sessions.end() && sIt->second.mode == GameMode::OpenWorld);

    ChatChannel ch = static_cast<ChatChannel>(chat->channel);

    if (ch == ChatChannel::Global) {
        // OPEN MMO WORLD CHAT: Broadcast to all operatives across the persistent world and lobbies!
        for (const auto& [cId, client] : m_clients) {
            sendRaw(buffer, len, client.address);
        }
        std::cout << "[World Chat // GLOBAL] " << chat->senderName << ": " << chat->message << "\n" << std::flush;
    }
    else if (ch == ChatChannel::Team) {
        // FACTION CHAT:
        // In Open World: Broadcast army-wide to ALL operatives of the same faction across the entire persistent world!
        // In TDM/Infiltration: Routed to teammates inside that active match.
        if (inOpenWorld) {
            for (const auto& [cId, client] : m_clients) {
                if (client.state.faction == chat->faction) {
                    sendRaw(buffer, len, client.address);
                }
            }
            std::string facStr = (chat->faction == 1) ? "CRIMSON ARMY" : "COALITION ARMY";
            std::cout << "[Faction Chat // " << facStr << "] " << chat->senderName << ": " << chat->message << "\n" << std::flush;
        } else if (sIt != m_sessions.end()) {
            for (uint32_t rId : sIt->second.playerIds) {
                auto rIt = m_clients.find(rId);
                if (rIt != m_clients.end() && rIt->second.state.faction == chat->faction) {
                    sendRaw(buffer, len, rIt->second.address);
                }
            }
            std::cout << "[Match Team Chat // Room #" << sId << "] " << chat->senderName << ": " << chat->message << "\n" << std::flush;
        }
    }
    else if (ch == ChatChannel::Whisper) {
        // DIRECT PRIVATE MESSAGE / WHISPER:
        // Peer-to-peer delivery to targetId anywhere on the hub server!
        auto targetIt = m_clients.find(chat->targetId);
        if (targetIt != m_clients.end()) {
            sendRaw(buffer, len, targetIt->second.address);
            sendRaw(buffer, len, senderIt->second.address); // Echo to sender
            std::cout << "[Whisper] " << chat->senderName << " -> Operative #" << chat->targetId << ": " << chat->message << "\n" << std::flush;
        }
    }
    else if (ch == ChatChannel::Zone) {
        // REGIONAL SECTOR CHAT:
        // Routed to all operatives in the same map/zone (e.g. Siversky Warzone)
        std::string currentMap = (sIt != m_sessions.end()) ? sIt->second.mapName : "";
        for (const auto& [cId, client] : m_clients) {
            auto clientSessIt = m_sessions.find(client.sessionId);
            if (clientSessIt != m_sessions.end() && clientSessIt->second.mapName == currentMap) {
                sendRaw(buffer, len, client.address);
            }
        }
        std::cout << "[Zone Chat // " << currentMap << "] " << chat->senderName << ": " << chat->message << "\n" << std::flush;
    }
    else if (ch == ChatChannel::Proximity) {
        // 3D SPATIAL PROXIMITY:
        // 35m radius in the same session/world space (enemies and friendlies hear it)
        if (sIt != m_sessions.end()) {
            for (uint32_t rId : sIt->second.playerIds) {
                auto rIt = m_clients.find(rId);
                if (rIt == m_clients.end()) continue;
                float dist = glm::distance(rIt->second.state.position, chat->position);
                if (dist <= 35.0f) {
                    sendRaw(buffer, len, rIt->second.address);
                }
            }
        }
    }
    else if (ch == ChatChannel::Match) {
        // MATCH ALL-TALK (TDM / INFILTRATION ONLY):
        // Delivered to all operatives inside this specific room
        if (sIt != m_sessions.end()) {
            for (uint32_t rId : sIt->second.playerIds) {
                auto rIt = m_clients.find(rId);
                if (rIt != m_clients.end()) {
                    sendRaw(buffer, len, rIt->second.address);
                }
            }
        }
    }
    else {
        // System / Default Fallback
        if (sIt != m_sessions.end()) {
            for (uint32_t rId : sIt->second.playerIds) {
                auto rIt = m_clients.find(rId);
                if (rIt != m_clients.end()) {
                    sendRaw(buffer, len, rIt->second.address);
                }
            }
        }
    }
}

void HubServer::handleVoiceData(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(VoiceDataPacket))) return;
    const VoiceDataPacket* voice = reinterpret_cast<const VoiceDataPacket*>(buffer + sizeof(PacketHeader));

    auto speakerIt = m_clients.find(voice->speakerId);
    if (speakerIt == m_clients.end()) return;
    if (speakerIt->second.address.sin_addr.s_addr != from.sin_addr.s_addr ||
        speakerIt->second.address.sin_port != from.sin_port) return;

    uint32_t sId = speakerIt->second.sessionId;
    auto sIt = m_sessions.find(sId);
    if (sIt == m_sessions.end()) return;

    // STRICT VOICE PARTITIONING BY SESSION
    for (uint32_t rId : sIt->second.playerIds) {
        if (rId == voice->speakerId) continue;
        auto rIt = m_clients.find(rId);
        if (rIt == m_clients.end()) continue;

        if (voice->channel == 0) { // Proximity Voice (40m)
            float dist = glm::distance(rIt->second.state.position, voice->speakerPos);
            if (dist <= 40.0f) {
                sendRaw(buffer, len, rIt->second.address);
            }
        } else { // Squad / Team Radio
            if (rIt->second.state.faction == speakerIt->second.state.faction) {
                sendRaw(buffer, len, rIt->second.address);
            }
        }
    }
}

void HubServer::handleDisconnect(const sockaddr_in& from) {
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it->second.address.sin_addr.s_addr == from.sin_addr.s_addr &&
            it->second.address.sin_port == from.sin_port) {
            uint32_t departedId = it->first;
            uint32_t sId = it->second.sessionId;
            auto sIt = m_sessions.find(sId);
            if (sIt != m_sessions.end()) {
                auto& pList = sIt->second.playerIds;
                pList.erase(std::remove(pList.begin(), pList.end(), departedId), pList.end());
                sIt->second.spatialGrid.removeEntity(departedId);

                // Authoritative broadcast of Disconnect packet to notify session players immediately
                PacketHeader discHdr;
                discHdr.magic = 0x5647;
                discHdr.type = PacketType::Disconnect;
                discHdr.sequenceNumber = departedId;
                discHdr.timestamp = 0.0f;
                for (uint32_t remId : pList) {
                    auto remIt = m_clients.find(remId);
                    if (remIt != m_clients.end()) {
                        sendRaw(&discHdr, sizeof(discHdr), remIt->second.address);
                    }
                }
            }
            std::cout << "[Hub] Operative #" << departedId << " disconnected from Session #" << sId << ".\n" << std::flush;
            m_clients.erase(it);
            break;
        }
    }
}

uint32_t HubServer::createSession(GameMode mode, const std::string& name, const std::string& mapName, uint16_t maxPlayers) {
    uint32_t newId = 1000 + static_cast<uint32_t>(m_sessions.size() + 1);
    HubSession sess;
    sess.id = newId;
    sess.mode = mode;
    sess.name = name;
    sess.mapName = mapName;
    sess.maxPlayers = maxPlayers;

    if (mode == GameMode::TeamDeathmatch) {
        sess.coalitionTickets = 100;
        sess.crimsonTickets = 100;
        sess.roundTimer = 600.0f;
    } else if (mode == GameMode::Infiltration) {
        sess.raidTimer = 900.0f;
        sess.exfilActive = false;
        sess.exfilZone = glm::vec3(0.0f, 0.0f, 180.0f);
    }
    m_sessions[newId] = sess;
    std::cout << "[Hub] Created new session #" << newId << " [" << name << "]\n" << std::flush;
    return newId;
}

bool HubServer::removeSession(uint32_t sessionId) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return false;
    for (uint32_t pId : it->second.playerIds) {
        auto cIt = m_clients.find(pId);
        if (cIt != m_clients.end()) {
            cIt->second.sessionId = 1001; // Fallback to Open World
        }
    }
    m_sessions.erase(it);
    return true;
}

HubSession* HubServer::getSession(uint32_t sessionId) {
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? &it->second : nullptr;
}

void HubServer::listSessionsConsole() const {
    std::cout << std::setfill(' ');
    std::cout << "\n============================== ACTIVE SESSIONS ==============================\n";
    std::cout << std::left << std::setw(8)  << "ID"
              << std::setw(16) << "MODE"
              << std::setw(34) << "SESSION NAME"
              << std::setw(14) << "MAP"
              << std::setw(12) << "OPERATIVES"
              << "STATUS / TICKETS\n";
    std::cout << "-----------------------------------------------------------------------------\n";

    for (const auto& [id, s] : m_sessions) {
        std::string modeStr;
        std::string statusStr;
        if (s.mode == GameMode::OpenWorld) {
            modeStr = "OPEN WORLD";
            statusStr = "Frontline Sector 9-Chunk AOI";
        } else if (s.mode == GameMode::TeamDeathmatch) {
            modeStr = "DEATHMATCH";
            statusStr = "COA: " + std::to_string(s.coalitionTickets) + " | CRIM: " + std::to_string(s.crimsonTickets);
        } else {
            modeStr = "INFILTRATION";
            statusStr = (s.exfilActive ? "EXFIL ACTIVE (" : "Infil (") + std::to_string((int)s.raidTimer) + "s)";
        }

        std::string countStr = std::to_string(s.playerIds.size()) + " / " + std::to_string(s.maxPlayers);

        std::cout << std::left << std::setw(8)  << id
                  << std::setw(16) << modeStr
                  << std::setw(34) << s.name.substr(0, 32)
                  << std::setw(14) << s.mapName
                  << std::setw(12) << countStr
                  << statusStr << "\n";
    }
    std::cout << "=============================================================================\n\n" << std::flush;
}

void HubServer::printStatus() const {
    auto now = std::chrono::steady_clock::now();
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(now - m_startTime).count();
    int h = static_cast<int>(sec / 3600);
    int m = static_cast<int>((sec % 3600) / 60);
    int s = static_cast<int>(sec % 60);

    std::cout << "\n[HUB STATUS] Uptime: " << std::setfill('0') << std::setw(2) << h << ":"
              << std::setfill('0') << std::setw(2) << m << ":" << std::setfill('0') << std::setw(2) << s
              << std::setfill(' ')
              << " | Total Operatives: " << m_clients.size()
              << " | Active Sessions: " << m_sessions.size() << "\n";

    if (!m_clients.empty()) {
        std::cout << "-----------------------------------------------------------------------------\n";
        for (const auto& [id, c] : m_clients) {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &c.address.sin_addr, ip, sizeof(ip));
            std::string fac = (c.state.faction == 1) ? "CRIMSON" : "COALITION";
            std::string cName = c.state.callsign[0] ? c.state.callsign : "OPERATIVE";
            std::cout << "  -> Operative #" << id << " [" << fac << "] " << cName
                      << " in Session #" << c.sessionId << " (" << ip << ":" << ntohs(c.address.sin_port) << ")"
                      << " [HP: " << (int)c.state.health << "%]\n";
        }
    }
    std::cout << "\n" << std::flush;
}

void HubServer::broadcastSystemMessage(const std::string& msg) {
    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::ChatMessage;
    hdr.sequenceNumber = 0;

    ChatMessagePacket chat{};
    chat.senderId = 0;
    chat.channel = 3; // System
    strncpy_s(chat.senderName, "[SERVER COMMAND]", sizeof(chat.senderName) - 1);
    strncpy_s(chat.message, msg.c_str(), sizeof(chat.message) - 1);

    char sendBuf[sizeof(PacketHeader) + sizeof(ChatMessagePacket)];
    std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), &chat, sizeof(ChatMessagePacket));

    for (const auto& [id, c] : m_clients) {
        sendRaw(sendBuf, sizeof(sendBuf), c.address);
    }
    std::cout << "[Broadcast Sent] " << msg << "\n" << std::flush;
}

void HubServer::sendSessionSystemMessage(uint32_t sessionId, const std::string& msg) {
    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) return;

    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::ChatMessage;

    ChatMessagePacket chat{};
    chat.senderId = 0;
    chat.channel = 3; // System
    strncpy_s(chat.senderName, "[MISSION DISPATCH]", sizeof(chat.senderName) - 1);
    strncpy_s(chat.message, msg.c_str(), sizeof(chat.message) - 1);

    char sendBuf[sizeof(PacketHeader) + sizeof(ChatMessagePacket)];
    std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), &chat, sizeof(ChatMessagePacket));

    for (uint32_t pId : it->second.playerIds) {
        auto cIt = m_clients.find(pId);
        if (cIt != m_clients.end()) {
            sendRaw(sendBuf, sizeof(sendBuf), cIt->second.address);
        }
    }
}

bool HubServer::kickPlayer(uint32_t playerId, const std::string& reason) {
    auto it = m_clients.find(playerId);
    if (it == m_clients.end()) return false;

    PacketHeader disc;
    disc.magic = 0x5647;
    disc.type = PacketType::Disconnect;
    sendRaw(&disc, sizeof(disc), it->second.address);

    uint32_t sId = it->second.sessionId;
    auto sIt = m_sessions.find(sId);
    if (sIt != m_sessions.end()) {
        auto& pList = sIt->second.playerIds;
        pList.erase(std::remove(pList.begin(), pList.end(), playerId), pList.end());
        sIt->second.spatialGrid.removeEntity(playerId);

        PacketHeader discHdr;
        discHdr.magic = 0x5647;
        discHdr.type = PacketType::Disconnect;
        discHdr.sequenceNumber = playerId;
        discHdr.timestamp = 0.0f;
        for (uint32_t remId : pList) {
            auto remIt = m_clients.find(remId);
            if (remIt != m_clients.end()) {
                sendRaw(&discHdr, sizeof(discHdr), remIt->second.address);
            }
        }
    }
    m_clients.erase(it);
    std::cout << "[Hub Admin] Kicked Operative #" << playerId << " (" << reason << ").\n" << std::flush;
    return true;
}

void HubServer::consoleCommandLoop() {
    std::string line;
    while (m_running && std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "help" || cmd == "?") {
            std::cout << "\n======================== HUB SERVER COMMANDS ========================\n"
                      << "  status              - Show live server health, uptime & connected operatives\n"
                      << "  sessions            - Display table of all active game rooms/sessions\n"
                      << "  create <m> <map> <n>- Create session (m: tdm, infil, openworld)\n"
                      << "  kick <id>           - Disconnect an operative by Player ID\n"
                      << "  say <id> <message>  - Dispatch message to specific session\n"
                      << "  broadcast <message> - Global announcement to ALL connected operatives\n"
                      << "  quit / exit         - Safely disconnect all operatives and shut down\n"
                      << "=====================================================================\n\n" << std::flush;
        } else if (cmd == "status") {
            printStatus();
        } else if (cmd == "sessions" || cmd == "list") {
            listSessionsConsole();
        } else if (cmd == "broadcast") {
            std::string msg;
            std::getline(iss, msg);
            if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
            broadcastSystemMessage(msg);
        } else if (cmd == "say") {
            uint32_t sId = 0;
            iss >> sId;
            std::string msg;
            std::getline(iss, msg);
            if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
            sendSessionSystemMessage(sId, msg);
        } else if (cmd == "kick") {
            uint32_t pId = 0;
            iss >> pId;
            kickPlayer(pId);
        } else if (cmd == "create") {
            std::string mStr, mapName, name;
            iss >> mStr >> mapName;
            std::getline(iss, name);
            if (!name.empty() && name[0] == ' ') name = name.substr(1);
            if (name.empty()) name = "CUSTOM SESSION";

            GameMode mode = GameMode::TeamDeathmatch;
            if (mStr == "openworld") mode = GameMode::OpenWorld;
            else if (mStr == "infil") mode = GameMode::Infiltration;

            createSession(mode, name, mapName, (mode == GameMode::OpenWorld) ? 3000 : 32);
        } else if (cmd == "quit" || cmd == "exit") {
            std::cout << "[Hub Server] Shutdown requested via console command.\n" << std::flush;
            m_running = false;
            break;
        } else {
            std::cout << "Unknown command '" << cmd << "'. Type 'help' for command list.\n" << std::flush;
        }
    }
}



uint32_t HubServer::spawnEphemeralTdmSession(const std::string& mapName) {
    uint32_t sId = m_nextSessionId++;
    HubSession sess;
    sess.id = sId;
    sess.mode = GameMode::TeamDeathmatch;
    sess.lifecycle = SessionLifecycle::EphemeralTDM;
    sess.name = "TDM // YARD ARENA (INSTANCE #" + std::to_string(sId) + ")";
    sess.mapName = mapName.empty() ? "tdm_yard" : mapName;
    sess.maxPlayers = 16;
    sess.coalitionTickets = 100;
    sess.crimsonTickets = 100;
    sess.roundTimer = 600.0f; // 10 minutes
    sess.sessionAge = 0.0f;
    sess.postMatchTimer = 0.0f;
    sess.isMarkedForDestruction = false;
    m_sessions[sId] = sess;

    std::cout << "[MicroServer Cluster] Spawning on-demand Ephemeral TDM Instance #" << sId
              << " [Map: " << sess.mapName << "] (Auto-destruct on completion)\n" << std::flush;
    return sId;
}

uint32_t HubServer::spawnExtractionRaidSession(const std::string& mapName) {
    uint32_t sId = m_nextSessionId++;
    HubSession sess;
    sess.id = sId;
    sess.mode = GameMode::Infiltration;
    sess.lifecycle = SessionLifecycle::ExtractionRaid40M;
    sess.name = "INFILTRATION // RAID (INSTANCE #" + std::to_string(sId) + ")";
    sess.mapName = mapName.empty() ? "fob_vanguard" : mapName;
    sess.maxPlayers = 16;
    sess.raidTimer = 2400.0f; // Strict 40:00 hardware countdown
    sess.sessionAge = 0.0f;
    sess.exfilActive = false;
    sess.exfilZone = glm::vec3(0.0f, 0.0f, 180.0f);
    sess.objectivesRemaining = 3;
    sess.hasCommittedStash = false;
    sess.postMatchTimer = 0.0f;
    sess.isMarkedForDestruction = false;
    m_sessions[sId] = sess;

    std::cout << "[MicroServer Cluster] Spawning on-demand Infiltration Raid Instance #" << sId
              << " [Map: " << sess.mapName << "] (Strict 40:00 Lifecycle Active)\n" << std::flush;
    return sId;
}

void HubServer::handleMatchmakeRequest(const sockaddr_in& from, const char* buffer, int len) {
    if (len < static_cast<int>(sizeof(PacketHeader) + sizeof(MatchmakeRequestPacket))) return;
    const auto* req = reinterpret_cast<const MatchmakeRequestPacket*>(buffer + sizeof(PacketHeader));

    uint32_t targetSessionId = 0;
    std::string sName = "";
    std::string mMap = "";
    float timeRem = 0.0f;

    if (req->desiredMode == static_cast<uint8_t>(GameMode::OpenWorld)) {
        targetSessionId = 1001;
        auto it = m_sessions.find(1001);
        if (it != m_sessions.end()) {
            sName = it->second.name;
            mMap = it->second.mapName;
        }
    } else if (req->desiredMode == static_cast<uint8_t>(GameMode::TeamDeathmatch)) {
        for (const auto& [sId, sess] : m_sessions) {
            if (sess.mode == GameMode::TeamDeathmatch && sess.lifecycle == SessionLifecycle::EphemeralTDM &&
                sess.matchStatus == 0 && sess.playerIds.size() < sess.maxPlayers) {
                targetSessionId = sId;
                sName = sess.name;
                mMap = sess.mapName;
                timeRem = sess.roundTimer;
                break;
            }
        }
        if (targetSessionId == 0) {
            std::string reqMap = req->mapName[0] ? req->mapName : "tdm_yard";
            targetSessionId = spawnEphemeralTdmSession(reqMap);
            auto it = m_sessions.find(targetSessionId);
            if (it != m_sessions.end()) {
                sName = it->second.name;
                mMap = it->second.mapName;
                timeRem = it->second.roundTimer;
            }
        }
    } else if (req->desiredMode == static_cast<uint8_t>(GameMode::Infiltration)) {
        for (const auto& [sId, sess] : m_sessions) {
            if (sess.mode == GameMode::Infiltration && sess.lifecycle == SessionLifecycle::ExtractionRaid40M &&
                sess.raidTimer > 900.0f && sess.playerIds.size() < sess.maxPlayers) {
                targetSessionId = sId;
                sName = sess.name;
                mMap = sess.mapName;
                timeRem = sess.raidTimer;
                break;
            }
        }
        if (targetSessionId == 0) {
            std::string reqMap = req->mapName[0] ? req->mapName : "fob_vanguard";
            targetSessionId = spawnExtractionRaidSession(reqMap);
            auto it = m_sessions.find(targetSessionId);
            if (it != m_sessions.end()) {
                sName = it->second.name;
                mMap = it->second.mapName;
                timeRem = it->second.raidTimer;
            }
        }
    }

    PacketHeader replyHdr;
    replyHdr.magic = 0x5647;
    replyHdr.type = PacketType::MatchmakeResponse;
    replyHdr.sequenceNumber = targetSessionId;
    replyHdr.timestamp = 0.0f;

    MatchmakeResponsePacket resp;
    resp.sessionId = targetSessionId;
    resp.mode = req->desiredMode;
    resp.success = (targetSessionId > 0) ? 1 : 0;
    resp.port = m_port;
    resp.timeRemaining = timeRem;
    std::strncpy(resp.sessionName, sName.c_str(), sizeof(resp.sessionName) - 1);
    std::strncpy(resp.mapName, mMap.c_str(), sizeof(resp.mapName) - 1);

    char sendBuf[sizeof(PacketHeader) + sizeof(MatchmakeResponsePacket)];
    std::memcpy(sendBuf, &replyHdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), &resp, sizeof(MatchmakeResponsePacket));
    sendRaw(sendBuf, sizeof(sendBuf), from);
}



void HubServer::handleAuthRequest(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(AuthRequestPacket))) return;
    const auto* req = reinterpret_cast<const AuthRequestPacket*>(buffer + sizeof(PacketHeader));

    PacketHeader respHdr;
    respHdr.magic = 0x5647;
    respHdr.type = PacketType::AuthResponse;
    respHdr.sequenceNumber = 0;
    respHdr.timestamp = 0.0f;

    AuthResponsePacket resp{};
    uint32_t accountId = 0;
    std::string callsign;

    if (req->authType == 0) {
        // Standalone Username + Password Authentication
        std::string user(req->username);
        std::string pass(req->passwordHash);
        if (DatabaseEngine::get().authenticateUser(user, pass, accountId, callsign)) {
            resp.success = 1;
            resp.accountId = accountId;
            strncpy_s(resp.callsign, callsign.c_str(), sizeof(resp.callsign) - 1);

            OperativeProfileData prof;
            if (DatabaseEngine::get().getOperativeProfile(accountId, prof)) {
                resp.faction = prof.faction;
                resp.level = prof.level;
                resp.cash = prof.currency;
                resp.goldBars = 25;
                resp.skinToneId = prof.custom.skinToneId;
                resp.faceId = prof.custom.faceId;
                resp.hairId = prof.custom.hairId;
                resp.beardId = prof.custom.beardId;
                resp.helmetId = prof.custom.helmetId;
                resp.vestId = prof.custom.vestId;
                resp.camoId = prof.custom.camoId;
                resp.backpackId = prof.custom.backpackId;
                resp.gloveId = prof.custom.gloveId;
                resp.armbandId = prof.custom.armbandId;
            }
            std::cout << "[Hub Auth] Operative #" << accountId << " (" << callsign << ") authenticated successfully (Standalone)\n" << std::flush;
        } else {
            resp.success = 0;
            strncpy_s(resp.errorMessage, "INVALID OPERATIVE CREDENTIALS", sizeof(resp.errorMessage) - 1);
            std::cout << "[Hub Auth] Standalone authentication failed for user: " << user << "\n" << std::flush;
        }
    } else if (req->authType == 1) {
        // Steam 1-Click Silent Authentication
        std::string steamId(req->steamId);
        std::string persona(req->personaName);
        if (DatabaseEngine::get().authenticateSteamUser(steamId, persona, accountId, callsign)) {
            resp.success = 1;
            resp.accountId = accountId;
            strncpy_s(resp.callsign, callsign.c_str(), sizeof(resp.callsign) - 1);

            OperativeProfileData prof;
            if (DatabaseEngine::get().getOperativeProfile(accountId, prof)) {
                resp.faction = prof.faction;
                resp.level = prof.level;
                resp.cash = prof.currency;
                resp.goldBars = 25;
                resp.skinToneId = prof.custom.skinToneId;
                resp.faceId = prof.custom.faceId;
                resp.hairId = prof.custom.hairId;
                resp.beardId = prof.custom.beardId;
                resp.helmetId = prof.custom.helmetId;
                resp.vestId = prof.custom.vestId;
                resp.camoId = prof.custom.camoId;
                resp.backpackId = prof.custom.backpackId;
                resp.gloveId = prof.custom.gloveId;
                resp.armbandId = prof.custom.armbandId;
            }
            std::cout << "[Hub Auth] Steam operative #" << accountId << " (" << callsign << " / SteamID " << steamId << ") authenticated\n" << std::flush;
        } else {
            resp.success = 0;
            strncpy_s(resp.errorMessage, "STEAM AUTHORIZATION REJECTED", sizeof(resp.errorMessage) - 1);
        }
    }

    char outBuf[sizeof(PacketHeader) + sizeof(AuthResponsePacket)];
    memcpy(outBuf, &respHdr, sizeof(PacketHeader));
    memcpy(outBuf + sizeof(PacketHeader), &resp, sizeof(AuthResponsePacket));
    sendRaw(outBuf, sizeof(outBuf), from);
}

void HubServer::handleRegisterRequest(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(RegisterRequestPacket))) return;
    const auto* req = reinterpret_cast<const RegisterRequestPacket*>(buffer + sizeof(PacketHeader));

    PacketHeader respHdr;
    respHdr.magic = 0x5647;
    respHdr.type = PacketType::RegisterResponse;
    respHdr.sequenceNumber = 0;
    respHdr.timestamp = 0.0f;

    RegisterResponsePacket resp{};
    std::string user(req->username);
    std::string pass(req->passwordHash);
    std::string callsign(req->callsign);

    if (user.empty() || pass.empty() || callsign.empty()) {
        resp.success = 0;
        strncpy_s(resp.errorMessage, "ALL DOSSIER FIELDS MANDATORY", sizeof(resp.errorMessage) - 1);
    } else {
        CharacterCustomization custom;
        custom.skinToneId = req->skinToneId;
        custom.faceId = req->faceId;
        custom.hairId = req->hairId;
        custom.beardId = req->beardId;
        uint32_t accountId = 0;
        if (DatabaseEngine::get().registerAccount(user, pass, callsign, req->faction, custom, accountId)) {
            resp.success = 1;
            resp.accountId = accountId;
            std::cout << "[Hub Auth] Enlisted new combat operative: #" << accountId << " (" << callsign << " / " << user << ")\n" << std::flush;
        } else {
            resp.success = 0;
            strncpy_s(resp.errorMessage, "USERNAME OR CALLSIGN ALREADY REGISTERED", sizeof(resp.errorMessage) - 1);
            std::cout << "[Hub Auth] Registration rejected: user/callsign already in central database (" << user << " / " << callsign << ")\n" << std::flush;
        }
    }

    char outBuf[sizeof(PacketHeader) + sizeof(RegisterResponsePacket)];
    memcpy(outBuf, &respHdr, sizeof(PacketHeader));
    memcpy(outBuf + sizeof(PacketHeader), &resp, sizeof(RegisterResponsePacket));
    sendRaw(outBuf, sizeof(outBuf), from);
}


glm::vec3 HubServer::getRewoundPosition(const HubClient& victim, float shotTimestamp) const {
    if (victim.history.empty()) return victim.state.position;
    if (shotTimestamp >= victim.history.back().timestamp) return victim.history.back().position;
    if (shotTimestamp <= victim.history.front().timestamp) return victim.history.front().position;

    for (size_t i = 1; i < victim.history.size(); ++i) {
        if (victim.history[i].timestamp >= shotTimestamp) {
            const auto& s0 = victim.history[i - 1];
            const auto& s1 = victim.history[i];
            float duration = s1.timestamp - s0.timestamp;
            float factor = (duration > 0.0001f) ? (shotTimestamp - s0.timestamp) / duration : 0.0f;
            factor = std::clamp(factor, 0.0f, 1.0f);
            return glm::mix(s0.position, s1.position, factor);
        }
    }
    return victim.history.back().position;
}

void HubServer::handleSectorHandoff(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(SectorHandoffPacket))) return;
    const SectorHandoffPacket* pkt = reinterpret_cast<const SectorHandoffPacket*>(buffer + sizeof(PacketHeader));
    auto it = m_clients.find(pkt->playerId);
    if (it == m_clients.end()) return;

    // Map target region string to child hex session id
    uint32_t targetChildSessionId = 101;
    std::string regStr = pkt->targetRegionId;
    if (regStr == "sumy") targetChildSessionId = 101;
    else if (regStr == "kharkiv") targetChildSessionId = 102;
    else if (regStr == "donetsk") targetChildSessionId = 103;
    else if (regStr == "luhansk") targetChildSessionId = 104;
    else if (regStr == "zaporizhzhia") targetChildSessionId = 105;
    else if (regStr == "kherson") targetChildSessionId = 106;

    uint32_t oldSessionId = it->second.sessionId;
    if (oldSessionId != targetChildSessionId) {
        // Remove from old hex child session
        auto oldSessIt = m_sessions.find(oldSessionId);
        if (oldSessIt != m_sessions.end()) {
            auto& pList = oldSessIt->second.playerIds;
            pList.erase(std::remove(pList.begin(), pList.end(), pkt->playerId), pList.end());
            oldSessIt->second.spatialGrid.removeEntity(pkt->playerId);
        }

        // Add to new hex child session
        auto newSessIt = m_sessions.find(targetChildSessionId);
        if (newSessIt != m_sessions.end()) {
            newSessIt->second.playerIds.push_back(pkt->playerId);
            newSessIt->second.spatialGrid.updateEntityPosition(pkt->playerId, pkt->globalPos);
        }
        it->second.sessionId = targetChildSessionId;
        std::cout << "[Child Microserver Handoff] Operative #" << pkt->playerId << " handed off from Child Hex #" 
                  << oldSessionId << " to Child Hex #" << targetChildSessionId << " (" << regStr << ") seamlessly!\n" << std::flush;
    }

    PacketHeader respHdr;
    respHdr.magic = 0x5647;
    respHdr.type = PacketType::SectorHandoffConfirm;
    respHdr.sequenceNumber = pkt->playerId;
    respHdr.timestamp = m_serverTime;

    char sendBuf[sizeof(PacketHeader) + sizeof(SectorHandoffPacket)];
    std::memcpy(sendBuf, &respHdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), pkt, sizeof(SectorHandoffPacket));
    sendRaw(sendBuf, sizeof(sendBuf), from);
}

void HubServer::handleStructureBuild(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(StructureBuildPacket))) return;
    const StructureBuildPacket* pkt = reinterpret_cast<const StructureBuildPacket*>(buffer + sizeof(PacketHeader));
    auto it = m_clients.find(pkt->builderId);
    if (it == m_clients.end()) return;

    WorldStructure ws;
    ws.structureType = pkt->structureType;
    ws.faction = pkt->faction;
    ws.regionId = pkt->regionId;
    ws.position = pkt->position;
    ws.rotYaw = pkt->rotYaw;
    ws.health = pkt->health;
    ws.maxHealth = pkt->health;
    ws.builtByAccountId = it->first;

    int64_t newId = 0;
    if (DatabaseEngine::get().createWorldStructure(ws, newId)) {
        std::cout << "[Construction] Placed persistent structure #" << newId << " (Type " << (int)pkt->structureType 
                  << ") in " << pkt->regionId << " by Operative #" << pkt->builderId << "\n" << std::flush;

        uint32_t sId = it->second.sessionId;
        auto sIt = m_sessions.find(sId);
        if (sIt != m_sessions.end()) {
            PacketHeader respHdr;
            respHdr.magic = 0x5647;
            respHdr.type = PacketType::StructureBuildConfirm;
            respHdr.sequenceNumber = static_cast<uint32_t>(newId);
            respHdr.timestamp = m_serverTime;

            char sendBuf[sizeof(PacketHeader) + sizeof(StructureBuildPacket)];
            std::memcpy(sendBuf, &respHdr, sizeof(PacketHeader));
            std::memcpy(sendBuf + sizeof(PacketHeader), pkt, sizeof(StructureBuildPacket));

            for (uint32_t rId : sIt->second.playerIds) {
                auto rIt = m_clients.find(rId);
                if (rIt != m_clients.end()) {
                    sendRaw(sendBuf, sizeof(sendBuf), rIt->second.address);
                }
            }
        }
    }
}

void HubServer::handleStructureDamage(const sockaddr_in& from, const char* buffer, int len) {
    if (len < (int)(sizeof(PacketHeader) + sizeof(StructureDamagePacket))) return;
    const StructureDamagePacket* pkt = reinterpret_cast<const StructureDamagePacket*>(buffer + sizeof(PacketHeader));
    auto it = m_clients.find(pkt->attackerId);
    if (it == m_clients.end()) return;

    uint32_t sId = it->second.sessionId;
    auto sIt = m_sessions.find(sId);
    if (sIt != m_sessions.end()) {
        for (uint32_t rId : sIt->second.playerIds) {
            auto rIt = m_clients.find(rId);
            if (rIt != m_clients.end()) {
                sendRaw(buffer, len, rIt->second.address);
            }
        }
    }
}


std::vector<SessionSummary> HubServer::getSessionsSnapshot() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    std::vector<SessionSummary> res;
    res.reserve(m_sessions.size());
    for (const auto& [sId, s] : m_sessions) {
        SessionSummary sum;
        sum.id = s.id;
        sum.mode = s.mode;
        sum.name = s.name;
        sum.mapName = s.mapName;
        sum.currentPlayers = static_cast<uint16_t>(s.playerIds.size());
        sum.maxPlayers = s.maxPlayers;
        sum.lifecycle = s.lifecycle;
        sum.sessionAge = s.sessionAge;
        sum.coalitionTickets = s.coalitionTickets;
        sum.crimsonTickets = s.crimsonTickets;
        sum.roundTimer = s.roundTimer;
        sum.matchStatus = s.matchStatus;
        sum.raidTimer = s.raidTimer;
        sum.exfilActive = s.exfilActive;
        res.push_back(sum);
    }
    return res;
}

std::vector<ClientSummary> HubServer::getClientsSnapshot() const {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    std::vector<ClientSummary> res;
    res.reserve(m_clients.size());
    for (const auto& [pId, c] : m_clients) {
        ClientSummary sum;
        sum.id = c.id;
        char ipBuf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &c.address.sin_addr, ipBuf, sizeof(ipBuf));
        sum.ipStr = ipBuf;
        sum.port = ntohs(c.address.sin_port);
        sum.sessionId = c.sessionId;
        sum.position = c.state.position;
        sum.yaw = c.state.yaw;
        sum.faction = c.state.faction;
        sum.stance = c.state.stance;
        sum.pingMs = static_cast<int>(c.estimatedRtt * 1000.0f);
        sum.lastHeartbeat = c.lastHeartbeat;
        res.push_back(sum);
    }
    return res;
}

uint32_t HubServer::spawnCustomTdm(const std::string& mapName) {
    return spawnEphemeralTdmSession(mapName.empty() ? "killhouse" : mapName);
}

uint32_t HubServer::spawnCustomInfiltration(const std::string& mapName) {
    return spawnExtractionRaidSession(mapName.empty() ? "fob_vanguard" : mapName);
}

void HubServer::appendLog(const std::string& msg) {
    std::lock_guard<std::mutex> lock(m_logMutex);
    m_guiLogs.push_back(msg);
    while (m_guiLogs.size() > 500) {
        m_guiLogs.pop_front();
    }
}

std::vector<std::string> HubServer::getLogMessages() const {
    std::lock_guard<std::mutex> lock(m_logMutex);
    return std::vector<std::string>(m_guiLogs.begin(), m_guiLogs.end());
}

} // namespace Mercenary

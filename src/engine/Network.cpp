#include "Network.hpp"
#include "CryptoUtils.hpp"
#include "game/CharacterProfile.hpp"
#include "CharacterRig.hpp"
#include "VoiceChat.hpp"
#include "game/ChatSystem.hpp"
#include "json.hpp"
#ifdef _WIN32
#include <wininet.h>
#endif
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace Mercenary {

NetworkEngine& NetworkEngine::get() {
    static NetworkEngine instance;
    return instance;
}

NetworkEngine::NetworkEngine() {
    m_shards = {
        { "cloud-master", "MERCENARY DYNAMIC FLEET // AUTO-DISCOVERY", "Global", "127.0.0.1", 7777, 999, 0, 3000, false, true, 0.0f, 99.0f },
        { "local-dev", "LOCAL DEV CLUSTER // LOOPBACK", "Local Host", "127.0.0.1", 7777, 999, 0, 3000, false, false, 0.0f, 99.0f }
    };
    loadShardsConfig();
    fetchRemoteShardsAsync();
}

NetworkEngine::~NetworkEngine() {
    shutdown();
}

bool NetworkEngine::init() {
    if (m_initialized) return true;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[Network Error] WSAStartup failed.\n";
        return false;
    }

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "[Network Error] Failed to create UDP socket.\n";
        WSACleanup();
        return false;
    }

    // Set non-blocking mode
    u_long nonBlocking = 1;
    if (ioctlsocket(m_socket, FIONBIO, &nonBlocking) != 0) {
        std::cerr << "[Network Error] Failed to set non-blocking socket.\n";
        closesocket(m_socket);
        WSACleanup();
        return false;
    }

#ifdef _WIN32
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
    BOOL bNewBehavior = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(m_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
#endif

    m_initialized = true;
    return true;
}

void NetworkEngine::shutdown() {
    if (!m_initialized) return;

    if (m_isServer) stopServer();
    if (m_isConnected) disconnectClient();

    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    WSACleanup();
    m_initialized = false;
}

bool NetworkEngine::sendRaw(const void* data, size_t size, const sockaddr_in& dest) {
    if (m_socket == INVALID_SOCKET) return false;
    int sent = sendto(m_socket, static_cast<const char*>(data), static_cast<int>(size), 0,
                      reinterpret_cast<const sockaddr*>(&dest), sizeof(dest));
    return sent > 0;
}

// ================= DEDICATED SERVER IMPLEMENTATION =================

bool NetworkEngine::startServer(uint16_t port) {
    if (!init()) return false;

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(m_socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "[Network Server] Failed to bind dedicated server to UDP port " << port << ".\n";
        return false;
    }

    m_isServer = true;
    m_isConnected = false;
    m_serverPort = port;
    m_nextPlayerId = 1;
    m_clients.clear();
    std::cout << "[Mercenary MMO Server] Dedicated Persistent Shard active on UDP port " << port << "\n";
    return true;
}

void NetworkEngine::stopServer() {
    m_isServer = false;
    m_clients.clear();
    m_remotePlayers.clear();
}

void NetworkEngine::processServerPackets() {
    char buffer[2048];
    sockaddr_in clientAddr{};
    int clientAddrLen = sizeof(clientAddr);

    while (true) {
        int bytesReceived = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                     reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (bytesReceived <= 0) break; // No more pending packets

        if (bytesReceived < (int)sizeof(PacketHeader)) continue;

        const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(buffer);
        if (hdr->magic != 0x5647) continue;

        if (hdr->type == PacketType::Heartbeat) {
            // Heartbeat / ping probe from client browser: echo back with current online count
            PacketHeader replyHdr;
            replyHdr.magic = 0x5647;
            replyHdr.type = PacketType::Heartbeat;
            replyHdr.sequenceNumber = static_cast<uint32_t>(m_clients.size());
            replyHdr.timestamp = hdr->timestamp;
            sendRaw(&replyHdr, sizeof(replyHdr), clientAddr);
        } else if (hdr->type == PacketType::ConnectRequest) {
            uint32_t newId = m_nextPlayerId++;
            ConnectedClient client;
            client.id = newId;
            client.address = clientAddr;
            client.lastHeartbeat = 0.0f;
            m_clients[newId] = client;
            m_spatialGrid.updateEntityPosition(newId, glm::vec3(0.0f, 0.0f, -220.0f));

            // Send ConnectAccept with assigned Player ID
            PacketHeader replyHdr;
            replyHdr.magic = 0x5647;
            replyHdr.type = PacketType::ConnectAccept;
            replyHdr.sequenceNumber = newId;
            replyHdr.timestamp = hdr->timestamp;
            sendRaw(&replyHdr, sizeof(replyHdr), clientAddr);

            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));
            std::cout << "[Mercenary Server] Operative #" << newId << " connected from " << clientIp 
                      << ":" << ntohs(clientAddr.sin_port) << " (Total Online: " << m_clients.size() << ")\n" << std::flush;
        } else if (hdr->type == PacketType::EntityState) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(PlayerNetworkState))) {
                const PlayerNetworkState* state = reinterpret_cast<const PlayerNetworkState*>(buffer + sizeof(PacketHeader));
                auto it = m_clients.find(state->playerId);
                if (it != m_clients.end()) {
                    it->second.state = *state;
                    it->second.lastHeartbeat = 0.0f;

                    // Update Spatial Interest Grid chunk for MMO spatial routing
                    m_spatialGrid.updateEntityPosition(state->playerId, state->position);

                    // Forward state: if <= 64 clients broadcast directly; if > 64 use 9-chunk AOI grid
                    if (m_clients.size() <= 64) {
                        for (auto& [cId, cClient] : m_clients) {
                            if (cId != state->playerId) {
                                sendRaw(buffer, bytesReceived, cClient.address);
                            }
                        }
                    } else {
                        auto relevantPlayerIds = m_spatialGrid.getRelevancyList(state->position);
                        for (uint32_t recipientId : relevantPlayerIds) {
                            if (recipientId != state->playerId) {
                                auto rIt = m_clients.find(recipientId);
                                if (rIt != m_clients.end()) {
                                    sendRaw(buffer, bytesReceived, rIt->second.address);
                                }
                            }
                        }
                    }
                }
            }
        } else if (hdr->type == PacketType::FireWeapon) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(BulletFireEvent))) {
                const BulletFireEvent* fireEvt = reinterpret_cast<const BulletFireEvent*>(buffer + sizeof(PacketHeader));
                if (m_clients.size() <= 64) {
                    for (auto& [cId, cClient] : m_clients) {
                        if (cId != fireEvt->shooterId) {
                            sendRaw(buffer, bytesReceived, cClient.address);
                        }
                    }
                } else {
                    auto relevantPlayerIds = m_spatialGrid.getRelevancyList(fireEvt->origin);
                    for (uint32_t recipientId : relevantPlayerIds) {
                        if (recipientId != fireEvt->shooterId) {
                            auto rIt = m_clients.find(recipientId);
                            if (rIt != m_clients.end()) {
                                sendRaw(buffer, bytesReceived, rIt->second.address);
                            }
                        }
                    }
                }
            }
                } else if (hdr->type == PacketType::HitConfirm) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(HitConfirmEvent))) {
                const HitConfirmEvent* hitEvt = reinterpret_cast<const HitConfirmEvent*>(buffer + sizeof(PacketHeader));
                auto victimIt = m_clients.find(hitEvt->victimId);
                if (victimIt != m_clients.end()) {
                    victimIt->second.state.health = std::max(0.0f, victimIt->second.state.health - hitEvt->damage);
                }
                for (auto& [cId, cClient] : m_clients) {
                    sendRaw(buffer, bytesReceived, cClient.address);
                }
            }
        } else if (hdr->type == PacketType::ChatMessage) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(ChatMessagePacket))) {
                const ChatMessagePacket* chatPkt = reinterpret_cast<const ChatMessagePacket*>(buffer + sizeof(PacketHeader));
                if (chatPkt->channel == 0) { // Proximity
                    for (auto& [cId, cClient] : m_clients) {
                        float dist = glm::distance(cClient.state.position, chatPkt->position);
                        if (dist <= 35.0f) {
                            sendRaw(buffer, bytesReceived, cClient.address);
                        }
                    }
                } else if (chatPkt->channel == 1) { // Team
                    for (auto& [cId, cClient] : m_clients) {
                        if (cClient.state.faction == chatPkt->faction) {
                            sendRaw(buffer, bytesReceived, cClient.address);
                        }
                    }
                } else { // Global
                    for (auto& [cId, cClient] : m_clients) {
                        sendRaw(buffer, bytesReceived, cClient.address);
                    }
                }
                const char* chName = (chatPkt->channel == 0) ? "PROX" : ((chatPkt->channel == 1) ? "TEAM" : "GLOBAL");
                std::cout << "[Shard Chat // " << chName << "] " << chatPkt->senderName << ": " << chatPkt->message << "\n" << std::flush;
            }
        } else if (hdr->type == PacketType::VoiceData) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(VoiceDataPacket))) {
                const VoiceDataPacket* voicePkt = reinterpret_cast<const VoiceDataPacket*>(buffer + sizeof(PacketHeader));
                if (voicePkt->channel == 0) { // Proximity Voice
                    for (auto& [cId, cClient] : m_clients) {
                        if (cId != voicePkt->speakerId) {
                            float dist = glm::distance(cClient.state.position, voicePkt->speakerPos);
                            if (dist <= 40.0f) {
                                sendRaw(buffer, bytesReceived, cClient.address);
                            }
                        }
                    }
                } else { // Squad Radio
                    uint8_t senderFaction = 0;
                    auto sIt = m_clients.find(voicePkt->speakerId);
                    if (sIt != m_clients.end()) senderFaction = sIt->second.state.faction;
                    for (auto& [cId, cClient] : m_clients) {
                        if (cId != voicePkt->speakerId && cClient.state.faction == senderFaction) {
                            sendRaw(buffer, bytesReceived, cClient.address);
                        }
                    }
                }
            }
        } else if (hdr->type == PacketType::Disconnect) {
            for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
                if (it->second.address.sin_addr.s_addr == clientAddr.sin_addr.s_addr &&
                    it->second.address.sin_port == clientAddr.sin_port) {
                    uint32_t departedId = it->first;
                    m_spatialGrid.removeEntity(departedId);
                    std::cout << "[Mercenary Server] Operative #" << departedId << " disconnected (Total Online: " 
                              << (m_clients.size() - 1) << ")\n" << std::flush;
                    m_clients.erase(it);

                    PacketHeader discHdr;
                    discHdr.magic = 0x5647;
                    discHdr.type = PacketType::Disconnect;
                    discHdr.sequenceNumber = departedId;
                    discHdr.timestamp = 0.0f;
                    for (const auto& [otherId, otherClient] : m_clients) {
                        sendRaw(&discHdr, sizeof(discHdr), otherClient.address);
                    }
                    break;
                }
            }
        }
    }
}

void NetworkEngine::updateServer(float dt) {
    if (!m_isServer) return;
    processServerPackets();

    std::vector<uint32_t> timedOut;
    for (auto& [id, client] : m_clients) {
        client.lastHeartbeat += dt;
        if (client.lastHeartbeat >= 8.0f) {
            timedOut.push_back(id);
        }
    }
    for (uint32_t id : timedOut) {
        m_spatialGrid.removeEntity(id);
        std::cout << "[Mercenary Server] Operative #" << id << " timed out (No heartbeat for 8s)\n" << std::flush;
        m_clients.erase(id);

        PacketHeader discHdr;
        discHdr.magic = 0x5647;
        discHdr.type = PacketType::Disconnect;
        discHdr.sequenceNumber = id;
        discHdr.timestamp = 0.0f;
        for (const auto& [otherId, otherClient] : m_clients) {
            sendRaw(&discHdr, sizeof(discHdr), otherClient.address);
        }
    }
}

// ================= CLIENT MATCHMAKING IMPLEMENTATION =================

bool NetworkEngine::connectToServer(const std::string& host, uint16_t port) {
    if (!init()) return false;

    resolveHostToSockaddr(host, port, m_serverAddr);

    m_localPlayerId = 0;
    m_connectRetryTimer = 0.0f;
    m_isConnected = true;
    m_isServer = false;
    m_remotePlayers.clear();

    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::ConnectRequest;
    hdr.sequenceNumber = 0;
    hdr.timestamp = 0.0f;
    sendRaw(&hdr, sizeof(hdr), m_serverAddr);

    std::cout << "[Mercenary Client] Initiating handshake to Shard Server at " << host << ":" << port << "...\n" << std::flush;
    return true;
}

void NetworkEngine::disconnectClient() {
    if (m_isConnected && !m_isServer) {
        PacketHeader hdr;
        hdr.magic = 0x5647;
        hdr.type = PacketType::Disconnect;
        sendRaw(&hdr, sizeof(hdr), m_serverAddr);
    }
    m_isConnected = false;
    m_localPlayerId = 0;
    m_currentSessionId = 0;
    m_isMatchmaking = false;
    m_pendingMatchmake = false;
    m_remotePlayers.clear();
}

void NetworkEngine::joinSession(uint32_t sessionId, uint8_t faction) {
    if (!m_isConnected || m_localPlayerId == 0) return;
    m_desiredSessionId = sessionId;

    char sendBuf[sizeof(PacketHeader) + sizeof(JoinSessionRequestPacket)];
    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::JoinSessionRequest;
    hdr.sequenceNumber = sessionId;
    hdr.timestamp = 0.0f;

    JoinSessionRequestPacket req;
    req.playerId = m_localPlayerId;
    req.sessionId = sessionId;
    req.faction = faction;

    std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), &req, sizeof(JoinSessionRequestPacket));
    sendRaw(sendBuf, sizeof(sendBuf), m_serverAddr);
    std::cout << "[Mercenary Client] Requesting to join Session #" << sessionId << "...\n" << std::flush;
}

void NetworkEngine::setServerHost(const std::string& host) {
    std::string cleanHost = host;
    while (!cleanHost.empty() && isspace((unsigned char)cleanHost.back())) cleanHost.pop_back();
    while (!cleanHost.empty() && isspace((unsigned char)cleanHost.front())) cleanHost.erase(0, 1);
    if (cleanHost.empty() || cleanHost == "auto") cleanHost = "127.0.0.1";

    int foundIdx = -1;
    for (size_t i = 0; i < m_shards.size(); ++i) {
        if (m_shards[i].host == cleanHost) {
            foundIdx = static_cast<int>(i);
            break;
        }
    }

    if (foundIdx >= 0) {
        m_selectedShardIndex = foundIdx;
    } else {
        if (m_shards.empty()) {
            FrontlineShard s;
            s.id = "custom";
            s.name = "CUSTOM DEDICATED SHARD";
            s.region = "Custom Host";
            s.host = cleanHost;
            s.port = 7777;
            m_shards.push_back(s);
            m_selectedShardIndex = 0;
        } else {
            m_shards[0].host = cleanHost;
            m_selectedShardIndex = 0;
        }
    }

    for (size_t i = 0; i < m_shards.size(); ++i) {
        m_shards[i].lastProbeTimer = 99.0f; // Force immediate probe
        m_shards[i].lastResponseTimer = 0.0f;
    }
}


void NetworkEngine::parseShardsJson(const std::string& jsonStr) {
    try {
        auto j = nlohmann::json::parse(jsonStr);
        if (!j.is_array()) return;

        std::vector<FrontlineShard> parsed;
        for (const auto& item : j) {
            FrontlineShard s;
            s.id = item.value("id", "shard");
            s.name = item.value("name", "FRONTLINE SHARD");
            s.region = item.value("region", "Global");
            s.host = item.value("host", "127.0.0.1");
            s.port = static_cast<uint16_t>(item.value("port", 7777));
            if (item.contains("max_capacity")) s.maxCapacity = item.value("max_capacity", 3000);
            else s.maxCapacity = item.value("maxCapacity", 3000);
            if (item.contains("is_official")) s.isOfficial = item.value("is_official", true);
            else s.isOfficial = item.value("isOfficial", true);
            s.pingMs = 999;
            s.onlineCount = item.value("active_players", 0);
            s.isOnline = false;
            s.lastProbeTimer = 99.0f;
            s.lastResponseTimer = 0.0f;

            for (const auto& existing : m_shards) {
                if (existing.host == s.host && existing.port == s.port) {
                    s.pingMs = existing.pingMs;
                    s.onlineCount = existing.onlineCount;
                    s.isOnline = existing.isOnline;
                    s.lastProbeTimer = existing.lastProbeTimer;
                    s.lastResponseTimer = existing.lastResponseTimer;
                    break;
                }
            }
            parsed.push_back(s);
        }

        if (!parsed.empty()) {
            bool hasLocal = false;
            for (const auto& p : parsed) {
                if (p.host == "127.0.0.1") { hasLocal = true; break; }
            }
            if (!hasLocal) {
                FrontlineShard localShard;
                localShard.id = "local-dev";
                localShard.name = "LOCAL DEV CLUSTER // LOOPBACK";
                localShard.region = "Local Host";
                localShard.host = "127.0.0.1";
                localShard.port = 7777;
                localShard.isOfficial = false;
                localShard.lastProbeTimer = 99.0f;
                parsed.push_back(localShard);
            }
            m_shards = std::move(parsed);
            std::cout << "[Network Shards] Loaded " << m_shards.size() << " cluster nodes into active routing table.\n" << std::flush;
        }
    } catch (...) {}
}

void NetworkEngine::loadShardsConfig() {
    std::string path = "assets/cluster_shards.json";
    if (std::filesystem::exists(path)) {
        std::ifstream in(path);
        if (in.is_open()) {
            std::stringstream buffer;
            buffer << in.rdbuf();
            parseShardsJson(buffer.str());
        }
    }
    if (m_shards.empty()) {
        FrontlineShard localShard;
        localShard.id = "local-dev";
        localShard.name = "PRIMARY BATTLEFIELD CLUSTER // DEDICATED";
        localShard.region = "Local Host";
        localShard.host = "127.0.0.1";
        localShard.port = 7777;
        localShard.isOfficial = true;
        localShard.lastProbeTimer = 99.0f;
        m_shards.push_back(localShard);
    }
}

void NetworkEngine::fetchRemoteShardsAsync() {
#ifdef _WIN32
    if (m_isFetchingShards.exchange(true)) return;

    std::thread([this]() {
        struct FetchGuard {
            std::atomic<bool>& flag;
            ~FetchGuard() { flag = false; }
        } guard{m_isFetchingShards};

        // Tier 1: Cloud Master Discovery Coordinator (Supabase REST)
        std::string cloudJson = CloudDiscoveryCoordinator::fetchActiveShardsJson();
        if (!cloudJson.empty() && cloudJson.front() == '[') {
            std::cout << "[Fleet Discovery] Successfully resolved cluster shards from Cloud Registry!\n" << std::flush;
            parseShardsJson(cloudJson);
            try {
                std::ofstream out("assets/cluster_shards.json");
                if (out.is_open()) out << cloudJson;
            } catch (...) {}
            return;
        }

        // Tier 2: Dynamic Fallback via GitHub Raw Clustered Config
        HINTERNET hInternet = InternetOpenA("MercenaryEngine/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (hInternet) {
            const char* url = "https://raw.githubusercontent.com/chaoseyexx/mygame/main/assets/cluster_shards.json";
            DWORD netFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS;
            HINTERNET hUrl = InternetOpenUrlA(hInternet, url, NULL, 0, netFlags, 0);
            if (hUrl) {
                std::string gitJson;
                char buffer[4096];
                DWORD bytesRead = 0;
                while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                    gitJson.append(buffer, bytesRead);
                }
                InternetCloseHandle(hUrl);
                if (!gitJson.empty() && gitJson.front() == '[') {
                    std::cout << "[Fleet Discovery] Resolved cluster shards via Tier-2 GitHub Raw fallback.\n" << std::flush;
                    parseShardsJson(gitJson);
                    try {
                        std::ofstream out("assets/cluster_shards.json");
                        if (out.is_open()) out << gitJson;
                    } catch (...) {}
                }
            }
            InternetCloseHandle(hInternet);
        }
    }).detach();
#endif
}

void NetworkEngine::probeShards(float dt) {
    if (m_authPending) {
        m_authTimer += dt;
        if (m_authTimer >= 6.0f) {
            m_authPending = false;
            m_authError = "CONNECTION TIMED OUT: MASTER SERVER UNREACHABLE";
        }
    }
    if (m_registerPending) {
        m_registerTimer += dt;
        if (m_registerTimer >= 6.0f) {
            m_registerPending = false;
            m_registerStatus = "CONNECTION TIMED OUT: MASTER SERVER UNREACHABLE";
        }
    }
    if (m_isServer) return;
    init();

    static auto startTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float currentSec = std::chrono::duration<float>(now - startTime).count();

    for (size_t i = 0; i < m_shards.size(); ++i) {
        auto& shard = m_shards[i];
        shard.lastProbeTimer += dt;
        shard.lastResponseTimer += dt;

        if (shard.lastResponseTimer > 2.2f) {
            shard.isOnline = false;
            shard.pingMs = 999;
        }

        if (shard.lastProbeTimer >= 1.2f) {
            shard.lastProbeTimer = 0.0f;

            sockaddr_in dest{};
            resolveHostToSockaddr(shard.host, shard.port, dest);

            PacketHeader hdr;
            hdr.magic = 0x5647;
            hdr.type = PacketType::Heartbeat;
            hdr.sequenceNumber = static_cast<uint32_t>(i);
            hdr.timestamp = currentSec;

            sendRaw(&hdr, sizeof(hdr), dest);
        }
    }

    // Process all incoming network packets (heartbeats, handshake accept, world entity updates)
    processClientPackets();

    if (m_isConnected && m_localPlayerId == 0) {
        // Pending handshake: retry sending ConnectRequest every 0.35s
        m_connectRetryTimer += dt;
        if (m_connectRetryTimer >= 0.35f) {
            m_connectRetryTimer = 0.0f;
            PacketHeader hdr;
            hdr.magic = 0x5647;
            hdr.type = PacketType::ConnectRequest;
            hdr.sequenceNumber = 0;
            hdr.timestamp = 0.0f;
            sendRaw(&hdr, sizeof(hdr), m_serverAddr);
        }
    }
}

void NetworkEngine::processClientPackets() {
    char buffer[2048];
    sockaddr_in senderAddr{};
    int senderAddrLen = sizeof(senderAddr);

    static auto startTime = std::chrono::steady_clock::now();

    while (true) {
        int bytesReceived = recvfrom(m_socket, buffer, sizeof(buffer), 0,
                                     reinterpret_cast<sockaddr*>(&senderAddr), &senderAddrLen);
        if (bytesReceived <= 0) {
            int err = WSAGetLastError();
            if (err == WSAECONNRESET) continue; // Skip ICMP port unreachable
            break;
        }

        if (bytesReceived < (int)sizeof(PacketHeader)) continue;

        const PacketHeader* hdr = reinterpret_cast<const PacketHeader*>(buffer);
        if (hdr->magic != 0x5647) continue;

        if (hdr->type == PacketType::Heartbeat) {
            auto now = std::chrono::steady_clock::now();
            float currentSec = std::chrono::duration<float>(now - startTime).count();
            float rtt = std::max(0.001f, currentSec - hdr->timestamp);
            int ping = static_cast<int>(rtt * 1000.0f);

            uint16_t senderPort = ntohs(senderAddr.sin_port);
            char senderIpStr[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &senderAddr.sin_addr, senderIpStr, sizeof(senderIpStr));

            for (size_t s = 0; s < m_shards.size(); ++s) {
                bool ipMatch = (m_shards[s].host == senderIpStr) ||
                               (m_shards[s].host == "127.0.0.1" && std::string(senderIpStr) == "127.0.0.1") ||
                               (m_shards[s].host == "localhost" && std::string(senderIpStr) == "127.0.0.1");

                if (m_shards[s].port == senderPort && ipMatch) {
                    m_shards[s].isOnline = true;
                    m_shards[s].pingMs = std::max(1, ping);
                    m_shards[s].onlineCount = static_cast<int>(hdr->sequenceNumber);
                    m_shards[s].lastResponseTimer = 0.0f;

                    // If current selection is offline, auto-switch to lowest ping online official shard
                    if (!m_shards[m_selectedShardIndex].isOnline && m_shards[s].isOfficial && ping < 900) {
                        m_selectedShardIndex = static_cast<int>(s);
                    }
                    break;
                }
            }
        } else if (hdr->type == PacketType::ConnectAccept) {
            m_localPlayerId = hdr->sequenceNumber;
            std::cout << "[Mercenary MMO Client] Handshake confirmed! Assigned Player ID #" << m_localPlayerId << "\n" << std::flush;
            if (m_pendingMatchmake) {
                m_pendingMatchmake = false;
                requestMatchmaking(m_pendingMode, m_pendingMap, m_pendingFaction);
            } else if (m_desiredSessionId > 0) {
                joinSession(m_desiredSessionId);
            }
        } else if (hdr->type == PacketType::JoinSessionResponse) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(JoinSessionResponsePacket))) {
                const auto* resp = reinterpret_cast<const JoinSessionResponsePacket*>(buffer + sizeof(PacketHeader));
                if (resp->success) {
                    m_currentSessionId = resp->sessionId;
                    m_currentMapName = resp->mapName;
                    m_spawnPosition = resp->spawnPosition;
                    std::cout << "[Mercenary MMO Client] Successfully joined Session #" << m_currentSessionId << " (" << resp->sessionName << ") on Map: " << resp->mapName << "\n" << std::flush;
                }
            }
        } else if (hdr->type == PacketType::AuthResponse) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(AuthResponsePacket))) {
                const auto* resp = reinterpret_cast<const AuthResponsePacket*>(buffer + sizeof(PacketHeader));
                m_authPending = false;
                if (resp->success == 1) {
                    m_isAuthenticated = true;
                    m_authError.clear();
                    m_authProfile = *resp;
                    ProfileManager::get().applyCloudProfile(*resp);
                    std::cout << "[Mercenary Auth] Cloud profile synchronized: " << resp->callsign << " (Account #" << resp->accountId << ")\n" << std::flush;
                } else {
                    m_isAuthenticated = false;
                    m_authError = resp->errorMessage;
                    std::cout << "[Mercenary Auth] Authentication rejected: " << resp->errorMessage << "\n" << std::flush;
                }
            }
        } else if (hdr->type == PacketType::RegisterResponse) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(RegisterResponsePacket))) {
                const auto* resp = reinterpret_cast<const RegisterResponsePacket*>(buffer + sizeof(PacketHeader));
                m_registerPending = false;
                if (resp->success == 1) {
                    m_registerSuccess = true;
                    m_registerStatus = "ENLISTMENT SUCCESSFUL! YOU MAY NOW LOGIN.";
                    std::cout << "[Mercenary Auth] Enlistment confirmed: Account #" << resp->accountId << "\n" << std::flush;
                } else {
                    m_registerSuccess = false;
                    m_registerStatus = resp->errorMessage;
                    std::cout << "[Mercenary Auth] Enlistment rejected: " << resp->errorMessage << "\n" << std::flush;
                }
            }
        } else if (hdr->type == PacketType::MatchmakeResponse) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(MatchmakeResponsePacket))) {
                const auto* resp = reinterpret_cast<const MatchmakeResponsePacket*>(buffer + sizeof(PacketHeader));
                if (resp->success) {
                    m_isMatchmaking = false;
                    std::cout << "[Mercenary MMO Client] Matchmaker confirmed! Auto-routing to Session #"
                              << resp->sessionId << " (" << resp->sessionName << ") on " << resp->mapName << "\n" << std::flush;
                    joinSession(resp->sessionId, m_pendingFaction);
                }
            } else if (hdr->type == PacketType::MatchStateSync) {
                if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(MatchStateSyncPacket))) {
                    const auto* sync = reinterpret_cast<const MatchStateSyncPacket*>(buffer + sizeof(PacketHeader));
                    m_latestMatchState = *sync;
                    m_hasMatchState = true;
                }
            }
        } else if (hdr->type == PacketType::EntityState) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(PlayerNetworkState))) {
                const PlayerNetworkState* state = reinterpret_cast<const PlayerNetworkState*>(buffer + sizeof(PacketHeader));
                if (state->playerId != m_localPlayerId) {
                    auto& remote = m_remotePlayers[state->playerId];
                    if (!remote.active) {
                        remote.currentState = *state;
                        remote.targetState = *state;
                        remote.rig = std::make_shared<CharacterRig>();
                        remote.rig->snapYaw(state->yaw);
                    } else {
                        remote.currentState = remote.targetState;
                        remote.targetState = *state;
                    }
                    if (!remote.rig) {
                        remote.rig = std::make_shared<CharacterRig>();
                        remote.rig->snapYaw(state->yaw);
                    }
                    remote.interpolationFactor = 0.0f;
                    remote.lastPacketTime = 0.0f;
                    remote.active = true;
                }
            }
        } else if (hdr->type == PacketType::FireWeapon) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(BulletFireEvent))) {
                const BulletFireEvent* fireEvt = reinterpret_cast<const BulletFireEvent*>(buffer + sizeof(PacketHeader));
                if (fireEvt->shooterId != m_localPlayerId) {
                    m_pendingBullets.push_back(*fireEvt);
                }
            }
        } else if (hdr->type == PacketType::HitConfirm) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(HitConfirmEvent))) {
                const HitConfirmEvent* hitEvt = reinterpret_cast<const HitConfirmEvent*>(buffer + sizeof(PacketHeader));
                m_pendingHits.push_back(*hitEvt);
            }
        } else if (hdr->type == PacketType::ChatMessage) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(ChatMessagePacket))) {
                const ChatMessagePacket* chatPkt = reinterpret_cast<const ChatMessagePacket*>(buffer + sizeof(PacketHeader));
                ChatSystem::get().onReceiveMessagePacket(*chatPkt, m_localPlayerPos);
            }
        } else if (hdr->type == PacketType::VoiceData) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(VoiceDataPacket))) {
                const VoiceDataPacket* voicePkt = reinterpret_cast<const VoiceDataPacket*>(buffer + sizeof(PacketHeader));
                VoiceChatManager::get().onReceiveVoicePacket(*voicePkt, m_localPlayerPos, m_cameraYaw);
            }
        } else if (hdr->type == PacketType::Disconnect) {
            uint32_t departedId = hdr->sequenceNumber;
            m_remotePlayers.erase(departedId);
            VoiceChatManager::get().removeRemoteSpeaker(departedId);
            std::cout << "[Mercenary Client] Operative #" << departedId << " departed the session.\n" << std::flush;
        } else if (hdr->type == PacketType::SectorHandoffConfirm) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(SectorHandoffPacket))) {
                const auto* handoff = reinterpret_cast<const SectorHandoffPacket*>(buffer + sizeof(PacketHeader));
                std::cout << "[Client Handoff] Confirmed transition into sector: " << handoff->targetRegionId << "\n" << std::flush;
                m_currentMapName = handoff->targetRegionId;
                m_handoffPendingMapSwitch = true;
                if (handoff->targetHost[0] != '\0' && handoff->targetPort > 0) {
                    resolveHostToSockaddr(handoff->targetHost, handoff->targetPort, m_serverAddr);
                }
            }
        } else if (hdr->type == PacketType::StructureBuildConfirm) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(StructureBuildPacket))) {
                const auto* sPkt = reinterpret_cast<const StructureBuildPacket*>(buffer + sizeof(PacketHeader));
                m_pendingStructures.push_back(*sPkt);
            }
        } else if (hdr->type == PacketType::LootDropEvent) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(LootDropEventPacket))) {
                const auto* drop = reinterpret_cast<const LootDropEventPacket*>(buffer + sizeof(PacketHeader));
                m_pendingLootDrops.push_back(*drop);
            }
        } else if (hdr->type == PacketType::LootPickupEvent) {
            if (bytesReceived >= (int)(sizeof(PacketHeader) + sizeof(LootPickupEventPacket))) {
                const auto* pickup = reinterpret_cast<const LootPickupEventPacket*>(buffer + sizeof(PacketHeader));
                m_pendingLootPickups.push_back(*pickup);
            }
        }
    }
}

void NetworkEngine::updateClient(float dt, const PlayerNetworkState& localState, float cameraYaw) {
    m_localPlayerPos = localState.position;
    m_cameraYaw = cameraYaw;
    if (m_isServer || !m_isConnected) return;
    processClientPackets();

    if (m_localPlayerId == 0) {
        // Pending handshake: retry sending ConnectRequest every 0.35s
        m_connectRetryTimer += dt;
        if (m_connectRetryTimer >= 0.35f) {
            m_connectRetryTimer = 0.0f;
            PacketHeader hdr;
            hdr.magic = 0x5647;
            hdr.type = PacketType::ConnectRequest;
            hdr.sequenceNumber = 0;
            hdr.timestamp = 0.0f;
            sendRaw(&hdr, sizeof(hdr), m_serverAddr);
        }
        return;
    }

    // Send local player state to dedicated server at 30 Hz tick rate
    m_tickTimer += dt;
    if (m_tickTimer >= (1.0f / 30.0f)) {
        m_tickTimer = 0.0f;

        char packet[sizeof(PacketHeader) + sizeof(PlayerNetworkState)];
        PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet);
        hdr->magic = 0x5647;
        hdr->type = PacketType::EntityState;
        hdr->sequenceNumber = 0;
        hdr->timestamp = 0.0f;

        PlayerNetworkState* state = reinterpret_cast<PlayerNetworkState*>(packet + sizeof(PacketHeader));
        *state = localState;
        state->playerId = m_localPlayerId;

        sendRaw(packet, sizeof(packet), m_serverAddr);
    }

    // Smoothly interpolate remote players between network ticks
    std::vector<uint32_t> deadRemotes;
    for (auto& [id, remote] : m_remotePlayers) {
        if (!remote.active) continue;
        remote.lastPacketTime += dt;
        if (remote.lastPacketTime > 2.5f) {
            deadRemotes.push_back(id);
            continue;
        }
        float snapDist = glm::distance(remote.targetState.position, remote.currentState.position);
        if (snapDist > 12.0f) {
            // Immediate snap on respawn / teleport to prevent long slide across map
            remote.currentState.position = remote.targetState.position;
            remote.currentState.yaw = remote.targetState.yaw;
        } else {
            float blendAlpha = std::clamp(dt * 18.0f, 0.0f, 1.0f);
            remote.currentState.position += (remote.targetState.position - remote.currentState.position) * blendAlpha;
            
            // Shortest angular difference for seamless rotation without 360 spin artifacts
            float dyaw = remote.targetState.yaw - remote.currentState.yaw;
            while (dyaw < -3.14159265f) dyaw += 6.2831853f;
            while (dyaw > 3.14159265f)  dyaw -= 6.2831853f;
            remote.currentState.yaw += dyaw * blendAlpha;
        }
    }
    for (uint32_t id : deadRemotes) {
        m_remotePlayers.erase(id);
        VoiceChatManager::get().removeRemoteSpeaker(id);
    }
}

bool NetworkEngine::requestMatchmaking(uint8_t desiredMode, const std::string& mapName, uint8_t faction) {
    m_isMatchmaking = true;
    m_pendingFaction = faction;
    m_pendingMode = desiredMode;
    m_pendingMap = mapName;
    if (!m_isConnected || m_localPlayerId == 0) {
        // Queue matchmaking to dispatch as soon as handshake completes
        m_pendingMatchmake = true;
        std::cout << "[Matchmaker Client] Queued MatchmakeRequest for mode " << (int)desiredMode << " (awaiting handshake)...\n" << std::flush;
        return true;
    }
    m_pendingMatchmake = false;

    char sendBuf[sizeof(PacketHeader) + sizeof(MatchmakeRequestPacket)];
    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::MatchmakeRequest;
    hdr.sequenceNumber = m_localPlayerId;
    hdr.timestamp = 0.0f;

    MatchmakeRequestPacket req;
    req.playerId = m_localPlayerId;
    req.desiredMode = desiredMode;
    req.faction = faction;
    req.partySize = 1;
    std::strncpy(req.mapName, mapName.c_str(), sizeof(req.mapName) - 1);

    std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), &req, sizeof(MatchmakeRequestPacket));
    sendRaw(sendBuf, sizeof(sendBuf), m_serverAddr);
    std::cout << "[Matchmaker Client] Sent MatchmakeRequest for mode " << (int)desiredMode << " on map " << mapName << "\n" << std::flush;
    return true;
}

void NetworkEngine::broadcastBulletFire(const BulletFireEvent& event) {
    if (!m_isConnected || m_localPlayerId == 0 || m_isServer) return;

    char packet[sizeof(PacketHeader) + sizeof(BulletFireEvent)];
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet);
    hdr->magic = 0x5647;
    hdr->type = PacketType::FireWeapon;
    hdr->sequenceNumber = 0;
    hdr->timestamp = 0.0f;

    BulletFireEvent* evt = reinterpret_cast<BulletFireEvent*>(packet + sizeof(PacketHeader));
    *evt = event;
    evt->shooterId = m_localPlayerId;

    sendRaw(packet, sizeof(packet), m_serverAddr);
}


void NetworkEngine::broadcastHitConfirm(const HitConfirmEvent& event) {
    if (!m_isConnected && !m_isServer) return;

    char packet[sizeof(PacketHeader) + sizeof(HitConfirmEvent)];
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet);
    hdr->magic = 0x5647;
    hdr->type = PacketType::HitConfirm;
    hdr->sequenceNumber = event.shooterId;
    hdr->timestamp = 0.0f;

    HitConfirmEvent* evt = reinterpret_cast<HitConfirmEvent*>(packet + sizeof(PacketHeader));
    *evt = event;

    if (m_isServer) {
        for (auto& [cId, cClient] : m_clients) {
            sendRaw(packet, sizeof(packet), cClient.address);
        }
    } else if (m_isConnected) {
        sendRaw(packet, sizeof(packet), m_serverAddr);
    }
}

void NetworkEngine::broadcastChatMessage(const ChatMessagePacket& pkt) {
    char packet[sizeof(PacketHeader) + sizeof(ChatMessagePacket)];
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet);
    hdr->magic = 0x5647;
    hdr->type = PacketType::ChatMessage;
    hdr->sequenceNumber = 0;
    hdr->timestamp = 0.0f;
    std::memcpy(packet + sizeof(PacketHeader), &pkt, sizeof(ChatMessagePacket));

    if (m_isServer) {
        if (pkt.channel == 0) {
            for (auto& [cId, cClient] : m_clients) {
                if (glm::distance(cClient.state.position, pkt.position) <= 35.0f) {
                    sendRaw(packet, sizeof(packet), cClient.address);
                }
            }
        } else if (pkt.channel == 1) {
            for (auto& [cId, cClient] : m_clients) {
                if (cClient.state.faction == pkt.faction) {
                    sendRaw(packet, sizeof(packet), cClient.address);
                }
            }
        } else {
            for (auto& [cId, cClient] : m_clients) {
                sendRaw(packet, sizeof(packet), cClient.address);
            }
        }
    } else if (m_isConnected) {
        sendRaw(packet, sizeof(packet), m_serverAddr);
    }
}

void NetworkEngine::broadcastVoiceData(const VoiceDataPacket& pkt) {
    if (!m_isConnected || m_isServer) return;

    char packet[sizeof(PacketHeader) + sizeof(VoiceDataPacket)];
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet);
    hdr->magic = 0x5647;
    hdr->type = PacketType::VoiceData;
    hdr->sequenceNumber = 0;
    hdr->timestamp = 0.0f;
    std::memcpy(packet + sizeof(PacketHeader), &pkt, sizeof(VoiceDataPacket));

    sendRaw(packet, sizeof(packet), m_serverAddr);
}


void NetworkEngine::sendAuthStandalone(const std::string& username, const std::string& password) {
    init();
    m_authPending = true;
    m_authTimer = 0.0f;
    m_authError.clear();

    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::AuthRequest;
    hdr.sequenceNumber = 0;
    hdr.timestamp = 0.0f;

    AuthRequestPacket req{};
    req.authType = 0;
    strncpy_s(req.username, username.c_str(), sizeof(req.username) - 1);
    std::string passHash = CryptoUtils::sha256(password);
    strncpy_s(req.passwordHash, passHash.c_str(), sizeof(req.passwordHash) - 1);

    char outBuf[sizeof(PacketHeader) + sizeof(AuthRequestPacket)];
    memcpy(outBuf, &hdr, sizeof(PacketHeader));
    memcpy(outBuf + sizeof(PacketHeader), &req, sizeof(AuthRequestPacket));

    sockaddr_in dest{};
    resolveHostToSockaddr(getActiveShard().host, getActiveShard().port, dest);

    sendRaw(outBuf, sizeof(outBuf), dest);
    std::cout << "[Network Client] Transmitting Standalone AuthRequest for '" << username << "' to " 
              << getActiveShard().host << ":" << getActiveShard().port << "...\n" << std::flush;
}

void NetworkEngine::sendAuthSteam(const std::string& steamId, const std::string& personaName) {
    init();
    m_authPending = true;
    m_authTimer = 0.0f;
    m_authError.clear();

    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::AuthRequest;
    hdr.sequenceNumber = 0;
    hdr.timestamp = 0.0f;

    AuthRequestPacket req{};
    req.authType = 1;
    strncpy_s(req.steamId, steamId.c_str(), sizeof(req.steamId) - 1);
    strncpy_s(req.personaName, personaName.c_str(), sizeof(req.personaName) - 1);

    char outBuf[sizeof(PacketHeader) + sizeof(AuthRequestPacket)];
    memcpy(outBuf, &hdr, sizeof(PacketHeader));
    memcpy(outBuf + sizeof(PacketHeader), &req, sizeof(AuthRequestPacket));

    sockaddr_in dest{};
    resolveHostToSockaddr(getActiveShard().host, getActiveShard().port, dest);

    sendRaw(outBuf, sizeof(outBuf), dest);
    std::cout << "[Network Client] Transmitting Steam 1-Click AuthRequest (" << personaName << " / " << steamId << ") to "
              << getActiveShard().host << ":" << getActiveShard().port << "...\n" << std::flush;
}

void NetworkEngine::sendRegister(const std::string& username, const std::string& password, const std::string& callsign, uint8_t faction, uint8_t skinToneId, uint8_t faceId, uint8_t hairId, uint8_t beardId) {
    init();
    m_registerPending = true;
    m_registerSuccess = false;
    m_registerTimer = 0.0f;
    m_registerStatus = "TRANSMITTING ENLISTMENT DATA...";

    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::RegisterRequest;
    hdr.sequenceNumber = 0;
    hdr.timestamp = 0.0f;

    RegisterRequestPacket req{};
    strncpy_s(req.username, username.c_str(), sizeof(req.username) - 1);
    std::string passHash = CryptoUtils::sha256(password);
    strncpy_s(req.passwordHash, passHash.c_str(), sizeof(req.passwordHash) - 1);
    strncpy_s(req.callsign, callsign.c_str(), sizeof(req.callsign) - 1);
    req.faction = faction;
    req.skinToneId = skinToneId;
    req.faceId = faceId;
    req.hairId = hairId;
    req.beardId = beardId;

    char outBuf[sizeof(PacketHeader) + sizeof(RegisterRequestPacket)];
    memcpy(outBuf, &hdr, sizeof(PacketHeader));
    memcpy(outBuf + sizeof(PacketHeader), &req, sizeof(RegisterRequestPacket));

    sockaddr_in dest{};
    resolveHostToSockaddr(getActiveShard().host, getActiveShard().port, dest);

    sendRaw(outBuf, sizeof(outBuf), dest);
    std::cout << "[Network Client] Transmitting RegisterRequest for '" << username << "' (" << callsign << ") to "
              << getActiveShard().host << ":" << getActiveShard().port << "...\n" << std::flush;
}


void NetworkEngine::requestSectorHandoff(const std::string& targetRegion, const glm::vec3& globalPos) {
    if (!m_isConnected) return;

    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::SectorHandoffRequest;
    hdr.sequenceNumber = m_localPlayerId;
    hdr.timestamp = 0.0f;

    SectorHandoffPacket pkt;
    pkt.playerId = m_localPlayerId;
    strncpy_s(pkt.targetRegionId, targetRegion.c_str(), sizeof(pkt.targetRegionId) - 1);
    pkt.globalPos = globalPos;

    char sendBuf[sizeof(PacketHeader) + sizeof(SectorHandoffPacket)];
    std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), &pkt, sizeof(SectorHandoffPacket));

    sendRaw(sendBuf, sizeof(sendBuf), m_serverAddr);
    std::cout << "[Continental Client] Requesting seamless handoff to sector: " << targetRegion << "\n" << std::flush;
}

void NetworkEngine::buildStructure(uint8_t structureType, uint8_t faction, const std::string& regionId, const glm::vec3& pos, float yaw) {
    if (!m_isConnected) return;

    PacketHeader hdr;
    hdr.magic = 0x5647;
    hdr.type = PacketType::StructureBuildRequest;
    hdr.sequenceNumber = m_localPlayerId;
    hdr.timestamp = 0.0f;

    StructureBuildPacket pkt;
    pkt.builderId = m_localPlayerId;
    pkt.structureType = structureType;
    pkt.faction = faction;
    strncpy_s(pkt.regionId, regionId.c_str(), sizeof(pkt.regionId) - 1);
    pkt.position = pos;
    pkt.rotYaw = yaw;
    pkt.health = 1000.0f;

    char sendBuf[sizeof(PacketHeader) + sizeof(StructureBuildPacket)];
    std::memcpy(sendBuf, &hdr, sizeof(PacketHeader));
    std::memcpy(sendBuf + sizeof(PacketHeader), &pkt, sizeof(StructureBuildPacket));

    sendRaw(sendBuf, sizeof(sendBuf), m_serverAddr);
    std::cout << "[Continental Client] Deploying persistent structure (Type " << (int)structureType 
              << ") at (" << pos.x << ", " << pos.z << ")\n" << std::flush;
}



void NetworkEngine::broadcastLootDrop(const std::string& itemName, uint8_t category, int quantity, int ammoCount, const glm::vec3& pos, float yaw) {
    if (!m_isConnected || m_localPlayerId == 0 || m_isServer) return;

    char packet[sizeof(PacketHeader) + sizeof(LootDropEventPacket)];
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet);
    hdr->magic = 0x5647;
    hdr->type = PacketType::LootDropEvent;
    hdr->sequenceNumber = 0;
    hdr->timestamp = 0.0f;

    LootDropEventPacket* evt = reinterpret_cast<LootDropEventPacket*>(packet + sizeof(PacketHeader));
    evt->lootId = 0;
    strncpy_s(evt->itemName, itemName.c_str(), sizeof(evt->itemName) - 1);
    evt->category = category;
    evt->quantity = quantity;
    evt->ammoCount = ammoCount;
    evt->position = pos;
    evt->yaw = yaw;
    evt->droppedByPlayerId = m_localPlayerId;

    sendRaw(packet, sizeof(packet), m_serverAddr);
}

void NetworkEngine::broadcastLootPickup(uint32_t lootId) {
    if (!m_isConnected || m_localPlayerId == 0 || m_isServer) return;

    char packet[sizeof(PacketHeader) + sizeof(LootPickupEventPacket)];
    PacketHeader* hdr = reinterpret_cast<PacketHeader*>(packet);
    hdr->magic = 0x5647;
    hdr->type = PacketType::LootPickupEvent;
    hdr->sequenceNumber = lootId;
    hdr->timestamp = 0.0f;

    LootPickupEventPacket* evt = reinterpret_cast<LootPickupEventPacket*>(packet + sizeof(PacketHeader));
    evt->lootId = lootId;
    evt->pickerPlayerId = m_localPlayerId;

    sendRaw(packet, sizeof(packet), m_serverAddr);
}

} // namespace Mercenary

#pragma once
#include "engine/NetworkTypes.hpp"
#include "engine/NetworkDiscovery.hpp"
#include "engine/SpatialGrid.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <vector>
#include <deque>
#include <string>
#include <atomic>
#include <memory>
#include <chrono>
#include <mutex>

namespace Mercenary {

struct PlayerTickSnapshot {
    uint32_t tick = 0;
    float timestamp = 0.0f;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
};

struct HubClient {
    uint32_t id = 0;
    sockaddr_in address{};
    float lastHeartbeat = 0.0f;
    uint32_t sessionId = 1001; // Default to Open World Shard
    PlayerNetworkState state{};
    std::deque<PlayerTickSnapshot> history; // 64 snapshots (~1000ms rewind buffer)
    float estimatedRtt = 0.05f;             // In seconds
};

enum class SessionLifecycle : uint8_t {
    PersistentMMO    = 0, // 24/7 Always-On frontline world (never destroyed)
    EphemeralTDM     = 1, // On-demand CQB match, commits stats and auto-destructs upon completion
    ExtractionRaid40M = 2  // On-demand raid, strict 40-minute countdown, remains alive even if empty, extracts loot & self-destructs at 40:00
};

struct SessionSummary {
    uint32_t id = 0;
    GameMode mode = GameMode::OpenWorld;
    std::string name;
    std::string mapName;
    uint16_t currentPlayers = 0;
    uint16_t maxPlayers = 64;
    SessionLifecycle lifecycle = SessionLifecycle::PersistentMMO;
    float sessionAge = 0.0f;
    int16_t coalitionTickets = 100;
    int16_t crimsonTickets = 100;
    float roundTimer = 600.0f;
    uint8_t matchStatus = 0;
    float raidTimer = 2400.0f;
    bool exfilActive = false;
};

struct ClientSummary {
    uint32_t id = 0;
    std::string ipStr;
    uint16_t port = 0;
    uint32_t sessionId = 1001;
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    uint8_t faction = 0;
    uint8_t stance = 0;
    int pingMs = 30;
    float lastHeartbeat = 0.0f;
};

struct HubSession {
    uint32_t id = 0;
    GameMode mode = GameMode::OpenWorld;
    std::string name;
    std::string mapName;
    uint16_t maxPlayers = 64;
    std::vector<uint32_t> playerIds;

    // Open World MMO Spatial Grid
    SpatialInterestGrid spatialGrid{100.0f};

    SessionLifecycle lifecycle = SessionLifecycle::PersistentMMO;
    float sessionAge = 0.0f;
    float postMatchTimer = 0.0f;
    bool isMarkedForDestruction = false;

    // Team Deathmatch State
    int16_t coalitionTickets = 100;
    int16_t crimsonTickets = 100;
    float roundTimer = 600.0f;
    uint8_t matchStatus = 0; // 0=InProgress, 1=CoalitionWon, 2=CrimsonWon, 3=ExfilActive, 4=ExfilSuccess

    // Infiltration / Extraction State (Strict 40:00 Hardware Lifecycle)
    float raidTimer = 2400.0f; // 40 minutes (2400.0s)
    bool exfilActive = false;
    glm::vec3 exfilZone{0.0f, 0.0f, 150.0f};
    int objectivesRemaining = 3;
    bool hasCommittedStash = false;

    float syncTimer = 0.0f;
    float territorySaveTimer = 0.0f; // For Persistent MMO periodic saves

    // Authoritative Ground Loot in Session
    std::vector<LootDropEventPacket> groundLoot;
    uint32_t nextLootId = 1000;
};

class HubServer {
public:
    HubServer();
    ~HubServer();

    bool init(uint16_t port = 7777);
    void run();
    void stop();

    // Session Management API
    uint32_t createSession(GameMode mode, const std::string& name, const std::string& mapName, uint16_t maxPlayers);
    bool removeSession(uint32_t sessionId);
    HubSession* getSession(uint32_t sessionId);
    void listSessionsConsole() const;
    void printStatus() const;
    void broadcastSystemMessage(const std::string& msg);
    void sendSessionSystemMessage(uint32_t sessionId, const std::string& msg);
    bool kickPlayer(uint32_t playerId, const std::string& reason = "Kicked by Server Admin");

    // GUI / Monitoring Inspection API
    bool isRunning() const { return m_running.load(); }
    uint16_t getPort() const { return m_port; }
    size_t getConnectedClientCount() const { return m_clients.size(); }
    const std::unordered_map<uint32_t, HubClient>& getClients() const { return m_clients; }
    const std::unordered_map<uint32_t, HubSession>& getSessions() const { return m_sessions; }
    uint32_t getServerTick() const { return m_serverTick; }
    float getServerTime() const { return m_serverTime; }
    std::string getPublicIP() const { return m_publicIP; }
    void setShardName(const std::string& name) { m_shardName = name; }
    void setRegion(const std::string& region) { m_regionName = region; }
    std::vector<SessionSummary> getSessionsSnapshot() const;
    std::vector<ClientSummary> getClientsSnapshot() const;
    uint32_t spawnCustomTdm(const std::string& mapName);
    uint32_t spawnCustomInfiltration(const std::string& mapName);
    std::vector<std::string> getLogMessages() const;
    void appendLog(const std::string& msg);

private:
    void update(float dt);
    void processPackets();
    bool sendRaw(const void* data, size_t size, const sockaddr_in& dest);

    // Packet Handlers
    void handleHeartbeat(const sockaddr_in& from, const PacketHeader& hdr);
    void handleConnectRequest(const sockaddr_in& from, const PacketHeader& hdr);
    void handleSessionListRequest(const sockaddr_in& from, const PacketHeader& hdr);
    void handleJoinSessionRequest(const sockaddr_in& from, const char* buffer, int len);
    void handleEntityState(const sockaddr_in& from, const char* buffer, int len);
    void handleFireWeapon(const sockaddr_in& from, const char* buffer, int len);
    void handleHitConfirm(const sockaddr_in& from, const char* buffer, int len);
    void handleChatMessage(const sockaddr_in& from, const char* buffer, int len);
    void handleVoiceData(const sockaddr_in& from, const char* buffer, int len);
    void handleDisconnect(const sockaddr_in& from);
    void handleMatchmakeRequest(const sockaddr_in& from, const char* buffer, int len);
    void handleAuthRequest(const sockaddr_in& from, const char* buffer, int len);
    void handleRegisterRequest(const sockaddr_in& from, const char* buffer, int len);
    void handleSectorHandoff(const sockaddr_in& from, const char* buffer, int len);
    void handleStructureBuild(const sockaddr_in& from, const char* buffer, int len);
    void handleStructureDamage(const sockaddr_in& from, const char* buffer, int len);
    void handleLootDrop(const sockaddr_in& from, const char* buffer, int len);
    void handleLootPickup(const sockaddr_in& from, const char* buffer, int len);

    // Sub-Tick Authoritative Rewind Lag Compensation
    glm::vec3 getRewoundPosition(const HubClient& victim, float shotTimestamp) const;

    // Ephemeral Micro-Server Spawners
    uint32_t spawnEphemeralTdmSession(const std::string& mapName);
    uint32_t spawnExtractionRaidSession(const std::string& mapName);

    void consoleCommandLoop();

    SOCKET m_socket = INVALID_SOCKET;
    uint16_t m_port = 27015;
    std::atomic<bool> m_running{false};
    uint32_t m_nextPlayerId = 1;
    uint32_t m_nextSessionId = 5000;
    float m_serverTime = 0.0f;
    uint32_t m_serverTick = 0;

    std::string m_publicIP = "127.0.0.1";
    std::string m_shardId = "hex-master-node";
    std::string m_shardName = "EUROPE CENTRAL // CLUSTER MASTER";
    std::string m_regionName = "Frankfurt, DE";
    float m_heartbeatCloudTimer = 0.0f;
    std::atomic<bool> m_cloudRegistered{false};

    std::unordered_map<uint32_t, HubClient> m_clients;
    std::unordered_map<uint32_t, HubSession> m_sessions;
    std::deque<std::string> m_guiLogs;
    mutable std::mutex m_logMutex;
    mutable std::mutex m_stateMutex;

    std::chrono::steady_clock::time_point m_startTime;
};

} // namespace Mercenary

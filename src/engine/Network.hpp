#pragma once
#include "NetworkTypes.hpp"
#include "NetworkDiscovery.hpp"
#include "SpatialGrid.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <atomic>

namespace Mercenary {

class CharacterRig;

struct FrontlineShard {
    std::string id = "default";
    std::string name;
    std::string region;
    std::string host;
    uint16_t port = 7777;
    int pingMs = 999;
    int onlineCount = 0;
    int maxCapacity = 3000;
    bool isOnline = false;
    bool isOfficial = true;
    float lastProbeTimer = 0.0f;
    float lastResponseTimer = 0.0f;
};

struct RemotePlayer {
    PlayerNetworkState currentState;
    PlayerNetworkState targetState;
    float interpolationFactor = 0.0f;
    float lastPacketTime = 0.0f;
    bool active = false;
    std::shared_ptr<CharacterRig> rig;
};

class NetworkEngine {
public:
    static NetworkEngine& get();

    bool init();
    void shutdown();

    // ================= DEDICATED SERVER API =================
    bool startServer(uint16_t port = 27015);
    void updateServer(float dt);
    void stopServer();
    bool isServerRunning() const { return m_isServer; }
    uint16_t getServerPort() const { return m_serverPort; }
    size_t getConnectedClientsCount() const { return m_clients.size(); }

    struct ConnectedClient {
        uint32_t id = 0;
        sockaddr_in address;
        float lastHeartbeat = 0.0f;
        PlayerNetworkState state;
    };
    const std::unordered_map<uint32_t, ConnectedClient>& getConnectedClients() const { return m_clients; }

    // ================= CLIENT MATCHMAKING API =================
    bool connectToServer(const std::string& host = "127.0.0.1", uint16_t port = 27015);
    void updateClient(float dt, const PlayerNetworkState& localState, float cameraYaw = 0.0f);
    void disconnectClient();
    bool isClientConnected() const { return m_isConnected && m_localPlayerId > 0; }
    uint32_t getLocalPlayerId() const { return m_localPlayerId; }
    void probeShards(float dt);

    uint32_t getDesiredSessionId() const { return m_desiredSessionId; }
    void setDesiredSessionId(uint32_t sId) { m_desiredSessionId = sId; }
    uint32_t getCurrentSessionId() const { return m_currentSessionId; }
    const std::string& getCurrentMapName() const { return m_currentMapName; }
    void setCurrentMapName(const std::string& mapName) { m_currentMapName = mapName; }
    const glm::vec3& getSpawnPosition() const { return m_spawnPosition; }
    bool hasPendingHandoffSwitch() { bool b = m_handoffPendingMapSwitch; m_handoffPendingMapSwitch = false; return b; }
    std::vector<StructureBuildPacket>& getPendingStructures() { return m_pendingStructures; }
    void clearPendingStructures() { m_pendingStructures.clear(); }
    void joinSession(uint32_t sessionId, uint8_t faction = 0);
    bool requestMatchmaking(uint8_t desiredMode, const std::string& mapName, uint8_t faction = 0);
    bool isMatchmaking() const { return m_isMatchmaking; }

    const std::unordered_map<uint32_t, RemotePlayer>& getRemotePlayers() const { return m_remotePlayers; }
    std::unordered_map<uint32_t, RemotePlayer>& getRemotePlayers() { return m_remotePlayers; }

    // Persistent Frontline Shards
    std::vector<FrontlineShard>& getShards() { return m_shards; }
    const std::vector<FrontlineShard>& getShards() const { return m_shards; }
    int getSelectedShardIndex() const { return m_selectedShardIndex; }
    void setSelectedShardIndex(int idx) {
        if (idx >= 0 && idx < (int)m_shards.size()) m_selectedShardIndex = idx;
    }
    const FrontlineShard& getActiveShard() const {
        static FrontlineShard s_fallback{"local-dev", "LOCAL DEV // LOOPBACK", "Local Host", "127.0.0.1", 7777, 3000, 0, 0, false, false, 0.0f, 0.0f};
        if (m_shards.empty() || m_selectedShardIndex < 0 || m_selectedShardIndex >= (int)m_shards.size()) {
            return s_fallback;
        }
        return m_shards[m_selectedShardIndex];
    }
    std::string getServerHost() const {
        return m_shards.empty() ? "127.0.0.1" : m_shards[m_selectedShardIndex].host;
    }
    void setServerHost(const std::string& host);
    void loadShardsConfig();
    void fetchRemoteShardsAsync();
    void parseShardsJson(const std::string& jsonStr);

    // Networked Ballistics Events
    std::vector<BulletFireEvent>& getPendingBullets() { return m_pendingBullets; }
    void clearPendingBullets() { m_pendingBullets.clear(); }
    void broadcastBulletFire(const BulletFireEvent& event);
    // Networked Hit Confirmation Events
    std::vector<HitConfirmEvent>& getPendingHits() { return m_pendingHits; }
    void clearPendingHits() { m_pendingHits.clear(); }
    void broadcastHitConfirm(const HitConfirmEvent& event);

    // Networked Chat & Proximity Voice
    void broadcastChatMessage(const ChatMessagePacket& pkt);
    void broadcastVoiceData(const VoiceDataPacket& pkt);

    // Continental Sector Handoff & Persistent Construction
    void requestSectorHandoff(const std::string& targetRegion, const glm::vec3& globalPos);
    void buildStructure(uint8_t structureType, uint8_t faction, const std::string& regionId, const glm::vec3& pos, float yaw);

    // ================= AUTHORITATIVE CLOUD AUTHENTICATION =================
    void sendAuthStandalone(const std::string& username, const std::string& password);
    void sendAuthSteam(const std::string& steamId, const std::string& personaName);
    void sendRegister(const std::string& username, const std::string& password, const std::string& callsign, uint8_t faction = 0, uint8_t skinToneId = 1, uint8_t faceId = 0, uint8_t hairId = 0, uint8_t beardId = 1);

    bool isAuthenticated() const { return m_isAuthenticated; }
    void setAuthenticated(bool a) { m_isAuthenticated = a; }
    bool isAuthPending() const { return m_authPending; }
    const std::string& getAuthError() const { return m_authError; }
    void clearAuthError() { m_authError.clear(); }
    const AuthResponsePacket& getAuthProfile() const { return m_authProfile; }

    bool isRegisterPending() const { return m_registerPending; }
    bool isRegisterSuccess() const { return m_registerSuccess; }
    const std::string& getRegisterStatus() const { return m_registerStatus; }
    void clearRegisterStatus() { m_registerStatus.clear(); m_registerSuccess = false; }

private:
    NetworkEngine();
    ~NetworkEngine();

    bool m_initialized = false;
    SOCKET m_socket = INVALID_SOCKET;

    // Server state
    bool m_isServer = false;
    uint16_t m_serverPort = 27015;
    uint32_t m_nextPlayerId = 1; // Dedicated server assigns Player 1, 2, 3...
    SpatialInterestGrid m_spatialGrid{100.0f};
    std::unordered_map<uint32_t, ConnectedClient> m_clients;

    // Client state
    bool m_isConnected = false;
    uint32_t m_localPlayerId = 0;
    sockaddr_in m_serverAddr{};
    float m_tickTimer = 0.0f;
    float m_connectRetryTimer = 0.0f;
    std::unordered_map<uint32_t, RemotePlayer> m_remotePlayers;
    std::vector<FrontlineShard> m_shards;
    int m_selectedShardIndex = 0;
    std::atomic<bool> m_isFetchingShards{false};
    std::vector<BulletFireEvent> m_pendingBullets;
    std::vector<HitConfirmEvent> m_pendingHits;
    glm::vec3 m_localPlayerPos{0.0f};
    float m_cameraYaw = 0.0f;

    // Authentication State
    bool m_isAuthenticated = false;
    bool m_authPending = false;
    float m_authTimer = 0.0f;
    std::string m_authError;
    AuthResponsePacket m_authProfile{};

    bool m_registerPending = false;
    bool m_registerSuccess = false;
    float m_registerTimer = 0.0f;
    std::string m_registerStatus;
    uint32_t m_desiredSessionId = 1001; // Default to OpenWorld Warzone
    uint32_t m_currentSessionId = 0;    // 0 = In Lobby / PMC Hideout (not in an active match session)
    bool m_isMatchmaking = false;
    bool m_pendingMatchmake = false;
    uint8_t m_pendingMode = 1;
    std::string m_pendingMap = "tdm_yard";
    uint8_t m_pendingFaction = 0;
    std::string m_currentMapName;
    glm::vec3 m_spawnPosition{0.0f};
    bool m_handoffPendingMapSwitch = false;
    std::vector<StructureBuildPacket> m_pendingStructures;

    void processServerPackets();
    void processClientPackets();
    bool sendRaw(const void* data, size_t size, const sockaddr_in& dest);
};

} // namespace Mercenary

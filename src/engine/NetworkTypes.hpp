#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Mercenary {

#pragma pack(push, 1)

enum class GameMode : uint8_t {
    OpenWorld      = 0, // 3000-player persistent frontline shard (Siversky Warzone)
    TeamDeathmatch = 1, // 16v16 / 32v32 ticket-based combat (Killhouse / Industrial)
    Infiltration   = 2  // Squad covert extraction raid (FOB Vanguard)
};

enum class PacketType : uint8_t {
    ConnectRequest       = 1,
    ConnectAccept        = 2,
    Disconnect           = 3,
    Heartbeat            = 4,
    SessionListRequest   = 5,
    SessionListResponse  = 6,
    JoinSessionRequest   = 7,
    JoinSessionResponse  = 8,
    PlayerInput          = 10,
    EntityState          = 11,
    WorldSnapshot        = 12,
    MatchStateSync       = 13,
    MatchmakeRequest     = 14,
    MatchmakeResponse    = 15,
    AuthRequest          = 16,
    AuthResponse         = 17,
    RegisterRequest      = 18,
    RegisterResponse     = 19,
    FireWeapon           = 20,
    HitConfirm           = 21,
    ChatMessage          = 30,
    VoiceData            = 31,
    SectorHandoffRequest  = 40,
    SectorHandoffConfirm  = 41,
    StructureBuildRequest = 50,
    StructureBuildConfirm = 51,
    StructureDamageEvent  = 52,
    StructureSyncSnapshot = 53,
    LootDropEvent         = 60,
    LootPickupEvent       = 61,
    LootSyncSnapshot      = 62
};

struct PacketHeader {
    uint16_t magic = 0x5647; // "VG"
    PacketType type;
    uint32_t sequenceNumber = 0;
    float timestamp = 0.0f;
};

struct PlayerNetworkState {
    uint32_t playerId = 0;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    uint8_t stance = 0;       // 0: Stand, 1: Crouch, 2: Prone
    uint8_t isSprinting = 0;
    float health = 100.0f;
    float blood = 5000.0f;
    uint8_t bleedCuts = 0;
    uint8_t faction = 0;      // 0: Coalition (Blue), 1: Crimson (Red)
    uint8_t helmetId = 0;
    uint8_t vestId = 0;
    uint8_t camoId = 0;
    uint8_t weaponId = 0;     // 0: None, 1: M4A1, 2: AK74, 3: Mosin, 4: M9, 5: DShK
    uint8_t isAiming = 0;
    uint8_t isMounted = 0;
    uint8_t faceId = 0;
    uint8_t skinToneId = 1;
    uint8_t hairId = 0;
    uint8_t beardId = 1;
    uint8_t backpackId = 2;
    uint8_t gloveId = 0;
    uint8_t armbandId = 0;
    char callsign[16] = {0};
};

struct BulletFireEvent {
    uint32_t shooterId = 0;
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f};
    float speed = 850.0f;
    float damage = 35.0f;
    uint8_t weaponType = 0;   // 0: Rifle, 1: Sniper, 2: Pistol, 3: HeavyMG
    uint8_t faction = 0;
};

struct HitConfirmEvent {
    uint32_t shooterId = 0;
    uint32_t victimId = 0;
    float damage = 35.0f;
    uint8_t hitRegion = 0;   // 0: Torso, 1: Head, 2: Limbs
    uint8_t isFatal = 0;
    glm::vec3 hitPoint{0.0f};
};

struct ChunkCoordinate {
    int16_t chunkX = 0;
    int16_t chunkZ = 0;

    bool operator==(const ChunkCoordinate& other) const {
        return chunkX == other.chunkX && chunkZ == other.chunkZ;
    }
};


enum class ChatChannel : uint8_t {
    Proximity = 0, // 35m spatial radius in same world space
    Team      = 1, // Faction radio (Coalition vs Crimson army-wide across open world)
    Global    = 2, // MMO World/Global chat (cross-shard open community)
    System    = 3, // Server announcements & mission dispatch
    Zone      = 4, // Regional sector broadcast (e.g. Siversky Warzone)
    Squad     = 5, // Persistent squad comms
    Whisper   = 6, // Private 1-on-1 direct message
    Match     = 7  // Match-only all-talk for active competitive rounds
};

struct ChatMessagePacket {
    uint32_t senderId = 0;
    uint32_t targetId = 0;     // For direct Whisper / PM (0 for broadcast)
    uint8_t channel = 0;       // ChatChannel
    uint8_t faction = 0;       // 0: Coalition (Blue), 1: Crimson (Red)
    glm::vec3 position{0.0f};  // Sender 3D position
    char senderName[24] = {0}; // Callsign
    char message[128] = {0};   // Message content
};

struct VoiceDataPacket {
    uint32_t speakerId = 0;
    uint8_t channel = 0;        // 0: Proximity, 1: Squad Radio
    uint8_t flags = 0;          // bit 0: talking active, bit 1: squelch start, bit 2: squelch end
    float amplitude = 0.0f;     // RMS energy for VU meters
    glm::vec3 speakerPos{0.0f}; // 3D emitter position
    char callsign[16] = {0};
    uint16_t sampleCount = 0;   // Number of 16-bit PCM samples
    int16_t samples[256] = {0}; // 256 samples @ 16kHz
};

// ================= MULTI-SESSION HUB ARCHITECTURE =================

struct SessionInfoItem {
    uint32_t sessionId = 0;
    uint8_t mode = 0; // GameMode
    char name[32] = {0};
    char mapName[32] = {0};
    uint16_t playerCount = 0;
    uint16_t maxPlayers = 0;
    int16_t coalitionScore = 0; // TDM tickets or OpenWorld capture %
    int16_t crimsonScore = 0;
    float timeRemaining = 0.0f; // Round timer or exfil countdown
};

struct JoinSessionRequestPacket {
    uint32_t playerId = 0;
    uint32_t sessionId = 0;
    uint8_t faction = 0; // 0=Coalition, 1=Crimson
};

struct JoinSessionResponsePacket {
    uint32_t sessionId = 0;
    uint8_t mode = 0; // GameMode
    uint8_t success = 0; // 1=Joined, 0=Full/Error
    char sessionName[32] = {0};
    char mapName[32] = {0};
    glm::vec3 spawnPosition{0.0f};
};

struct MatchStateSyncPacket {
    uint32_t sessionId = 0;
    uint8_t mode = 0; // GameMode
    int16_t coalitionScore = 0; // TDM tickets
    int16_t crimsonScore = 0;
    float roundTimer = 0.0f;
    uint8_t matchStatus = 0; // 0: InProgress, 1: CoalitionVictory, 2: CrimsonVictory, 3: ExfilActive, 4: ExfilSuccess
};

struct MatchmakeRequestPacket {
    uint32_t playerId = 0;
    uint8_t desiredMode = 1; // 0: OpenWorld, 1: TDM, 2: Infiltration
    uint8_t faction = 0;     // 0: Coalition, 1: Crimson
    uint8_t partySize = 1;
    char mapName[32] = {0};
};

struct MatchmakeResponsePacket {
    uint32_t sessionId = 0;
    uint8_t mode = 0;
    uint8_t success = 1; // 1: Joined/Spawned, 0: Error
    uint16_t port = 7777;
    char sessionName[32] = {0};
    char mapName[32] = {0};
    float timeRemaining = 0.0f;
};


// ================= MASTER SERVER CLUSTER AUTHENTICATION =================

struct AuthRequestPacket {
    uint8_t authType = 0; // 0: Standalone (Username + Password), 1: Steam (SteamID + Persona)
    char username[32] = {0};
    char passwordHash[65] = {0}; // 64 hex chars + null
    char steamId[32] = {0};
    char personaName[32] = {0};
};

struct AuthResponsePacket {
    uint8_t success = 0; // 1: Verified, 0: Invalid / Not Found, 2: Account Banned
    uint32_t accountId = 0;
    char callsign[32] = {0};
    uint8_t faction = 0; // 0: Coalition (Blue), 1: Crimson (Red)
    uint8_t skinToneId = 1;
    uint8_t faceId = 0;
    uint8_t hairId = 0;
    uint8_t beardId = 1;
    uint8_t helmetId = 4;
    uint8_t vestId = 3;
    uint8_t camoId = 0;
    uint8_t backpackId = 2;
    uint8_t gloveId = 0;
    uint8_t armbandId = 0;
    int32_t cash = 5000;
    int32_t goldBars = 25;
    int32_t level = 1;
    char errorMessage[64] = {0};
};

struct RegisterRequestPacket {
    char username[32] = {0};
    char passwordHash[65] = {0};
    char callsign[32] = {0};
    uint8_t faction = 0; // 0: Coalition, 1: Crimson
    uint8_t skinToneId = 1;
    uint8_t faceId = 0;
    uint8_t hairId = 0;
    uint8_t beardId = 1;
};

struct RegisterResponsePacket {
    uint8_t success = 0; // 1: Enlisted, 0: Failed
    uint32_t accountId = 0;
    char errorMessage[64] = {0};
};

struct SectorHandoffPacket {
    uint32_t playerId = 0;
    char targetRegionId[16] = {0};
    char targetHost[32] = {0};
    uint16_t targetPort = 7777;
    uint32_t sessionToken = 0;
    glm::vec3 globalPos{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    float health = 100.0f;
};

struct StructureBuildPacket {
    uint32_t builderId = 0;
    uint8_t structureType = 1; // 1: FOB, 2: Trench, etc.
    uint8_t faction = 0;
    char regionId[16] = {0};
    glm::vec3 position{0.0f};
    float rotYaw = 0.0f;
    float health = 1000.0f;
};

struct StructureDamagePacket {
    int64_t structureId = 0;
    float damage = 0.0f;
    uint32_t attackerId = 0;
    uint8_t weaponType = 0;
};

struct LootDropEventPacket {
    uint32_t lootId = 0;
    char itemName[32] = {0};
    uint8_t category = 0; // 0: Weapon, 1: Ammo, 2: Armor, etc.
    int32_t quantity = 1;
    int32_t ammoCount = 30;
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    uint32_t droppedByPlayerId = 0;
};

struct LootPickupEventPacket {
    uint32_t lootId = 0;
    uint32_t pickerPlayerId = 0;
};

#pragma pack(pop)

} // namespace Mercenary

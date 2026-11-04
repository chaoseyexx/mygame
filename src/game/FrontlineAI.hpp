#pragma once
#include "engine/Types.hpp"
#include "engine/Renderer.hpp"
#include "CharacterProfile.hpp"
#include "Ballistics.hpp"
#include "World.hpp"
#include "BodySystem.hpp"
#include "engine/CharacterRig.hpp"
#include <vector>
#include <memory>

namespace Mercenary {

enum class BotState {
    Advancing,
    InCoverFiring,
    PerformingTriage,
    WoundedDown, // Down-But-Not-Out (DBNO): Crawls in mud calling for medic
    Dead
};

struct FrontlineBot {
    uint32_t id = 0;
    Faction faction = Faction::FactionA;
    CombatRole role = CombatRole::Rifleman;
    std::string rank = "PVT";
    std::string callsign = "Trooper";
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float yaw = 0.0f;
    float health = 80.0f;
    float maxHealth = 80.0f;
    BotState state = BotState::Advancing;
    float fireCooldown = 0.0f;
    int burstShotsRemaining = 0;
    float burstIntervalTimer = 0.0f;
    float respawnTimer = 0.0f;
    float healTimer = 0.0f;
    float rallyTimer = 0.0f;
    bool isRallied = false;
    float throttleAccumulator = 0.0f;
    glm::vec3 coverPosition{0.0f};

    // Tactical Vision, Datalink, DBNO & Suppression
    bool isVisibleToPlayer = true;
    bool isSpottedViaRadio = false;
    float suppression = 0.0f;
    bool isPinned = false;
    float dbnoTimer = 0.0f;
    float flinchPitch = 0.0f;
    float barkTimer = 0.0f;
    float footstepTimer = 0.0f;
    bool isCrouched = false;
    float grenadeCooldown = 12.0f;
    int grenadeCount = 2;

    // Dynamic Tactical Combat Maneuvers (Tarkov/CS2 Standard)
    bool isSliding = false;
    float slideTimer = 0.0f;
    glm::vec3 slideDirection{0.0f};
    bool isTacticalSprinting = false;
    float tacticalSprintTimer = 0.0f;
    float lean = 0.0f;
    uint32_t partnerBotId = 0;
    bool isOverwatchSuppressor = false;
    float overwatchCycleTimer = 0.0f;
    int smokeGrenadeCount = 1;
    float targetStationaryTimer = 0.0f;
    glm::vec3 lastTargetCoverPos{0.0f};
    float tacticalRetreatTimer = 0.0f;

    // Real Anatomical Organ & Limb System for Bots
    BodySystem body;

    // Frontline Skirmish & Notice System
    float playerNoticeTime = 0.0f;      // Accumulated exposure time noticing player
    bool hasNoticedPlayer = false;       // Only shoots at player if actively noticed/provoked!
    float coverSeekTimer = 0.0f;
    bool hasCoverPosition = false;
    uint32_t currentEnemyTargetId = 0;   // Enemy frontline bot target
    bool currentTargetIsPlayer = false;  // Actively tracking & aiming at human player
    glm::vec3 currentTargetPos{0.0f};    // Direct 3D aim point of currently acquired hostile
    int tdmLaneIndex = 0;                // 0 = West Freight, 1 = Center Killzone, 2 = East Depot
    float recentDamageTakenTimer = 0.0f; // React immediately when struck by gunfire
    float peekTimer = 0.0f;              // Peeking out from cover to shoot then ducking
    bool isPeeking = false;

    // Base Garrison & Tactical Patrols
    bool isPatrolBot = false;
    std::vector<glm::vec3> patrolWaypoints;
    int currentWaypoint = 0;
    bool isStationarySentry = false;
    std::string assignment = "Garrison Sentry";
    std::string dialogue = "All sectors green.";
    float sentryLookTimer = 0.0f;
    float baseFacingYaw = 0.0f;

    // Tactical CQB & Articulated Skeletal Animation
    bool isCqbAggressor = false;
    glm::vec3 guardAnchor{0.0f};
    std::shared_ptr<CharacterRig> rig;
};

class FrontlineWarfare {
public:
    FrontlineWarfare();

    void init(int botsPerTeam = 10);
    void initTrainingGarrison(World& world, Faction playerFaction = Faction::FactionA);
    void initTeamDeathmatch(World& world, Faction playerFaction = Faction::FactionA, bool spawnAiBots = true);
    void update(float dt, const glm::vec3& playerPos, float playerYaw, Faction playerFaction, World& world, BallisticsSystem& ballistics, CharacterProfile& profile, float cameraYawRad = 0.0f, bool playerSpawnProtected = false);
    void render(Renderer& renderer, const glm::vec3& cameraPos, const glm::vec3& playerPos = glm::vec3(0.0f), float playerYaw = 0.0f, Faction playerFaction = Faction::None, bool playerHasComms = true);

    bool isPlayerConnectedToDatalink(Faction playerFaction, const glm::vec3& playerPos) const;

    std::vector<FrontlineBot>& getBots() { return m_bots; }
    const std::vector<FrontlineBot>& getBots() const { return m_bots; }

    // 5v5 Team Deathmatch Match Telemetry & State
    bool isTdmMode() const { return m_isTdmMode; }
    bool isTdmAgainstAi() const { return m_tdmAgainstAi; }
    void setTdmAgainstAi(bool ai) { m_tdmAgainstAi = ai; }
    int getTeamAKills() const { return m_teamAKills; }
    int getTeamBKills() const { return m_teamBKills; }
    int getScoreLimit() const { return m_scoreLimit; }
    float getMatchTime() const { return m_matchTime; }
    bool isTdmMatchOver() const { return m_tdmMatchOver; }
    Faction getTdmWinner() const { return m_tdmWinner; }
    void recordTdmKill(Faction killerFaction);
    void resetTdmMatch();
    void syncServerState(int teamAScore, int teamBScore, float roundTimer, uint8_t matchStatus) {
        m_teamAKills = teamAScore;
        m_teamBKills = teamBScore;
        m_matchTime = roundTimer;
        if (matchStatus == 1) {
            m_tdmMatchOver = true;
            m_tdmWinner = Faction::FactionA;
        } else if (matchStatus == 2) {
            m_tdmMatchOver = true;
            m_tdmWinner = Faction::FactionB;
        }
    }

private:
    std::vector<FrontlineBot> m_bots;
    uint32_t m_nextId = 100;
    bool m_isTdmMode = false;
    bool m_tdmAgainstAi = true;
    int m_teamAKills = 0;
    int m_teamBKills = 0;
    int m_scoreLimit = 30;
    float m_matchTime = 600.0f; // 10 minutes match clock
    bool m_tdmMatchOver = false;
    Faction m_tdmWinner = Faction::None;
    float m_teamAAverageZ = -28.0f;
    float m_teamBAverageZ = 28.0f;
    bool m_spawnFlipped = false;
    float m_spawnFlipCheckTimer = 0.0f;

    void spawnBot(FrontlineBot& bot, Faction faction, CombatRole role);
    void spawnTdmBot(FrontlineBot& bot, Faction faction, CombatRole role);
};

} // namespace Mercenary

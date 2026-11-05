#include "FrontlineAI.hpp"
#include "engine/Audio.hpp"
#include <cmath>
#include <random>
#include <algorithm>

namespace Mercenary {

FrontlineWarfare::FrontlineWarfare() {}

static CombatRole assignRole(int index) {
    int mod = index % 10;
    if (mod == 0) return CombatRole::SquadLeader;      // 10% Squad Leader
    if (mod == 1 || mod == 2) return CombatRole::Medic; // 20% Field Medic
    if (mod == 3 || mod == 4 || mod == 5) return CombatRole::AutomaticRifleman; // 30% Gunner
    return CombatRole::Rifleman;                       // 40% Standard Rifleman
}

void FrontlineWarfare::init(int botsPerTeam) {
    m_bots.clear();
    for (int i = 0; i < botsPerTeam; ++i) {
        FrontlineBot botA;
        spawnBot(botA, Faction::FactionA, assignRole(i));
        m_bots.push_back(botA);

        FrontlineBot botB;
        spawnBot(botB, Faction::FactionB, assignRole(i));
        m_bots.push_back(botB);
    }
}

void FrontlineWarfare::spawnBot(FrontlineBot& bot, Faction faction, CombatRole role) {
    static std::mt19937 gen(444);
    std::uniform_real_distribution<float> disX(-160.0f, 160.0f);
    std::uniform_real_distribution<float> disZ(-8.0f, 8.0f);

    bot.id = m_nextId++;
    bot.faction = faction;
    bot.role = role;

    // Assign realistic tactical military rank and callsign
    if (faction == Faction::FactionA) {
        static const char* s_leadCalls[] = { "Viper-1", "Ghost", "Hunter-6", "Reaper", "Warlord", "Bishop" };
        static const char* s_gunnerCalls[] = { "Havoc", "Titan", "Grizzly", "Rhino", "Hammer", "Bolo" };
        static const char* s_medicCalls[] = { "Doc", "Patch", "Lifeline", "Mercy", "Trauma", "Cross" };
        static const char* s_rifleCalls[] = { "Specter", "Echo-2", "Raptor", "Cobra", "Hawk", "Striker", "Sabre", "Phantom", "Apex", "Wolf" };

        if (role == CombatRole::SquadLeader) {
            bot.rank = (gen() % 2 == 0) ? "SSG" : "SGT";
            bot.callsign = s_leadCalls[gen() % (sizeof(s_leadCalls) / sizeof(s_leadCalls[0]))];
        } else if (role == CombatRole::AutomaticRifleman) {
            bot.rank = "CPL";
            bot.callsign = s_gunnerCalls[gen() % (sizeof(s_gunnerCalls) / sizeof(s_gunnerCalls[0]))];
        } else if (role == CombatRole::Medic) {
            bot.rank = (gen() % 2 == 0) ? "CPL" : "PFC";
            bot.callsign = s_medicCalls[gen() % (sizeof(s_medicCalls) / sizeof(s_medicCalls[0]))];
        } else {
            bot.rank = (gen() % 3 == 0) ? "CPL" : ((gen() % 2 == 0) ? "PFC" : "PVT");
            bot.callsign = s_rifleCalls[gen() % (sizeof(s_rifleCalls) / sizeof(s_rifleCalls[0]))];
        }
    } else {
        static const char* s_leadCalls[] = { "Buran", "Voron", "Sever", "Shtorm", "Grom", "Varyag" };
        static const char* s_gunnerCalls[] = { "Medved", "Molot", "Ural", "Kaban", "Rys", "Bulat" };
        static const char* s_medicCalls[] = { "Znak", "Bint", "Klyuch", "Valet", "Apteka", "Shina" };
        static const char* s_rifleCalls[] = { "Sokol", "Berkut", "Volk", "Taiga", "Bars", "Strela", "Granit", "Korsak", "Yastreb", "Zenit" };

        if (role == CombatRole::SquadLeader) {
            bot.rank = (gen() % 2 == 0) ? "SR-SGT" : "SGT";
            bot.callsign = s_leadCalls[gen() % (sizeof(s_leadCalls) / sizeof(s_leadCalls[0]))];
        } else if (role == CombatRole::AutomaticRifleman) {
            bot.rank = "JR-SGT";
            bot.callsign = s_gunnerCalls[gen() % (sizeof(s_gunnerCalls) / sizeof(s_gunnerCalls[0]))];
        } else if (role == CombatRole::Medic) {
            bot.rank = "EFR";
            bot.callsign = s_medicCalls[gen() % (sizeof(s_medicCalls) / sizeof(s_medicCalls[0]))];
        } else {
            bot.rank = (gen() % 3 == 0) ? "JR-SGT" : ((gen() % 2 == 0) ? "EFR" : "RYAD");
            bot.callsign = s_rifleCalls[gen() % (sizeof(s_rifleCalls) / sizeof(s_rifleCalls[0]))];
        }
    }

    // Role-specific baseline durability
    switch (role) {
    case CombatRole::SquadLeader:
        bot.maxHealth = 120.0f;
        break;
    case CombatRole::AutomaticRifleman:
        bot.maxHealth = 95.0f;
        break;
    case CombatRole::Medic:
        bot.maxHealth = 85.0f;
        break;
    case CombatRole::Rifleman:
    default:
        bot.maxHealth = 80.0f;
        break;
    }
    bot.health = bot.maxHealth;

    bot.state = BotState::Advancing;
    bot.fireCooldown = 3.5f + (gen() % 100) * 0.03f;
    bot.burstShotsRemaining = 0;
    bot.burstIntervalTimer = 0.0f;
    bot.respawnTimer = 0.0f;
    bot.healTimer = 0.0f;
    bot.rallyTimer = 0.0f;
    bot.isRallied = false;
    bot.throttleAccumulator = 0.0f;

    bot.isVisibleToPlayer = true;
    bot.isSpottedViaRadio = false;
    bot.suppression = 0.0f;
    bot.isPinned = false;
    bot.dbnoTimer = 0.0f;
    bot.flinchPitch = 0.0f;
    bot.barkTimer = 2.0f + (gen() % 30) * 0.1f;

    // Real anatomical organ & armor initialization
    bot.body.init();
    if (role == CombatRole::SquadLeader) {
        bot.body.applyArmor(BodyRegion::UpperChest, 4, 65.0f);
        bot.body.applyArmor(BodyRegion::LowerChest, 4, 65.0f);
        bot.body.applyArmor(BodyRegion::Head, 4, 55.0f);
    } else if (role == CombatRole::AutomaticRifleman) {
        bot.body.applyArmor(BodyRegion::UpperChest, 4, 75.0f);
        bot.body.applyArmor(BodyRegion::LowerChest, 4, 75.0f);
        bot.body.applyArmor(BodyRegion::Head, 4, 55.0f);
    } else {
        bot.body.applyArmor(BodyRegion::UpperChest, 3, 45.0f);
        bot.body.applyArmor(BodyRegion::LowerChest, 3, 45.0f);
        bot.body.applyArmor(BodyRegion::Head, 3, 40.0f);
    }

    // Notice & Frontline cover state
    bot.hasNoticedPlayer = false;
    bot.playerNoticeTime = 0.0f;
    bot.coverSeekTimer = 0.0f;
    bot.hasCoverPosition = false;
    bot.currentEnemyTargetId = 0;
    bot.peekTimer = (gen() % 50) * 0.04f;
    bot.isPeeking = true;
    bot.rig = std::make_shared<CharacterRig>();
    bot.rig->snapYaw(bot.yaw);

    if (faction == Faction::FactionA) {
        // Team A (Coalition) starts clustered along northern front line (-36m to -18m)
        bot.position = glm::vec3(disX(gen) * 0.65f, 0.0f, -28.0f + disZ(gen) * 0.8f);
        bot.position.y = World::sampleTerrainHeight(bot.position.x, bot.position.z);
        bot.yaw = 0.0f; // Facing +Z (South toward bridge)
    } else {
        // Team B (Crimson) starts clustered along southern front line (+18m to +36m)
        bot.position = glm::vec3(disX(gen) * 0.65f, 0.0f, 28.0f + disZ(gen) * 0.8f);
        bot.position.y = World::sampleTerrainHeight(bot.position.x, bot.position.z);
        bot.yaw = 3.14159265f; // Facing -Z (North toward bridge)
    }
}

bool FrontlineWarfare::isPlayerConnectedToDatalink(Faction playerFaction, const glm::vec3& playerPos) const {
    if (playerFaction == Faction::None) return false;
    for (const auto& ally : m_bots) {
        if (ally.faction == playerFaction && ally.state != BotState::Dead && ally.role == CombatRole::SquadLeader) {
            if (glm::distance(ally.position, playerPos) <= 140.0f) {
                return true;
            }
        }
    }
    return false;
}

void FrontlineWarfare::update(float dt, const glm::vec3& playerPos, float playerYaw, Faction playerFaction, World& world, BallisticsSystem& ballistics, CharacterProfile& profile, float cameraYawRad, bool playerSpawnProtected) {
    static std::mt19937 rnd(888);
    std::uniform_real_distribution<float> spreadDist(-0.06f, 0.06f);

    if (m_isTdmMode && !m_tdmMatchOver) {
        m_matchTime -= dt;
        if (m_matchTime <= 0.0f) {
            m_matchTime = 0.0f;
            m_tdmMatchOver = true;
            if (m_teamAKills > m_teamBKills) m_tdmWinner = Faction::FactionA;
            else if (m_teamBKills > m_teamAKills) m_tdmWinner = Faction::FactionB;
            else m_tdmWinner = Faction::None;
        }

        // Dynamic Base Flip Detection: prevent spawn camping
        m_spawnFlipCheckTimer += dt;
        if (m_spawnFlipCheckTimer >= 2.0f) {
            m_spawnFlipCheckTimer = 0.0f;
            float sumZA = 0.0f, sumZB = 0.0f;
            int countA = 0, countB = 0;
            for (const auto& b : m_bots) {
                if (b.state != BotState::Dead) {
                    if (b.faction == Faction::FactionA) { sumZA += b.position.z; countA++; }
                    else if (b.faction == Faction::FactionB) { sumZB += b.position.z; countB++; }
                }
            }
            if (countA > 0) m_teamAAverageZ = sumZA / countA;
            if (countB > 0) m_teamBAverageZ = sumZB / countB;

            if (!m_spawnFlipped && m_teamAAverageZ > 14.0f) {
                m_spawnFlipped = true;
            } else if (m_spawnFlipped && m_teamBAverageZ < -14.0f) {
                m_spawnFlipped = false;
            }
        }
    }

    glm::vec3 playerEye = playerPos + glm::vec3(0, 1.4f, 0);
    glm::vec3 playerLook(std::sin(playerYaw), 0.0f, std::cos(playerYaw));

    // ================= FOG OF WAR & TACTICAL RADIO DATALINK =================
    for (auto& bot : m_bots) {
        if (bot.state == BotState::Dead) continue;

        if (bot.faction == playerFaction || playerFaction == Faction::None) {
            bot.isVisibleToPlayer = true;
            bot.isSpottedViaRadio = false;
            continue;
        }

        // 1. Direct Player Eye-Level Line of Sight
        float distToPlayer = glm::distance(bot.position, playerPos);
        bool directSeen = false;
        if (distToPlayer <= 9.5f) {
            directSeen = true; // Close proximity 360 audio awareness
        } else if (distToPlayer <= 62.0f) {
            glm::vec3 toBot = glm::normalize(bot.position - playerPos);
            float dotSight = glm::dot(toBot, playerLook);
            if (dotSight > -0.22f && world.hasLineOfSight(playerEye, bot.position + glm::vec3(0, 1.1f, 0))) {
                directSeen = true;
            }
        }

        // 2. Squad Radio Datalink Net: Check if any friendly bot sees this enemy
        bool radioSpotted = false;
        for (const auto& friendly : m_bots) {
            if (friendly.state == BotState::Dead || friendly.state == BotState::WoundedDown || friendly.faction != playerFaction) continue;
            float distToEnemy = glm::distance(friendly.position, bot.position);
            if (distToEnemy <= 48.0f && world.hasLineOfSight(friendly.position + glm::vec3(0, 1.2f, 0), bot.position + glm::vec3(0, 1.2f, 0))) {
                radioSpotted = true;
                break;
            }
        }

        if (radioSpotted && !bot.isSpottedViaRadio && !directSeen) {
            // Radio datalink target ping sound!
            AudioEngine::get().playSound(SoundType::RadioBeepDatalink, 0.45f, 1.0f);
        }

        bot.isSpottedViaRadio = radioSpotted;
        bot.isVisibleToPlayer = directSeen || radioSpotted;
    }

    // 1. Squad Leader Rally Buff Aura Update
    for (auto& leader : m_bots) {
        if (leader.state == BotState::Dead || leader.role != CombatRole::SquadLeader) continue;
        leader.rallyTimer += dt;
        if (leader.rallyTimer >= 1.5f) {
            leader.rallyTimer = 0.0f;
            // Rally nearby friendly combatants within 22 meters
            for (auto& ally : m_bots) {
                if (ally.state == BotState::Dead || ally.faction != leader.faction) continue;
                if (glm::distance(leader.position, ally.position) <= 22.0f) {
                    ally.isRallied = true;
                }
            }
        }
    }

    // 2. Main Bot AI Cycle
    for (auto& bot : m_bots) {
        float botDt = dt;
        glm::vec3 prevBotPos = bot.position;
        if (bot.state == BotState::Dead) {
            bot.respawnTimer += botDt;
            if (m_isTdmMode) {
                if (bot.respawnTimer >= 4.0f) {
                    spawnTdmBot(bot, bot.faction, bot.role);
                }
                continue;
            }
            if (bot.respawnTimer >= 6.0f) {
                if (bot.isCqbAggressor) {
                    bot.state = BotState::InCoverFiring;
                    bot.health = bot.maxHealth;
                    bot.respawnTimer = 0.0f;
                    bot.position = bot.guardAnchor;
                    bot.yaw = 0.0f;
                    if (bot.rig) bot.rig->resetDeath();
                } else if (bot.isPatrolBot || bot.isStationarySentry) {
                    bot.state = BotState::Advancing;
                    bot.health = bot.maxHealth;
                    bot.respawnTimer = 0.0f;
                    if (bot.rig) bot.rig->resetDeath();
                } else {
                    spawnBot(bot, bot.faction, bot.role);
                }
            }
            continue;
        }

        // Down-But-Not-Out (DBNO) Crawling State
        if (bot.state == BotState::WoundedDown) {
            bot.dbnoTimer -= botDt;
            bot.barkTimer -= botDt;
            if (bot.barkTimer <= 0.0f) {
                bot.barkTimer = 4.2f + (rnd() % 20) * 0.1f;
                AudioEngine::get().playSound3D(SoundType::SoldierBarkWounded, bot.position, playerPos, cameraYawRad, 0.65f, 40.0f);
            }
            // Crawl slowly toward friendly territory
            float crawlDirZ = (bot.faction == Faction::FactionA) ? -1.0f : 1.0f;
            bot.position += glm::vec3(0.0f, 0.0f, crawlDirZ * 0.65f * botDt);
            world.checkCollision(bot.position, 0.24f);

            if (bot.dbnoTimer <= 0.0f) {
                bot.state = BotState::Dead;

                // Drop fallen soldier's weapon in-world
                SupplyCrate botLoot;
                botLoot.position = bot.position + glm::vec3(0.0f, 0.05f, 0.0f);
                botLoot.name = bot.rank + " " + bot.callsign + "'s Weapon";
                if (bot.faction == Faction::FactionA) {
                    botLoot.contents.push_back(Inventory::createM4A1());
                    botLoot.contents.push_back(Inventory::create556Ammo(30));
                } else {
                    botLoot.contents.push_back(Inventory::createAK74());
                    botLoot.contents.push_back(Inventory::create545Ammo(30));
                }
                botLoot.looted = false;
                botLoot.isGroundItem = true;
                botLoot.yaw = bot.yaw + 1.5707f;
                world.getCrates().push_back(botLoot);
            }
            continue;
        }

        BotState prevState = bot.state;

        // Handle Friendly Garrison Stationary Sentries
        if (bot.isStationarySentry) {
            bot.sentryLookTimer += botDt;
            float distToPlayer = glm::distance(bot.position, playerPos);
            if (distToPlayer < 4.5f) {
                glm::vec3 toPlayer = playerPos - bot.position;
                bot.yaw = std::atan2(toPlayer.x, toPlayer.z);
            } else {
                bot.yaw = bot.baseFacingYaw + std::sin(bot.sentryLookTimer * 0.45f) * 0.38f;
            }
            bot.state = BotState::InCoverFiring;
            bot.isCrouched = false;
            bot.position.y = world.sampleTerrainHeight(bot.position.x, bot.position.z);
        bot.velocity = (botDt > 0.0001f) ? (bot.position - prevBotPos) / botDt : glm::vec3(0.0f);
            continue;
        }

        // Handle Base Garrison Patrol Squads (smooth waypoint patrol routing)
        if (bot.isPatrolBot && !bot.patrolWaypoints.empty()) {
            glm::vec3 targetWp = bot.patrolWaypoints[bot.currentWaypoint];
            glm::vec3 toWp = targetWp - bot.position;
            toWp.y = 0.0f;
            float dist = glm::length(toWp);
            if (dist < 1.4f) {
                bot.currentWaypoint = (bot.currentWaypoint + 1) % bot.patrolWaypoints.size();
            } else {
                glm::vec3 moveDir = toWp / dist;
                bot.yaw = std::atan2(moveDir.x, moveDir.z);
                float patrolSpeed = (bot.role == CombatRole::SquadLeader) ? 1.7f : 1.45f;
                bot.position += moveDir * patrolSpeed * botDt;
                world.checkCollision(bot.position, 0.24f);
                bot.state = BotState::Advancing;
            }
            bot.position.y = world.sampleTerrainHeight(bot.position.x, bot.position.z);
        bot.velocity = (botDt > 0.0001f) ? (bot.position - prevBotPos) / botDt : glm::vec3(0.0f);
            continue;
        }

        // Handle CQB Shoot House OPFOR Aggressors (Room Ambush Defense)
        if (bot.isCqbAggressor) {
            float distToPlayer = glm::distance(bot.position, playerPos);
            bool playerInKillhouse = (playerPos.z >= 26.0f && playerPos.z <= 90.0f && playerPos.x >= -36.0f && playerPos.x <= 20.0f);
            bool playerSpotted = (distToPlayer <= 22.0f && playerInKillhouse && world.hasLineOfSight(bot.position + glm::vec3(0, 1.2f, 0), playerPos + glm::vec3(0, 1.2f, 0)));
            if (distToPlayer <= 5.5f && playerInKillhouse) playerSpotted = true;

            if (playerSpotted) {
                bot.hasNoticedPlayer = true;
            }

            if (bot.hasNoticedPlayer && (playerInKillhouse || distToPlayer < 24.0f)) {
                // Engage player in CQB room defense
                glm::vec3 diff = playerPos - bot.position;
                bot.yaw = std::atan2(diff.x, diff.z);
                bot.state = BotState::InCoverFiring;

                bot.peekTimer -= botDt;
                if (bot.peekTimer <= 0.0f) {
                    bot.isPeeking = !bot.isPeeking;
                    bot.peekTimer = bot.isPeeking ? 1.4f : 1.0f;
                }
                bot.isCrouched = !bot.isPeeking;

                bot.fireCooldown -= botDt;
                if (bot.isPeeking && bot.fireCooldown <= 0.0f && bot.burstShotsRemaining == 0) {
                    bot.burstShotsRemaining = 3 + (rnd() % 2);
                    bot.burstIntervalTimer = 0.10f;
                    bot.fireCooldown = 1.8f + (rnd() % 40) * 0.02f;
                }
            } else {
                // Return to or hold guard anchor in room
                glm::vec3 toAnchor = bot.guardAnchor - bot.position;
                toAnchor.y = 0.0f;
                float anchorDist = glm::length(toAnchor);
                if (anchorDist > 0.8f) {
                    glm::vec3 moveDir = toAnchor / anchorDist;
                    bot.yaw = std::atan2(moveDir.x, moveDir.z);
                    bot.position += moveDir * 1.5f * botDt;
                    world.checkCollision(bot.position, 0.24f);
                    bot.state = BotState::Advancing;
                } else {
                    bot.state = BotState::InCoverFiring;
                    bot.isCrouched = false;
                    bot.isPeeking = true;
                    bot.yaw = 0.0f; // Face compound entrance
                }
            }
            bot.position.y = world.sampleTerrainHeight(bot.position.x, bot.position.z);
        bot.velocity = (botDt > 0.0001f) ? (bot.position - prevBotPos) / botDt : glm::vec3(0.0f);
            continue;
        }

        // Suppression decay & pinning check for bots
        bot.suppression = std::max(0.0f, bot.suppression - botDt * 18.0f);
        bot.isPinned = (bot.suppression >= 85.0f);

        // Distance Throttling for CPU Performance (>130m away updates at 10Hz)
        float distToPlayer = glm::distance(bot.position, playerPos);
        if (distToPlayer > 130.0f) {
            bot.throttleAccumulator += dt;
            if (bot.throttleAccumulator < 0.10f) {
                continue; // Skip AI tick to save CPU cycles
            }
            botDt = bot.throttleAccumulator;
            bot.throttleAccumulator = 0.0f;
        }

        // --- Active Burst Fire Execution ---
        if (bot.burstShotsRemaining > 0) {
            bot.burstIntervalTimer -= botDt;
            if (bot.burstIntervalTimer <= 0.0f) {
                bot.burstShotsRemaining--;
                bot.burstIntervalTimer = (bot.role == CombatRole::AutomaticRifleman) ? 0.095f : 0.125f;

                float botChestY = (bot.state == BotState::WoundedDown) ? 0.25f : (bot.isCrouched ? 0.75f : 1.15f);
                glm::vec3 muzzleOrigin = bot.position + glm::vec3(0.0f, botChestY, 0.0f);
                glm::vec3 aimDir(std::sin(bot.yaw), 0.0f, std::cos(bot.yaw));

                // Direct accurate target acquisition: Human Player or Enemy Bot
                if (bot.isCqbAggressor && bot.hasNoticedPlayer) {
                    glm::vec3 diff = (playerPos + glm::vec3(0, 1.05f, 0)) - muzzleOrigin;
                    if (glm::length(diff) > 0.1f) aimDir = glm::normalize(diff);
                } else if (bot.currentTargetIsPlayer) {
                    glm::vec3 diff = (playerPos + glm::vec3(0, 1.15f, 0)) - muzzleOrigin;
                    if (glm::length(diff) > 0.1f) aimDir = glm::normalize(diff);
                } else if (bot.currentEnemyTargetId != 0) {
                    for (const auto& tgtBot : m_bots) {
                        if (tgtBot.id == bot.currentEnemyTargetId && tgtBot.state != BotState::Dead) {
                            float tgtY = (tgtBot.isCrouched ? 0.75f : 1.15f);
                            glm::vec3 diff = (tgtBot.position + glm::vec3(0, tgtY, 0)) - muzzleOrigin;
                            if (glm::length(diff) > 0.1f) aimDir = glm::normalize(diff);
                            break;
                        }
                    }
                } else if (glm::length(bot.currentTargetPos) > 0.1f) {
                    glm::vec3 diff = (bot.currentTargetPos + glm::vec3(0, 1.0f, 0)) - muzzleOrigin;
                    if (glm::length(diff) > 0.1f) aimDir = glm::normalize(diff);
                }

                aimDir.x += spreadDist(rnd) * ((bot.role == CombatRole::AutomaticRifleman) ? 2.2f : 1.6f);
                aimDir.y += spreadDist(rnd) * ((bot.role == CombatRole::AutomaticRifleman) ? 1.8f : 1.2f);
                aimDir.z += spreadDist(rnd) * ((bot.role == CombatRole::AutomaticRifleman) ? 2.2f : 1.6f);
                aimDir = glm::normalize(aimDir);

                glm::vec3 muzzle = muzzleOrigin + aimDir * 0.52f;
                bool isFriendlyToPlayer = (bot.faction == playerFaction);

                int botWType = (bot.role == CombatRole::SquadLeader) ? 1 : 0;
                float flashScale = (bot.role == CombatRole::SquadLeader) ? 1.45f : ((bot.role == CombatRole::AutomaticRifleman) ? 1.30f : 1.0f);
                float smokeScale = (bot.role == CombatRole::AutomaticRifleman) ? 1.20f : 0.90f;

                glm::vec3 upRef = (std::abs(aimDir.y) > 0.95f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
                glm::vec3 rightDir = glm::normalize(glm::cross(aimDir, upRef));
                glm::vec3 trueUp   = glm::normalize(glm::cross(rightDir, aimDir));

                // Bot Muzzle Flash, Volumetric Gunsmoke & Ejected Spent Brass Casings
                ballistics.spawnMuzzleFlash(muzzle, aimDir, flashScale, botWType);
                ballistics.spawnMuzzleSmoke(muzzle, aimDir, smokeScale);

                glm::vec3 casingPos = muzzle - aimDir * 0.35f + rightDir * 0.08f + trueUp * 0.04f;
                ballistics.spawnShellCasing(casingPos, rightDir, trueUp, aimDir, botWType);

                float bulletSpeed = (bot.faction == Faction::FactionA) ? 880.0f : 900.0f;
                float bulletDmg = (bot.role == CombatRole::AutomaticRifleman) ? 26.0f : ((bot.role == CombatRole::SquadLeader) ? 48.0f : 22.0f);
                float maxRange = (bot.role == CombatRole::AutomaticRifleman) ? 90.0f : ((bot.role == CombatRole::SquadLeader) ? 120.0f : 75.0f);
                if (m_isTdmMode) {
                    maxRange = 250.0f; // Guarantee bullets reach enemies across the arena
                }
                ballistics.fireBullet(muzzle, aimDir, bulletSpeed, bulletDmg, 1.0f, isFriendlyToPlayer, bot.faction, static_cast<int>(bot.id), maxRange, botWType);

                // Dynamic environmental acoustic processing & wall occlusion
                AcousticEnvironment botEnv = world.detectAcousticEnvironment(muzzle);
                float botOcc = world.evaluateAcousticOcclusion(muzzle, playerPos);
                AudioEngine::get().playSound3D(SoundType::RifleShot, muzzle, playerPos, cameraYawRad, 0.45f, 85.0f, botEnv, botOcc);
                float distToPlayerShot = glm::distance(muzzle, playerPos);
            }
        }

        // --- Medic Triage Evaluation ---
        if (bot.role == CombatRole::Medic && bot.burstShotsRemaining == 0) {
            FrontlineBot* woundedAlly = nullptr;
            float nearestWoundedDist = 20.0f;

            for (auto& ally : m_bots) {
                if (ally.state == BotState::Dead || ally.faction != bot.faction || &ally == &bot) continue;
                if (ally.state == BotState::WoundedDown || ally.health < ally.maxHealth * 0.70f) {
                    float d = glm::distance(bot.position, ally.position);
                    if (d < nearestWoundedDist) {
                        nearestWoundedDist = d;
                        woundedAlly = &ally;
                    }
                }
            }

            if (woundedAlly) {
                if (nearestWoundedDist > 2.8f) {
                    // Navigate toward wounded ally to provide field dressing
                    glm::vec3 triageDir = glm::normalize(woundedAlly->position - bot.position);
                    bot.position += triageDir * 3.1f * botDt;
                    bot.yaw = std::atan2(triageDir.x, triageDir.z);
                    world.checkCollision(bot.position, 0.24f);
                    bot.state = BotState::Advancing;
                    continue;
                } else {
                    // In triage proximity: administer Ratnik / IFAK trauma dressing
                    bot.state = BotState::PerformingTriage;
                    bot.healTimer += botDt;
                    if (bot.healTimer >= 1.6f) {
                        if (woundedAlly->state == BotState::WoundedDown) {
                            woundedAlly->state = BotState::Advancing;
                            woundedAlly->health = 35.0f;
                            woundedAlly->dbnoTimer = 0.0f;
                        } else {
                            woundedAlly->health = std::min(woundedAlly->maxHealth, woundedAlly->health + 35.0f);
                        }
                        bot.healTimer = 0.0f;
                        // 3D Spatialized field dressing (audible only if within 14m)
                        AudioEngine::get().playSound3D(SoundType::ItemEquip, bot.position, playerPos, cameraYawRad, 0.40f, 14.0f);
                    }
                    continue;
                }
            }
        }

        // --- Player Threat & Notice Evaluation ---
        if (playerSpawnProtected) {
            bot.hasNoticedPlayer = false;
            bot.playerNoticeTime = 0.0f;
        } else if (bot.recentDamageTakenTimer > 0.0f) {
            bot.recentDamageTakenTimer = std::max(0.0f, bot.recentDamageTakenTimer - botDt);
            bot.hasNoticedPlayer = true;
        }

        if (playerFaction != bot.faction && playerFaction != Faction::None && !playerSpawnProtected) {
            float distToPlayer = glm::distance(bot.position, playerPos);
            glm::vec3 toPlayer = glm::normalize(playerPos - bot.position);
            glm::vec3 botFacing(std::sin(bot.yaw), 0.0f, std::cos(bot.yaw));
            float viewDot = glm::dot(toPlayer, botFacing);

            // In TDM mode, human-like reaction time and line-of-sight awareness
            if (m_isTdmMode) {
                bool losToPlayer = world.hasLineOfSight(bot.position + glm::vec3(0, 1.25f, 0), playerPos + glm::vec3(0, 1.25f, 0));
                if (losToPlayer) {
                    if (distToPlayer <= 5.0f) {
                        bot.hasNoticedPlayer = true;
                    } else if (viewDot > 0.20f) {
                        bot.playerNoticeTime += botDt * 1.8f; // ~0.55s reaction time
                        if (bot.playerNoticeTime >= 1.0f) {
                            bot.hasNoticedPlayer = true;
                        }
                    } else if (distToPlayer <= 10.0f) {
                        bot.playerNoticeTime += botDt * 0.9f;
                        if (bot.playerNoticeTime >= 1.2f) {
                            bot.hasNoticedPlayer = true;
                        }
                    }
                } else {
                    bot.playerNoticeTime = std::max(0.0f, bot.playerNoticeTime - botDt * 0.8f);
                }
            } else {
                // Open World notice
                if (distToPlayer <= 8.5f) {
                    bot.hasNoticedPlayer = true;
                } else if (distToPlayer <= 45.0f && viewDot > 0.30f && world.hasLineOfSight(bot.position + glm::vec3(0, 1.2f, 0), playerPos + glm::vec3(0, 1.2f, 0))) {
                    bot.playerNoticeTime += botDt * 2.0f;
                    if (bot.playerNoticeTime >= 1.0f) {
                        bot.hasNoticedPlayer = true;
                    }
                } else {
                    bot.playerNoticeTime = std::max(0.0f, bot.playerNoticeTime - botDt * 0.6f);
                }
            }
        }

        if (bot.barkTimer > 0.0f) bot.barkTimer -= botDt;
        if (bot.grenadeCooldown > 0.0f) bot.grenadeCooldown -= botDt;

        // Suppression Combat Bark
        if (bot.suppression > 45.0f && bot.barkTimer <= 0.0f) {
            bot.barkTimer = 6.5f + (rnd() % 20) * 0.1f;
            AudioEngine::get().playSound3D(SoundType::SoldierBarkSuppressed, bot.position, playerPos, cameraYawRad, 0.65f, 32.0f);
        }

        // --- Combat Targeting: Evaluates All Enemies (Human Player + Enemy Bots) ---
        glm::vec3 targetPos(0.0f);
        bool hasTarget = false;
        float bestTargetScore = -9999.0f;
        uint32_t chosenBotId = 0;
        bool chosenIsPlayer = false;

        // 1. Evaluate Human Player as a Combat Target
        if (playerFaction != bot.faction && playerFaction != Faction::None && bot.hasNoticedPlayer && !playerSpawnProtected) {
            float distToPlayer = glm::distance(bot.position, playerPos);
            float maxEngagementRange = m_isTdmMode ? 65.0f : 55.0f;
            if (distToPlayer <= maxEngagementRange) {
                bool los = world.hasLineOfSight(bot.position + glm::vec3(0, 1.25f, 0), playerPos + glm::vec3(0, 1.25f, 0));
                if (los || distToPlayer <= 6.0f) {
                    float score = (maxEngagementRange - distToPlayer);
                    if (bot.recentDamageTakenTimer > 0.0f) score += 30.0f;
                    if (distToPlayer < 12.0f) score += 15.0f;
                    if (score > bestTargetScore) {
                        bestTargetScore = score;
                        targetPos = playerPos;
                        chosenIsPlayer = true;
                        chosenBotId = 0;
                        hasTarget = true;
                    }
                }
            }
        }

        // 2. Evaluate Opposing Faction Frontline Bots
        for (const auto& other : m_bots) {
            if (other.state == BotState::Dead || other.state == BotState::WoundedDown || other.faction == bot.faction) continue;
            float dist = glm::distance(bot.position, other.position);
            float maxRange = m_isTdmMode ? 65.0f : 58.0f;
            if (dist <= maxRange) {
                bool los = world.hasLineOfSight(bot.position + glm::vec3(0, 1.25f, 0), other.position + glm::vec3(0, 1.25f, 0));
                if (los || dist <= 7.0f) {
                    float score = (maxRange - dist);
                    if (score > bestTargetScore) {
                        bestTargetScore = score;
                        targetPos = other.position;
                        chosenIsPlayer = false;
                        chosenBotId = other.id;
                        hasTarget = true;
                    }
                }
            }
        }

        bot.currentTargetIsPlayer = chosenIsPlayer;
        bot.currentEnemyTargetId = chosenBotId;
        bot.currentTargetPos = targetPos;

        // --- Tactical Cover Seeking (Relative to Target/Threat) ---
        bot.coverSeekTimer += botDt;
        bool shouldSeekCover = (bot.coverSeekTimer >= 2.5f || !bot.hasCoverPosition || bot.suppression > 25.0f || bot.recentDamageTakenTimer > 0.0f);
        if (hasTarget && shouldSeekCover) {
            bot.coverSeekTimer = (rnd() % 40) * 0.02f;
            float bestCoverDist = 26.0f;
            glm::vec3 threatPos = targetPos;

            for (const auto& obs : world.getObstacles()) {
                if (!obs.blocksLineOfSight || obs.size.y < 0.75f) continue;
                if (obs.type == ObstacleType::StreetLamp || obs.type == ObstacleType::PerimeterFence) continue;

                float d = glm::distance(bot.position, obs.position);
                if (d < bestCoverDist) {
                    // Safe tactical position is directly BEHIND obstacle relative to threat
                    glm::vec3 threatToObs = obs.position - threatPos;
                    threatToObs.y = 0.0f;
                    float tDist = glm::length(threatToObs);
                    if (tDist > 0.1f) {
                        threatToObs /= tDist;
                        float coverDepth = std::clamp(std::min(obs.size.x, obs.size.z) * 0.5f + 0.75f, 0.9f, 2.8f);
                        glm::vec3 candidateCover = obs.position + threatToObs * coverDepth;
                        candidateCover.y = world.sampleTerrainHeight(candidateCover.x, candidateCover.z);

                        CoverType cov = world.evaluateCoverAt(candidateCover, threatPos);
                        bool losBlocked = !world.hasLineOfSight(candidateCover + glm::vec3(0, 1.1f, 0), threatPos + glm::vec3(0, 1.2f, 0));
                        if (cov != CoverType::Exposed || losBlocked) {
                            bestCoverDist = d;
                            bot.coverPosition = candidateCover;
                            bot.hasCoverPosition = true;
                        }
                    }
                }
            }
        }

        // Tactical Smoke Screen Deployment when Pinned or Critical
        if (m_isTdmMode && (bot.suppression > 42.0f || bot.health < 32.0f) && bot.smokeGrenadeCount > 0 && hasTarget) {
            bot.smokeGrenadeCount--;
            glm::vec3 throwOrigin = bot.position + glm::vec3(0.0f, 1.4f, 0.0f);
            glm::vec3 throwAim = glm::normalize(targetPos - bot.position);
            glm::vec3 throwVel = throwAim * 13.5f + glm::vec3(0.0f, 4.0f, 0.0f);
            ballistics.throwGrenade(throwOrigin, throwVel, true /* isSmoke */, false, bot.faction, bot.id);
            AudioEngine::get().playSound3D(SoundType::GrenadePin, bot.position, playerPos, cameraYawRad, 0.60f, 22.0f);
        }

        // Squad Tactical Grenade Deployment & Camper Flushing
        float distToTarget = hasTarget ? glm::distance(bot.position, targetPos) : 999.0f;
        if (hasTarget && bot.grenadeCount > 0 && bot.grenadeCooldown <= 0.0f && distToTarget >= 12.0f && distToTarget <= 28.0f) {
            CoverType targetCover = world.evaluateCoverAt(targetPos, bot.position);
            if (targetCover != CoverType::Exposed || bot.role == CombatRole::SquadLeader || bot.role == CombatRole::Rifleman) {
                bot.grenadeCooldown = 22.0f + (rnd() % 80) * 0.1f;
                bot.grenadeCount--;

                // Squad bark: FRAG OUT!
                bot.barkTimer = 4.5f;
                AudioEngine::get().playSound3D(SoundType::SoldierBarkFragOut, bot.position, playerPos, cameraYawRad, 0.75f, 38.0f);
                AudioEngine::get().playSound3D(SoundType::GrenadePin, bot.position, playerPos, cameraYawRad, 0.60f, 22.0f);

                glm::vec3 throwOrigin = bot.position + glm::vec3(0.0f, 1.4f, 0.0f);
                glm::vec3 throwAim = glm::normalize(targetPos - bot.position);
                float throwDist = distToTarget;
                float throwSpeed = std::max(11.0f, throwDist * 0.84f);
                glm::vec3 throwVel = throwAim * throwSpeed + glm::vec3(0.0f, 3.8f + throwDist * 0.10f, 0.0f);

                ballistics.throwGrenade(throwOrigin, throwVel, false, false, bot.faction, bot.id);
            }
        }

        // --- AI Behavior Decision: Cover, Peeking & Volleys ---
        if (hasTarget) {
            if (prevState != BotState::InCoverFiring && bot.barkTimer <= 0.0f) {
                bot.barkTimer = 8.0f + (rnd() % 30) * 0.1f;
                AudioEngine::get().playSound3D(SoundType::SoldierBarkContact, bot.position, playerPos, cameraYawRad, 0.70f, 38.0f);
            }
            bot.state = BotState::InCoverFiring;

            // Turn towards combat target
            glm::vec3 diff = targetPos - bot.position;
            bot.yaw = std::atan2(diff.x, diff.z);

            // Cover Peeking & Jiggle Leaning Cycle
            bot.peekTimer -= botDt;
            if (bot.peekTimer <= 0.0f) {
                bot.isPeeking = !bot.isPeeking;
                bot.peekTimer = bot.isPeeking ? (0.65f + (rnd() % 10) * 0.04f) : (0.80f + (rnd() % 10) * 0.04f);
                if (bot.isPeeking) {
                    bot.lean = (bot.id % 2 == 0) ? 0.32f : -0.32f;
                } else {
                    bot.lean = 0.0f;
                }
            }

            // Crouch if in low cover or ducking or suppressed
            CoverType cover = world.evaluateCoverAt(bot.position, targetPos);
            bot.isCrouched = (cover != CoverType::Exposed || !bot.isPeeking || bot.suppression > 25.0f);

            // Tactical Dash to Cover: If not yet in cover position, sprint behind obstacle!
            if (bot.hasCoverPosition && !bot.isPinned) {
                float distToCover = glm::distance(glm::vec2(bot.position.x, bot.position.z), glm::vec2(bot.coverPosition.x, bot.coverPosition.z));
                if (distToCover > 0.85f) {
                    glm::vec3 toCover = glm::normalize(bot.coverPosition - bot.position);
                    toCover.y = 0.0f;

                    // Combat Slide Trigger when exposed under fire
                    if (m_isTdmMode && (bot.suppression > 16.0f || bot.recentDamageTakenTimer > 0.0f) && distToCover <= 4.5f && !bot.isSliding) {
                        bot.isSliding = true;
                        bot.slideTimer = 0.50f;
                        bot.slideDirection = toCover;
                        bot.isCrouched = true;
                        AudioEngine::get().playSound3D(SoundType::SprintSlide, bot.position, playerPos, cameraYawRad, 0.70f, 28.0f);
                    }

                    if (bot.isSliding && bot.slideTimer > 0.0f) {
                        bot.slideTimer -= botDt;
                        bot.position += bot.slideDirection * (5.2f * botDt);
                        bot.isCrouched = true;
                        if (bot.slideTimer <= 0.0f) bot.isSliding = false;
                    } else {
                        float sprintSpeed = (bot.suppression > 20.0f || bot.recentDamageTakenTimer > 0.0f) ? 4.2f : 3.0f;
                        bot.position += toCover * sprintSpeed * botDt;
                        bot.isCrouched = false;
                    }
                    bot.yaw = std::atan2(toCover.x, toCover.z);
                    world.checkCollision(bot.position, 0.24f);
                }
            }

            bot.fireCooldown -= botDt;
            // Shouted Reload Bark
            if (bot.fireCooldown > 1.8f && (rnd() % 100) < 4 && bot.barkTimer <= 0.0f) {
                bot.barkTimer = 6.0f;
                AudioEngine::get().playSound3D(SoundType::SoldierBarkReloading, bot.position, playerPos, cameraYawRad, 0.60f, 30.0f);
            }

            if ((bot.isPeeking || cover == CoverType::Exposed) && bot.fireCooldown <= 0.0f && bot.burstShotsRemaining == 0) {
                if (bot.role == CombatRole::AutomaticRifleman) {
                    bot.burstShotsRemaining = 4 + (rnd() % 3);
                    bot.burstIntervalTimer = 0.09f;
                    bot.fireCooldown = 1.6f + (rnd() % 40) * 0.02f;
                } else if (bot.role == CombatRole::SquadLeader) {
                    bot.burstShotsRemaining = 2 + (rnd() % 2);
                    bot.burstIntervalTimer = 0.16f;
                    bot.fireCooldown = 1.4f + (rnd() % 30) * 0.02f;
                } else {
                    bot.burstShotsRemaining = 3 + (rnd() % 2);
                    bot.burstIntervalTimer = 0.10f;
                    bot.fireCooldown = 1.2f + (rnd() % 40) * 0.02f;
                }
            }
        } else {
            bot.state = BotState::Advancing;
            bot.isCrouched = false;
            if (bot.barkTimer <= 0.0f && (rnd() % 1000) < 5) {
                for (const auto& ally : m_bots) {
                    if (ally.id != bot.id && ally.faction == bot.faction && ally.state != BotState::Dead && glm::distance(bot.position, ally.position) < 24.0f) {
                        bot.barkTimer = 9.0f + (rnd() % 30) * 0.1f;
                        AudioEngine::get().playSound3D(SoundType::SoldierBarkFlanking, bot.position, playerPos, cameraYawRad, 0.70f, 34.0f);
                        break;
                    }
                }
            }

            if (m_isTdmMode) {
                // Bounding Overwatch & Tactical 3-Lane CQB Navigation:
                // Lanes: 0 = West Freight Rail (-20m), 1 = Center Warehouse Killhouse (0m), 2 = East Courtyard Ruins (+21m)
                float laneX = (bot.tdmLaneIndex == 0) ? -20.0f : ((bot.tdmLaneIndex == 2) ? 21.0f : 0.0f);
                float enemyZ = (bot.faction == Faction::FactionA) ? 22.0f : -22.0f;
                if (m_spawnFlipped) enemyZ = -enemyZ;

                // Dynamic Tactical Flanking Check:
                if (std::abs(bot.position.z) < 8.0f && (rnd() % 1000) < 6) {
                    if (bot.tdmLaneIndex == 1) {
                        bot.tdmLaneIndex = (rnd() % 2 == 0) ? 0 : 2;
                    } else {
                        bot.tdmLaneIndex = 1; // Flank into center killhouse!
                    }
                    laneX = (bot.tdmLaneIndex == 0) ? -20.0f : ((bot.tdmLaneIndex == 2) ? 21.0f : 0.0f);
                }

                glm::vec3 tdmNavGoal(laneX, 0.0f, 0.0f);
                float distToEnemyBase = std::abs(enemyZ - bot.position.z);
                float zToMid = (bot.faction == Faction::FactionA) ? (0.0f - bot.position.z) : (bot.position.z - 0.0f);
                if (m_spawnFlipped) zToMid = -zToMid;

                if (distToEnemyBase < 4.5f) {
                    const FrontlineBot* nearestHostile = nullptr;
                    float nearestHostileDist = 999.0f;
                    for (const auto& other : m_bots) {
                        if (other.state != BotState::Dead && other.faction != bot.faction) {
                            float d = glm::distance(bot.position, other.position);
                            if (d < nearestHostileDist) {
                                nearestHostileDist = d;
                                nearestHostile = &other;
                            }
                        }
                    }
                    if (nearestHostile && nearestHostileDist < 45.0f) {
                        tdmNavGoal = nearestHostile->position;
                    } else if (playerFaction != bot.faction && playerFaction != Faction::None) {
                        tdmNavGoal = playerPos;
                    } else {
                        tdmNavGoal = glm::vec3(laneX, 0.0f, 0.0f);
                    }
                } else if (zToMid < 2.0f) {
                    tdmNavGoal = glm::vec3(laneX, 0.0f, enemyZ);
                }

                glm::vec3 toGoal = tdmNavGoal - bot.position;
                toGoal.y = 0.0f;
                float goalDist = glm::length(toGoal);
                glm::vec3 steerDir(0.0f, 0.0f, (bot.faction == Faction::FactionA ? 1.0f : -1.0f));
                if (m_spawnFlipped) steerDir = -steerDir;
                if (goalDist > 1.0f) {
                    steerDir = toGoal / goalDist;
                }

                // Autonomous Whisker Obstacle Avoidance for barriers and freight cars
                glm::vec3 probeStart = bot.position + glm::vec3(0.0f, 0.6f, 0.0f);
                glm::vec3 probeForward = probeStart + steerDir * 2.2f;
                glm::vec3 hitP, hitN;
                WorldObstacle* hitObs = nullptr;
                if (world.raycast(probeStart, probeForward, hitP, hitN, hitObs)) {
                    glm::vec3 leftProbe = probeStart + glm::vec3(steerDir.z, 0.0f, -steerDir.x) * 2.0f;
                    glm::vec3 rightProbe = probeStart + glm::vec3(-steerDir.z, 0.0f, steerDir.x) * 2.0f;
                    bool leftBlocked = world.raycast(probeStart, leftProbe, hitP, hitN, hitObs);
                    bool rightBlocked = world.raycast(probeStart, rightProbe, hitP, hitN, hitObs);
                    if (!leftBlocked) steerDir = glm::normalize(steerDir + glm::vec3(steerDir.z, 0.0f, -steerDir.x) * 1.3f);
                    else if (!rightBlocked) steerDir = glm::normalize(steerDir + glm::vec3(-steerDir.z, 0.0f, steerDir.x) * 1.3f);
                }

                // Bounding Overwatch Pace: Sprint if covering partner or advancing
                bool isAdvancingSquad = false;
                for (const auto& ally : m_bots) {
                    if (ally.id != bot.id && ally.faction == bot.faction && ally.tdmLaneIndex == bot.tdmLaneIndex) {
                        if (ally.state == BotState::InCoverFiring) isAdvancingSquad = true;
                    }
                }

                float speed = isAdvancingSquad ? 3.8f : ((bot.role == CombatRole::AutomaticRifleman) ? 2.6f : 3.0f);
                bot.position += steerDir * speed * botDt;
                bot.yaw = std::atan2(steerDir.x, steerDir.z);
                world.checkCollision(bot.position, 0.24f);

                // Footstep audio
                bot.footstepTimer += botDt * (speed * 1.35f);
                if (bot.footstepTimer >= 1.0f) {
                    bot.footstepTimer = 0.0f;
                    float distToPlayer = glm::distance(bot.position, playerPos);
                    if (distToPlayer <= 28.0f) {
                        GroundSurface surf = world.getGroundSurface(bot.position);
                        AcousticEnvironment botEnv = world.detectAcousticEnvironment(bot.position);
                        float botOcc = world.evaluateAcousticOcclusion(bot.position, playerPos);
                        SoundType stepSnd = SoundType::FootstepConcrete;
                        if (surf == GroundSurface::Mud) stepSnd = SoundType::FootstepMud;
                        else if (surf == GroundSurface::Dirt) stepSnd = SoundType::FootstepDirt;
                        AudioEngine::get().playSound3D(stepSnd, bot.position, playerPos, cameraYawRad, 0.32f, 28.0f, botEnv, botOcc);
                    }
                }
            } else {
                // Advance toward frontline river bridges (Z = 0) or enemy perimeter
                float moveDirZ = (bot.faction == Faction::FactionA) ? 1.0f : -1.0f;
                bool reachedPerimeter = (bot.faction == Faction::FactionA && bot.position.z >= 210.0f) ||
                                       (bot.faction == Faction::FactionB && bot.position.z <= -210.0f);
                if (!reachedPerimeter) {
                    float speed = (bot.role == CombatRole::AutomaticRifleman) ? 2.3f : 2.7f;
                    if (world.isPositionInRiver(bot.position)) {
                        speed *= 0.45f; // River water & mud slowdown
                    }

                    // Autonomous Whisker Obstacle Avoidance:
                    glm::vec3 probeStart = bot.position + glm::vec3(0.0f, 0.6f, 0.0f);
                    glm::vec3 probeForward = probeStart + glm::vec3(0.0f, 0.0f, moveDirZ * 1.8f);
                    glm::vec3 hitP, hitN;
                    WorldObstacle* hitObs = nullptr;
                    glm::vec3 steerDir(0.0f, 0.0f, moveDirZ);

                    if (world.raycast(probeStart, probeForward, hitP, hitN, hitObs)) {
                        glm::vec3 leftProbe = probeStart + glm::vec3(1.6f, 0.0f, moveDirZ * 0.8f);
                        glm::vec3 rightProbe = probeStart + glm::vec3(-1.6f, 0.0f, moveDirZ * 0.8f);
                        bool leftBlocked = world.raycast(probeStart, leftProbe, hitP, hitN, hitObs);
                        bool rightBlocked = world.raycast(probeStart, rightProbe, hitP, hitN, hitObs);

                        if (!leftBlocked && rightBlocked) {
                            steerDir = glm::normalize(glm::vec3(0.85f, 0.0f, moveDirZ * 0.5f));
                        } else if (leftBlocked && !rightBlocked) {
                            steerDir = glm::normalize(glm::vec3(-0.85f, 0.0f, moveDirZ * 0.5f));
                        } else {
                            float lateral = (bot.position.x > 0.0f) ? -0.85f : 0.85f;
                            steerDir = glm::normalize(glm::vec3(lateral, 0.0f, moveDirZ * 0.5f));
                        }
                    }

                    glm::vec3 moveStep = steerDir * speed * botDt;
                    bot.position += moveStep;
                    bot.yaw = std::atan2(steerDir.x, steerDir.z);

                    world.checkCollision(bot.position, 0.24f);

                    // Surface-Aware 3D Spatialized AI Footsteps
                    bot.footstepTimer += botDt * (speed * 1.35f);
                    if (bot.footstepTimer >= 1.0f) {
                        bot.footstepTimer = 0.0f;
                        float distToPlayer = glm::distance(bot.position, playerPos);
                        if (distToPlayer <= 28.0f) {
                            GroundSurface surf = world.getGroundSurface(bot.position);
                            AcousticEnvironment botEnv = world.detectAcousticEnvironment(bot.position);
                            float botOcc = world.evaluateAcousticOcclusion(bot.position, playerPos);
                            SoundType stepSnd = SoundType::FootstepDirt;
                            if (surf == GroundSurface::Mud) stepSnd = SoundType::FootstepMud;
                            else if (surf == GroundSurface::Water) stepSnd = SoundType::FootstepWater;
                            else if (surf == GroundSurface::Wood) stepSnd = SoundType::FootstepWood;
                            else if (surf == GroundSurface::Concrete) stepSnd = SoundType::FootstepConcrete;
                            AudioEngine::get().playSound3D(stepSnd, bot.position, playerPos, cameraYawRad, 0.32f, 28.0f, botEnv, botOcc);
                        }
                    }
                }
            }
        }

        if (bot.state != prevState) {
            auto stateToStr = [](BotState s) {
                switch (s) {
                    case BotState::Advancing: return "Advancing";
                    case BotState::InCoverFiring: return "InCoverFiring";
                    case BotState::PerformingTriage: return "PerformingTriage";
                    case BotState::WoundedDown: return "WoundedDown";
                    case BotState::Dead: return "Dead";
                    default: return "Unknown";
                }
            };
            std::string roleStr = (bot.role == CombatRole::SquadLeader ? "SquadLeader" :
                                   (bot.role == CombatRole::AutomaticRifleman ? "AutoRifleman" :
                                   (bot.role == CombatRole::Medic ? "Medic" : "Rifleman")));
        }
        bot.position.y = world.sampleTerrainHeight(bot.position.x, bot.position.z);
        bot.velocity = (botDt > 0.0001f) ? (bot.position - prevBotPos) / botDt : glm::vec3(0.0f);
    }

    // 3. Unified Biomechanical Skeletal Animation & Locomotion Rig Update
    for (auto& bot : m_bots) {
        if (bot.state == BotState::Dead) continue;
        if (!bot.rig) {
            bot.rig = std::make_shared<CharacterRig>();
            bot.rig->snapYaw(bot.yaw);
        }

        float moveSpeed = glm::length(glm::vec2(bot.velocity.x, bot.velocity.z));
        bool isMoving = (moveSpeed > 0.06f);
        bool isSprinting = (moveSpeed > 2.4f);
        int stance = (bot.state == BotState::WoundedDown ? 2 : (bot.isCrouched ? 1 : 0));
        bool isFiring = (bot.burstShotsRemaining > 0);
        bool isAiming = (bot.state == BotState::InCoverFiring || isFiring);

        std::string weaponName = "M4A1";
        if (bot.faction == Faction::FactionB) {
            weaponName = (bot.role == CombatRole::SquadLeader) ? "Mosin-Nagant" : "AK-74M";
        } else {
            weaponName = (bot.role == CombatRole::SquadLeader) ? "M4A1" : "M4A1";
        }

        glm::vec3 aimTarget;
        if (bot.isCqbAggressor && bot.hasNoticedPlayer) {
            aimTarget = playerPos + glm::vec3(0.0f, 1.1f, 0.0f);
        } else if (bot.state == BotState::InCoverFiring || isFiring) {
            if (glm::length(bot.currentTargetPos) > 0.1f) {
                aimTarget = bot.currentTargetPos + glm::vec3(0.0f, 1.0f, 0.0f);
            } else {
                aimTarget = bot.position + glm::vec3(std::sin(bot.yaw), 0.0f, std::cos(bot.yaw)) * 15.0f + glm::vec3(0.0f, 1.0f, 0.0f);
            }
        } else {
            aimTarget = bot.position + glm::vec3(std::sin(bot.yaw), 0.0f, std::cos(bot.yaw)) * 10.0f;
        }

        bool inCover = (bot.state == BotState::InCoverFiring || (bot.hasCoverPosition && bot.isCrouched));
        float coverAimWeight = (bot.state == BotState::InCoverFiring && bot.isPeeking) ? 1.0f : 0.0f;
        bot.rig->setCoverParameters(inCover, true, 0.95f, coverAimWeight);

        bot.rig->setActiveWeaponName(weaponName);
        bot.rig->update9D(
            dt,
            bot.position,
            bot.yaw,
            aimTarget,
            bot.velocity,
            isMoving,
            isSprinting,
            stance,
            isFiring,
            isAiming,
            true
        );
    }
}

void FrontlineWarfare::render(Renderer& renderer, const glm::vec3& cameraPos, const glm::vec3& playerPos, float playerYaw, Faction playerFaction, bool playerHasComms) {
    (void)playerPos;
    (void)playerYaw;
    (void)playerHasComms;

    for (const auto& bot : m_bots) {
        if (bot.state == BotState::Dead) continue;

        // FOG OF WAR & TACTICAL RADIO DATALINK CULLING:
        // Hostile soldiers hidden behind walls or outside line-of-sight are NOT rendered
        // unless transmitted over the tactical squad radio net!
        if (playerFaction != Faction::None && bot.faction != playerFaction && !bot.isVisibleToPlayer) {
            continue; // Hidden in Fog of War
        }

        // Use 2D horizontal ground distance so top-down camera height (20-30m) does not artificially degrade bot LOD
        float horizDist = glm::distance(glm::vec2(cameraPos.x, cameraPos.z), glm::vec2(bot.position.x, bot.position.z));
        int lod = 0;
        if (horizDist >= 120.0f) {
            lod = 2; // Far distance: streamlined anatomical silhouette with helmet, vest & rifle
        } else if (horizDist >= 65.0f) {
            lod = 1; // Mid distance: full tactical infantry geometry
        } else {
            lod = 0; // Near distance: ultra high detail tactical uniforms, camo, ballistic plate carriers, comms & weapons
        }

        // Contact shadow rendered for near & mid distance
        if (lod <= 1) {
        }

        // Down-But-Not-Out (DBNO) pose or tactical low cover crouch
        float pitch = (bot.state == BotState::WoundedDown) ? -75.0f : (bot.isCrouched ? 14.0f : 0.0f);
        glm::vec3 renderPos = bot.position;

        // Render distinct faction infantry with full articulated skeletal animation & camo
        if (horizDist < 95.0f && bot.rig) {
            CharacterCustomization botCustom;
            if (bot.faction == Faction::FactionB) {
                botCustom.camoId = 2; // Russian Digital EMR Flora
                botCustom.armbandId = 1; // Red Armband
                botCustom.helmetId = 1; // 6B47
                botCustom.vestId = 1;   // 6B45
                botCustom.skinToneId = 1;
            } else {
                botCustom.camoId = 1; // Ukrainian MM-14 Digital
                botCustom.armbandId = 0; // Blue Armband
                botCustom.helmetId = 0; // FAST MT
                botCustom.vestId = 0;   // JPC 2.0
                botCustom.skinToneId = 1;
            }
            bot.rig->render(renderer, botCustom);
        } else {
            renderer.drawInfantry(bot.faction, bot.role, renderPos, bot.yaw, pitch, lod);
        }

        // Tactical Radio Datalink Marker:
        // If enemy is revealed through the squad radio datalink, render red tactical diamond beacon overhead!
        if (bot.isSpottedViaRadio && bot.faction != playerFaction) {
            glm::vec3 beaconPos = bot.position + glm::vec3(0.0f, 2.2f, 0.0f);
            renderer.drawAimReticle(beaconPos, 0.28f, glm::vec4(1.0f, 0.22f, 0.22f, 0.95f));
        }
    }
}




void FrontlineWarfare::initTrainingGarrison(World& world, Faction playerFaction) {
    (void)world;
    m_bots.clear();
    m_nextId = 500;

    Faction friendlyFaction = (playerFaction == Faction::FactionB) ? Faction::FactionB : Faction::FactionA;
    Faction opforFaction    = (friendlyFaction == Faction::FactionA) ? Faction::FactionB : Faction::FactionA;

    // 1. South Checkpoint Sentry Left: PVT Kowalski
    FrontlineBot gate1;
    gate1.id = m_nextId++;
    gate1.faction = friendlyFaction;
    gate1.role = CombatRole::Rifleman;
    gate1.rank = "PVT";
    gate1.callsign = "Kowalski";
    gate1.position = glm::vec3(-3.2f, 0.0f, -20.5f);
    gate1.yaw = 3.14159265f;
    gate1.baseFacingYaw = 3.14159265f;
    gate1.isStationarySentry = true;
    gate1.assignment = "South Gate Sentry";
    gate1.dialogue = "Halt. South perimeter checkpoint secure. Proving grounds live ahead.";
    gate1.health = 100.0f;
    gate1.maxHealth = 100.0f;
    gate1.body.init();
    gate1.body.applyArmor(BodyRegion::UpperChest, 4, 55.0f);
    gate1.body.applyArmor(BodyRegion::Head, 4, 50.0f);
    gate1.rig = std::make_shared<CharacterRig>(); gate1.rig->snapYaw(gate1.yaw);
    m_bots.push_back(gate1);

    // 2. South Checkpoint Sentry Right: CPL Miller
    FrontlineBot gate2;
    gate2.id = m_nextId++;
    gate2.faction = friendlyFaction;
    gate2.role = CombatRole::Rifleman;
    gate2.rank = "CPL";
    gate2.callsign = "Miller";
    gate2.position = glm::vec3(3.2f, 0.0f, -20.5f);
    gate2.yaw = 3.14159265f;
    gate2.baseFacingYaw = 3.14159265f;
    gate2.isStationarySentry = true;
    gate2.assignment = "South Gate Sentry";
    gate2.dialogue = "Welcome to FOB Vanguard, operator. Weapons hot on range.";
    gate2.health = 100.0f;
    gate2.maxHealth = 100.0f;
    gate2.body.init();
    gate2.body.applyArmor(BodyRegion::UpperChest, 4, 55.0f);
    gate2.body.applyArmor(BodyRegion::Head, 4, 50.0f);
    gate2.rig = std::make_shared<CharacterRig>(); gate2.rig->snapYaw(gate2.yaw);
    m_bots.push_back(gate2);

    // 3. Range Safety Officer: SGT Bishop
    FrontlineBot rso;
    rso.id = m_nextId++;
    rso.faction = friendlyFaction;
    rso.role = CombatRole::SquadLeader;
    rso.rank = "SGT";
    rso.callsign = "Bishop";
    rso.position = glm::vec3(16.0f, 0.0f, -6.0f);
    rso.yaw = 1.5707963f;
    rso.baseFacingYaw = 1.5707963f;
    rso.isStationarySentry = true;
    rso.assignment = "Range Safety Officer";
    rso.dialogue = "Range is HOT. Target lanes calibrated 15m to 200m.";
    rso.health = 120.0f;
    rso.maxHealth = 120.0f;
    rso.body.init();
    rso.body.applyArmor(BodyRegion::UpperChest, 4, 75.0f);
    rso.body.applyArmor(BodyRegion::Head, 4, 60.0f);
    rso.rig = std::make_shared<CharacterRig>(); rso.rig->snapYaw(rso.yaw);
    m_bots.push_back(rso);

    // 4. Base Armorer & Quartermaster: SSG Reeves
    FrontlineBot qm;
    qm.id = m_nextId++;
    qm.faction = friendlyFaction;
    qm.role = CombatRole::SquadLeader;
    qm.rank = "SSG";
    qm.callsign = "Reeves";
    qm.position = glm::vec3(-7.0f, 0.0f, 0.0f);
    qm.yaw = 1.5707963f;
    qm.baseFacingYaw = 1.5707963f;
    qm.isStationarySentry = true;
    qm.assignment = "Base Quartermaster";
    qm.dialogue = "Armory tables stocked with fresh weapons and armor.";
    qm.health = 110.0f;
    qm.maxHealth = 110.0f;
    qm.body.init();
    qm.body.applyArmor(BodyRegion::UpperChest, 4, 65.0f);
    qm.body.applyArmor(BodyRegion::Head, 4, 55.0f);
    qm.rig = std::make_shared<CharacterRig>(); qm.rig->snapYaw(qm.yaw);
    m_bots.push_back(qm);

    // 5. Compound Patrol Lead: SSG Vance
    FrontlineBot pat1;
    pat1.id = m_nextId++;
    pat1.faction = friendlyFaction;
    pat1.role = CombatRole::SquadLeader;
    pat1.rank = "SSG";
    pat1.callsign = "Vance";
    pat1.position = glm::vec3(0.0f, 0.0f, -18.0f);
    pat1.isPatrolBot = true;
    pat1.assignment = "Base Patrol (Lead)";
    pat1.dialogue = "Patrolling central compound roads.";
    pat1.patrolWaypoints = {
        glm::vec3(0.0f, 0.0f, -18.0f),
        glm::vec3(12.0f, 0.0f, -6.0f),
        glm::vec3(0.0f, 0.0f, 15.0f),
        glm::vec3(-10.0f, 0.0f, 38.0f),
        glm::vec3(-30.0f, 0.0f, 15.0f),
        glm::vec3(0.0f, 0.0f, 0.0f)
    };
    pat1.currentWaypoint = 0;
    pat1.health = 110.0f;
    pat1.maxHealth = 110.0f;
    pat1.body.init();
    pat1.body.applyArmor(BodyRegion::UpperChest, 4, 65.0f);
    pat1.body.applyArmor(BodyRegion::Head, 4, 55.0f);
    pat1.rig = std::make_shared<CharacterRig>(); pat1.rig->snapYaw(pat1.yaw);
    m_bots.push_back(pat1);

    // 6. Obstacle Course Patrol: CPL Hayes
    FrontlineBot pat2;
    pat2.id = m_nextId++;
    pat2.faction = friendlyFaction;
    pat2.role = CombatRole::Rifleman;
    pat2.rank = "CPL";
    pat2.callsign = "Hayes";
    pat2.position = glm::vec3(-40.0f, 0.0f, 10.0f);
    pat2.isPatrolBot = true;
    pat2.assignment = "Obstacle Yard Security";
    pat2.dialogue = "Mobility course perimeter secure.";
    pat2.patrolWaypoints = {
        glm::vec3(-40.0f, 0.0f, 10.0f),
        glm::vec3(-60.0f, 0.0f, 10.0f),
        glm::vec3(-60.0f, 0.0f, 50.0f),
        glm::vec3(-40.0f, 0.0f, 50.0f)
    };
    pat2.currentWaypoint = 0;
    pat2.health = 95.0f;
    pat2.maxHealth = 95.0f;
    pat2.body.init();
    pat2.body.applyArmor(BodyRegion::UpperChest, 4, 55.0f);
    pat2.body.applyArmor(BodyRegion::Head, 3, 45.0f);
    pat2.rig = std::make_shared<CharacterRig>(); pat2.rig->snapYaw(pat2.yaw);
    m_bots.push_back(pat2);

    // 7. North Logistics Sentry: PFC Cross
    FrontlineBot pat3;
    pat3.id = m_nextId++;
    pat3.faction = friendlyFaction;
    pat3.role = CombatRole::AutomaticRifleman;
    pat3.rank = "PFC";
    pat3.callsign = "Cross";
    pat3.position = glm::vec3(0.0f, 0.0f, 25.0f);
    pat3.isPatrolBot = true;
    pat3.assignment = "North Logistics Security";
    pat3.dialogue = "Logistics road clear. Killhouse active to the northwest.";
    pat3.patrolWaypoints = {
        glm::vec3(0.0f, 0.0f, 25.0f),
        glm::vec3(-15.0f, 0.0f, 25.0f),
        glm::vec3(-15.0f, 0.0f, 35.0f),
        glm::vec3(0.0f, 0.0f, 35.0f)
    };
    pat3.currentWaypoint = 0;
    pat3.health = 95.0f;
    pat3.maxHealth = 95.0f;
    pat3.body.init();
    pat3.body.applyArmor(BodyRegion::UpperChest, 4, 65.0f);
    pat3.body.applyArmor(BodyRegion::Head, 3, 45.0f);
    pat3.rig = std::make_shared<CharacterRig>(); pat3.rig->snapYaw(pat3.yaw);
    m_bots.push_back(pat3);

    // 8. OPFOR CQB Aggressor 1 (Killhouse Room 1 - South Entry)
    FrontlineBot op1;
    op1.id = m_nextId++;
    op1.faction = opforFaction;
    op1.role = CombatRole::Rifleman;
    op1.rank = "CADRE";
    op1.callsign = "Aggressor-1";
    op1.position = glm::vec3(-15.0f, 0.0f, 44.0f);
    op1.guardAnchor = glm::vec3(-15.0f, 0.0f, 44.0f);
    op1.isCqbAggressor = true;
    op1.yaw = 3.14159265f;
    op1.assignment = "OPFOR CQB Aggressor";
    op1.dialogue = "[OPFOR] Hostile training target.";
    op1.health = 80.0f;
    op1.maxHealth = 80.0f;
    op1.body.init();
    op1.body.applyArmor(BodyRegion::UpperChest, 3, 40.0f);
    op1.body.applyArmor(BodyRegion::Head, 3, 35.0f);
    op1.rig = std::make_shared<CharacterRig>();
    op1.rig->snapYaw(op1.yaw);
    m_bots.push_back(op1);

    // 9. OPFOR CQB Aggressor 2 (Killhouse Room 2 - East Chamber)
    FrontlineBot op2;
    op2.id = m_nextId++;
    op2.faction = opforFaction;
    op2.role = CombatRole::AutomaticRifleman;
    op2.rank = "CADRE";
    op2.callsign = "Aggressor-2";
    op2.position = glm::vec3(-5.0f, 0.0f, 52.0f);
    op2.guardAnchor = glm::vec3(-5.0f, 0.0f, 52.0f);
    op2.isCqbAggressor = true;
    op2.yaw = 0.0f;
    op2.assignment = "OPFOR CQB Aggressor";
    op2.dialogue = "[OPFOR] Hostile training target.";
    op2.health = 90.0f;
    op2.maxHealth = 90.0f;
    op2.body.init();
    op2.body.applyArmor(BodyRegion::UpperChest, 3, 45.0f);
    op2.body.applyArmor(BodyRegion::Head, 3, 35.0f);
    op2.rig = std::make_shared<CharacterRig>();
    op2.rig->snapYaw(op2.yaw);
    m_bots.push_back(op2);

    // 10. OPFOR CQB Aggressor 3 (Killhouse Room 3 - North Interior)
    FrontlineBot op3;
    op3.id = m_nextId++;
    op3.faction = opforFaction;
    op3.role = CombatRole::SquadLeader;
    op3.rank = "CADRE";
    op3.callsign = "Aggressor-3";
    op3.position = glm::vec3(-12.0f, 0.0f, 62.0f);
    op3.guardAnchor = glm::vec3(-12.0f, 0.0f, 62.0f);
    op3.isCqbAggressor = true;
    op3.yaw = 3.14159265f;
    op3.assignment = "OPFOR CQB Aggressor";
    op3.dialogue = "[OPFOR] Hostile training target.";
    op3.health = 100.0f;
    op3.maxHealth = 100.0f;
    op3.body.init();
    op3.body.applyArmor(BodyRegion::UpperChest, 4, 55.0f);
    op3.body.applyArmor(BodyRegion::Head, 4, 45.0f);
    op3.rig = std::make_shared<CharacterRig>();
    op3.rig->snapYaw(op3.yaw);
    m_bots.push_back(op3);

    std::cout << "[FrontlineAI] FOB Vanguard garrison initialized (" << m_bots.size() << " bots: 7 Garrison Allies, 3 CQB OPFOR Aggressors).\n";
}

void FrontlineWarfare::resetTdmMatch() {
    m_teamAKills = 0;
    m_teamBKills = 0;
    m_matchTime = 600.0f;
    m_tdmMatchOver = false;
    m_tdmWinner = Faction::None;
}

void FrontlineWarfare::recordTdmKill(Faction killerFaction) {
    if (!m_isTdmMode || m_tdmMatchOver) return;
    if (killerFaction == Faction::FactionA) {
        m_teamAKills++;
        if (m_teamAKills >= m_scoreLimit) {
            m_tdmMatchOver = true;
            m_tdmWinner = Faction::FactionA;
        }
    } else if (killerFaction == Faction::FactionB) {
        m_teamBKills++;
        if (m_teamBKills >= m_scoreLimit) {
            m_tdmMatchOver = true;
            m_tdmWinner = Faction::FactionB;
        }
    }
}

void FrontlineWarfare::spawnTdmBot(FrontlineBot& bot, Faction faction, CombatRole role) {
    static std::mt19937 gen(1337);
    bot.faction = faction;
    bot.role = role;
    bot.isPatrolBot = false;
    bot.isStationarySentry = false;
    bot.isCqbAggressor = false;

    // Tactical Ranks and Callsigns
    if (faction == Faction::FactionA) {
        static const char* s_aCalls[] = { "Viper-1", "Ghost", "Hunter-6", "Reaper", "Warlord", "Bishop" };
        bot.callsign = s_aCalls[bot.id % 6];
        bot.rank = (role == CombatRole::SquadLeader) ? "SSG" : (role == CombatRole::AutomaticRifleman ? "CPL" : (role == CombatRole::Medic ? "PFC" : "PVT"));
    } else {
        static const char* s_bCalls[] = { "Buran", "Voron", "Sever", "Shtorm", "Grom", "Varyag" };
        bot.callsign = s_bCalls[bot.id % 6];
        bot.rank = (role == CombatRole::SquadLeader) ? "SR-SGT" : (role == CombatRole::AutomaticRifleman ? "JR-SGT" : (role == CombatRole::Medic ? "EFR" : "RYAD"));
    }

    switch (role) {
    case CombatRole::SquadLeader: bot.maxHealth = 120.0f; break;
    case CombatRole::AutomaticRifleman: bot.maxHealth = 95.0f; break;
    case CombatRole::Medic: bot.maxHealth = 85.0f; break;
    default: bot.maxHealth = 80.0f; break;
    }
    bot.health = bot.maxHealth;

    bot.state = BotState::Advancing;
    bot.fireCooldown = 1.2f + (gen() % 35) * 0.03f;
    bot.burstShotsRemaining = 0;
    bot.burstIntervalTimer = 0.0f;
    bot.respawnTimer = 0.0f;
    bot.healTimer = 0.0f;
    bot.rallyTimer = 0.0f;
    bot.isRallied = false;
    bot.throttleAccumulator = 0.0f;
    bot.isVisibleToPlayer = true;
    bot.isSpottedViaRadio = false;
    bot.suppression = 0.0f;
    bot.isPinned = false;
    bot.dbnoTimer = 0.0f;
    bot.flinchPitch = 0.0f;
    bot.hasNoticedPlayer = false;
    bot.playerNoticeTime = 0.0f;
    bot.coverSeekTimer = 0.0f;
    bot.hasCoverPosition = false;
    bot.currentEnemyTargetId = 0;
    bot.currentTargetIsPlayer = false;
    bot.currentTargetPos = glm::vec3(0.0f);
    bot.tdmLaneIndex = bot.id % 3;
    bot.recentDamageTakenTimer = 0.0f;
    bot.peekTimer = (gen() % 40) * 0.05f;
    bot.isPeeking = true;
    bot.isSliding = false;
    bot.slideTimer = 0.0f;
    bot.slideDirection = glm::vec3(0.0f);
    bot.isTacticalSprinting = false;
    bot.tacticalSprintTimer = 0.0f;
    bot.lean = 0.0f;
    bot.partnerBotId = 0;
    bot.isOverwatchSuppressor = false;
    bot.overwatchCycleTimer = (gen() % 30) * 0.1f;
    bot.smokeGrenadeCount = 1;
    bot.targetStationaryTimer = 0.0f;
    bot.lastTargetCoverPos = glm::vec3(0.0f);
    bot.tacticalRetreatTimer = 0.0f;

    bot.body.init();
    bot.body.applyArmor(BodyRegion::UpperChest, 4, 65.0f);
    bot.body.applyArmor(BodyRegion::LowerChest, 4, 65.0f);
    bot.body.applyArmor(BodyRegion::Head, 4, 55.0f);

    float offsetX = -10.0f + static_cast<float>((bot.id * 7) % 21);
    float offsetZ = static_cast<float>((bot.id * 3) % 5) - 2.0f;

    bool flipA = (faction == Faction::FactionA && m_spawnFlipped);
    bool flipB = (faction == Faction::FactionB && m_spawnFlipped);

    if ((faction == Faction::FactionA && !flipA) || flipB) {
        // Northern Substation compound
        bot.position = glm::vec3(offsetX, 0.0f, -26.5f + offsetZ);
        bot.yaw = 0.0f; // Facing South toward killhouse
    } else {
        // Southern Motor Pool compound
        bot.position = glm::vec3(offsetX, 0.0f, 26.5f - offsetZ);
        bot.yaw = 3.14159265f; // Facing North toward killhouse
    }

    if (!bot.rig) {
        bot.rig = std::make_shared<CharacterRig>();
    }
    bot.rig->resetDeath();
    bot.rig->snapYaw(bot.yaw);
}

void FrontlineWarfare::initTeamDeathmatch(World& world, Faction playerFaction, bool spawnAiBots) {
    (void)world;
    m_bots.clear();
    m_nextId = 600;
    m_isTdmMode = true;
    m_tdmAgainstAi = spawnAiBots;
    resetTdmMatch();

    if (!spawnAiBots) {
        std::cout << "[FrontlineAI] 5v5 Team Deathmatch Initialized: PLAYERS ONLY (PvP // AI Bots Disabled).\n";
        return;
    }

    Faction humanFaction = (playerFaction == Faction::FactionB) ? Faction::FactionB : Faction::FactionA;
    Faction enemyFaction = (humanFaction == Faction::FactionA) ? Faction::FactionB : Faction::FactionA;

    // 5v5 Structure:
    // Friendly team has 4 AI bots (player fills the 5th operative slot)
    // Enemy team has 5 AI bots
    // Guaranteed 5 vs 5!
    CombatRole friendlyRoles[4] = {
        CombatRole::SquadLeader,
        CombatRole::AutomaticRifleman,
        CombatRole::Medic,
        CombatRole::Rifleman
    };

    CombatRole enemyRoles[5] = {
        CombatRole::SquadLeader,
        CombatRole::AutomaticRifleman,
        CombatRole::Medic,
        CombatRole::Rifleman,
        CombatRole::Rifleman
    };

    for (int i = 0; i < 4; ++i) {
        FrontlineBot ally;
        ally.id = m_nextId++;
        spawnTdmBot(ally, humanFaction, friendlyRoles[i]);
        m_bots.push_back(ally);
    }

    for (int i = 0; i < 5; ++i) {
        FrontlineBot enemy;
        enemy.id = m_nextId++;
        spawnTdmBot(enemy, enemyFaction, enemyRoles[i]);
        m_bots.push_back(enemy);
    }

    std::cout << "[FrontlineAI] 5v5 Team Deathmatch Initialized: 1 Human + 4 Friendly Bots vs 5 Enemy Bots (Total 10 Operatives).\n";
}

} // namespace Mercenary

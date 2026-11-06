#include "World.hpp"
#include "GoreSystem.hpp"
#include "FrontlineAI.hpp"
#include "engine/Audio.hpp"
#include "engine/PhysicsEngine.hpp"
#include "engine/ProceduralMesh.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace Mercenary {


void World::initExtractionZones() {
    m_extractionZones.clear();
    // 1. North Allied Helipad Evac (Base Alpha)
    ExtractionZone z1;
    z1.name = "Helipad Alpha (FOB Evac)";
    z1.code = "EXFIL-01";
    z1.position = glm::vec3(35.0f, sampleTerrainHeight(35.0f, -225.0f), -225.0f);
    z1.radius = 9.5f;
    z1.countdown = 7.0f;
    z1.maxCountdown = 7.0f;
    z1.flareColor = glm::vec3(0.15f, 0.95f, 0.35f);
    m_extractionZones.push_back(z1);

    // 2. West River Pontoon Evac (Sector Charlie)
    ExtractionZone z2;
    z2.name = "Pontoon Boat (River Cross)";
    z2.code = "EXFIL-02";
    z2.position = glm::vec3(-170.0f, sampleTerrainHeight(-170.0f, 0.0f), 0.0f);
    z2.radius = 9.0f;
    z2.countdown = 7.0f;
    z2.maxCountdown = 7.0f;
    z2.flareColor = glm::vec3(0.20f, 0.85f, 0.95f);
    m_extractionZones.push_back(z2);

    // 3. Ruined Factory Bunker Evac (Sector Delta)
    ExtractionZone z3;
    z3.name = "Delta Sub-Bunker (Factory Evac)";
    z3.code = "EXFIL-03";
    z3.position = glm::vec3(180.0f, sampleTerrainHeight(180.0f, 120.0f), 120.0f);
    z3.radius = 8.5f;
    z3.countdown = 7.0f;
    z3.maxCountdown = 7.0f;
    z3.flareColor = glm::vec3(0.95f, 0.85f, 0.20f);
    m_extractionZones.push_back(z3);
}

void World::initTurrets() {
    m_turrets.clear();
    // Turret 1: Overlooking Central Highway Bridge from North Allied Trench
    MountedTurret t1;
    t1.id = 1;
    t1.position = glm::vec3(0.0f, sampleTerrainHeight(0.0f, -52.0f), -52.0f);
    t1.yaw = 0.0f; // Facing south towards bridge
    t1.ammo = 150;
    t1.maxAmmo = 150;
    t1.name = "DShK 12.7mm (Bridge Overlook)";
    m_turrets.push_back(t1);

    // Turret 2: Guarding West River Pontoon Crossing
    MountedTurret t2;
    t2.id = 2;
    t2.position = glm::vec3(-162.0f, sampleTerrainHeight(-162.0f, -14.0f), -14.0f);
    t2.yaw = 0.45f;
    t2.ammo = 150;
    t2.maxAmmo = 150;
    t2.name = "DShK 12.7mm (West River Bank)";
    m_turrets.push_back(t2);

    // Turret 3: Crimson Trench Pillbox guarding Southern Approach
    MountedTurret t3;
    t3.id = 3;
    t3.position = glm::vec3(12.0f, sampleTerrainHeight(12.0f, 52.0f), 52.0f);
    t3.yaw = 3.14159f; // Facing north towards river
    t3.ammo = 150;
    t3.maxAmmo = 150;
    t3.name = "DShK 12.7mm (Crimson Redoubt)";
    m_turrets.push_back(t3);
}

ExtractionZone* World::getActiveExtractZone(const glm::vec3& playerPos) {
    for (auto& zone : m_extractionZones) {
        if (!zone.isActive) continue;
        float dist = glm::distance(glm::vec2(playerPos.x, playerPos.z), glm::vec2(zone.position.x, zone.position.z));
        if (dist <= zone.radius) {
            return &zone;
        }
    }
    return nullptr;
}

MountedTurret* World::getNearbyTurret(const glm::vec3& pos, float radius) {
    MountedTurret* best = nullptr;
    float bestDist = radius;
    for (auto& turret : m_turrets) {
        float d = glm::distance(glm::vec2(pos.x, pos.z), glm::vec2(turret.position.x, turret.position.z));
        if (d < bestDist) {
            bestDist = d;
            best = &turret;
        }
    }
    return best;
}

bool World::saveMap(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out.is_open()) {
        std::cerr << "[World] Failed to open map file for saving: " << filepath << "\n";
        return false;
    }

    out << "# MERCENARY TACTICAL MAP FILE v1.0\n";
    out << "TIME_OF_DAY " << std::fixed << std::setprecision(2) << m_timeOfDay << "\n";
    out << "WEATHER " << static_cast<int>(m_weather) << "\n";
    if (s_flatCanvasMode) {
        out << "CANVAS_MODE flat\n";
    }
    out << "\n";
    if (!s_terrainDeltas.empty()) {
        out << "[TERRAIN_DELTAS]\n";
        out << "COUNT " << s_terrainDeltas.size() << "\n";
        for (const auto& [key, delta] : s_terrainDeltas) {
            int cx = (int)(key >> 32);
            int cz = (int)(key & 0xFFFFFFFFLL);
            out << "DELTA " << cx << " " << cz << " " << std::fixed << std::setprecision(3) << delta << "\n";
        }
        out << "\n";
    }

    // Sectors
    out << "[SECTORS]\n";
    out << "COUNT " << m_sectors.size() << "\n";
    for (const auto& sec : m_sectors) {
        out << "SECTOR " << sec.name << " | " << sec.code << " | "
            << sec.minCoord.x << " " << sec.minCoord.y << " "
            << sec.maxCoord.x << " " << sec.maxCoord.y << " | "
            << static_cast<int>(sec.controllingFaction) << "\n";
    }
    out << "\n";

    // Obstacles
    out << "[OBSTACLES]\n";
    out << "COUNT " << m_obstacles.size() << "\n";
    for (const auto& obs : m_obstacles) {
        out << "OBSTACLE " << obstacleTypeToString(obs.type) << " | "
            << obs.position.x << " " << obs.position.y << " " << obs.position.z << " | "
            << obs.size.x << " " << obs.size.y << " " << obs.size.z << " | "
            << obs.yaw << " " << obs.pitch << " " << obs.roll << " | "
            << obs.color.r << " " << obs.color.g << " " << obs.color.b << " " << obs.color.a << " | "
            << (obs.blocksLineOfSight ? 1 : 0) << " "
            << (obs.destructible ? 1 : 0) << " "
            << obs.health << "\n";
    }
    out << "\n";

    // Crates
    out << "[CRATES]\n";
    out << "COUNT " << m_crates.size() << "\n";
    for (const auto& crate : m_crates) {
        out << "CRATE " << crate.name << " | "
            << crate.position.x << " " << crate.position.y << " " << crate.position.z << " | "
            << crate.contents.size() << "\n";
        for (const auto& it : crate.contents) {
            out << "ITEM " << it.id << " " << it.quantity << " " << it.durability << "\n";
        }
    }
    out << "\n";

    // Targets
    out << "[TARGETS]\n";
    out << "COUNT " << m_targets.size() << "\n";
    for (const auto& tgt : m_targets) {
        out << "TARGET " << tgt.position.x << " " << tgt.position.y << " " << tgt.position.z << " | "
            << tgt.maxHealth << "\n";
    }
    out << "\n";

    // Craters
    out << "[CRATERS]\n";
    out << "COUNT " << m_craterPositions.size() << "\n";
    for (const auto& cp : m_craterPositions) {
        out << "CRATER " << cp.x << " " << cp.y << " " << cp.z << "\n";
    }

    bool success = out.good();
    out.close();
    if (!success) {
        std::cerr << "[World] Error writing to map file: " << filepath << "\n";
        return false;
    }
    std::cout << "[World] Saved map to " << filepath << " (" << m_obstacles.size() << " obstacles, "
              << m_crates.size() << " crates, " << m_sectors.size() << " sectors, "
              << m_craterPositions.size() << " craters)\n";
    return true;
}

bool World::loadMap(const std::string& filepath) {
    std::string actualPath = filepath;
    std::ifstream in(actualPath);
    if (!in.is_open()) {
        std::vector<std::string> candidates = {
            "../" + filepath,
            "../../" + filepath,
            "assets/maps/" + filepath,
            "../assets/maps/" + filepath,
            "ASSETS/maps/" + filepath,
            "../ASSETS/maps/" + filepath
        };
        for (const auto& cand : candidates) {
            in.open(cand);
            if (in.is_open()) {
                actualPath = cand;
                break;
            }
        }
    }
    if (!in.is_open()) {
        std::cerr << "[World] Failed to open map file: " << filepath << " (checked candidates)\n";
        return false;
    }

    std::vector<TacticalSector> tempSectors;
    std::vector<WorldObstacle> tempObstacles;
    std::vector<SupplyCrate> tempCrates;
    std::vector<TargetDummy> tempTargets;
    std::vector<glm::vec3> tempCraters;
    std::unordered_map<int64_t, float> tempTerrainDeltas;
    bool tempFlatCanvas = false;
    float tempTimeOfDay = m_timeOfDay;
    WeatherType tempWeather = m_weather;

    std::string line;
    std::string currentSection;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line.rfind("TIME_OF_DAY", 0) == 0) {
            std::istringstream iss(line.substr(11));
            iss >> tempTimeOfDay;
            continue;
        }
        if (line.rfind("WEATHER", 0) == 0) {
            int w = 0;
            std::istringstream iss(line.substr(7));
            if (iss >> w) {
                if (w < 0 || w > 3) w = 0;
                tempWeather = static_cast<WeatherType>(w);
            }
            continue;
        }
        if (line.rfind("CANVAS_MODE flat", 0) == 0) {
            tempFlatCanvas = true;
            continue;
        }

        if (line[0] == '[') {
            size_t closePos = line.find(']');
            if (closePos != std::string::npos) {
                currentSection = line.substr(1, closePos - 1);
            }
            continue;
        }

        if (currentSection == "SECTORS") {
            if (line.rfind("SECTOR ", 0) == 0) {
                std::string content = line.substr(7);
                size_t p1 = content.find('|');
                size_t p2 = content.find('|', p1 + 1);
                size_t p3 = content.find('|', p2 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
                    TacticalSector sec;
                    sec.name = content.substr(0, p1);
                    while (!sec.name.empty() && sec.name.back() == ' ') sec.name.pop_back();
                    sec.code = content.substr(p1 + 1, p2 - p1 - 1);
                    while (!sec.code.empty() && sec.code.front() == ' ') sec.code.erase(0, 1);
                    while (!sec.code.empty() && sec.code.back() == ' ') sec.code.pop_back();

                    std::istringstream coords(content.substr(p2 + 1, p3 - p2 - 1));
                    coords >> sec.minCoord.x >> sec.minCoord.y >> sec.maxCoord.x >> sec.maxCoord.y;

                    int fac = 0;
                    std::istringstream facStream(content.substr(p3 + 1));
                    facStream >> fac;
                    sec.controllingFaction = static_cast<Faction>(fac);
                    tempSectors.push_back(sec);
                }
            }
        } else if (currentSection == "OBSTACLES") {
            if (line.rfind("OBSTACLE ", 0) == 0) {
                std::string content = line.substr(9);
                size_t p1 = content.find('|');
                size_t p2 = content.find('|', p1 + 1);
                size_t p3 = content.find('|', p2 + 1);
                size_t p4 = content.find('|', p3 + 1);
                size_t p5 = content.find('|', p4 + 1);

                if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos &&
                    p4 != std::string::npos && p5 != std::string::npos) {
                    WorldObstacle obs;
                    std::string typeStr = content.substr(0, p1);
                    while (!typeStr.empty() && typeStr.back() == ' ') typeStr.pop_back();
                    obs.type = stringToObstacleType(typeStr);

                    std::istringstream posStream(content.substr(p1 + 1, p2 - p1 - 1));
                    posStream >> obs.position.x >> obs.position.y >> obs.position.z;

                    std::istringstream sizeStream(content.substr(p2 + 1, p3 - p2 - 1));
                    sizeStream >> obs.size.x >> obs.size.y >> obs.size.z;

                    std::istringstream yawStream(content.substr(p3 + 1, p4 - p3 - 1));
                    float y = 0.0f, p = 0.0f, r = 0.0f;
                    if (yawStream >> y) {
                        obs.yaw = y;
                        if (yawStream >> p >> r) {
                            obs.pitch = p;
                            obs.roll = r;
                        }
                    }

                    std::istringstream colStream(content.substr(p4 + 1, p5 - p4 - 1));
                    colStream >> obs.color.r >> obs.color.g >> obs.color.b >> obs.color.a;

                    int blos = 1, dest = 0;
                    std::istringstream flagStream(content.substr(p5 + 1));
                    flagStream >> blos >> dest >> obs.health;
                    obs.blocksLineOfSight = (blos != 0);
                    obs.destructible = (dest != 0);
                    if (obs.type == ObstacleType::StreetLamp || obs.type == ObstacleType::CommunicationTower) {
                        obs.blocksLineOfSight = true;
                        obs.destructible = true;
                        if (obs.health <= 0.0f) obs.health = 60.0f;
                    }

                    tempObstacles.push_back(obs);
                }
            }
        } else if (currentSection == "CRATES") {
            if (line.rfind("CRATE ", 0) == 0) {
                std::string content = line.substr(6);
                size_t p1 = content.find('|');
                size_t p2 = content.find('|', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    SupplyCrate crate;
                    crate.name = content.substr(0, p1);
                    while (!crate.name.empty() && crate.name.back() == ' ') crate.name.pop_back();

                    std::istringstream posStream(content.substr(p1 + 1, p2 - p1 - 1));
                    posStream >> crate.position.x >> crate.position.y >> crate.position.z;
                    crate.position.y = sampleTerrainHeight(crate.position.x, crate.position.z);

                    tempCrates.push_back(crate);
                }
            } else if (line.rfind("ITEM ", 0) == 0 && !tempCrates.empty()) {
                std::istringstream iss(line.substr(5));
                int id = 0, qty = 1;
                float cond = 100.0f;
                iss >> id >> qty >> cond;
                Item it;
                it.id = id;
                it.quantity = qty;
                it.durability = cond;
                tempCrates.back().contents.push_back(it);
            }
        } else if (currentSection == "TARGETS") {
            if (line.rfind("TARGET ", 0) == 0) {
                std::string content = line.substr(7);
                size_t p1 = content.find('|');
                TargetDummy tgt;
                if (p1 != std::string::npos) {
                    std::istringstream posStream(content.substr(0, p1));
                    posStream >> tgt.position.x >> tgt.position.y >> tgt.position.z;
                    std::istringstream hpStream(content.substr(p1 + 1));
                    hpStream >> tgt.maxHealth;
                    tgt.health = tgt.maxHealth;
                    tgt.isAlive = true;
                    tempTargets.push_back(tgt);
                }
            }
        } else if (currentSection == "CRATERS") {
            if (line.rfind("CRATER ", 0) == 0) {
                std::istringstream iss(line.substr(7));
                glm::vec3 cp;
                if (iss >> cp.x >> cp.y >> cp.z) {
                    tempCraters.push_back(cp);
                }
            }
        } else if (currentSection == "TERRAIN_DELTAS") {
            if (line.rfind("DELTA ", 0) == 0) {
                std::istringstream iss(line.substr(6));
                int cx = 0, cz = 0;
                float delta = 0.0f;
                if (iss >> cx >> cz >> delta) {
                    tempTerrainDeltas[terrainCellKey(cx, cz)] = delta;
                }
            }
        }
    }

    in.close();

    // Transactional commit: only replace current map if file read was valid
    clearMap();
    m_sectors = std::move(tempSectors);
    m_obstacles = std::move(tempObstacles);
    m_crates = std::move(tempCrates);
    m_targets = std::move(tempTargets);
    m_craterPositions = std::move(tempCraters);
    m_timeOfDay = tempTimeOfDay;
    setWeather(tempWeather);

    initExtractionZones();
    initTurrets();
    std::cout << "[World] Loaded map from " << filepath << " (" << m_obstacles.size() << " obstacles, "
              << m_crates.size() << " crates, " << m_sectors.size() << " sectors, "
              << m_craterPositions.size() << " craters)\n";
    return true;
}

World::World() {
    generateTeamDeathmatch();

    m_rainParticles.resize(600);
    std::mt19937 rnd(777);
    std::uniform_real_distribution<float> disXZ(-35.0f, 35.0f);
    std::uniform_real_distribution<float> disY(2.0f, 25.0f);
    std::uniform_real_distribution<float> disSpd(20.0f, 28.0f);
    std::uniform_real_distribution<float> disLen(0.50f, 0.85f);
    for (auto& p : m_rainParticles) {
        p.position = glm::vec3(disXZ(rnd), disY(rnd), disXZ(rnd));
        p.speed = disSpd(rnd);
        p.length = disLen(rnd);
    }
}

void World::update(float dt, Renderer& renderer, const glm::vec3& cameraPos) {
    // Update Jolt Physics Dynamic Severed Props
    for (auto it = m_severedProps.begin(); it != m_severedProps.end(); ) {
        it->lifetime -= dt;
        if (it->lifetime <= 0.0f) {
            PhysicsEngine::get().removeBody(it->joltBodyID);
            it = m_severedProps.erase(it);
        } else {
            PhysicsEngine::get().getBodyTransform(it->joltBodyID, it->position, it->rotation);
            ++it;
        }
    }

    // Prune empty looted ground items
    m_crates.erase(std::remove_if(m_crates.begin(), m_crates.end(),
        [](const SupplyCrate& c) { return c.isGroundItem && (c.looted || c.contents.empty()); }),
        m_crates.end());

    // 1. Target dummies respawn & animation decay
    for (auto& target : m_targets) {
        if (!target.isAlive) {
            target.respawnTimer += dt;
            if (target.respawnTimer >= 5.0f) {
                target.isAlive = true;
                target.health = target.maxHealth;
                target.respawnTimer = 0.0f;
                target.knockdownAngle = 0.0f;
            }
        } else {
            if (target.knockdownAngle > 0.0f) {
                target.knockdownAngle = std::max(0.0f, target.knockdownAngle - dt * 2.0f);
            }
        }
        if (target.hitFlashTimer > 0.0f) {
            target.hitFlashTimer = std::max(0.0f, target.hitFlashTimer - dt);
        }
    }

    // 2. Realistic 24-Hour Day/Night Cycle
    m_timeOfDay = std::fmod(m_timeOfDay + dt * m_timeSpeed, 24.0f);

    float sunProgress = (m_timeOfDay / 24.0f) * 2.0f * 3.14159265f - 1.5707963f;
    float sunHeight = -std::sin(sunProgress);
    glm::vec3 sunDir = glm::normalize(glm::vec3(std::cos(sunProgress) * 0.70f, sunHeight, std::sin(sunProgress * 0.45f) * 0.40f));

    glm::vec3 sunColor;
    glm::vec3 ambientColor;
    float diffuseIntensity = 0.85f;
    float ambientIntensity = 0.32f;

    if (m_timeOfDay >= 5.0f && m_timeOfDay < 7.5f) {
        // DAWN (Warm golden-rose low sun)
        float t = (m_timeOfDay - 5.0f) / 2.5f;
        sunColor = glm::mix(glm::vec3(1.0f, 0.45f, 0.25f), glm::vec3(1.0f, 0.88f, 0.68f), t);
        ambientColor = glm::mix(glm::vec3(0.18f, 0.18f, 0.28f), glm::vec3(0.24f, 0.28f, 0.36f), t);
        diffuseIntensity = glm::mix(0.55f, 0.85f, t);
    } else if (m_timeOfDay >= 7.5f && m_timeOfDay < 17.0f) {
        // DAYTIME (Crisp bright daylight)
        sunColor = glm::vec3(1.0f, 0.98f, 0.92f);
        ambientColor = glm::vec3(0.26f, 0.30f, 0.38f);
        diffuseIntensity = 0.90f;
    } else if (m_timeOfDay >= 17.0f && m_timeOfDay < 19.5f) {
        // DUSK / SUNSET (Golden hour into burning crimson dusk)
        float t = (m_timeOfDay - 17.0f) / 2.5f;
        sunColor = glm::mix(glm::vec3(1.0f, 0.75f, 0.45f), glm::vec3(0.95f, 0.35f, 0.18f), t);
        ambientColor = glm::mix(glm::vec3(0.25f, 0.26f, 0.34f), glm::vec3(0.16f, 0.15f, 0.24f), t);
        diffuseIntensity = glm::mix(0.85f, 0.45f, t);
    } else {
        // NIGHT (Cool tactical moonlight)
        sunDir = -sunDir;
        if (sunDir.y > -0.2f) sunDir.y = -0.6f;
        sunDir = glm::normalize(sunDir);
        sunColor = glm::vec3(0.18f, 0.25f, 0.42f);
        ambientColor = glm::vec3(0.06f, 0.08f, 0.12f);
        diffuseIntensity = 0.35f;
        ambientIntensity = 0.20f;
    }

    // Weather Rain Modulation
    if (m_rainIntensity > 0.05f) {
        sunColor *= (1.0f - m_rainIntensity * 0.42f);
        ambientColor = glm::mix(ambientColor, glm::vec3(0.15f, 0.18f, 0.22f), m_rainIntensity * 0.65f);
        diffuseIntensity *= (1.0f - m_rainIntensity * 0.35f);
    }

    renderer.getLight().direction = sunDir;
    renderer.getLight().color = sunColor;
    renderer.getLight().diffuseIntensity = diffuseIntensity;
    renderer.getLight().ambientIntensity = ambientIntensity;

    AudioEngine::get().setRainIntensity(m_rainIntensity);

    if (m_rainIntensity > 0.4f) {
        m_thunderTimer -= dt;
        if (m_thunderTimer <= 0.0f) {
            AudioEngine::get().playSound(SoundType::Thunder, 0.75f);
            m_thunderTimer = 16.0f + static_cast<float>(rand() % 24);
        }
    }

    for (auto& turret : m_turrets) {
        if (turret.fireCooldown > 0.0f) {
            turret.fireCooldown -= dt;
        }
    }

    if (m_rainIntensity > 0.05f) {
        for (auto& p : m_rainParticles) {
            p.position.y -= p.speed * dt;
            p.position.x += 2.2f * dt;
            p.position.z += 1.1f * dt;

            if (p.position.y <= 0.0f ||
                std::abs(p.position.x - cameraPos.x) > 35.0f ||
                std::abs(p.position.z - cameraPos.z) > 35.0f) {
                p.position.y = cameraPos.y + 12.0f + static_cast<float>(rand() % 100) * 0.12f;
                p.position.x = cameraPos.x + static_cast<float>(rand() % 700 - 350) * 0.1f;
                p.position.z = cameraPos.z + static_cast<float>(rand() % 700 - 350) * 0.1f;
            }
        }
    }
}

void World::render(Renderer& renderer, const glm::vec3& cameraPos) {
    if (m_mapType == MapType::TeamDeathmatch) {
        // 1. Tactical TDM Industrial Yard (Flat concrete arena surface at Y=0.0f, no tall black grass or river)
        renderer.drawArenaGround(140.0f);

        // 2. World Obstacles
        for (const auto& obs : m_obstacles) {
            renderer.drawObstacle(obs);
        }

        // 3. Interactive Military Arsenal Crates & Ground Items
        for (const auto& crate : m_crates) {
            if (crate.isGroundItem) {
                if (crate.looted || crate.contents.empty()) continue;
                if (!crate.contents.empty() && crate.contents[0].category == ItemCategory::Weapon) {
                    renderer.drawDroppedWeapon(crate.contents[0].name, crate.position, crate.yaw);
                } else if (!crate.contents.empty() && crate.contents[0].category == ItemCategory::Ammo) {
                    renderer.drawCube(crate.position + glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.24f, 0.14f, 0.16f), glm::vec4(0.28f, 0.32f, 0.22f, 1.0f), crate.yaw);
                } else {
                    renderer.drawCube(crate.position + glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.22f, 0.12f, 0.15f), glm::vec4(0.85f, 0.85f, 0.85f, 1.0f), crate.yaw);
                }
            } else {
                renderer.drawMilitaryCrate(crate.position, crate.yaw, crate.looted);
            }
        }

        // 4. Tactical Steel Reactive Shooting Range Targets
        for (const auto& target : m_targets) {
            renderShootingTarget(renderer, target);
        }

        // 5. Dynamic Jolt Severed Obstacles
        for (const auto& prop : m_severedProps) {
            if (prop.mesh) renderer.drawSeveredProp(*prop.mesh, prop.position, prop.rotation, prop.color);
        }
        return;
    } else if (m_mapType == MapType::TrainingBase) {
        // 1. Tactical Combat Arena / Parade Grounds
        renderer.drawGround(650.0f, true);
        renderer.drawGrassField(true);

        // 3. World Obstacles
        for (const auto& obs : m_obstacles) {
            renderer.drawObstacle(obs);
        }

        // 4. Interactive Military Arsenal Crates & Ground Items
        for (const auto& crate : m_crates) {
            if (crate.isGroundItem) {
                if (crate.looted || crate.contents.empty()) continue;
                if (!crate.contents.empty() && crate.contents[0].category == ItemCategory::Weapon) {
                    renderer.drawDroppedWeapon(crate.contents[0].name, crate.position, crate.yaw);
                } else if (!crate.contents.empty() && crate.contents[0].category == ItemCategory::Ammo) {
                    renderer.drawCube(crate.position + glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.24f, 0.14f, 0.16f), glm::vec4(0.28f, 0.32f, 0.22f, 1.0f), crate.yaw);
                } else {
                    renderer.drawCube(crate.position + glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.22f, 0.12f, 0.15f), glm::vec4(0.85f, 0.85f, 0.85f, 1.0f), crate.yaw);
                }
            } else {
                renderer.drawMilitaryCrate(crate.position, crate.yaw, crate.looted);
            }
        }

        // 5. Tactical Steel Reactive Shooting Range Targets
        for (const auto& target : m_targets) {
            renderShootingTarget(renderer, target);
        }

        // 6. Dynamic Jolt Severed Obstacles
        for (const auto& prop : m_severedProps) {
            if (prop.mesh) renderer.drawSeveredProp(*prop.mesh, prop.position, prop.rotation, prop.color);
        }
        return;
    }

    // 1. Vast 650m Frontline Terrain
    renderer.drawGround(650.0f);

    // 2. Dense Lush 3D Grass Field
    renderer.drawGrassField();

    // 3. River Siversky Water Surface
    renderer.drawRiverWater(glm::vec3(0.0f, -0.35f, 0.0f));

    // 4. World Obstacles & Frontline Defense Fortifications
    for (const auto& obs : m_obstacles) {
        renderer.drawObstacle(obs);
    }

    // 5. Military Lootable Supply Crates & Real 3D Dropped Ground Items
    for (const auto& crate : m_crates) {
        if (crate.isGroundItem) {
            if (crate.looted || crate.contents.empty()) continue;
            if (!crate.contents.empty() && crate.contents[0].category == ItemCategory::Weapon) {
                renderer.drawDroppedWeapon(crate.contents[0].name, crate.position, crate.yaw);
            } else if (!crate.contents.empty() && crate.contents[0].category == ItemCategory::Ammo) {
                renderer.drawCube(crate.position + glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.24f, 0.14f, 0.16f), glm::vec4(0.28f, 0.32f, 0.22f, 1.0f), crate.yaw);
            } else {
                renderer.drawCube(crate.position + glm::vec3(0.0f, 0.08f, 0.0f), glm::vec3(0.22f, 0.12f, 0.15f), glm::vec4(0.85f, 0.85f, 0.85f, 1.0f), crate.yaw);
            }
        } else {
            renderer.drawMilitaryCrate(crate.position, crate.yaw, crate.looted);
        }
    }

    // 6. Extraction Zone Smoke Flares
    for (const auto& zone : m_extractionZones) {
        if (!zone.isActive) continue;
        WorldObstacle flareObs;
        flareObs.position = zone.position;
        flareObs.size = glm::vec3(0.35f, 0.85f, 0.35f);
        flareObs.type = ObstacleType::SignalFlare;
        flareObs.color = glm::vec4(zone.flareColor, 1.0f);
        renderer.drawObstacle(flareObs);
    }

    // 7. Target Dummies
    for (const auto& target : m_targets) {
        renderShootingTarget(renderer, target);
    }

    // 7b. Dynamic Jolt Severed Obstacles
    for (const auto& prop : m_severedProps) {
        if (prop.mesh) renderer.drawSeveredProp(*prop.mesh, prop.position, prop.rotation, prop.color);
    }

    // 8. Dynamic Falling Rain Streaks
    if (m_rainIntensity > 0.05f && m_weatherParticlesEnabled) {
        glm::vec4 rainColor(0.70f, 0.78f, 0.88f, 0.32f * m_rainIntensity);
        glm::vec3 rainSlope(0.06f, -1.0f, 0.03f);
        for (const auto& p : m_rainParticles) {
            renderer.drawLine(p.position, p.position + rainSlope * p.length, rainColor);
        }
    }
}

float World::getWalkableSurfaceHeight(const glm::vec3& pos, float radius) const {
    float surfaceY = sampleTerrainHeight(pos.x, pos.z);

    // 1. Check solid world obstacles (crates, sandbags, bunkers, bridges, shipping containers, pallets)
    for (const auto& obs : m_obstacles) {
        if (obs.type == ObstacleType::GrassClump || obs.type == ObstacleType::BushFoliage ||
            obs.type == ObstacleType::CampfireSmoke || obs.type == ObstacleType::SignalFlare ||
            obs.type == ObstacleType::ShootingTarget || obs.type == ObstacleType::PineTree) {
            continue;
        }

        if (obs.type == ObstacleType::RiverBridge) {
            float hx = obs.size.x * 0.5f;
            float hz = obs.size.z * 0.5f;
            if (std::abs(pos.x - obs.position.x) <= (hx + radius * 0.5f) &&
                std::abs(pos.z - obs.position.z) <= (hz + radius * 0.5f)) {
                surfaceY = std::max(surfaceY, obs.position.y + 0.20f);
            }
            continue;
        }

        glm::vec3 relPos = pos - obs.position;
        if (std::abs(obs.yaw) > 0.001f) {
            float c = std::cos(-obs.yaw);
            float s = std::sin(-obs.yaw);
            relPos = glm::vec3(c * relPos.x - s * relPos.z, relPos.y, s * relPos.x + c * relPos.z);
        }

        glm::vec3 half = obs.size * 0.5f;
        // Check if player footprint is within top boundary
        if (std::abs(relPos.x) <= (half.x + radius * 0.5f) &&
            std::abs(relPos.z) <= (half.z + radius * 0.5f)) {
            float topY = obs.position.y + obs.size.y;
            // Adopt top surface if player is near top (within step-up tolerance) or above
            if (pos.y >= (topY - 0.36f)) {
                surfaceY = std::max(surfaceY, topY);
            }
        }
    }

    // 2. Check supply crates
    for (const auto& crate : m_crates) {
        float hx = 0.65f;
        float hz = 0.45f;
        float topY = crate.position.y + 0.70f;
        if (std::abs(pos.x - crate.position.x) <= (hx + radius * 0.5f) &&
            std::abs(pos.z - crate.position.z) <= (hz + radius * 0.5f)) {
            if (pos.y >= (topY - 0.36f)) {
                surfaceY = std::max(surfaceY, topY);
            }
        }
    }

    return surfaceY;
}

bool World::checkCollision(glm::vec3& pos, float radius) const {
    bool collided = false;
    // Map outer boundaries: Support both Training Base and MMO Continental Theater (2600m x 3800m)
    if (m_mapType == MapType::TrainingBase) {
        pos.x = std::clamp(pos.x, -110.0f, 260.0f);
        pos.z = std::clamp(pos.z, -140.0f, 115.0f);
    } else {
        pos.x = std::clamp(pos.x, -1280.0f, 1280.0f);
        pos.z = std::clamp(pos.z, -1880.0f, 1880.0f);
    }

    // Ground player Y on walkable surface (terrain, bridge, crate, or barricade roof)
    float surfaceY = getWalkableSurfaceHeight(pos, radius);
    if (pos.y < surfaceY) {
        pos.y = surfaceY;
    }

    for (const auto& obs : m_obstacles) {
        if (obs.type == ObstacleType::PineTree) {
            float trunkR = 0.36f;
            float dx = pos.x - obs.position.x;
            float dz = pos.z - obs.position.z;
            float distSq = dx * dx + dz * dz;
            float minD = radius + trunkR;
            if (distSq < minD * minD && distSq > 0.00001f) {
                float dist = std::sqrt(distSq);
                float push = minD - dist;
                pos.x += (dx / dist) * push;
                pos.z += (dz / dist) * push;
                collided = true;
            }
            continue;
        }

        if (obs.type == ObstacleType::RiverBridge ||
            obs.type == ObstacleType::GrassClump || obs.type == ObstacleType::BushFoliage ||
            obs.type == ObstacleType::CampfireSmoke || obs.type == ObstacleType::SignalFlare ||
            obs.type == ObstacleType::ShootingTarget) {
            continue; // Passable foliage/atmosphere/targets
        }

        glm::vec3 relPos = pos - obs.position;
        if (std::abs(obs.yaw) > 0.001f) {
            float c = std::cos(-obs.yaw);
            float s = std::sin(-obs.yaw);
            relPos = glm::vec3(c * relPos.x - s * relPos.z, relPos.y, s * relPos.x + c * relPos.z);
        }

        // Realistic Step-Over Mantling (curbs, fallen planks, pallets <= 0.35m)
        if (obs.size.y <= 0.35f && relPos.y >= obs.size.y - 0.35f) {
            pos.y = std::max(pos.y, obs.position.y + obs.size.y);
            continue; // Smoothly step onto/over low object without blocking horizontal velocity
        }

        // RuinedWall window aperture pass-through (when player jumps/mantles above 1.15m sill)
        if (obs.type == ObstacleType::RuinedWall && std::abs(relPos.x) < 1.4f && pos.y >= obs.position.y + 1.15f) {
            pos.y = std::max(pos.y, obs.position.y + 1.20f);
            continue; // Moving through open window opening
        }

        // If player is on top of or above this obstacle, don't collide horizontally with its side walls!
        float topFaceY = obs.position.y + obs.size.y;
        if (pos.y >= (topFaceY - 0.08f)) {
            continue; // Standing on top or airborne over obstacle
        }

        glm::vec3 half = obs.size * 0.5f;

        // 2D Capsule-to-Box penetration with smooth rounded corner sliding
        float clampedX = std::clamp(relPos.x, -half.x, half.x);
        float clampedZ = std::clamp(relPos.z, -half.z, half.z);
        float dx = relPos.x - clampedX;
        float dz = relPos.z - clampedZ;
        float distSq = dx * dx + dz * dz;

        if (distSq < (radius * radius) && relPos.y >= -0.5f && relPos.y <= obs.size.y + 0.5f) {
            if (distSq > 0.00001f) {
                // Outside box in corner zone: push radially out to rounded capsule surface
                float dist = std::sqrt(distSq);
                float push = radius - dist;
                relPos.x += (dx / dist) * push;
                relPos.z += (dz / dist) * push;
            } else {
                // Deep inside box: push out along minimal penetration axis
                float overlapX = (half.x + radius) - std::abs(relPos.x);
                float overlapZ = (half.z + radius) - std::abs(relPos.z);
                if (overlapX < overlapZ) {
                    relPos.x += (relPos.x > 0.0f ? overlapX : -overlapX);
                } else {
                    relPos.z += (relPos.z > 0.0f ? overlapZ : -overlapZ);
                }
            }

            if (std::abs(obs.yaw) > 0.001f) {
                float c = std::cos(obs.yaw);
                float s = std::sin(obs.yaw);
                pos.x = obs.position.x + (c * relPos.x - s * relPos.z);
                pos.z = obs.position.z + (s * relPos.x + c * relPos.z);
            } else {
                pos.x = obs.position.x + relPos.x;
                pos.z = obs.position.z + relPos.z;
            }
            collided = true;
        }
    }
    return collided;
}

bool World::raycast(const glm::vec3& start, const glm::vec3& end, glm::vec3& hitPoint, glm::vec3& hitNormal, WorldObstacle*& outHitObstacle) {
    float nearestDist = 1e9f;
    bool hit = false;
    glm::vec3 dir = end - start;
    float dist = glm::length(dir);
    if (dist < 0.0001f) return false;
    glm::vec3 rayDir = dir / dist;

    for (auto& obs : m_obstacles) {
        if (!obs.blocksLineOfSight) continue;

        glm::vec3 localRayStart;
        glm::vec3 localRayDir;
        bool hasYaw = (std::abs(obs.yaw) > 0.001f);

        if (hasYaw) {
            float cosY = std::cos(-obs.yaw);
            float sinY = std::sin(-obs.yaw);
            glm::vec3 relStart = start - obs.position;
            localRayStart = glm::vec3(cosY * relStart.x - sinY * relStart.z, relStart.y, sinY * relStart.x + cosY * relStart.z);
            localRayDir   = glm::vec3(cosY * rayDir.x - sinY * rayDir.z, rayDir.y, sinY * rayDir.x + cosY * rayDir.z);
        } else {
            localRayStart = start - obs.position;
            localRayDir   = rayDir;
        }

        // Local AABB intersection helper
        auto intersectBox = [&](const glm::vec3& minB, const glm::vec3& maxB, float& tOut, glm::vec3& nOut) -> bool {
            float tmin = -1e9f;
            float tmax =  1e9f;
            glm::vec3 normalCandidate(0.0f);

            // X slab
            if (std::abs(localRayDir.x) > 0.00001f) {
                float t1 = (minB.x - localRayStart.x) / localRayDir.x;
                float t2 = (maxB.x - localRayStart.x) / localRayDir.x;
                glm::vec3 n1(-1.0f, 0.0f, 0.0f);
                glm::vec3 n2( 1.0f, 0.0f, 0.0f);
                if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
                if (t1 > tmin) { tmin = t1; normalCandidate = n1; }
                if (t2 < tmax) { tmax = t2; }
                if (tmin > tmax) return false;
            } else {
                if (localRayStart.x < minB.x || localRayStart.x > maxB.x) return false;
            }

            // Y slab
            if (std::abs(localRayDir.y) > 0.00001f) {
                float t1 = (minB.y - localRayStart.y) / localRayDir.y;
                float t2 = (maxB.y - localRayStart.y) / localRayDir.y;
                glm::vec3 n1(0.0f, -1.0f, 0.0f);
                glm::vec3 n2(0.0f,  1.0f, 0.0f);
                if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
                if (t1 > tmin) { tmin = t1; normalCandidate = n1; }
                if (t2 < tmax) { tmax = t2; }
                if (tmin > tmax) return false;
            } else {
                if (localRayStart.y < minB.y || localRayStart.y > maxB.y) return false;
            }

            // Z slab
            if (std::abs(localRayDir.z) > 0.00001f) {
                float t1 = (minB.z - localRayStart.z) / localRayDir.z;
                float t2 = (maxB.z - localRayStart.z) / localRayDir.z;
                glm::vec3 n1(0.0f, 0.0f, -1.0f);
                glm::vec3 n2(0.0f, 0.0f,  1.0f);
                if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
                if (t1 > tmin) { tmin = t1; normalCandidate = n1; }
                if (t2 < tmax) { tmax = t2; }
                if (tmin > tmax) return false;
            } else {
                if (localRayStart.z < minB.z || localRayStart.z > maxB.z) return false;
            }

            if (tmax < 0.0f) return false;
            float tHit = (tmin > 0.0f) ? tmin : tmax;
            if (tHit > dist || tHit < 0.0f) return false;
            tOut = tHit;
            nOut = normalCandidate;
            return true;
        };

        float obsTHit = 1e9f;
        glm::vec3 obsNormal(0.0f);
        bool obsHitFound = false;

        if (obs.type == ObstacleType::RuinedWall) {
            // Compound 4-sub-part collision with open window aperture
            // Procedural RuinedWall geometry layout:
            // 1. Left Pillar:   center {-2.2, 1.6, 0.0}, size {1.4, 3.2, 0.45} -> X[-2.9, -1.5], Y[0.0, 3.2], Z[-0.25, 0.25]
            // 2. Right Pillar:  center { 2.2, 1.4, 0.0}, size {1.4, 2.8, 0.45} -> X[ 1.5,  2.9], Y[0.0, 2.8], Z[-0.25, 0.25]
            // 3. Lower Sill:    center { 0.0, 0.6, 0.0}, size {3.0, 1.2, 0.45} -> X[-1.5,  1.5], Y[0.0, 1.20], Z[-0.25, 0.25]
            // 4. Upper Lintel:  center { 0.0, 2.4, 0.0}, size {3.0, 0.3, 0.50} -> X[-1.5,  1.5], Y[2.25, 2.55], Z[-0.25, 0.25]
            // Note: Window Opening X in [-1.5, 1.5], Y in [1.20, 2.25] has NO collision - bullets and sight pass cleanly through!
            struct SubBox { glm::vec3 minB; glm::vec3 maxB; };
            SubBox subBoxes[4] = {
                { glm::vec3(-2.9f, 0.0f, -0.25f), glm::vec3(-1.5f, 3.20f, 0.25f) },
                { glm::vec3( 1.5f, 0.0f, -0.25f), glm::vec3( 2.9f, 2.80f, 0.25f) },
                { glm::vec3(-1.5f, 0.0f, -0.25f), glm::vec3( 1.5f, 1.20f, 0.25f) },
                { glm::vec3(-1.5f, 2.25f, -0.25f), glm::vec3( 1.5f, 2.55f, 0.25f) }
            };
            for (const auto& sb : subBoxes) {
                float tSub;
                glm::vec3 nSub;
                if (intersectBox(sb.minB, sb.maxB, tSub, nSub)) {
                    if (tSub < obsTHit) {
                        obsTHit = tSub;
                        obsNormal = nSub;
                        obsHitFound = true;
                    }
                }
            }
        } else {
            glm::vec3 halfSize = obs.size * 0.5f;
            glm::vec3 minB(-halfSize.x, 0.0f, -halfSize.z);
            glm::vec3 maxB( halfSize.x, obs.size.y, halfSize.z);
            float tSub;
            glm::vec3 nSub;
            if (intersectBox(minB, maxB, tSub, nSub)) {
                obsTHit = tSub;
                obsNormal = nSub;
                obsHitFound = true;
            }
        }

        if (obsHitFound && obsTHit < nearestDist) {
            nearestDist = obsTHit;
            hit = true;
            hitPoint = start + rayDir * obsTHit;
            if (hasYaw) {
                float cosY = std::cos(obs.yaw);
                float sinY = std::sin(obs.yaw);
                hitNormal = glm::normalize(glm::vec3(cosY * obsNormal.x - sinY * obsNormal.z, obsNormal.y, sinY * obsNormal.x + cosY * obsNormal.z));
            } else {
                hitNormal = obsNormal;
            }
            outHitObstacle = &obs;
        }
    }
    return hit;
}

bool World::hasLineOfSight(const glm::vec3& from, const glm::vec3& to) const {
    glm::vec3 hitPoint, hitNormal;
    WorldObstacle* hitObs = nullptr;
    return !const_cast<World*>(this)->raycast(from, to, hitPoint, hitNormal, hitObs);
}

} // namespace Mercenary

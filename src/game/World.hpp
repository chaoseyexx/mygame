#pragma once
#include "engine/Types.hpp"
#include "engine/Renderer.hpp"
#include "Inventory.hpp"
#include <vector>
#include <string>


namespace Mercenary {

struct FrontlineBot;

enum class SculptBrushMode {
    Raise,
    Lower,
    Flatten,
    Smooth
};

inline int64_t terrainCellKey(int cx, int cz) {
    return ((int64_t)cx << 32) | ((int64_t)cz & 0xFFFFFFFFLL);
}

enum class ObstacleType {
    Sandbag,
    ConcreteBunker,
    WoodenCrate,
    RadioTower,
    BarbedWire,
    PineTree,
    DragonTooth,
    TankWreck,
    RuinedWall,
    RiverBridge,
    HescoBastion,
    CommandTent,
    FuelDrum,
    CargoPallet,
    FieldGenerator,
    MilitaryTruck,
    DamagedBuilding,
    GrassClump,
    BushFoliage,
    CampfireSmoke,
    SignalFlare,
    ConcreteBarrier,
    MountedHMG,
    UrbanApartment,
    IndustrialWarehouse,
    TownChurch,
    FreightBoxcar,
    StreetLamp,
    CommunicationTower,
    RefineryTank,
    // New Tactical & Training Assets
    ShootingTarget,
    Watchtower,
    GuardBooth,
    PerimeterFence,
    GunRack,
    ObstacleWall
};

struct ExtractionZone {
    std::string name = "EXFIL ALPHA";
    std::string code = "EXFIL-01";
    glm::vec3 position{0.0f};
    float radius = 9.5f;
    bool isActive = true;
    float countdown = 7.0f;
    float maxCountdown = 7.0f;
    bool isPlayerInside = false;
    glm::vec3 flareColor{0.15f, 0.95f, 0.35f};
};

struct MountedTurret {
    uint32_t id = 0;
    glm::vec3 position{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    bool isOccupied = false;
    int ammo = 150;
    int maxAmmo = 150;
    float fireCooldown = 0.0f;
    std::string name = "DShK 12.7mm HMG";
};

struct WorldObstacle {
    glm::vec3 position;
    glm::vec3 size;
    ObstacleType type;
    glm::vec4 color{1.0f};
    bool blocksLineOfSight = true;
    bool destructible = false;
    float health = 100.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    bool isSeveredStump = false;
    std::shared_ptr<Mesh> customMesh;
};

struct SeveredProp {
    uint32_t joltBodyID = 0;
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    float radius = 0.10f;
    float halfHeight = 1.0f;
    bool hasLampHead = false;
    glm::vec4 color{0.24f, 0.26f, 0.28f, 1.0f};
    std::shared_ptr<Mesh> mesh;
    float lifetime = 45.0f;
};

struct SupplyCrate {
    glm::vec3 position;
    std::string name;
    std::vector<Item> contents;
    bool looted = false;
    bool isGroundItem = false;
    float yaw = 0.0f;
    uint32_t lootId = 0;
};

struct TargetDummy {
    glm::vec3 position;
    float health = 100.0f;
    float maxHealth = 100.0f;
    bool isAlive = true;
    float respawnTimer = 0.0f;
    float distanceMeters = 0.0f;
    float knockdownAngle = 0.0f;
    float hitFlashTimer = 0.0f;
    std::string laneLabel = "TARGET";
};

enum class MapType {
    TeamDeathmatch = 0,
    TrainingBase = 1,
    Warzone = 0
};

struct TacticalSector {
    std::string name;
    std::string code;
    glm::vec2 minCoord;
    glm::vec2 maxCoord;
    Faction controllingFaction;
    float captureProgress = 1.0f; // 1.0 = fully controlled
    Faction capturingFaction = Faction::FactionA;
    int friendlyCount = 0;
    int enemyCount = 0;
};

struct ArtilleryStrike {
    glm::vec3 targetPosition{0.0f};
    float countdown = 1.35f;
    bool exploded = false;
    float blastRadius = 8.5f;
    float damage = 75.0f;
};

enum class WeatherType {
    Clear,
    DawnMist,
    OvercastRain,
    HeavyThunderstorm
};

struct RainParticle {
    glm::vec3 position;
    float speed;
    float length;
};

class World {
public:
    World();

    void generateTeamDeathmatch();
    void generateWarzone();
    void generateMilitaryBase();
    void severObstacle(WorldObstacle& obs, const glm::vec3& hitPoint, const glm::vec3& bulletVelocity);
    std::vector<SeveredProp>& getSeveredProps() { return m_severedProps; }
    const std::vector<SeveredProp>& getSeveredProps() const { return m_severedProps; }
    MapType getMapType() const { return m_mapType; }
    void setMapType(MapType type);
    static bool isTrainingMapActive();
    static void setTrainingMapActive(bool active);
    void replenishTrainingCrate(SupplyCrate& crate);
    void renderShootingTarget(Renderer& renderer, const TargetDummy& target);
    void update(float dt, Renderer& renderer, const glm::vec3& cameraPos);
    void render(Renderer& renderer, const glm::vec3& cameraPos);

    bool checkCollision(glm::vec3& pos, float radius) const;
    bool raycast(const glm::vec3& start, const glm::vec3& end, glm::vec3& hitPoint, glm::vec3& hitNormal, WorldObstacle*& outHitObstacle);
    bool hasLineOfSight(const glm::vec3& from, const glm::vec3& to) const;

    SupplyCrate* getNearbyCrate(const glm::vec3& playerPos, float interactRadius = 2.5f);
    std::vector<SupplyCrate>& getCrates() { return m_crates; }
    const std::vector<SupplyCrate>& getCrates() const { return m_crates; }
    std::vector<TargetDummy>& getTargets() { return m_targets; }
    const std::vector<TargetDummy>& getTargets() const { return m_targets; }
    std::vector<WorldObstacle>& getObstacles() { return m_obstacles; }
    const std::vector<WorldObstacle>& getObstacles() const { return m_obstacles; }
    std::vector<TacticalSector>& getSectors() { return m_sectors; }
    const std::vector<TacticalSector>& getSectors() const { return m_sectors; }

    std::string getCurrentSectorName(const glm::vec3& pos) const;
    TacticalSector* getSectorAt(const glm::vec3& pos);
    const TacticalSector* getSectorAt(const glm::vec3& pos) const;
    void updateSectors(float dt, const glm::vec3& playerPos, Faction playerFaction, const std::vector<FrontlineBot>* bots = nullptr);

    void initExtractionZones();
    void initTurrets();

    std::vector<ExtractionZone>& getExtractionZones() { return m_extractionZones; }
    const std::vector<ExtractionZone>& getExtractionZones() const { return m_extractionZones; }
    ExtractionZone* getActiveExtractZone(const glm::vec3& playerPos);

    std::vector<MountedTurret>& getTurrets() { return m_turrets; }
    const std::vector<MountedTurret>& getTurrets() const { return m_turrets; }
    MountedTurret* getNearbyTurret(const glm::vec3& pos, float radius = 2.2f);

    GroundSurface getGroundSurface(const glm::vec3& pos) const;
    bool isPositionInRiver(const glm::vec3& pos) const;

    // Environmental Acoustic Simulation & Reverb Zones
    AcousticEnvironment detectAcousticEnvironment(const glm::vec3& pos) const;
    float evaluateAcousticOcclusion(const glm::vec3& soundPos, const glm::vec3& listenerPos) const;

    // Foxhole Combat Engineering: Sandbag Barricade Deployment
    bool deploySandbagBarricade(const glm::vec3& playerPos, float yaw);

    // Company of Heroes / Foxhole Directional Cover & Battlefield Craters
    CoverType evaluateCoverAt(const glm::vec3& position, const glm::vec3& incomingThreatOrigin) const;
    void createCraterAt(const glm::vec3& position, float radius = 4.5f, float depth = 0.95f);
    const std::vector<glm::vec3>& getCraters() const { return m_craterPositions; }

    // Frontline Artillery & Mortar Barrage
    void triggerArtilleryStrike(const glm::vec3& targetPos);
    void updateArtillery(float dt, const glm::vec3& playerPos, float& outPlayerDamage, float cameraYawRad = 0.0f);

    // Map File Serialization & Level Editing
    bool saveMap(const std::string& filepath) const;
    bool loadMap(const std::string& filepath);
    void clearMap();

    static float sampleTerrainHeight(float x, float z);
    static float getBaseTerrainHeight(float x, float z);
    static float getCustomTerrainHeightDelta(float x, float z);
    static void sculptTerrain(const glm::vec3& center, float radius, float strength, SculptBrushMode mode, float targetHeight, float dt);
    static void clearTerrainDeltas();
    static void setFlatCanvasMode(bool flat) { s_flatCanvasMode = flat; }
    static bool isFlatCanvasMode() { return s_flatCanvasMode; }
    static const std::unordered_map<int64_t, float>& getTerrainDeltas() { return s_terrainDeltas; }
    static void setTerrainDeltas(const std::unordered_map<int64_t, float>& deltas) { s_terrainDeltas = deltas; }
    static std::string obstacleTypeToString(ObstacleType type);
    static ObstacleType stringToObstacleType(const std::string& str);

    float getTimeOfDay() const { return m_timeOfDay; }
    void setTimeOfDay(float time) { m_timeOfDay = std::fmod(time, 24.0f); }
    WeatherType getWeather() const { return m_weather; }
    void setWeather(WeatherType weather);
    float getRainIntensity() const { return m_rainIntensity; }
    void setWeatherParticlesEnabled(bool enabled) { m_weatherParticlesEnabled = enabled; }
    bool isWeatherParticlesEnabled() const { return m_weatherParticlesEnabled; }

private:
    std::vector<WorldObstacle> m_obstacles;
    std::vector<SupplyCrate> m_crates;
    std::vector<TargetDummy> m_targets;
    std::vector<TacticalSector> m_sectors;
    std::vector<ArtilleryStrike> m_artilleryStrikes;
    std::vector<glm::vec3> m_craterPositions;
    float m_ambientArtilleryTimer = 35.0f;

    // Dynamic Time-of-Day and Atmospheric Weather
    float m_timeOfDay = 6.75f; // 06:45 Dawn
    float m_timeSpeed = 0.05f; // Continuous cycle
    WeatherType m_weather = WeatherType::Clear;
    float m_rainIntensity = 0.0f;
    bool m_weatherParticlesEnabled = true;
    float m_thunderTimer = 14.0f;
    std::vector<RainParticle> m_rainParticles;
    std::vector<ExtractionZone> m_extractionZones;
    std::vector<MountedTurret> m_turrets;
    std::vector<SeveredProp> m_severedProps;
    MapType m_mapType = MapType::TeamDeathmatch;
    static bool s_isTrainingMapActive;
    static std::unordered_map<int64_t, float> s_terrainDeltas;
    static bool s_flatCanvasMode;
};

} // namespace Mercenary

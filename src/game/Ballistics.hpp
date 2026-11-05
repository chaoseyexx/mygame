#pragma once
#include "engine/Types.hpp"
#include "engine/Renderer.hpp"
#include <vector>

namespace Mercenary {

class World;

struct Bullet {
    glm::vec3 position{0.0f};
    glm::vec3 prevPosition{0.0f};
    glm::vec3 origin{0.0f};
    glm::vec3 velocity{0.0f};
    float damage = 35.0f;
    float penetrationPower = 1.0f;
    float lifetime = 0.0f;
    float maxLifetime = 3.0f;
    float maxRange = 150.0f;
    float distanceTraveled = 0.0f;
    float caliberSize = 0.08f;
    float dragCoeff = 0.000048f;
    bool isTracer = true;
    glm::vec4 tracerColor{1.0f, 0.90f, 0.35f, 1.0f};
    bool flybyTriggered = false;
    bool isPlayer = true;
    Faction faction = Faction::None;
    int shooterId = -1;
    int weaponType = 0;
    bool alive = true;
};

struct ShellCasing {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 angularVelocity{0.0f};
    float radius = 0.010f;
    float length = 0.045f;
    float life = 0.0f;
    float maxLife = 14.0f;
    int bounces = 0;
    bool atRest = false;
    bool isHeavy = false;
};

struct SmokeParticle {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec4 color{0.82f, 0.82f, 0.82f, 0.60f};
    float size = 0.06f;
    float growthRate = 0.38f;
    float life = 0.0f;
    float maxLife = 0.85f;
    float rotation = 0.0f;
    float rotSpeed = 0.0f;
    int particleType = 0; // 0: Gunsmoke, 1: Sparks, 2: Ground Dust, 3: Blood Mist
};

struct MuzzleFlashFX {
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, 1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float life = 0.0f;
    float maxLife = 0.052f;
    float scale = 1.0f;
    int weaponType = 0; // 0: Rifle, 1: Sniper, 2: Pistol, 3: Heavy HMG
    float randomAngle = 0.0f;
    float seed = 0.0f;
};

struct TacticalGrenade {
    glm::vec3 position{0.0f};
    glm::vec3 prevPosition{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 angularVelocity{0.0f};
    float fuseTimer = 3.0f;
    float maxFuse = 3.0f;
    bool isSmoke = false;
    bool isDetonated = false;
    bool isExpired = false;
    float effectTimer = 0.0f;
    Faction faction = Faction::None;
    int throwerId = -1;
    bool isPlayer = true;
    int bounces = 0;
};

struct ExplosionEvent {
    glm::vec3 position{0.0f};
    float blastRadius = 7.5f;
    float damage = 125.0f;
    bool isSmoke = false;
    bool isPlayer = true;
    Faction faction = Faction::None;
    int throwerId = -1;
};

class BallisticsSystem {
public:
    BallisticsSystem();

    void fireBullet(const glm::vec3& origin, const glm::vec3& direction, float speedMs, float damage,
                    float penetration, bool isPlayer, Faction faction = Faction::None,
                    int shooterId = -1, float maxRange = 150.0f, int weaponType = 0, int forceTracer = -1);

    void spawnShellCasing(const glm::vec3& origin, const glm::vec3& rightDir, const glm::vec3& upDir,
                          const glm::vec3& aimDir, int weaponType = 0);

    void spawnMuzzleFlash(const glm::vec3& pos, const glm::vec3& forwardDir, float scale = 1.0f, int weaponType = 0);

    void spawnMuzzleSmoke(const glm::vec3& pos, const glm::vec3& forwardDir, float intensity = 1.0f);

    void spawnImpactFX(const glm::vec3& pos, const glm::vec3& normal, int surfaceType = 0);

    void spawnImpactSparks(const glm::vec3& pos, const glm::vec3& normal);

    void throwGrenade(const glm::vec3& origin, const glm::vec3& velocity, bool isSmoke, bool isPlayer = true, Faction faction = Faction::None, int throwerId = -1);
    void spawnExplosionFX(const glm::vec3& pos, float blastRadius = 7.5f);
    void spawnSmokeCloudFX(const glm::vec3& pos, float radius = 5.5f);

    void update(float dt, const glm::vec3& listenerPos = glm::vec3(0.0f), float cameraYaw = 0.0f, const World* world = nullptr);
    void render(Renderer& renderer);

    std::vector<Bullet>& getBullets() { return m_bullets; }
    const std::vector<ShellCasing>& getCasings() const { return m_casings; }
    std::vector<TacticalGrenade>& getGrenades() { return m_grenades; }
    const std::vector<TacticalGrenade>& getGrenades() const { return m_grenades; }
    std::vector<ExplosionEvent>& getPendingExplosions() { return m_pendingExplosions; }
    const std::vector<ExplosionEvent>& getPendingExplosions() const { return m_pendingExplosions; }
    void clearPendingExplosions() { m_pendingExplosions.clear(); }
    int getActiveBulletCount() const {
        int count = 0;
        for (const auto& b : m_bullets) if (b.alive) count++;
        return count;
    }

private:
    std::vector<Bullet> m_bullets;
    std::vector<ShellCasing> m_casings;
    std::vector<TacticalGrenade> m_grenades;
    std::vector<ExplosionEvent> m_pendingExplosions;
    std::vector<SmokeParticle> m_particles;
    std::vector<MuzzleFlashFX> m_muzzleFlashes;
    uint32_t m_shotCounter = 0;
};

} // namespace Mercenary

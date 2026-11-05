#include "Ballistics.hpp"
#include "World.hpp"
#include "PunctureSystem.hpp"
#include "DecalSystem.hpp"
#include "engine/Audio.hpp"
#include <random>
#include <algorithm>
#include <cmath>

namespace Mercenary {

BallisticsSystem::BallisticsSystem() {}

void BallisticsSystem::fireBullet(const glm::vec3& origin, const glm::vec3& direction, float speedMs, float damage,
                                  float penetration, bool isPlayer, Faction faction, int shooterId, float maxRange, int weaponType, int forceTracer) {
    Bullet b;
    b.position = origin;
    b.prevPosition = origin;
    b.origin = origin;

    // Determine tracer status:
    // Authentic military ammo: 1 in 3 rounds is tracer for rifles/pistols, 100% for snipers & heavy MGs
    if (forceTracer == 1) {
        b.isTracer = true;
    } else if (forceTracer == 0) {
        b.isTracer = false;
    } else {
        if (weaponType == 3 || weaponType == 1) {
            b.isTracer = true;
        } else {
            b.isTracer = (m_shotCounter % 4 == 0);
        }
    }
    m_shotCounter++;

    // Optical combat zeroing compensation (100m for rifles/snipers, 25m for pistols)
    glm::vec3 fireDir = glm::normalize(direction);
    glm::vec3 upRef = (std::abs(fireDir.y) > 0.95f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 rightDir = glm::normalize(glm::cross(fireDir, upRef));
    glm::vec3 trueUp   = glm::normalize(glm::cross(rightDir, fireDir));

    float actualSpeed = speedMs;
    if (actualSpeed <= 0.0f) {
        if (weaponType == 3) actualSpeed = 820.0f;
        else if (weaponType == 1) actualSpeed = 865.0f;
        else if (weaponType == 2) actualSpeed = 380.0f;
        else actualSpeed = 880.0f;
    }

    float zeroRange = (weaponType == 2) ? 25.0f : 100.0f;
    float sightHeight = (weaponType == 2) ? 0.025f : 0.050f;
    float effectiveSpeed = std::max(120.0f, actualSpeed);
    float tZero = zeroRange / effectiveSpeed;
    float gravityDropAtZero = 0.5f * 9.81f * (tZero * tZero);
    float zeroElevationAngle = (gravityDropAtZero + sightHeight) / zeroRange;

    // Apply combat zero elevation so rounds strike reticle center at zero range
    fireDir = glm::normalize(fireDir + trueUp * zeroElevationAngle);

    float dragFactor = 0.000048f;
    if (weaponType == 3) {
        // DShK 12.7mm Heavy AP-I Round
        b.caliberSize = 0.12f;
        b.tracerColor = glm::vec4(1.0f, 0.55f, 0.12f, 1.0f); // Fiery molten orange
        b.maxRange = (maxRange > 0.0f) ? std::max(maxRange, 650.0f) : 650.0f;
        dragFactor = 0.000028f;
    } else if (weaponType == 1) {
        // Mosin 7.62x54R Match Grade Sniper Tracer
        b.caliberSize = 0.09f;
        b.tracerColor = glm::vec4(1.0f, 0.94f, 0.58f, 0.95f); // Bright golden-white
        b.maxRange = (maxRange > 0.0f) ? std::max(maxRange, 600.0f) : 600.0f;
        dragFactor = 0.000032f;
    } else if (weaponType == 2) {
        // 9mm Pistol
        b.caliberSize = 0.06f;
        b.tracerColor = glm::vec4(1.0f, 0.82f, 0.42f, 0.85f); // Warm bronze-gold
        b.maxRange = (maxRange > 0.0f) ? std::max(maxRange, 120.0f) : 120.0f;
        dragFactor = 0.000120f;
    } else {
        // Standard Military 5.56 NATO (amber/red) / 5.45 Russian (emerald green)
        b.caliberSize = 0.075f;
        if (faction == Faction::FactionB) {
            // Russian / Eastern Bloc: Iconic Emerald Green barium nitrate tracer
            b.tracerColor = glm::vec4(0.32f, 1.0f, 0.42f, 0.95f);
            dragFactor = 0.000045f;
        } else {
            // Allied NATO / Ukrainian: Amber-Red strontium nitrate tracer
            b.tracerColor = isPlayer ? glm::vec4(1.0f, 0.52f, 0.18f, 0.95f) : glm::vec4(1.0f, 0.40f, 0.15f, 0.95f);
            dragFactor = 0.000048f;
        }
        b.maxRange = (maxRange > 0.0f) ? std::max(maxRange, 450.0f) : 450.0f;
    }

    b.dragCoeff = dragFactor;
    b.velocity = fireDir * actualSpeed;
    b.damage = damage;
    b.penetrationPower = penetration;
    b.isPlayer = isPlayer;
    b.faction = faction;
    b.shooterId = shooterId;
    b.lifetime = 0.0f;
    b.maxLifetime = (b.maxRange / std::max(50.0f, actualSpeed)) * 1.5f;
    b.distanceTraveled = 0.0f;
    b.flybyTriggered = false;
    b.weaponType = weaponType;
    b.alive = true;

    m_bullets.push_back(b);
}

void BallisticsSystem::spawnShellCasing(const glm::vec3& origin, const glm::vec3& rightDir, const glm::vec3& upDir,
                                       const glm::vec3& aimDir, int weaponType) {
    static std::mt19937 gen(1337);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    std::uniform_real_distribution<float> posDis(0.85f, 1.15f);

    ShellCasing c;
    c.position = origin;

    float lateralSpeed = 2.4f * posDis(gen);
    float upwardSpeed  = 1.3f * posDis(gen);
    float backSpeed    = 0.35f * posDis(gen);

    if (weaponType == 3) {
        // 12.7x108mm Heavy Brass
        c.radius = 0.018f;
        c.length = 0.085f;
        c.isHeavy = true;
        lateralSpeed = 1.8f;
        upwardSpeed = 0.8f;
    } else if (weaponType == 1) {
        // 7.62x54R Rimmed Brass
        c.radius = 0.012f;
        c.length = 0.054f;
    } else if (weaponType == 2) {
        // 9x19mm Luger
        c.radius = 0.009f;
        c.length = 0.024f;
        upwardSpeed = 1.8f;
    } else {
        // 5.56x45mm / 5.45x39mm NATO/Russian Mil-Spec Brass
        c.radius = 0.010f;
        c.length = 0.045f;
    }

    c.velocity = rightDir * lateralSpeed + upDir * upwardSpeed - aimDir * backSpeed;
    c.velocity += glm::vec3(dis(gen) * 0.25f, dis(gen) * 0.15f, dis(gen) * 0.25f);

    c.rotation = glm::vec3(dis(gen) * 3.14f, dis(gen) * 3.14f, dis(gen) * 3.14f);
    c.angularVelocity = glm::vec3(dis(gen) * 24.0f + 16.0f, dis(gen) * 32.0f, dis(gen) * 18.0f);

    c.life = 0.0f;
    c.maxLife = 14.0f;
    c.bounces = 0;
    c.atRest = false;

    m_casings.push_back(c);

    // Limit maximum persistent spent brass to preserve frame rates
    if (m_casings.size() > 140) {
        m_casings.erase(m_casings.begin(), m_casings.begin() + 20);
    }
}

void BallisticsSystem::spawnMuzzleFlash(const glm::vec3& pos, const glm::vec3& forwardDir, float scale, int weaponType) {
    MuzzleFlashFX mf;
    mf.position = pos;
    mf.forward = glm::normalize(forwardDir);

    glm::vec3 upRef = (std::abs(mf.forward.y) > 0.95f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    mf.right = glm::normalize(glm::cross(mf.forward, upRef));
    mf.up = glm::normalize(glm::cross(mf.right, mf.forward));

    static std::mt19937 flashRnd(4096);
    std::uniform_real_distribution<float> angleDis(0.0f, 6.28318f);
    std::uniform_real_distribution<float> seedDis(0.0f, 100.0f);

    mf.life = 0.0f;
    mf.maxLife = 0.052f;
    mf.scale = scale;
    mf.weaponType = weaponType;
    mf.randomAngle = angleDis(flashRnd);
    mf.seed = seedDis(flashRnd);

    m_muzzleFlashes.push_back(mf);

    // Forward supersonic burning propellant embers & powder sparks
    static std::mt19937 sparkRnd(555);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    int sparkCount = (weaponType == 3) ? 8 : ((weaponType == 1) ? 5 : ((weaponType == 2) ? 2 : 4));
    for (int i = 0; i < sparkCount; ++i) {
        SmokeParticle sp;
        sp.position = pos + mf.forward * (0.05f + std::abs(dis(sparkRnd)) * 0.04f);
        glm::vec3 sparkSpread = mf.right * (dis(sparkRnd) * 0.35f) + mf.up * (dis(sparkRnd) * 0.35f);
        sp.velocity = glm::normalize(mf.forward + sparkSpread) * (14.0f + std::abs(dis(sparkRnd)) * 8.0f) * scale;
        sp.color = glm::vec4(1.0f, 0.88f, 0.38f, 1.0f);
        sp.size = 0.045f * scale;
        sp.growthRate = -0.02f;
        sp.life = 0.0f;
        sp.maxLife = 0.14f + std::abs(dis(sparkRnd)) * 0.10f;
        sp.rotation = dis(sparkRnd) * 3.1415f;
        sp.rotSpeed = dis(sparkRnd) * 8.0f;
        sp.particleType = 1; // Spark
        m_particles.push_back(sp);
    }
}

void BallisticsSystem::spawnMuzzleSmoke(const glm::vec3& pos, const glm::vec3& forwardDir, float intensity) {
    static std::mt19937 gen(999);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    std::uniform_real_distribution<float> speedDis(0.6f, 1.6f);

    glm::vec3 upRef = (std::abs(forwardDir.y) > 0.95f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 rightDir = glm::normalize(glm::cross(forwardDir, upRef));
    glm::vec3 trueUp   = glm::normalize(glm::cross(rightDir, forwardDir));

    int puffCount = std::clamp(static_cast<int>(5 * intensity), 3, 8);
    for (int i = 0; i < puffCount; ++i) {
        SmokeParticle p;
        p.position = pos + forwardDir * (0.05f + static_cast<float>(i) * 0.04f);
        p.position += rightDir * (dis(gen) * 0.03f) + trueUp * (dis(gen) * 0.03f);

        float fwdSpeed = speedDis(gen) * 2.2f * intensity;
        p.velocity = forwardDir * fwdSpeed + trueUp * (0.35f + std::abs(dis(gen)) * 0.25f) + rightDir * (dis(gen) * 0.35f);

        float shade = 0.80f + dis(gen) * 0.10f;
        p.color = glm::vec4(shade, shade, shade, 0.55f);
        p.size = 0.11f + static_cast<float>(i) * 0.035f;
        p.growthRate = 0.36f + std::abs(dis(gen)) * 0.14f;
        p.life = 0.0f;
        p.maxLife = 1.25f + std::abs(dis(gen)) * 0.40f; // Lingers realistically
        p.rotation = dis(gen) * 3.1415f;
        p.rotSpeed = dis(gen) * 1.4f;
        p.particleType = 0; // Gunsmoke

        m_particles.push_back(p);
    }
}

void BallisticsSystem::spawnImpactFX(const glm::vec3& pos, const glm::vec3& normal, int surfaceType) {
    static std::mt19937 gen(777);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    if (surfaceType == 2) {
        // Flesh / Target hit: arterial blood spatter & red mist
        for (int i = 0; i < 8; ++i) {
            SmokeParticle p;
            p.position = pos;
            p.velocity = normal * (1.6f + dis(gen) * 0.8f) + glm::vec3(dis(gen) * 1.3f, std::abs(dis(gen)) * 0.9f, dis(gen) * 1.3f);
            p.color = glm::vec4(0.78f, 0.05f, 0.05f, 0.90f);
            p.size = 0.08f;
            p.growthRate = 0.22f;
            p.life = 0.0f;
            p.maxLife = 0.45f;
            p.rotation = dis(gen) * 3.1415f;
            p.rotSpeed = dis(gen) * 2.0f;
            p.particleType = 3; // Blood
            m_particles.push_back(p);
        }
    } else if (surfaceType == 3) {
        // Metal / Heavy Armor Ricochet: violent bright spark shower
        spawnImpactSparks(pos, normal);
        for (int i = 0; i < 6; ++i) {
            SmokeParticle p;
            p.position = pos;
            p.velocity = normal * (2.8f + dis(gen) * 1.2f) + glm::vec3(dis(gen) * 1.5f, std::abs(dis(gen)) * 1.5f, dis(gen) * 1.5f);
            p.color = glm::vec4(1.0f, 0.95f, 0.65f, 1.0f);
            p.size = 0.04f;
            p.growthRate = -0.04f;
            p.life = 0.0f;
            p.maxLife = 0.22f;
            p.particleType = 1; // High-temp spark
            m_particles.push_back(p);
        }
    } else if (surfaceType == 1) {
        // Concrete / Sandbag / Obstacle: sparks + pulverised grey debris cloud
        spawnImpactSparks(pos, normal);
        for (int i = 0; i < 5; ++i) {
            SmokeParticle p;
            p.position = pos;
            p.velocity = normal * (1.3f + dis(gen) * 0.5f) + glm::vec3(dis(gen) * 0.7f, std::abs(dis(gen)) * 0.8f, dis(gen) * 0.7f);
            p.color = glm::vec4(0.72f, 0.70f, 0.68f, 0.65f);
            p.size = 0.08f;
            p.growthRate = 0.26f;
            p.life = 0.0f;
            p.maxLife = 0.60f;
            p.particleType = 2; // Concrete dust
            m_particles.push_back(p);
        }
    } else {
        // Soil / Mud / Dirt terrain: brown soil clods & rising dust plume
        for (int i = 0; i < 7; ++i) {
            SmokeParticle p;
            p.position = pos;
            p.velocity = normal * (2.2f + dis(gen) * 0.9f) + glm::vec3(dis(gen) * 1.1f, std::abs(dis(gen)) * 1.2f, dis(gen) * 1.1f);
            p.color = glm::vec4(0.42f, 0.32f, 0.22f, 0.75f); // Muddy brown
            p.size = 0.09f;
            p.growthRate = 0.32f;
            p.life = 0.0f;
            p.maxLife = 0.75f;
            p.particleType = 2; // Dirt
            m_particles.push_back(p);
        }
    }
}

void BallisticsSystem::throwGrenade(const glm::vec3& origin, const glm::vec3& velocity, bool isSmoke, bool isPlayer, Faction faction, int throwerId) {
    TacticalGrenade g;
    g.position = origin;
    g.prevPosition = origin;
    g.velocity = velocity;
    g.fuseTimer = 3.0f;
    g.maxFuse = 3.0f;
    g.isSmoke = isSmoke;
    g.isDetonated = false;
    g.isExpired = false;
    g.effectTimer = 0.0f;
    g.faction = faction;
    g.throwerId = throwerId;
    g.isPlayer = isPlayer;
    g.bounces = 0;

    static std::mt19937 rGen(1337);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    g.angularVelocity = glm::vec3(dis(rGen) * 14.0f, dis(rGen) * 12.0f, dis(rGen) * 14.0f);

    m_grenades.push_back(g);
    if (m_grenades.size() > 30) {
        m_grenades.erase(m_grenades.begin());
    }
}

void BallisticsSystem::spawnExplosionFX(const glm::vec3& pos, float blastRadius) {
    (void)blastRadius;
    static std::mt19937 gen(7721);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    // 1. Central incandescent white-hot fireball
    for (int i = 0; i < 14; ++i) {
        SmokeParticle p;
        p.position = pos + glm::vec3(dis(gen) * 0.25f, 0.2f + std::abs(dis(gen)) * 0.4f, dis(gen) * 0.25f);
        p.velocity = glm::vec3(dis(gen) * 4.5f, 3.5f + std::abs(dis(gen)) * 6.5f, dis(gen) * 4.5f);
        p.color = glm::vec4(1.0f, 0.85f, 0.35f, 1.0f);
        p.size = 0.45f;
        p.growthRate = 1.8f;
        p.life = 0.0f;
        p.maxLife = 0.35f;
        p.particleType = 1;
        m_particles.push_back(p);
    }

    // 2. Dense billowing dark blast smoke & dirt plume
    for (int i = 0; i < 18; ++i) {
        SmokeParticle p;
        p.position = pos + glm::vec3(dis(gen) * 0.4f, 0.1f, dis(gen) * 0.4f);
        p.velocity = glm::vec3(dis(gen) * 3.8f, 2.0f + std::abs(dis(gen)) * 4.5f, dis(gen) * 3.8f);
        p.color = glm::vec4(0.24f, 0.22f, 0.20f, 0.85f);
        p.size = 0.65f;
        p.growthRate = 2.2f;
        p.life = 0.0f;
        p.maxLife = 1.4f + std::abs(dis(gen)) * 0.6f;
        p.particleType = 0;
        m_particles.push_back(p);
    }

    // 3. Ground shockwave dust ring
    for (int i = 0; i < 20; ++i) {
        SmokeParticle p;
        float angle = static_cast<float>(i) * (6.28318f / 20.0f);
        glm::vec3 dir(std::cos(angle), 0.05f, std::sin(angle));
        p.position = pos + dir * 0.5f;
        p.velocity = dir * (7.0f + dis(gen) * 1.5f);
        p.color = glm::vec4(0.55f, 0.50f, 0.42f, 0.70f);
        p.size = 0.50f;
        p.growthRate = 1.4f;
        p.life = 0.0f;
        p.maxLife = 0.90f;
        p.particleType = 2;
        m_particles.push_back(p);
    }
}

void BallisticsSystem::spawnSmokeCloudFX(const glm::vec3& pos, float radius) {
    (void)radius;
    static std::mt19937 gen(8832);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (int i = 0; i < 28; ++i) {
        SmokeParticle p;
        p.position = pos + glm::vec3(dis(gen) * 1.2f, 0.15f + std::abs(dis(gen)) * 0.6f, dis(gen) * 1.2f);
        p.velocity = glm::vec3(dis(gen) * 0.8f, 0.35f + std::abs(dis(gen)) * 0.5f, dis(gen) * 0.8f);
        p.color = glm::vec4(0.88f, 0.90f, 0.92f, 0.82f);
        p.size = 1.2f;
        p.growthRate = 0.45f;
        p.life = 0.0f;
        p.maxLife = 16.0f + std::abs(dis(gen)) * 3.0f;
        p.particleType = 0;
        m_particles.push_back(p);
    }
}

void BallisticsSystem::spawnImpactSparks(const glm::vec3& pos, const glm::vec3& normal) {
    static std::mt19937 gen(42);
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

    for (int i = 0; i < 8; ++i) {
        SmokeParticle p;
        p.position = pos;
        glm::vec3 randDir(dis(gen), dis(gen) * 0.5f + 0.5f, dis(gen));
        p.velocity = glm::normalize(normal + randDir) * (4.0f + dis(gen) * 2.5f);
        p.color = glm::vec4(1.0f, 0.90f, 0.45f, 1.0f);
        p.size = 0.045f;
        p.growthRate = -0.02f;
        p.life = 0.0f;
        p.maxLife = 0.28f + dis(gen) * 0.10f;
        p.rotation = dis(gen) * 3.1415f;
        p.rotSpeed = dis(gen) * 6.0f;
        p.particleType = 1; // Spark
        m_particles.push_back(p);
    }
}

void BallisticsSystem::update(float dt, const glm::vec3& listenerPos, float cameraYaw, const World* world) {
    const glm::vec3 gravity(0.0f, -9.81f, 0.0f);

    // 1. Update Bullets with Aerodynamic Drag & True Ballistic Drop
    for (auto& b : m_bullets) {
        if (!b.alive) continue;

        b.prevPosition = b.position;

        // Quadratic aerodynamic velocity drag: a_drag = k * v^2 opposite to velocity vector
        float speed = glm::length(b.velocity);
        if (speed > 0.001f) {
            float dragDecel = b.dragCoeff * speed * speed;
            glm::vec3 dragDir = -b.velocity / speed;
            b.velocity += (dragDir * dragDecel + gravity) * dt;
        } else {
            b.velocity += gravity * dt;
        }

        glm::vec3 step = b.velocity * dt;
        b.position += step;
        b.distanceTraveled += glm::length(step);
        b.lifetime += dt;

        float terrainY = World::sampleTerrainHeight(b.position.x, b.position.z);
        if (b.lifetime >= b.maxLifetime || b.distanceTraveled >= b.maxRange || b.position.y <= terrainY) {
            b.alive = false;
            // Only spawn ground impact decals when the bullet physically strikes terrain, NEVER on mid-air range expiration!
            if (b.position.y <= terrainY + 0.08f) {
                spawnImpactFX(glm::vec3(b.position.x, terrainY + 0.05f, b.position.z), glm::vec3(0, 1, 0), 0);
                PunctureSystem::get().addPuncture(glm::vec3(b.position.x, terrainY + 0.02f, b.position.z), glm::vec3(0, 1, 0), b.velocity, 3, false, b.caliberSize, b.weaponType);
                DecalSystem::get().addBulletHole(glm::vec3(b.position.x, terrainY + 0.02f, b.position.z), glm::vec3(0, 1, 0), DecalSurface::Dirt, b.caliberSize, b.weaponType);
            }
        }
    }

    m_bullets.erase(
        std::remove_if(m_bullets.begin(), m_bullets.end(), [](const Bullet& b) { return !b.alive; }),
        m_bullets.end()
    );

    // 2. Update 3D Brass Shell Casings (Gravity, Drag, Bounce & Sound)
    for (auto& c : m_casings) {
        c.life += dt;
        if (c.atRest) continue;

        c.velocity += gravity * dt;
        c.velocity *= (1.0f - dt * 0.35f);
        c.rotation += c.angularVelocity * dt;
        c.position += c.velocity * dt;

        float terrainY = World::sampleTerrainHeight(c.position.x, c.position.z);
        if (c.position.y <= terrainY + c.radius) {
            c.position.y = terrainY + c.radius;

            float speed = glm::length(c.velocity);
            if (c.bounces < 2 && speed > 1.15f) {
                // Energetic bounce
                c.velocity.y = -c.velocity.y * 0.38f;
                c.velocity.x *= 0.65f;
                c.velocity.z *= 0.65f;
                c.angularVelocity *= 0.60f;
                c.bounces++;

                // Strict proximity culling: Casings are physically quiet events (inaudible beyond 7.5m)
                float distToListener = glm::distance(c.position, listenerPos);
                if (distToListener <= 7.5f) {
                    GroundSurface surf = world ? world->getGroundSurface(c.position) : GroundSurface::Concrete;
                    SoundType sndType = (surf == GroundSurface::Grass || surf == GroundSurface::Dirt || surf == GroundSurface::Mud)
                                        ? SoundType::ShellCasingDirt : SoundType::ShellCasing;
                    float vol = (surf == GroundSurface::Concrete) ? 0.22f : 0.14f;
                    AudioEngine::get().playSound3D(sndType, c.position, listenerPos, cameraYaw, vol, 7.5f);
                }
            } else {
                // Settle flat on terrain
                c.atRest = true;
                c.velocity = glm::vec3(0.0f);
                c.angularVelocity = glm::vec3(0.0f);
            }
        }
    }

    m_casings.erase(
        std::remove_if(m_casings.begin(), m_casings.end(), [](const ShellCasing& c) { return c.life >= c.maxLife; }),
        m_casings.end()
    );

    // 3. Update Muzzle Flashes
    for (auto& mf : m_muzzleFlashes) {
        mf.life += dt;
    }
    m_muzzleFlashes.erase(
        std::remove_if(m_muzzleFlashes.begin(), m_muzzleFlashes.end(), [](const MuzzleFlashFX& mf) { return mf.life >= mf.maxLife; }),
        m_muzzleFlashes.end()
    );

    // 4. Update Volumetric Smoke & Dust Particles
    for (auto& p : m_particles) {
        p.position += p.velocity * dt;
        p.rotation += p.rotSpeed * dt;
        if (p.particleType == 0) {
            // Gunsmoke: upward thermal buoyancy & aerodynamic expansion
            p.velocity.y += 0.22f * dt;
            p.velocity *= (1.0f - dt * 1.2f);
            p.size += p.growthRate * dt;
        } else if (p.particleType == 1) {
            // Sparks: gravity
            p.velocity += gravity * 0.65f * dt;
        } else if (p.particleType >= 2) {
            // Dirt / Blood: heavy gravity
            p.velocity += gravity * 0.90f * dt;
            p.size += p.growthRate * dt;
        }
        p.life += dt;
    }

    m_particles.erase(
        std::remove_if(m_particles.begin(), m_particles.end(), [](const SmokeParticle& p) { return p.life >= p.maxLife; }),
        m_particles.end()
    );

    // 3. Update Tactical Grenades
    for (auto& g : m_grenades) {
        if (g.isExpired) continue;

        g.prevPosition = g.position;
        g.velocity += gravity * dt;
        g.position += g.velocity * dt;
        g.rotation += g.angularVelocity * dt;

        float terrainY = World::sampleTerrainHeight(g.position.x, g.position.z);
        if (g.position.y <= terrainY + 0.08f) {
            g.position.y = terrainY + 0.08f;
            if (std::abs(g.velocity.y) > 0.8f) {
                AudioEngine::get().playSound(SoundType::GrenadeBounce, 0.45f);
                g.bounces++;
            }
            g.velocity.y = -g.velocity.y * 0.38f;
            g.velocity.x *= 0.62f;
            g.velocity.z *= 0.62f;
            g.angularVelocity *= 0.60f;
        }

        if (!g.isDetonated) {
            g.fuseTimer -= dt;
            if (g.fuseTimer <= 0.0f) {
                g.isDetonated = true;
                ExplosionEvent evt;
                evt.position = g.position;
                evt.blastRadius = g.isSmoke ? 5.5f : 7.5f;
                evt.damage = g.isSmoke ? 0.0f : 125.0f;
                evt.isSmoke = g.isSmoke;
                evt.isPlayer = g.isPlayer;
                evt.faction = g.faction;
                evt.throwerId = g.throwerId;
                m_pendingExplosions.push_back(evt);

                if (!g.isSmoke) {
                    spawnExplosionFX(g.position, 7.5f);
                    g.isExpired = true;
                } else {
                    spawnSmokeCloudFX(g.position, 5.5f);
                    g.effectTimer = 18.0f;
                }
            }
        } else if (g.isSmoke) {
            g.effectTimer -= dt;
            if (g.effectTimer <= 0.0f) {
                g.isExpired = true;
            }
        }
    }

    m_grenades.erase(
        std::remove_if(m_grenades.begin(), m_grenades.end(), [](const TacticalGrenade& g) { return g.isExpired; }),
        m_grenades.end()
    );
}

void BallisticsSystem::render(Renderer& renderer) {
    // 0. Render Tactical Grenades (Olive-Drab iron grenades)
    for (const auto& g : m_grenades) {
        if (g.isExpired || g.isDetonated) continue;
        glm::vec4 gCol = g.isSmoke ? glm::vec4(0.85f, 0.88f, 0.90f, 1.0f) : glm::vec4(0.24f, 0.34f, 0.20f, 1.0f);
        renderer.drawParticleBillboard(g.position, 0.14f, gCol, g.rotation.y, 1);
        renderer.drawParticleBillboard(g.position + glm::vec3(0, 0.04f, 0), 0.08f, glm::vec4(0.6f, 0.6f, 0.6f, 1.0f), 0.0f, 1);
    }
    // 1. Render Spent Brass Shell Casings (Authentic 3D brass cylinders with specular metallic GGX)
    for (const auto& c : m_casings) {
        renderer.drawShellCasing(c.position, c.rotation, c.radius, c.length, c.atRest, c.isHeavy);
    }

    // 2. Render Volumetric Gunsmoke, Impact Dust, Sparks & Blood Mist (Soft camera-facing billboards)
    for (const auto& p : m_particles) {
        float normLife = p.life / p.maxLife;
        float alpha = (1.0f - normLife) * p.color.a;
        glm::vec4 c = p.color;
        c.a = std::clamp(alpha, 0.0f, 1.0f);
        int pType = (p.particleType == 0) ? 0 : ((p.particleType == 1) ? 4 : 2);
        renderer.drawParticleBillboard(p.position, p.size, c, p.rotation, pType);
    }

    // 3. Render High-Intensity Multi-Petal Muzzle Flash FX & Dynamic Ground Illumination
    for (const auto& mf : m_muzzleFlashes) {
        float s = mf.scale;
        float progress = mf.life / mf.maxLife;
        float alpha = std::clamp(1.0f - progress, 0.0f, 1.0f);

        // A. Dynamic Ground Flash Illumination Decal
        float groundY = World::sampleTerrainHeight(mf.position.x, mf.position.z);
        float glowRadius = 2.4f * s;
        glm::vec4 groundGlow(1.0f, 0.68f, 0.20f, alpha * 0.50f);
        renderer.drawGroundDecal(glm::vec3(mf.position.x, groundY + 0.02f, mf.position.z), glowRadius, groundGlow);

        // B. Directional Supersonic Flame Lance along barrel axis
        float lanceLen = (mf.weaponType == 3) ? (0.68f * s) : ((mf.weaponType == 1) ? (0.58f * s) : ((mf.weaponType == 2) ? (0.24f * s) : (0.42f * s)));
        float lanceWidth = (mf.weaponType == 3) ? (0.34f * s) : ((mf.weaponType == 1) ? (0.22f * s) : ((mf.weaponType == 2) ? (0.16f * s) : (0.22f * s)));
        renderer.drawDirectionalFlame(mf.position, mf.forward, lanceLen, lanceWidth, glm::vec4(1.0f, 0.88f, 0.35f, alpha), mf.seed);

        // C. Multi-Petal Flash-Hider Compensator Star Flash
        float starSize = (mf.weaponType == 3) ? (0.54f * s) : ((mf.weaponType == 1) ? (0.40f * s) : ((mf.weaponType == 2) ? (0.28f * s) : (0.36f * s)));
        renderer.drawParticleBillboard(mf.position + mf.forward * 0.04f, starSize, glm::vec4(1.0f, 0.85f, 0.30f, alpha), mf.randomAngle, 1);

        // D. Secondary Offset Petal for depth & chaotic combustion
        renderer.drawParticleBillboard(mf.position + mf.forward * 0.08f, starSize * 0.75f, glm::vec4(1.0f, 0.78f, 0.22f, alpha * 0.90f), mf.randomAngle + 0.785f, 1);

        // E. Incandescent White-Hot Needle Core at muzzle crown
        renderer.drawParticleBillboard(mf.position + mf.forward * 0.02f, 0.16f * s, glm::vec4(1.0f, 1.0f, 0.96f, alpha * 1.3f), mf.randomAngle * 2.0f, 1);
    }

    // 4. Authentic Ballistic Projectile Rendering (Muzzle-Anchored Tracers & Clean Mach Projectile Glints)
    for (const auto& b : m_bullets) {
        if (!b.alive) continue;

        glm::vec3 dir = glm::normalize(b.velocity);

        if (b.isTracer) {
            // Glowing Phosphorus Tracer Ribbon (clean, realistic length, seamlessly anchored to muzzle crown on firing)
            float targetStreakLen = (b.weaponType == 3) ? 2.2f : ((b.weaponType == 1) ? 1.8f : ((b.weaponType == 2) ? 0.95f : 1.5f));
            glm::vec3 tail;
            if (b.distanceTraveled < targetStreakLen) {
                // While emerging from the muzzle, tail remains locked to exact muzzle crown (zero floating gap)
                tail = b.origin;
            } else {
                // Curvature-preserving tracer ribbon: follow current trajectory tangent backward from actual projectile position
                tail = b.position - dir * targetStreakLen;
            }

            float tracerWidth = (b.weaponType == 3) ? 0.052f : ((b.weaponType == 1) ? 0.040f : ((b.weaponType == 2) ? 0.024f : 0.032f));
            renderer.drawTracerBeam(tail, b.position, tracerWidth, b.tracerColor);

            // Small incandescent projectile tip spark
            float sparkSize = (b.weaponType == 3) ? 0.065f : 0.038f;
            renderer.drawParticleBillboard(b.position, sparkSize, glm::vec4(1.0f, 1.0f, 0.96f, 1.0f), 0.0f, 1);
        } else {
            // Realistic military ball ammunition: No artificial trace line/beam, only tangible high-speed projectile spark
            float glintSize = (b.weaponType == 3) ? 0.045f : 0.028f;
            renderer.drawParticleBillboard(b.position, glintSize, glm::vec4(1.0f, 0.96f, 0.88f, 0.65f), 0.0f, 1);
        }
    }

    // Flush all batched particle passes to GPU
    renderer.flushParticles();
}

} // namespace Mercenary

#include "GoreSystem.hpp"
#include "World.hpp"
#include <random>
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace Mercenary {

static std::mt19937 s_goreRng(1337);
static std::uniform_real_distribution<float> s_disMinusOneToOne(-1.0f, 1.0f);
static std::uniform_real_distribution<float> s_disZeroToOne(0.0f, 1.0f);

GoreSystem& GoreSystem::get() {
    static GoreSystem s_instance;
    return s_instance;
}

void GoreSystem::clear() {
    m_bloodParticles.clear();
    m_bloodPools.clear();
    m_gibs.clear();
    m_droppedGuns.clear();
    m_corpses.clear();
}

void GoreSystem::spawnBloodBurst(const glm::vec3& position, const glm::vec3& hitDir, int particleCount, float velocityScale) {
    glm::vec3 normDir = (glm::length(hitDir) > 0.001f) ? glm::normalize(hitDir) : glm::vec3(0, 1, 0);

    for (int i = 0; i < particleCount; ++i) {
        BloodParticle p;
        p.position = position + glm::vec3(s_disMinusOneToOne(s_goreRng) * 0.06f,
                                          s_disMinusOneToOne(s_goreRng) * 0.06f,
                                          s_disMinusOneToOne(s_goreRng) * 0.06f);

        // High-velocity arterial cone with chaotic turbulence
        float speed = (3.5f + s_disZeroToOne(s_goreRng) * 5.5f) * velocityScale;
        glm::vec3 spread(s_disMinusOneToOne(s_goreRng) * 2.4f,
                         s_disZeroToOne(s_goreRng) * 2.8f + 0.6f,
                         s_disMinusOneToOne(s_goreRng) * 2.4f);

        p.velocity = normDir * speed + spread;
        p.size = 0.07f + s_disZeroToOne(s_goreRng) * 0.11f;
        p.growthRate = 0.16f + s_disZeroToOne(s_goreRng) * 0.12f;
        p.life = 0.0f;
        p.maxLife = 0.45f + s_disZeroToOne(s_goreRng) * 0.40f;

        float crimsonShade = 0.44f + s_disZeroToOne(s_goreRng) * 0.16f;
        p.color = glm::vec4(crimsonShade, 0.02f, 0.02f, 0.94f);
        p.rotation = s_disMinusOneToOne(s_goreRng) * 3.14159f;
        p.rotSpeed = s_disMinusOneToOne(s_goreRng) * 3.5f;

        m_bloodParticles.push_back(p);
    }
}

void GoreSystem::spawnArterialSpurt(const glm::vec3& stumpPos, const glm::vec3& stumpNormal, int dropletCount) {
    glm::vec3 norm = (glm::length(stumpNormal) > 0.001f) ? glm::normalize(stumpNormal) : glm::vec3(0, 1, 0);

    for (int i = 0; i < dropletCount; ++i) {
        BloodParticle p;
        p.position = stumpPos + glm::vec3(s_disMinusOneToOne(s_goreRng) * 0.03f,
                                          s_disMinusOneToOne(s_goreRng) * 0.03f,
                                          s_disMinusOneToOne(s_goreRng) * 0.03f);

        float spurtSpeed = 4.2f + s_disZeroToOne(s_goreRng) * 4.8f;
        glm::vec3 jitter(s_disMinusOneToOne(s_goreRng) * 1.1f,
                         s_disZeroToOne(s_goreRng) * 1.4f + 0.5f,
                         s_disMinusOneToOne(s_goreRng) * 1.1f);

        p.velocity = norm * spurtSpeed + jitter;
        p.size = 0.08f + s_disZeroToOne(s_goreRng) * 0.09f;
        p.growthRate = 0.14f;
        p.life = 0.0f;
        p.maxLife = 0.50f + s_disZeroToOne(s_goreRng) * 0.35f;
        p.color = glm::vec4(0.52f, 0.02f, 0.02f, 0.96f);
        p.rotation = s_disMinusOneToOne(s_goreRng) * 3.14159f;
        p.rotSpeed = s_disMinusOneToOne(s_goreRng) * 4.0f;

        m_bloodParticles.push_back(p);
    }
}

void GoreSystem::spawnExplosionGore(const glm::vec3& blastPos, float radius, int bodyCount) {
    // Massive catastrophic blood cloud: 120-160 particles
    int bloodCount = 120 * bodyCount;
    for (int i = 0; i < bloodCount; ++i) {
        BloodParticle p;
        p.position = blastPos + glm::vec3(s_disMinusOneToOne(s_goreRng) * 0.25f,
                                          s_disZeroToOne(s_goreRng) * 0.40f,
                                          s_disMinusOneToOne(s_goreRng) * 0.25f);

        // Omnidirectional high-pressure blast burst
        glm::vec3 dir(s_disMinusOneToOne(s_goreRng),
                      s_disZeroToOne(s_goreRng) * 1.2f + 0.3f,
                      s_disMinusOneToOne(s_goreRng));
        if (glm::length(dir) > 0.001f) dir = glm::normalize(dir);

        float speed = 5.0f + s_disZeroToOne(s_goreRng) * 9.0f;
        p.velocity = dir * speed;
        p.size = 0.10f + s_disZeroToOne(s_goreRng) * 0.16f;
        p.growthRate = 0.22f;
        p.life = 0.0f;
        p.maxLife = 0.60f + s_disZeroToOne(s_goreRng) * 0.50f;
        p.color = glm::vec4(0.48f + s_disZeroToOne(s_goreRng) * 0.14f, 0.02f, 0.02f, 0.95f);
        p.rotation = s_disMinusOneToOne(s_goreRng) * 3.14159f;
        p.rotSpeed = s_disMinusOneToOne(s_goreRng) * 5.0f;

        m_bloodParticles.push_back(p);
    }

    // Spawn massive crater blood pool
    addBloodPool(blastPos + glm::vec3(0, 0.03f, 0), 0.35f, 1.45f, 0.35f);

    // Spawn flying viscera & severed limb gibs
    CharacterCustomization defaultCustom;
    for (int b = 0; b < bodyCount; ++b) {
        spawnGib(GibType::Head, blastPos + glm::vec3(0, 1.4f, 0), glm::vec3(s_disMinusOneToOne(s_goreRng) * 5.0f, 6.5f, s_disMinusOneToOne(s_goreRng) * 5.0f), defaultCustom);
        spawnGib(GibType::ArmLeft, blastPos + glm::vec3(-0.3f, 1.2f, 0), glm::vec3(s_disMinusOneToOne(s_goreRng) * 6.0f, 5.5f, s_disMinusOneToOne(s_goreRng) * 6.0f), defaultCustom);
        spawnGib(GibType::ArmRight, blastPos + glm::vec3(0.3f, 1.2f, 0), glm::vec3(s_disMinusOneToOne(s_goreRng) * 6.0f, 5.5f, s_disMinusOneToOne(s_goreRng) * 6.0f), defaultCustom);
        spawnGib(GibType::LegLeft, blastPos + glm::vec3(-0.2f, 0.6f, 0), glm::vec3(s_disMinusOneToOne(s_goreRng) * 5.5f, 4.8f, s_disMinusOneToOne(s_goreRng) * 5.5f), defaultCustom);
        spawnGib(GibType::LegRight, blastPos + glm::vec3(0.2f, 0.6f, 0), glm::vec3(s_disMinusOneToOne(s_goreRng) * 5.5f, 4.8f, s_disMinusOneToOne(s_goreRng) * 5.5f), defaultCustom);

        for (int v = 0; v < 6; ++v) {
            spawnGib(GibType::Viscera, blastPos + glm::vec3(0, 0.8f, 0),
                     glm::vec3(s_disMinusOneToOne(s_goreRng) * 7.0f, 4.0f + s_disZeroToOne(s_goreRng) * 4.0f, s_disMinusOneToOne(s_goreRng) * 7.0f), defaultCustom);
        }
    }
}

void GoreSystem::spawnGib(GibType type, const glm::vec3& pos, const glm::vec3& impulse, const CharacterCustomization& custom) {
    if (m_gibs.size() >= 64) {
        m_gibs.erase(m_gibs.begin());
    }

    if (m_gibs.size() >= 48) {
        m_gibs.erase(m_gibs.begin());
    }

    GibEntity gib;
    gib.type = type;
    gib.position = pos;
    gib.velocity = impulse;
    gib.rotation = glm::vec3(s_disZeroToOne(s_goreRng) * 6.28f, s_disZeroToOne(s_goreRng) * 6.28f, s_disZeroToOne(s_goreRng) * 6.28f);
    gib.angularVelocity = glm::vec3(s_disMinusOneToOne(s_goreRng) * 6.0f, s_disMinusOneToOne(s_goreRng) * 8.0f, s_disMinusOneToOne(s_goreRng) * 6.0f);
    gib.scale = 0.8f + s_disZeroToOne(s_goreRng) * 0.4f;
    gib.atRest = false;
    gib.trailTimer = 0.0f;
    gib.lifeTime = 0.0f;
    gib.custom = custom;

    m_gibs.push_back(gib);
}

void GoreSystem::spawnDroppedWeapon(const std::string& weaponName, const glm::vec3& pos, const glm::vec3& impulse, float yaw) {
    if (weaponName.empty() || weaponName == "None") return;

    if (m_droppedGuns.size() >= 36) {
        m_droppedGuns.erase(m_droppedGuns.begin());
    }

    DroppedGunEntity gun;
    gun.weaponName = weaponName;
    gun.position = pos;
    gun.velocity = impulse;
    gun.yaw = yaw;
    gun.atRest = false;

    m_droppedGuns.push_back(gun);
}

void GoreSystem::addBloodPool(const glm::vec3& position, float initialRadius, float targetRadius, float growthSpeed, const glm::vec4& color) {
    if (m_bloodPools.size() >= 160) {
        m_bloodPools.erase(m_bloodPools.begin());
    }

    BloodPool pool;
    pool.position = position;
    pool.currentRadius = initialRadius;
    pool.targetRadius = targetRadius;
    pool.growthSpeed = growthSpeed;
    pool.color = color;

    m_bloodPools.push_back(pool);
}

void GoreSystem::addCorpse(std::shared_ptr<CharacterRig> rig, const CharacterCustomization& custom, const glm::vec3& pos, float yaw,
                           DeathType deathType, const glm::vec3& hitImpulse, bool deathGrip, uint32_t dismemberMask, const std::string& weaponName) {
    if (!rig) return;

    if (m_corpses.size() >= 48) {
        m_corpses.erase(m_corpses.begin());
    }

    BattlefieldCorpse corpse;
    corpse.rig = rig;
    corpse.custom = custom;
    corpse.position = pos;
    corpse.yaw = yaw;
    corpse.deathType = deathType;
    corpse.hitImpulse = hitImpulse;
    corpse.deathGrip = deathGrip;
    corpse.dismemberMask = dismemberMask;
    corpse.weaponName = weaponName;
    corpse.deathTimer = 0.0f;
    corpse.poolSpawned = false;
    corpse.weaponSpawned = false;
    corpse.arterialPumpsRemaining = (dismemberMask != 0 && !(dismemberMask & DISMEMBER_EXPLODED)) ? 6 : 0;
    corpse.pumpIntervalTimer = 0.0f;

    corpse.rig->triggerDeath(deathType, hitImpulse, deathGrip, dismemberMask);
    corpse.rig->setActiveWeaponName(weaponName);

    m_corpses.push_back(corpse);
}

void GoreSystem::update(float dt, World* world) {
    // 1. Update Blood Particles
    for (size_t i = 0; i < m_bloodParticles.size(); ) {
        auto& p = m_bloodParticles[i];
        p.life += dt;
        if (p.life >= p.maxLife) {
            m_bloodParticles[i] = m_bloodParticles.back();
            m_bloodParticles.pop_back();
            continue;
        }

        // Heavy gravity & air drag
        p.velocity.y -= 14.5f * dt;
        p.velocity.x *= (1.0f - dt * 1.1f);
        p.velocity.z *= (1.0f - dt * 1.1f);

        p.position += p.velocity * dt;
        p.size += p.growthRate * dt;
        p.rotation += p.rotSpeed * dt;

        float groundY = world ? world->sampleTerrainHeight(p.position.x, p.position.z) : 0.0f;
        if (p.position.y <= groundY + 0.02f) {
            // Impact with terrain: chance to deposit micro splatter
            if (s_disZeroToOne(s_goreRng) < 0.12f) {
                addBloodPool(glm::vec3(p.position.x, groundY + 0.015f, p.position.z), 0.06f, 0.18f, 0.35f);
            }
            m_bloodParticles[i] = m_bloodParticles.back();
            m_bloodParticles.pop_back();
            continue;
        }

        ++i;
    }

    // 2. Expand Blood Pools
    for (auto& pool : m_bloodPools) {
        if (pool.currentRadius < pool.targetRadius) {
            pool.currentRadius = std::min(pool.targetRadius, pool.currentRadius + pool.growthSpeed * dt);
        }
    }

    // 3. Update Severed Limb Gib Entities
    for (size_t i = 0; i < m_gibs.size(); ++i) {
        auto& gib = m_gibs[i];
        gib.lifeTime += dt;

        if (!gib.atRest) {
            gib.velocity.y -= 14.2f * dt;
            gib.velocity.x *= (1.0f - dt * 0.8f);
            gib.velocity.z *= (1.0f - dt * 0.8f);

            gib.position += gib.velocity * dt;
            gib.rotation += gib.angularVelocity * dt;

            // Emit arterial blood droplets while in mid-air
            gib.trailTimer += dt;
            if (gib.trailTimer >= 0.035f) {
                gib.trailTimer = 0.0f;
                spawnBloodBurst(gib.position, -gib.velocity * 0.35f, 2, 0.4f);
            }

            float groundY = world ? world->sampleTerrainHeight(gib.position.x, gib.position.z) : 0.0f;
            float restAltitude = (gib.type == GibType::Head) ? 0.09f : 0.06f;

            if (gib.position.y <= groundY + restAltitude) {
                gib.position.y = groundY + restAltitude;

                if (std::abs(gib.velocity.y) > 1.4f) {
                    // Physical ground bounce with restitution 0.32
                    gib.velocity.y = -gib.velocity.y * 0.32f;
                    gib.velocity.x *= 0.65f;
                    gib.velocity.z *= 0.65f;
                    gib.angularVelocity *= 0.55f;
                    spawnArterialSpurt(gib.position, glm::vec3(0, 1, 0), 4);
                } else {
                    gib.atRest = true;
                    gib.velocity = glm::vec3(0.0f);
                    gib.angularVelocity = glm::vec3(0.0f);
                    addBloodPool(gib.position, 0.18f, 0.58f, 0.16f);
                }
            }
        }
    }

    // 4. Update Dropped Guns
    for (auto& gun : m_droppedGuns) {
        if (!gun.atRest) {
            gun.velocity.y -= 14.0f * dt;
            gun.position += gun.velocity * dt;

            float groundY = world ? world->sampleTerrainHeight(gun.position.x, gun.position.z) : 0.0f;
            if (gun.position.y <= groundY + 0.04f) {
                gun.position.y = groundY + 0.04f;
                gun.atRest = true;
                gun.velocity = glm::vec3(0.0f);
            }
        }
    }

    // 5. Update Battlefield Persistent Corpses
    for (auto& corpse : m_corpses) {
        corpse.deathTimer += dt;

        float groundY = world ? world->sampleTerrainHeight(corpse.position.x, corpse.position.z) : corpse.position.y;
        corpse.position.y = groundY;

        if (corpse.rig) {
            corpse.rig->setTerrainHeight(groundY);
            corpse.rig->update9D(dt, corpse.position, corpse.yaw, corpse.position + glm::vec3(0, 1, 0),
                                 glm::vec3(0.0f), false, false, 0, false, false, corpse.deathGrip, false, 0.0f);
        }

        // Trigger expanding ground blood pool beneath corpse
        if (corpse.deathTimer >= 0.35f && !corpse.poolSpawned) {
            corpse.poolSpawned = true;
            addBloodPool(corpse.position + glm::vec3(0, 0.02f, 0), 0.22f, 0.95f, 0.16f);

            if (corpse.dismemberMask & DISMEMBER_HEAD) {
                glm::vec3 headPoolPos = corpse.position + glm::vec3(std::sin(corpse.yaw), 0, std::cos(corpse.yaw)) * 0.42f;
                addBloodPool(headPoolPos, 0.18f, 0.72f, 0.20f);
            }
        }

        // Trigger physical dropped weapon if not gripped in death
        if (corpse.deathTimer >= 0.22f && !corpse.weaponSpawned) {
            corpse.weaponSpawned = true;
            bool shouldDrop = !corpse.deathGrip || (corpse.dismemberMask & DISMEMBER_R_ARM);
            if (shouldDrop && !corpse.weaponName.empty() && corpse.weaponName != "None") {
                float cy = std::cos(corpse.yaw);
                float sy = std::sin(corpse.yaw);
                glm::vec3 gunDropPos = corpse.position + glm::vec3(cy * 0.28f, 0.25f, -sy * 0.28f);
                glm::vec3 gunImpulse = glm::vec3(cy * 1.2f, 1.4f, -sy * 1.2f);
                spawnDroppedWeapon(corpse.weaponName, gunDropPos, gunImpulse, corpse.yaw + 0.85f);
            }
        }

        // Rhythmic arterial pumps from severed limb stump
        if (corpse.arterialPumpsRemaining > 0) {
            corpse.pumpIntervalTimer += dt;
            if (corpse.pumpIntervalTimer >= 0.32f) {
                corpse.pumpIntervalTimer = 0.0f;
                corpse.arterialPumpsRemaining--;

                glm::vec3 stumpPos = corpse.position + glm::vec3(0, 0.22f, 0);
                glm::vec3 stumpNorm(0, 1, 0);

                if (corpse.dismemberMask & DISMEMBER_HEAD) {
                    stumpPos = corpse.rig ? corpse.rig->getHeadPosition() : (corpse.position + glm::vec3(0, 0.25f, 0));
                    stumpNorm = glm::vec3(0, 1, 0);
                } else if (corpse.dismemberMask & DISMEMBER_L_ARM) {
                    float cy = std::cos(corpse.yaw);
                    float sy = std::sin(corpse.yaw);
                    stumpPos = corpse.position + glm::vec3(-cy * 0.25f, 0.28f, sy * 0.25f);
                    stumpNorm = glm::vec3(-cy, 0.6f, sy);
                } else if (corpse.dismemberMask & DISMEMBER_R_ARM) {
                    float cy = std::cos(corpse.yaw);
                    float sy = std::sin(corpse.yaw);
                    stumpPos = corpse.position + glm::vec3(cy * 0.25f, 0.28f, -sy * 0.25f);
                    stumpNorm = glm::vec3(cy, 0.6f, -sy);
                }
                spawnArterialSpurt(stumpPos, glm::normalize(stumpNorm), 12);
            }
        }
    }
}

void GoreSystem::render(Renderer& renderer, const glm::vec3& cameraPos) {
    // 1. Render Persistent & Expanding Ground Blood Pools (Decal Shader Type 7.0f)
    for (const auto& pool : m_bloodPools) {
        if (glm::distance(pool.position, cameraPos) < 140.0f) {
            renderer.drawBloodDecal(pool.position, pool.currentRadius, pool.color);
        }
    }

    // 2. Render In-World Dropped Weapons
    for (const auto& gun : m_droppedGuns) {
        if (glm::distance(gun.position, cameraPos) < 120.0f) {
            renderer.drawDroppedWeapon(gun.weaponName, gun.position, gun.yaw);
        }
    }

    // 3. Render Persistent Battlefield Corpses
    for (const auto& corpse : m_corpses) {
        if (corpse.rig && glm::distance(corpse.position, cameraPos) < 150.0f) {
            corpse.rig->render(renderer, corpse.custom);
        }
    }

    // 4. Render Physical Gib Entities
    for (const auto& gib : m_gibs) {
        if (glm::distance(gib.position, cameraPos) > 120.0f) continue;

        if (gib.type == GibType::Head) {
            // Cranial bone / head with helmet/hair tint
            renderer.drawCube(gib.position, glm::vec3(0.18f, 0.20f, 0.18f), glm::vec4(0.75f, 0.60f, 0.48f, 1.0f), gib.rotation.y);
            // Severed bloody neck stump
            renderer.drawCube(gib.position - glm::vec3(0, 0.10f, 0), glm::vec3(0.11f, 0.04f, 0.11f), glm::vec4(0.50f, 0.03f, 0.03f, 0.95f), gib.rotation.y);
        } else if (gib.type == GibType::ArmLeft || gib.type == GibType::ArmRight) {
            // Uniform camo sleeve
            renderer.drawCube(gib.position, glm::vec3(0.12f, 0.38f, 0.12f), glm::vec4(0.38f, 0.40f, 0.32f, 1.0f), gib.rotation.y);
            // Severed bone & arterial stump cap
            renderer.drawCube(gib.position + glm::vec3(0, 0.18f, 0), glm::vec3(0.08f, 0.03f, 0.08f), glm::vec4(0.55f, 0.03f, 0.03f, 0.95f), gib.rotation.y);
        } else if (gib.type == GibType::LegLeft || gib.type == GibType::LegRight) {
            // Trouser leg & combat boot
            renderer.drawCube(gib.position, glm::vec3(0.14f, 0.44f, 0.14f), glm::vec4(0.38f, 0.40f, 0.32f, 1.0f), gib.rotation.y);
            renderer.drawCube(gib.position + glm::vec3(0, 0.22f, 0), glm::vec3(0.10f, 0.04f, 0.10f), glm::vec4(0.55f, 0.03f, 0.03f, 0.95f), gib.rotation.y);
        } else {
            // Viscera / Torso flesh chunk
            renderer.drawCube(gib.position, glm::vec3(0.12f * gib.scale, 0.09f * gib.scale, 0.13f * gib.scale),
                              glm::vec4(0.48f, 0.02f, 0.02f, 0.95f), gib.rotation.y);
        }
    }

    // 5. Render Volumetric Blood Particles & Red Arterial Mist Billboards
    for (const auto& p : m_bloodParticles) {
        float normLife = p.life / p.maxLife;
        float alpha = (1.0f - normLife) * p.color.a;
        glm::vec4 c = p.color;
        c.a = std::clamp(alpha, 0.0f, 1.0f);
        renderer.drawParticleBillboard(p.position, p.size, c, p.rotation, 2);
    }
}

} // namespace Mercenary

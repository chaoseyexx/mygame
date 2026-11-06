#pragma once
#include "Types.hpp"
#include "Shader.hpp"
#include "Mesh.hpp"
#include "Camera.hpp"
#include "ShadowMap.hpp"

namespace Mercenary {

class World;
class Player;
class FrontlineWarfare;

struct DirectionalLight {
    glm::vec3 direction{-0.4f, -0.9f, -0.5f};
    glm::vec3 color{1.0f, 0.98f, 0.92f};
    float ambientIntensity = 0.42f;
    float diffuseIntensity = 1.0f;
};

struct WorldObstacle;
struct AcousticWaveEvent;

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init();

    void beginFrame(const Camera& camera, float aspectRatio, const glm::mat4& lightSpaceMatrix, GLuint shadowDepthTexture);
    void endFrame();

    void renderShadowPass(Shader* shadowShader, const World& world, const Player& player, const FrontlineWarfare* frontline = nullptr);

    void drawMesh(const Mesh& mesh, const glm::mat4& modelMatrix, const glm::vec4& tint = glm::vec4(1.0f));
    void drawTexturedMesh(const Mesh& mesh, const glm::mat4& modelMatrix, GLuint albedoTexture, GLuint normalTexture = 0, const glm::vec4& tint = glm::vec4(1.0f));
    void drawCube(const glm::vec3& position, const glm::vec3& size, const glm::vec4& color, float yaw = 0.0f);
    void drawTexturedCube(const glm::vec3& position, const glm::vec3& size, GLuint albedoTex, GLuint normalTex = 0, const glm::vec4& tint = glm::vec4(1.0f), float yaw = 0.0f);
    void drawCylinder(const glm::vec3& position, float radius, float height, const glm::vec4& color);
    void drawGround(float size = 400.0f, bool isTrainingBase = false);
    void drawArenaGround(float size = 160.0f);
    void drawGrassField(bool isTrainingBase = false);
    void drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
    void drawAimReticle(const glm::vec3& worldPos, float bloomRadius, const glm::vec4& color, bool withTickMarks = true);

    // AAA Acoustic Wavefront & Sensor Beacon Rendering (F3 Debug Visualizer)
    void drawAcousticWaveRing(const glm::vec3& center, float radius, const glm::vec4& color, int segments = 36);
    void drawAcousticBeacon(const glm::vec3& origin, const glm::vec4& color, float height = 1.8f);
    void renderAcousticWaves(const std::vector<AcousticWaveEvent>& waves);
    void drawAimingRay(const glm::vec3& start, const glm::vec3& end, const glm::vec3& hitNormal, bool hitSurface, float maxRange, float spread = 0.0f, float heat = 0.0f, bool hitActor = false);
    void drawContactShadow(const glm::vec3& centerPos, float radiusX, float radiusZ, float opacity = 0.65f);

    // Detailed Procedural Models (Zero external asset dependencies)
    void drawSoldier(const glm::vec3& position, float yaw, float pitch = 0.0f, const glm::vec4& camoTint = glm::vec4(1.0f));
    void drawInfantry(Faction faction, CombatRole role, const glm::vec3& position, float yaw, float pitch = 0.0f, int lod = 0);
    void drawCustomPlayer(const CharacterCustomization& custom, const glm::vec3& position, float yaw, float pitch = 0.0f);
    void updateCustomPlayerMesh(const CharacterCustomization& custom);
    void drawRifle(const glm::vec3& position, float yaw, float pitch = 0.0f);
    void drawWeapon(const std::string& weaponName, const glm::vec3& position, float yaw, float pitch = 0.0f);
    void drawWeapon(const std::string& weaponName, const glm::mat4& model);
    void drawWeaponShadow(const std::string& weaponName, Shader* shadowShader, const glm::mat4& model);
    void drawDroppedWeapon(const std::string& weaponName, const glm::vec3& position, float yaw);
    void drawDroppedWeaponShadow(const std::string& weaponName, Shader* shadowShader, const glm::vec3& position, float yaw);
    void drawSandbagWall(const glm::vec3& position, float yaw = 0.0f);
    void drawMilitaryCrate(const glm::vec3& position, float yaw = 0.0f, bool looted = false);

    // Frontline Environment Assets (Foxhole / Thunder Tier One style)
    void drawPineTree(const glm::vec3& position, float yaw = 0.0f);
    void drawDragonTooth(const glm::vec3& position, float yaw = 0.0f);
    void drawBarbedWire(const glm::vec3& position, float yaw = 0.0f);
    void drawTankWreck(const glm::vec3& position, float yaw = 0.0f);
    void drawRuinedWall(const glm::vec3& position, float yaw = 0.0f);
    void drawBridge(const glm::vec3& position, float yaw = 0.0f);
    void drawRiverWater(const glm::vec3& position);
    void drawBunkerHideout(const glm::vec3& centerPos);
    void drawHescoBastion(const glm::vec3& position, float yaw = 0.0f);
    void drawCommandTent(const glm::vec3& position, float yaw = 0.0f);
    void drawFuelDrum(const glm::vec3& position, float yaw = 0.0f);
    void drawCargoPallet(const glm::vec3& position, float yaw = 0.0f);
    void drawFieldGenerator(const glm::vec3& position, float yaw = 0.0f);
    void drawMilitaryTruck(const glm::vec3& position, float yaw = 0.0f);
    void drawDamagedBuilding(const glm::vec3& position, float yaw = 0.0f, const glm::vec3& size = glm::vec3(15.22f, 10.69f, 10.30f));
    void drawMountedHMG(const glm::vec3& position, float yaw, float pitch = 0.0f);
    void drawSeveredProp(const Mesh& mesh, const glm::vec3& position, const glm::quat& rotation, const glm::vec4& color);
    void drawObstacle(const WorldObstacle& obs);
    void rebuildTerrainMesh(bool isTraining = false);

    // High-Fidelity Particle & FX Rendering
    void drawParticleBillboard(const glm::vec3& position, float size, const glm::vec4& color, float rotation = 0.0f, int particleType = 0);
    void drawDirectionalFlame(const glm::vec3& position, const glm::vec3& forwardDir, float length, float width, const glm::vec4& color, float seed = 0.0f);
    void drawTracerBeam(const glm::vec3& start, const glm::vec3& end, float width, const glm::vec4& color);
    void drawGroundDecal(const glm::vec3& position, float radius, const glm::vec4& color);
    void drawBloodDecal(const glm::vec3& position, float radius, const glm::vec4& color = glm::vec4(0.48f, 0.02f, 0.02f, 0.94f));
    void drawShellCasing(const glm::vec3& position, const glm::vec3& rotation, float radius, float length, bool atRest = false, bool isHeavy = false);
    void flushParticles();

    DirectionalLight& getLight() { return m_light; }
    Shader* getStandardShader() const { return m_standardShader.get(); }
    GLuint getSoldierTexture() const { return m_soldierTex; }
    GLuint getSoldierNormal() const { return m_soldierNorm; }
    std::shared_ptr<Mesh> getDamagedBuildingMesh() const { return m_damagedBuildingMesh; }
    std::shared_ptr<Mesh> getCubeMesh() const { return m_cubeMesh; }

private:
    std::unique_ptr<Shader> m_standardShader;
    std::unique_ptr<Shader> m_lineShader;

    std::shared_ptr<Mesh> m_cubeMesh;
    std::shared_ptr<Mesh> m_cylinderMesh;
    std::shared_ptr<Mesh> m_groundMesh;
    std::shared_ptr<Mesh> m_grassMesh;
    std::shared_ptr<Mesh> m_trainingGroundMesh;
    std::shared_ptr<Mesh> m_arenaGroundMesh;
    std::shared_ptr<Mesh> m_trainingGrassMesh;

    // Detailed procedural meshes
    std::shared_ptr<Mesh> m_soldierMesh;
    std::shared_ptr<Mesh> m_rifleMesh;
    std::shared_ptr<Mesh> m_ak74Mesh;
    std::shared_ptr<Mesh> m_mosinMesh;
    std::shared_ptr<Mesh> m_m9Mesh;
    std::shared_ptr<Mesh> m_sandbagMesh;
    std::shared_ptr<Mesh> m_crateMesh;
    std::shared_ptr<Mesh> m_pineTreeMesh;
    std::shared_ptr<Mesh> m_dragonToothMesh;
    std::shared_ptr<Mesh> m_barbedWireMesh;
    std::shared_ptr<Mesh> m_tankWreckMesh;
    std::shared_ptr<Mesh> m_ruinedWallMesh;
    std::shared_ptr<Mesh> m_bridgeMesh;
    std::shared_ptr<Mesh> m_riverWaterMesh;

    // Modular Regional & Urban Architecture Meshes
    std::shared_ptr<Mesh> m_urbanApartmentMesh;
    std::shared_ptr<Mesh> m_industrialWarehouseMesh;
    std::shared_ptr<Mesh> m_townChurchMesh;
    std::shared_ptr<Mesh> m_freightBoxcarMesh;
    std::shared_ptr<Mesh> m_streetLampMesh;
    std::shared_ptr<Mesh> m_commTowerMesh;
    std::shared_ptr<Mesh> m_refineryTankMesh;
    std::shared_ptr<Mesh> m_hescoMesh;
    std::shared_ptr<Mesh> m_commandTentMesh;
    std::shared_ptr<Mesh> m_fuelDrumMesh;
    std::shared_ptr<Mesh> m_cargoPalletMesh;
    std::shared_ptr<Mesh> m_generatorMesh;
    std::shared_ptr<Mesh> m_militaryTruckMesh;
    std::shared_ptr<Mesh> m_damagedBuildingMesh;
    std::shared_ptr<Mesh> m_grassClumpMesh;
    std::shared_ptr<Mesh> m_bushFoliageMesh;
    std::shared_ptr<Mesh> m_concreteBarrierMesh;
    std::shared_ptr<Mesh> m_campfireSmokeMesh;
    std::shared_ptr<Mesh> m_signalFlareMesh;
    std::shared_ptr<Mesh> m_hmgMesh;
    std::shared_ptr<Mesh> m_shootingTargetMesh;
    std::shared_ptr<Mesh> m_watchtowerMesh;
    std::shared_ptr<Mesh> m_guardBoothMesh;
    std::shared_ptr<Mesh> m_perimeterFenceMesh;
    std::shared_ptr<Mesh> m_gunRackMesh;
    std::shared_ptr<Mesh> m_obstacleWallMesh;

    // AI Mesh Variants: [Role (4)][LOD (3)]
    std::shared_ptr<Mesh> m_ruInfantry[4][3];
    std::shared_ptr<Mesh> m_uaInfantry[4][3];

    // Cached Player Custom Mesh
    std::shared_ptr<Mesh> m_customPlayerMesh;
    CharacterCustomization m_cachedCustomization;
    bool m_customMeshDirty = true;

    // Line rendering buffers
    GLuint m_lineVao = 0;
    GLuint m_lineVbo = 0;

    // Bunker & Terrain PBR Textures
    GLuint m_concreteFloorTex = 0;
    GLuint m_concreteFloorNorm = 0;
    GLuint m_steelWallTex = 0;
    GLuint m_steelWallNorm = 0;
    GLuint m_terrainMudTex = 0;
    GLuint m_terrainMudNorm = 0;
    GLuint m_sandbagTex = 0;
    GLuint m_sandbagNorm = 0;
    GLuint m_terrainRoadTex = 0;
    GLuint m_terrainRoadNorm = 0;
    GLuint m_terrainGrassTex = 0;
    GLuint m_terrainGrassNorm = 0;
    GLuint m_hescoTex = 0;
    GLuint m_hescoNorm = 0;
    GLuint m_drumTex = 0;
    GLuint m_drumNorm = 0;
    GLuint m_soldierTex = 0;
    GLuint m_soldierNorm = 0;
    GLuint m_buildingTex = 0;
    GLuint m_buildingNorm = 0;

    // Particle rendering
    std::unique_ptr<Shader> m_particleShader;
    GLuint m_particleVao = 0;
    GLuint m_particleVbo = 0;

    struct ParticleVertex {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec4 color;
        float type;
    };
    std::vector<ParticleVertex> m_alphaParticles;
    std::vector<ParticleVertex> m_additiveParticles;

    glm::vec3 m_camRight{1.0f, 0.0f, 0.0f};
    glm::vec3 m_camUp{0.0f, 1.0f, 0.0f};
    glm::mat4 m_viewMatrix{1.0f};

    DirectionalLight m_light;
    glm::mat4 m_viewProjMatrix{1.0f};
    glm::vec3 m_cameraPos{0.0f};
};

} // namespace Mercenary

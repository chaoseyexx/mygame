#pragma once
#include "Mesh.hpp"
#include <vector>
#include <memory>

namespace Mercenary {

class ProceduralMesh {
public:
    // Realistic military operator with human anatomy, tactical helmet, NVG mount, plate carrier, ComTac comms, and faction armbands
    static std::shared_ptr<Mesh> createDetailedSoldier(bool isTeamA = true);

    // Russian AI Infantry (Faction RU): Contemporary EMR/Digital Flora camo, 6B47 Ratnik helmet, 6B45 assault vest, Red/White armbands (4 roles, 3 LODs)
    static std::shared_ptr<Mesh> createRussianInfantry(CombatRole role, int lod = 0);

    // Ukrainian AI Infantry (Faction UA): Contemporary MM-14 Pixel / Multicam camo, FAST high-cut helmet + ComTacs, Blue/Yellow armbands (4 roles, 3 LODs)
    static std::shared_ptr<Mesh> createUkrainianInfantry(CombatRole role, int lod = 0);

    // Player Mercenary: Fully modular procedural customizer (face, skin, hair, beard, uniform camo, helmet, vest, backpack, armbands)
    static std::shared_ptr<Mesh> createCustomSoldier(const CharacterCustomization& custom);

    // Tactical rifle with holographic optic, PEQ-15 IR laser, tactical light, vertical grip, and Magpul PMAG
    static std::shared_ptr<Mesh> createDetailedRifle();
    static std::shared_ptr<Mesh> createM4A1();
    static std::shared_ptr<Mesh> createAK74M();
    static std::shared_ptr<Mesh> createMosinNagant();
    static std::shared_ptr<Mesh> createBerettaM9();

    // Layered sandbag defensive fortification with timber revetment
    static std::shared_ptr<Mesh> createDetailedSandbagWall();

    // Military supply crate with steel corner brackets and stencil marks
    static std::shared_ptr<Mesh> createDetailedCrate();

    // Realistic Pine Tree with textured trunk and 4-tier needle foliage canopy
    static std::shared_ptr<Mesh> createPineTree();

    // Concrete Pyramidal Anti-Tank "Dragon's Tooth" fortification
    static std::shared_ptr<Mesh> createDragonTooth();

    // HESCO Defensive Bastion (welded galvanized wire mesh, tan geotextile fabric, crushed rock infill)
    static std::shared_ptr<Mesh> createHescoBastion();

    // Military Command Post Field Tent with camouflage netting and structural frame
    static std::shared_ptr<Mesh> createCommandTent();

    // 55-Gallon Steel Fuel/Oil Drum with reinforcement ribs, chime rims, and bung plugs
    static std::shared_ptr<Mesh> createFuelDrum();

    // Heavy Slatted Cargo Pallet loaded with olive-drab munitions spam cans
    static std::shared_ptr<Mesh> createCargoPallet();

    // Military Tactical Diesel Generator with floodlight mast
    static std::shared_ptr<Mesh> createFieldGenerator();

    // Heavy 6x6 Tactical Military Transport Truck (chassis, mud tires, armored cab, canvas cargo bed)
    static std::shared_ptr<Mesh> createMilitaryTruck();

    // Barbed Wire Obstacle (wooden X-frame hedgehogs with coiled wire)
    static std::shared_ptr<Mesh> createBarbedWire();

    // Dense tactical steppe grass clump
    static std::shared_ptr<Mesh> createGrassClump();

    // Concealment bush & shrubbery
    static std::shared_ptr<Mesh> createBushFoliage();

    // Reinforced concrete Jersey highway barricade
    static std::shared_ptr<Mesh> createConcreteBarrier();

    // Battlefield smoke emitter pillar / burning wreck
    static std::shared_ptr<Mesh> createCampfireSmoke();

    // Tactical red illumination signal flare
    static std::shared_ptr<Mesh> createSignalFlare();

    // Heavy 12.7mm DShK / M2 Tripod-Mounted Machine Gun Emplacement
    static std::shared_ptr<Mesh> createMountedHMG();

    // Burnt-out combat tank / armored vehicle hulk (scorched steel hull, blown turret, road wheels)
    static std::shared_ptr<Mesh> createTankWreck();

    // Shattered concrete & brick building ruin with window apertures, exposed steel rebar, and rubble
    static std::shared_ptr<Mesh> createRuinedWall();

    // Heavy reinforced wooden & steel truss bridge segment for river crossings
    static std::shared_ptr<Mesh> createRiverBridge();

    // Animated / specular river water plane with realistic flow geometry
    static std::shared_ptr<Mesh> createRiverWater(float width, float length);

    // Modular Urban & Regional MMO Architecture
    static std::shared_ptr<Mesh> createUrbanApartment();
    static std::shared_ptr<Mesh> createIndustrialWarehouse();
    static std::shared_ptr<Mesh> createTownChurch();
    static std::shared_ptr<Mesh> createFreightBoxcar();
    static std::shared_ptr<Mesh> createStreetLamp();
    static std::shared_ptr<Mesh> createCommunicationTower();
    static std::shared_ptr<Mesh> createRefineryTank();

    // Multi-elevation procedural frontline terrain (rolling steppe hills, riverbed trough, trench berms, multi-tonal ground textures)
    static std::shared_ptr<Mesh> createWarzoneTerrain(float sizeX, float sizeZ, int subdivisionsX = 250, int subdivisionsZ = 0);
    static std::shared_ptr<Mesh> createArenaTerrain(float sizeX = 160.0f, float sizeZ = 160.0f, int subdivisions = 64);

    // Dense 3D procedural wind-animated grass blades and meadow tufts
    static std::shared_ptr<Mesh> createGrassField(float fieldSize, int numTufts);
    static std::shared_ptr<Mesh> createTrainingTerrain(float sizeX, float sizeZ, int subdivisions);
    static std::shared_ptr<Mesh> createTrainingGrassField(float fieldSize, int numTufts);

    // Training, Proving Grounds & Perimeter Defenses
    static std::shared_ptr<Mesh> createShootingTarget();
    static std::shared_ptr<Mesh> createWatchtower();
    static std::shared_ptr<Mesh> createGuardBooth();
    static std::shared_ptr<Mesh> createPerimeterFence();
    static std::shared_ptr<Mesh> createGunRack();
    static std::shared_ptr<Mesh> createObstacleWall();

    // Dynamic Severed Physical Geometry
    static std::shared_ptr<Mesh> createSeveredCylinder(float radius, float height, const glm::vec4& color = glm::vec4(0.24f, 0.26f, 0.28f, 1.0f), bool hasLampHead = false);
    static std::shared_ptr<Mesh> createStumpCylinder(float radius, float height, const glm::vec4& color = glm::vec4(0.24f, 0.26f, 0.28f, 1.0f));

    // Helper geometry builders
    static void appendBox(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                          const glm::vec3& center, const glm::vec3& size, const glm::vec4& color,
                          const glm::vec3& rotation = glm::vec3(0.0f));
    
    static void appendCylinder(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                              const glm::vec3& center, float radius, float height, const glm::vec4& color,
                              int segments = 12, float matType = 0.0f, bool capTop = false, bool capBottom = false);

    static void appendPyramid(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                             const glm::vec3& center, float baseWidth, float height, const glm::vec4& color, float matType = 0.0f);

    // Anatomical MetaHuman-Grade Smooth Geometry Builders (Smooth Normals & PBR Material Tags)
    static void appendSmoothEllipsoid(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                     const glm::vec3& center, const glm::vec3& radii, const glm::vec4& color,
                                     float matType = 0.0f, int segments = 16, int rings = 12);

    static void appendSmoothCapsule(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                   const glm::vec3& start, const glm::vec3& end, float radiusStart, float radiusEnd,
                                   const glm::vec4& color, float matType = 0.0f, int segments = 12);

    static void appendAnatomicalHead(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                    const glm::vec3& headCenter, const glm::vec4& skinCol,
                                    int faceId, int hairId, const glm::vec4& hairCol, int beardId);

    static void appendModernTacticalHelmet(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                          const glm::vec3& headCenter, const glm::vec4& camoCol, int helmetId);

    static void appendModernPlateCarrier(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                        const glm::vec3& torsoCenter, const glm::vec4& vestCol, int vestId);
};

} // namespace Mercenary

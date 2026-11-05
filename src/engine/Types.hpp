#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#ifndef MERCENARY_HEADLESS_SERVER
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <cstdint>

namespace Mercenary {

enum class Faction : uint8_t {
    None = 0,
    FactionA = 1, // Mercenary Coalition (UA / Allied Northern Forces - Multicam/Blue/Yellow)
    FactionB = 2  // Crimson Legion (RU / Eastern Shock Troops - EMR Flora/Red/White)
};

enum class CombatRole : uint8_t {
    Rifleman = 0,
    AutomaticRifleman = 1,
    Medic = 2,
    SquadLeader = 3
};

enum class GroundSurface : uint8_t {
    Dirt = 0,
    Mud = 1,
    Pavement = 2,
    Concrete = 3,
    Grass = 4,
    Wood = 5,
    Water = 6
};

enum class AcousticEnvironment : uint8_t {
    OpenField = 0,          // Outdoors: expansive rolling tail, Stokes atmospheric dissipation
    DenseForest = 1,        // Wooded canopy: scattered diffusion, early reflection notches
    TrenchNetwork = 2,      // Dirt/mud revetment trench: close earthen slapback (<18ms), channeled bottom-end
    ConcreteBunker = 3,     // Hard concrete bunker: harsh metallic slapback flutter (<8ms), sharp wall reflections, intense acoustic ear pressure
    IndustrialInterior = 4  // Cavernous industrial warehouse / damaged hall: large space, dense multipath diffuse reverb tail (1.2s - 1.8s)
};

inline const char* getAcousticEnvironmentName(AcousticEnvironment env) {
    switch (env) {
        case AcousticEnvironment::OpenField:          return "Open Field (Wide Open Acoustic Space)";
        case AcousticEnvironment::DenseForest:        return "Dense Forest (Diffused Foliage Reflection)";
        case AcousticEnvironment::TrenchNetwork:      return "Trench Network (Earthen Slapback)";
        case AcousticEnvironment::ConcreteBunker:     return "Concrete Bunker (Harsh Resonant Slapback)";
        case AcousticEnvironment::IndustrialInterior: return "Industrial Interior (Cavernous Echo)";
    }
    return "Unknown";
}

enum class CoverType : uint8_t {
    Exposed = 0, // Red cover (+15% damage, 0% suppression resistance)
    Light = 1,   // Yellow cover (-30% damage, -40% suppression resistance)
    Heavy = 2    // Green cover (-55% damage, -75% suppression resistance)
};

enum DismembermentFlags : uint32_t {
    DISMEMBER_NONE        = 0,
    DISMEMBER_HEAD        = 1 << 0, // Cranium/decapitation
    DISMEMBER_L_ARM       = 1 << 1, // Left arm severed
    DISMEMBER_R_ARM       = 1 << 2, // Right arm severed
    DISMEMBER_L_LEG       = 1 << 3, // Left leg severed
    DISMEMBER_R_LEG       = 1 << 4, // Right leg severed
    DISMEMBER_EXPLODED    = 1 << 5  // Total catastrophic explosion / obliterated
};

struct CharacterCustomization {
    int skinToneId = 1;   // 0: Fair Light, 1: Slavic/Tan, 2: Olive/Mediterranean, 3: Warm Deep
    int faceId = 0;       // 0: Nomad, 1: Razor, 2: Viper, 3: Titan
    int hairId = 0;       // 0: High & Tight Buzzcut, 1: Operator Undercut, 2: Military Crew Cut, 3: Shaved Bald
    int hairColorId = 0;  // 0: Dark Brown, 1: Jet Black, 2: Dirty Blonde, 3: Grizzled Silver
    int beardId = 1;      // 0: Tactical Full Beard, 1: Operator Stubble, 2: Tactical Goatee, 3: Clean Shaven
    int camoId = 0;       // Mercenary: 0=MultiCam OCP, 1=MM-14 Digital | Crimson: 2=Covert Charcoal, 3=M81 Woodland
    int helmetId = 0;     // 0: FAST MT High-Cut + ComTacs, 1: 6B47 Ratnik Helmet, 2: Tactical Boonie Hat, 3: Balaclava Only, 4: None (Bare Head)
    int vestId = 0;       // 0: Crye JPC 2.0 Plate Carrier, 1: 6B45 Heavy Ratnik Assault Armor, 2: D3CRX Chest Rig, 3: None (Fatigues)
    int backpackId = 1;   // 0: 3-Day Assault Pack 30L, 1: Compact Patrol Hydration Pack, 2: Slick / None
    int gloveId = 0;      // 0: Mechanix Coyote Tan, 1: Black Hard-Knuckle, 2: Olive Drab Mil-Spec
    int armbandId = 0;    // Mercenary: 0=Blue, 2=Yellow, 4=None | Crimson: 1=Red, 3=White, 4=None

    // --- MMO Parametric Facial Morphology Sliders ---
    float age = 28.0f;       // Operative Age: 20 to 65
    float melanin = 0.35f;   // Continuous melanin complexion: 0.0 (Fair Nordic) to 1.0 (Deep Warm)
    float jawWidth = 1.0f;   // Jaw width scale: 0.85 to 1.25
    float chinLength = 0.0f; // Chin length offset: -0.012 to +0.015
    float noseScale = 1.0f;  // Nose bridge & tip scale: 0.85 to 1.25

    bool operator==(const CharacterCustomization& o) const {
        return skinToneId == o.skinToneId && faceId == o.faceId && hairId == o.hairId &&
               hairColorId == o.hairColorId && beardId == o.beardId && camoId == o.camoId &&
               helmetId == o.helmetId && vestId == o.vestId && backpackId == o.backpackId &&
               gloveId == o.gloveId && armbandId == o.armbandId &&
               age == o.age && melanin == o.melanin && jawWidth == o.jawWidth &&
               chinLength == o.chinLength && noseScale == o.noseScale;
    }
    bool operator!=(const CharacterCustomization& o) const {
        return !(*this == o);
    }
};

} // namespace Mercenary

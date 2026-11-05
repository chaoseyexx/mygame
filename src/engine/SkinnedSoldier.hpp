#pragma once
#include "Types.hpp"
#include "Mesh.hpp"
#include "game/CharacterProfile.hpp"
#include <vector>
#include <memory>

namespace Mercenary {

enum CharacterBone : uint8_t {
    BONE_PELVIS = 0,
    BONE_SPINE_CHEST = 1,
    BONE_HEAD = 2,
    BONE_L_UPPER_ARM = 3,
    BONE_L_FOREARM = 4,
    BONE_L_HAND = 5,
    BONE_R_UPPER_ARM = 6,
    BONE_R_FOREARM = 7,
    BONE_R_HAND = 8,
    BONE_L_THIGH = 9,
    BONE_L_CALF = 10,
    BONE_L_FOOT = 11,
    BONE_R_THIGH = 12,
    BONE_R_CALF = 13,
    BONE_R_FOOT = 14,
    BONE_R_TRIGGER_FINGER = 15,
    NUM_CHARACTER_BONES = 16
};

enum CharacterPart : uint8_t {
    PART_BODY = 0,         // Uniform pants, combat boots, arms, hands, sleeves
    PART_FACE = 1,         // Anatomical cranium, face, jaw, cheeks, chin, neck
    PART_HELMET_FAST = 2,  // FAST MT Ballistic Helmet & Goggles
    PART_HAIR_BUZZCUT = 3, // Military Buzzcut
    PART_HAIR_UNDERCUT = 4,// Operator Undercut
    PART_HAIR_CREWCUT = 5, // Military Crew Cut
    PART_BEARD_FULL = 6,   // Tactical Full Beard
    PART_BEARD_GOATEE = 7, // Tactical Goatee
    PART_VEST = 8,         // Plate Carrier, Kangaroo pouches, Radio, Antenna
    PART_ARMBAND = 9       // IFF Identification Armband
};

struct SkinnedVertex {
    glm::vec3 bindPos;
    glm::vec3 bindNormal;
    glm::vec2 uv;
    glm::vec4 color; // RGB = tint/base, A = PBR material tag
    uint8_t bone0 = 0;
    uint8_t bone1 = 0;
    float weight0 = 1.0f;
    float weight1 = 0.0f;
    uint8_t part = PART_BODY;
};

struct SkeletonPose {
    glm::vec3 bonePositions[NUM_CHARACTER_BONES];
    glm::mat3 boneRotations[NUM_CHARACTER_BONES];
};

class SkinnedSoldier {
public:
    SkinnedSoldier();
    ~SkinnedSoldier();

    // Generate deformed mesh vertices and indices for the given skeleton pose and customization options
    void deform(const SkeletonPose& pose,
                const CharacterCustomization& custom,
                std::vector<Vertex>& outVertices,
                std::vector<uint32_t>& outIndices,
                uint32_t dismemberMask = 0) const;

    // Build the reference bind-pose soldier model
    void buildBindPoseModel();

    // Check if the reference model has been initialized
    bool isInitialized() const { return !m_bindVertices.empty(); }

    // Retrieve bind pose joint coordinate
    glm::vec3 getBindJoint(CharacterBone bone) const { return (bone < NUM_CHARACTER_BONES) ? m_bindJoints[bone] : glm::vec3(0.0f); }

    // Static helper to construct bone rotation aligning bind limb direction to posed limb direction
    static glm::mat3 computeLimbRotation(const glm::vec3& bindDir, const glm::vec3& posedDir, const glm::mat3& baseRot = glm::mat3(1.0f));

private:
    std::vector<SkinnedVertex> m_bindVertices;
    std::vector<uint32_t> m_indices;
    mutable std::vector<int> m_remapCache;

    glm::vec3 m_bindJoints[NUM_CHARACTER_BONES];

    // Sub-assembly builders for the realistic human military character
    void buildTorsoAndPants();
    void buildArmsAndSleeves();
    void buildTacticalGlovesAndFingers();
    void buildCombatBoots();
    void buildPlateCarrierAndGear();
    void buildBallisticHelmetAndEyewear();
    void buildAnatomicalHeadAndFace();
    void buildHairstyles();
    void buildFacialHair();
    void buildArmbands();

    // Helper to append a continuous skinned tube with smooth dual-bone weight blending
    void appendSkinnedTube(const std::vector<glm::vec3>& ringCenters,
                           const std::vector<glm::vec2>& ringRadii,
                           const std::vector<float>& ringWeightsBone0,
                           uint8_t bone0, uint8_t bone1,
                           const glm::vec2& uvMin, const glm::vec2& uvMax,
                           const glm::vec4& color, int radialSegments = 16,
                           const std::vector<float>& ringBulge = {},
                           uint8_t part = PART_BODY,
                           bool capTop = false,
                           const std::vector<uint8_t>& ringBones0 = {},
                           const std::vector<uint8_t>& ringBones1 = {});

    // Helper to build a physically thick 3D volumetric IFF identification armband
    void appendVolumetricArmband(const std::vector<glm::vec3>& ringCenters,
                                 const std::vector<glm::vec2>& outerRadii,
                                 const std::vector<glm::vec2>& innerRadii,
                                 const std::vector<float>& ringWeightsBone0,
                                 uint8_t bone0, uint8_t bone1,
                                 const glm::vec4& color, int radialSegments = 32);
};

} // namespace Mercenary

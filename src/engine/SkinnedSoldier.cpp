#include "SkinnedSoldier.hpp"

#include <cmath>

#include <algorithm>

#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Mercenary {

SkinnedSoldier::SkinnedSoldier() {

    buildBindPoseModel();

}

SkinnedSoldier::~SkinnedSoldier() {}

glm::mat3 SkinnedSoldier::computeLimbRotation(const glm::vec3& bindDir, const glm::vec3& posedDir, const glm::mat3& baseRot) {

    if (glm::length(posedDir) < 0.0001f || glm::length(bindDir) < 0.0001f) {

        return baseRot;

    }

    // 1. Transform bind limb vector into posed parent frame

    glm::vec3 vBindWorld = glm::normalize(baseRot * glm::normalize(bindDir));

    glm::vec3 vPosed = glm::normalize(posedDir);

    float cosTheta = glm::clamp(glm::dot(vBindWorld, vPosed), -1.0f, 1.0f);

    if (cosTheta > 0.99995f) {

        return baseRot;

    }

    if (cosTheta < -0.99995f) {

        glm::vec3 ortho = glm::cross(vBindWorld, glm::vec3(0.0f, 1.0f, 0.0f));

        if (glm::length(ortho) < 0.1f) ortho = glm::cross(vBindWorld, glm::vec3(1.0f, 0.0f, 0.0f));

        ortho = glm::normalize(ortho);

        return glm::mat3(glm::rotate(glm::mat4(1.0f), glm::pi<float>(), ortho)) * baseRot;

    }

    // 2. Compute smooth angular deflection in world frame and composite with base rotation

    glm::vec3 crossVec = glm::cross(vBindWorld, vPosed);
    float crossLen = glm::length(crossVec);
    if (crossLen < 0.0001f) {
        return baseRot;
    }
    glm::vec3 axis = crossVec / crossLen;

    float angle = std::acos(cosTheta);

    glm::mat3 aimRot = glm::mat3(glm::rotate(glm::mat4(1.0f), angle, axis));

    return aimRot * baseRot;

}

void SkinnedSoldier::buildBindPoseModel() {

    m_bindVertices.clear();

    m_indices.clear();

    m_bindVertices.reserve(16000);
    m_indices.reserve(32000);

    // 1. Establish Bind Pose Joint Coordinates (Athletic modern male operator, 1.82m height)

    m_bindJoints[BONE_PELVIS]          = glm::vec3( 0.00f, 0.88f,  0.00f);

    m_bindJoints[BONE_SPINE_CHEST]     = glm::vec3( 0.00f, 1.20f,  0.00f);

    m_bindJoints[BONE_HEAD]            = glm::vec3( 0.00f, 1.455f, 0.00f);

    m_bindJoints[BONE_L_UPPER_ARM]     = glm::vec3(-0.24f, 1.36f,  0.00f);

    m_bindJoints[BONE_L_FOREARM]       = glm::vec3(-0.32f, 1.06f,  0.00f);

    m_bindJoints[BONE_L_HAND]          = glm::vec3(-0.34f, 0.76f,  0.00f);

    m_bindJoints[BONE_R_UPPER_ARM]     = glm::vec3( 0.24f, 1.36f,  0.00f);

    m_bindJoints[BONE_R_FOREARM]       = glm::vec3( 0.32f, 1.06f,  0.00f);

    m_bindJoints[BONE_R_HAND]          = glm::vec3( 0.34f, 0.76f,  0.00f);

    m_bindJoints[BONE_L_THIGH]         = glm::vec3(-0.13f, 0.86f,  0.00f);

    m_bindJoints[BONE_L_CALF]          = glm::vec3(-0.13f, 0.46f,  0.00f);

    m_bindJoints[BONE_L_FOOT]          = glm::vec3(-0.13f, 0.08f,  0.00f);

    m_bindJoints[BONE_R_THIGH]         = glm::vec3( 0.13f, 0.86f,  0.00f);

    m_bindJoints[BONE_R_CALF]          = glm::vec3( 0.13f, 0.46f,  0.00f);

    m_bindJoints[BONE_R_FOOT]          = glm::vec3( 0.13f, 0.08f,  0.00f);

    // Rigged index trigger finger bone matches the exact MCP base of the right index finger

    m_bindJoints[BONE_R_TRIGGER_FINGER] = glm::vec3(0.324f, 0.706f, 0.002f);

    // 2. Build All Sub-Assemblies with Continuous Topology and PBR Material Tags

    buildTorsoAndPants();

    buildArmsAndSleeves();

    buildTacticalGlovesAndFingers();

    buildCombatBoots();

    buildPlateCarrierAndGear();

    buildBallisticHelmetAndEyewear();

    buildAnatomicalHeadAndFace();

    buildHairstyles();

    buildFacialHair();

    buildArmbands();

    std::cout << "[SkinnedSoldier] Generated modern military operator mesh ("

              << m_bindVertices.size() << " verts, " << (m_indices.size() / 3) << " tris)\n" << std::flush;

}

void SkinnedSoldier::appendSkinnedTube(

    const std::vector<glm::vec3>& ringCenters,

    const std::vector<glm::vec2>& ringRadii,

    const std::vector<float>& ringWeightsBone0,

    uint8_t bone0, uint8_t bone1,

    const glm::vec2& uvMin, const glm::vec2& uvMax,

    const glm::vec4& color, int radialSegments,

    const std::vector<float>& ringBulge,

    uint8_t part,

    bool capTop,
    const std::vector<uint8_t>& ringBones0,
    const std::vector<uint8_t>& ringBones1)

{

    size_t numRings = std::min({ringCenters.size(), ringRadii.size(), ringWeightsBone0.size()});

    if (numRings < 2) return;

    uint32_t baseVertex = static_cast<uint32_t>(m_bindVertices.size());

    for (size_t r = 0; r < numRings; ++r) {

        float tV = static_cast<float>(r) / static_cast<float>(numRings - 1);

        float v = uvMin.y + tV * (uvMax.y - uvMin.y);

        float w0 = ringWeightsBone0[r];

        float w1 = 1.0f - w0;

        uint8_t curBone0 = (!ringBones0.empty() && r < ringBones0.size()) ? ringBones0[r] : bone0;
        uint8_t curBone1 = (!ringBones1.empty() && r < ringBones1.size()) ? ringBones1[r] : bone1;

        float rx = ringRadii[r].x;

        float rz = ringRadii[r].y;

        float extraBulge = (r < ringBulge.size()) ? ringBulge[r] : 0.0f;

        // V tangent between previous and next ring centers & radii

        glm::vec3 cPrev = (r > 0) ? ringCenters[r - 1] : ringCenters[r];

        glm::vec3 cNext = (r + 1 < numRings) ? ringCenters[r + 1] : ringCenters[r];

        float rxPrev = (r > 0) ? (ringRadii[r - 1].x + ((r - 1 < ringBulge.size()) ? ringBulge[r - 1] : 0.0f)) : (rx + extraBulge);

        float rxNext = (r + 1 < numRings) ? (ringRadii[r + 1].x + ((r + 1 < ringBulge.size()) ? ringBulge[r + 1] : 0.0f)) : (rx + extraBulge);

        float rzPrev = (r > 0) ? (ringRadii[r - 1].y + ((r - 1 < ringBulge.size()) ? ringBulge[r - 1] : 0.0f)) : (rz + extraBulge);

        float rzNext = (r + 1 < numRings) ? (ringRadii[r + 1].y + ((r + 1 < ringBulge.size()) ? ringBulge[r + 1] : 0.0f)) : (rz + extraBulge);

        for (int s = 0; s < radialSegments; ++s) {

            float tU = static_cast<float>(s) / static_cast<float>(radialSegments);

            float u = uvMin.x + tU * (uvMax.x - uvMin.x);

            float theta = tU * 2.0f * glm::pi<float>();

            float cosT = std::cos(theta);

            float sinT = std::sin(theta);

            // Subtle organic cloth ripples & tension micro-folds for military combat fabric

            float wrinkle = 0.0f;

            if (part == PART_BODY) {

                float freq = static_cast<float>(r) * 1.85f;

                wrinkle = std::sin(theta * 3.0f + freq) * 0.0022f + std::cos(theta * 2.0f - freq * 1.4f) * 0.0018f;

            }

            float radX = rx + extraBulge + wrinkle;

            float radZ = rz + extraBulge + wrinkle;

            glm::vec3 p = ringCenters[r] + glm::vec3(cosT * radX, 0.0f, sinT * radZ);

            // Calculate precise 3D surface normal using circumferential and longitudinal slopes

            glm::vec3 pPrevPoint = cPrev + glm::vec3(cosT * rxPrev, 0.0f, sinT * rzPrev);

            glm::vec3 pNextPoint = cNext + glm::vec3(cosT * rxNext, 0.0f, sinT * rzNext);

            glm::vec3 tanV = pNextPoint - pPrevPoint;

            if (glm::length(tanV) < 0.0001f) tanV = glm::vec3(0.0f, 1.0f, 0.0f);

            glm::vec3 tanU = glm::vec3(-sinT * radX, 0.0f, cosT * radZ);

            glm::vec3 n = glm::cross(tanV, tanU);

            if (glm::length(n) > 0.0001f) {

                n = glm::normalize(n);

            } else {

                n = glm::normalize(glm::vec3(cosT / rx, 0.0f, sinT / rz));

            }

            // Enforce that normal always points strictly outward away from the tube axis
            glm::vec3 radialDir = p - ringCenters[r];
            radialDir.y = 0.0f;
            if (glm::length(radialDir) > 0.0001f) {
                radialDir = glm::normalize(radialDir);
                if (glm::dot(n, radialDir) < 0.0f) {
                    n = -n;
                }
            }

            SkinnedVertex sv;

            sv.bindPos = p;

            sv.bindNormal = n;

            sv.uv = glm::vec2(u, v);

            sv.color = color;

            sv.bone0 = curBone0;

            sv.bone1 = curBone1;

            sv.weight0 = w0;

            sv.weight1 = w1;

            sv.part = part;

            m_bindVertices.push_back(sv);

        }

    }

    // Detect if tube is built downwards (arms/legs from shoulder/hip to wrist/foot)
    // or upwards (torso/head from pelvis/neck to skull)
    bool goesDown = (ringCenters.back().y < ringCenters.front().y);

    // Connect rings with strictly CCW outward-facing quad triangles
    for (size_t r = 0; r < numRings - 1; ++r) {
        for (int s = 0; s < radialSegments; ++s) {
            int nextS = (s + 1) % radialSegments;

            uint32_t i00 = baseVertex + static_cast<uint32_t>(r * radialSegments + s);
            uint32_t i01 = baseVertex + static_cast<uint32_t>(r * radialSegments + nextS);
            uint32_t i10 = baseVertex + static_cast<uint32_t>((r + 1) * radialSegments + s);
            uint32_t i11 = baseVertex + static_cast<uint32_t>((r + 1) * radialSegments + nextS);

            if (goesDown) {
                // When rings go downwards (r=0 top, r+1 bottom), this winding faces outwards:
                m_indices.push_back(i00);
                m_indices.push_back(i01);
                m_indices.push_back(i10);

                m_indices.push_back(i01);
                m_indices.push_back(i11);
                m_indices.push_back(i10);
            } else {
                // When rings go upwards (r=0 bottom, r+1 top), this winding faces outwards:
                m_indices.push_back(i00);
                m_indices.push_back(i10);
                m_indices.push_back(i01);

                m_indices.push_back(i01);
                m_indices.push_back(i10);
                m_indices.push_back(i11);
            }
        }
    }

    // Seal top ring with center apex cap fan if requested

    if (capTop && numRings > 0) {

        uint32_t topApexIdx = static_cast<uint32_t>(m_bindVertices.size());

        SkinnedVertex apexVert;

        apexVert.bindPos = ringCenters.back() + glm::vec3(0.0f, ringRadii.back().y * 0.20f, 0.0f);

        apexVert.bindNormal = glm::vec3(0.0f, 1.0f, 0.0f);

        apexVert.uv = (uvMin + uvMax) * 0.5f;

        apexVert.color = color;

        apexVert.bone0 = bone0;

        apexVert.bone1 = bone1;

        apexVert.weight0 = ringWeightsBone0.back();

        apexVert.weight1 = 1.0f - ringWeightsBone0.back();

        apexVert.part = part;

        m_bindVertices.push_back(apexVert);

        size_t lastRing = numRings - 1;

        for (int s = 0; s < radialSegments; ++s) {

            int nextS = (s + 1) % radialSegments;

            uint32_t i0 = baseVertex + static_cast<uint32_t>(lastRing * radialSegments + s);

            uint32_t i1 = baseVertex + static_cast<uint32_t>(lastRing * radialSegments + nextS);

            m_indices.push_back(topApexIdx);

            m_indices.push_back(i1);

            m_indices.push_back(i0);

        }

    }

}

// ----------------------------------------------------------------------------

// 1. TORSO, WAIST & COMBAT PANTS (CRYE G3)

// ----------------------------------------------------------------------------

void SkinnedSoldier::buildTorsoAndPants() {

    glm::vec4 uniformMat(1.0f, 1.0f, 1.0f, 1.00f); // Mat Tag 1.0 = Ripstop Fabric (roughness 0.74, metallic 0.0)

    glm::vec4 gearMat(0.18f, 0.18f, 0.18f, 0.80f);    // Tactical Polymer

    glm::vec4 buckleMat(0.12f, 0.12f, 0.12f, 0.70f);  // Cobra Metal Buckle

    // --- TORSO (Pelvis to Mandarin Collar with Anatomical Ribs & Pectorals) ---

    std::vector<glm::vec3> torsoCenters = {
        glm::vec3(0.0f, 0.84f,  0.005f), // Pelvic floor / perineum base
        glm::vec3(0.0f, 0.88f,  0.004f), // Lower pelvis / hip line
        glm::vec3(0.0f, 0.94f,  0.000f), // Iliac crest / Belt line
        glm::vec3(0.0f, 1.00f, -0.005f), // Navel / Lower abdomen
        glm::vec3(0.0f, 1.06f, -0.008f), // Athletic tapered waist
        glm::vec3(0.0f, 1.14f, -0.005f), // Lower ribs / Serratus anterior
        glm::vec3(0.0f, 1.22f,  0.003f), // Thorax / Mid chest
        glm::vec3(0.0f, 1.30f,  0.010f), // Latissimus dorsi & pectoral base
        glm::vec3(0.0f, 1.35f,  0.016f), // Pectoral major prominence
        glm::vec3(0.0f, 1.37f,  0.008f), // Clavicle / Shoulder line
        glm::vec3(0.0f, 1.395f, 0.002f)  // Mandarin zipper collar (snugly wraps over neck base!)
    };

    std::vector<glm::vec2> torsoRadii = {
        glm::vec2(0.180f, 0.126f), // Pelvis floor
        glm::vec2(0.182f, 0.126f), // Lower pelvis
        glm::vec2(0.176f, 0.120f), // Belt line
        glm::vec2(0.168f, 0.114f), // Navel
        glm::vec2(0.162f, 0.110f), // Tapered athletic waist
        glm::vec2(0.174f, 0.118f), // Lower ribs
        glm::vec2(0.188f, 0.126f), // Mid chest
        glm::vec2(0.198f, 0.132f), // Chest breadth
        glm::vec2(0.208f, 0.138f), // Pectorals
        glm::vec2(0.165f, 0.110f), // Clavicle
        glm::vec2(0.065f, 0.068f)  // Mandarin collar (snugly wraps over neck base!)
    };

    // Smooth dual-bone weighting from Pelvis -> Spine/Chest

    std::vector<float> torsoWeights = {

        1.00f, // 100% Pelvis

        0.90f, // 90% Pelvis, 10% Spine

        0.75f, // 75% Pelvis, 25% Spine

        0.50f, // 50% Pelvis, 50% Spine

        0.25f, // 25% Pelvis, 75% Spine

        0.05f, // 5% Pelvis, 95% Spine

        0.00f, // 100% Spine/Chest

        0.00f, // 100% Spine/Chest

        0.00f, // 100% Spine/Chest

        0.00f, // 100% Spine/Chest

        0.00f  // 100% Spine/Chest (collar base is 100% Spine/Chest!)

    };

    appendSkinnedTube(torsoCenters, torsoRadii, torsoWeights,

                      BONE_PELVIS, BONE_SPINE_CHEST,

                      glm::vec2(0.05f, 0.05f), glm::vec2(0.45f, 0.45f),

                      uniformMat, 32);

    // Tactical Heavy-Duty Rigger Belt with Cobra Buckle (Rigged to Pelvis)

    std::vector<glm::vec3> beltCenters = {

        glm::vec3(0.0f, 0.925f, 0.000f),

        glm::vec3(0.0f, 0.955f, 0.000f)

    };

    std::vector<glm::vec2> beltRadii = {

        glm::vec2(0.182f, 0.125f),

        glm::vec2(0.182f, 0.125f)

    };

    std::vector<float> beltWeights = { 0.85f, 0.85f };

    appendSkinnedTube(beltCenters, beltRadii, beltWeights,

                      BONE_PELVIS, BONE_SPINE_CHEST,

                      glm::vec2(0.15f, 0.80f), glm::vec2(0.35f, 0.90f),

                      gearMat, 32);

    // AustriAlpin Cobra Quick-Release Metal Buckle at front center

    uint32_t buckBase = static_cast<uint32_t>(m_bindVertices.size());

    glm::vec3 buckCenter(0.0f, 0.940f, 0.128f);

    glm::vec3 buckBox[8] = {

        buckCenter + glm::vec3(-0.028f, -0.016f, -0.004f),

        buckCenter + glm::vec3( 0.028f, -0.016f, -0.004f),

        buckCenter + glm::vec3( 0.028f,  0.016f, -0.004f),

        buckCenter + glm::vec3(-0.028f,  0.016f, -0.004f),

        buckCenter + glm::vec3(-0.028f, -0.016f,  0.008f),

        buckCenter + glm::vec3( 0.028f, -0.016f,  0.008f),

        buckCenter + glm::vec3( 0.028f,  0.016f,  0.008f),

        buckCenter + glm::vec3(-0.028f,  0.016f,  0.008f)

    };

    for (int i = 0; i < 8; ++i) {

        SkinnedVertex bv;

        bv.bindPos = buckBox[i];

        bv.bindNormal = (i >= 4) ? glm::vec3(0, 0, 1.0f) : glm::vec3(0, 0, -1.0f);

        bv.uv = glm::vec2(0.25f, 0.85f);

        bv.color = buckleMat;

        bv.bone0 = BONE_PELVIS;

        bv.bone1 = BONE_SPINE_CHEST;

        bv.weight0 = 0.85f;

        bv.weight1 = 0.15f;

        bv.part = PART_BODY;

        m_bindVertices.push_back(bv);

    }

    // Front face

    m_indices.push_back(buckBase + 4); m_indices.push_back(buckBase + 5); m_indices.push_back(buckBase + 6);

    m_indices.push_back(buckBase + 4); m_indices.push_back(buckBase + 6); m_indices.push_back(buckBase + 7);

    // Top & Bottom

    m_indices.push_back(buckBase + 3); m_indices.push_back(buckBase + 2); m_indices.push_back(buckBase + 6);

    m_indices.push_back(buckBase + 3); m_indices.push_back(buckBase + 6); m_indices.push_back(buckBase + 7);

    m_indices.push_back(buckBase + 0); m_indices.push_back(buckBase + 4); m_indices.push_back(buckBase + 5);

    m_indices.push_back(buckBase + 0); m_indices.push_back(buckBase + 5); m_indices.push_back(buckBase + 1);

    // --- LEGS & CRYE G3 COMBAT PANTS WITH AIRFLEX KNEEPADS ---

    for (int side = 0; side < 2; ++side) {

        float sX = (side == 0) ? -0.13f : 0.13f;

        uint8_t thighBone = (side == 0) ? BONE_L_THIGH : BONE_R_THIGH;

        uint8_t calfBone  = (side == 0) ? BONE_L_CALF  : BONE_R_CALF;

        std::vector<glm::vec3> legCenters = {

            glm::vec3(sX, 0.88f,  0.005f), // 0: Hip socket interior overlap (prevents groin holes!)

            glm::vec3(sX, 0.82f,  0.004f), // 1: Upper gluteus

            glm::vec3(sX, 0.74f,  0.002f), // 2: Upper quadriceps

            glm::vec3(sX, 0.66f,  0.000f), // 3: Mid thigh (Cargo pocket zone)

            glm::vec3(sX, 0.58f, -0.004f), // 4: Lower quadriceps

            glm::vec3(sX, 0.51f,  0.006f), // 5: Suprapatellar fold

            glm::vec3(sX, 0.48f,  0.014f), // 6: Upper patella

            glm::vec3(sX, 0.45f,  0.020f), // 7: Patella peak (kneecap)

            glm::vec3(sX, 0.42f,  0.012f), // 8: Infrapatellar fat pad

            glm::vec3(sX, 0.38f, -0.004f), // 9: Below knee tendon

            glm::vec3(sX, 0.33f, -0.006f), // 10: Gastrocnemius upper calf peak

            glm::vec3(sX, 0.27f, -0.005f), // 11: Mid calf belly

            glm::vec3(sX, 0.21f, -0.002f), // 12: Lower calf / Soleus taper

            glm::vec3(sX, 0.15f,  0.000f), // 13: Shin / Achilles tendon

            glm::vec3(sX, 0.11f,  0.006f), // 14: Bloused pant cuff roll

            glm::vec3(sX, 0.08f,  0.002f)  // 15: Boot collar tuck

        };

        std::vector<glm::vec2> legRadii = {

            glm::vec2(0.090f, 0.086f), // 0

            glm::vec2(0.088f, 0.084f), // 1

            glm::vec2(0.086f, 0.082f), // 2

            glm::vec2(0.083f, 0.079f), // 3

            glm::vec2(0.076f, 0.072f), // 4

            glm::vec2(0.070f, 0.068f), // 5

            glm::vec2(0.068f, 0.067f), // 6

            glm::vec2(0.068f, 0.067f), // 7

            glm::vec2(0.066f, 0.064f), // 8

            glm::vec2(0.064f, 0.062f), // 9

            glm::vec2(0.072f, 0.070f), // 10 (Calf muscle belly swell)

            glm::vec2(0.066f, 0.063f), // 11

            glm::vec2(0.058f, 0.055f), // 12

            glm::vec2(0.054f, 0.052f), // 13

            glm::vec2(0.064f, 0.062f), // 14 (Flared blousing roll)

            glm::vec2(0.055f, 0.053f)  // 15

        };

        // Smooth cosine skinning across the knee joint

        std::vector<float> legWeights = {

            1.00f, 1.00f, 1.00f, 0.95f, 0.85f,

            0.68f, 0.52f, 0.48f, 0.32f, 0.15f,

            0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f

        };

        // Knee joint bulge compensation eliminates LBS volume loss when running/kneeling
        std::vector<float> legBulge = {
            0.000f, 0.000f, 0.000f, 0.000f, 0.004f,
            0.010f, 0.012f, 0.009f, 0.004f, 0.000f,
            0.000f, 0.000f
        };

        appendSkinnedTube(legCenters, legRadii, legWeights,
                          thighBone, calfBone,
                          glm::vec2(0.05f, 0.05f), glm::vec2(0.45f, 0.45f),
                          uniformMat, 32, legBulge);

        // Seal top hip / groin socket with smooth convex fan
        uint32_t hipApexIdx = static_cast<uint32_t>(m_bindVertices.size());
        SkinnedVertex hipApex;
        hipApex.bindPos = legCenters[0] + glm::vec3(0.0f, 0.012f, 0.0f);
        hipApex.bindNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        hipApex.uv = glm::vec2(0.25f, 0.25f);
        hipApex.color = uniformMat;
        hipApex.bone0 = thighBone;
        hipApex.bone1 = calfBone;
        hipApex.weight0 = 1.0f;
        hipApex.weight1 = 0.0f;
        hipApex.part = PART_BODY;
        m_bindVertices.push_back(hipApex);

        uint32_t hipRing0Base = hipApexIdx - static_cast<uint32_t>(legCenters.size() * 32);
        for (int s = 0; s < 32; ++s) {
            int nextS = (s + 1) % 32;
            m_indices.push_back(hipApexIdx);
            m_indices.push_back(hipRing0Base + s);
            m_indices.push_back(hipRing0Base + nextS);
        }

        // 3D Bellows Cargo Pocket on Thigh with Physical Depth

        float flapX = (side == 0) ? -0.218f : 0.218f;

        glm::vec3 pocketCenter(flapX, 0.65f, 0.005f);

        uint32_t pocBase = static_cast<uint32_t>(m_bindVertices.size());

        float pW = 0.048f, pH = 0.075f, pD = 0.024f;

        float dirX = (side == 0) ? -1.0f : 1.0f;

        glm::vec3 pBox[8] = {

            pocketCenter + glm::vec3(0,         -pH, -pW),

            pocketCenter + glm::vec3(0,         -pH,  pW),

            pocketCenter + glm::vec3(0,          pH,  pW),

            pocketCenter + glm::vec3(0,          pH, -pW),

            pocketCenter + glm::vec3(dirX * pD, -pH, -pW),

            pocketCenter + glm::vec3(dirX * pD, -pH,  pW),

            pocketCenter + glm::vec3(dirX * pD,  pH,  pW),

            pocketCenter + glm::vec3(dirX * pD,  pH, -pW)

        };

        for (int i = 0; i < 8; ++i) {

            SkinnedVertex psv;

            psv.bindPos = pBox[i];

            psv.bindNormal = (i >= 4) ? glm::vec3(dirX, 0, 0) : glm::vec3(-dirX, 0, 0);

            psv.uv = glm::vec2(0.20f, 0.35f);

            psv.color = uniformMat;

            psv.bone0 = thighBone;

            psv.bone1 = thighBone;

            psv.weight0 = 1.0f;

            psv.weight1 = 0.0f;

            psv.part = PART_BODY;

            m_bindVertices.push_back(psv);

        }

        if (side == 0) {
            // Left thigh pocket (dirX = -1.0f): Invert index order to preserve outward CCW winding
            // Outer face
            m_indices.push_back(pocBase + 4); m_indices.push_back(pocBase + 6); m_indices.push_back(pocBase + 5);
            m_indices.push_back(pocBase + 4); m_indices.push_back(pocBase + 7); m_indices.push_back(pocBase + 6);

            // Top flap
            m_indices.push_back(pocBase + 3); m_indices.push_back(pocBase + 6); m_indices.push_back(pocBase + 2);
            m_indices.push_back(pocBase + 3); m_indices.push_back(pocBase + 7); m_indices.push_back(pocBase + 6);

            // Front & Back bellows
            m_indices.push_back(pocBase + 2); m_indices.push_back(pocBase + 5); m_indices.push_back(pocBase + 1);
            m_indices.push_back(pocBase + 2); m_indices.push_back(pocBase + 6); m_indices.push_back(pocBase + 5);

            m_indices.push_back(pocBase + 0); m_indices.push_back(pocBase + 7); m_indices.push_back(pocBase + 3);
            m_indices.push_back(pocBase + 0); m_indices.push_back(pocBase + 4); m_indices.push_back(pocBase + 7);
        } else {
            // Right thigh pocket (dirX = 1.0f): Outward CCW winding
            // Outer face
            m_indices.push_back(pocBase + 4); m_indices.push_back(pocBase + 5); m_indices.push_back(pocBase + 6);
            m_indices.push_back(pocBase + 4); m_indices.push_back(pocBase + 6); m_indices.push_back(pocBase + 7);

            // Top flap
            m_indices.push_back(pocBase + 3); m_indices.push_back(pocBase + 2); m_indices.push_back(pocBase + 6);
            m_indices.push_back(pocBase + 3); m_indices.push_back(pocBase + 6); m_indices.push_back(pocBase + 7);

            // Front & Back bellows
            m_indices.push_back(pocBase + 2); m_indices.push_back(pocBase + 1); m_indices.push_back(pocBase + 5);
            m_indices.push_back(pocBase + 2); m_indices.push_back(pocBase + 5); m_indices.push_back(pocBase + 6);

            m_indices.push_back(pocBase + 0); m_indices.push_back(pocBase + 3); m_indices.push_back(pocBase + 7);
            m_indices.push_back(pocBase + 0); m_indices.push_back(pocBase + 7); m_indices.push_back(pocBase + 4);
        }

    }

}

// ----------------------------------------------------------------------------

// 2. ARMS & SLEEVES (CRYE G3 COMBAT SHIRT)

// ----------------------------------------------------------------------------

void SkinnedSoldier::buildArmsAndSleeves() {
    glm::vec4 uniformMat(1.0f, 1.0f, 1.0f, 1.00f);

    for (int side = 0; side < 2; ++side) {
        float sX = (side == 0) ? -1.0f : 1.0f;
        uint8_t upperBone = (side == 0) ? BONE_L_UPPER_ARM : BONE_R_UPPER_ARM;
        uint8_t foreBone  = (side == 0) ? BONE_L_FOREARM   : BONE_R_FOREARM;

        // Smooth continuous 12 rings along arm from clavicle/shoulder through elbow into wrist
        std::vector<glm::vec3> armCenters = {
            glm::vec3(sX * 0.185f, 1.360f,  0.002f), // 0: Clavicle socket / Deltoid root
            glm::vec3(sX * 0.230f, 1.340f,  0.000f), // 1: Deltoid peak / acromion curve
            glm::vec3(sX * 0.260f, 1.285f, -0.003f), // 2: Deltoid belly
            glm::vec3(sX * 0.280f, 1.225f, -0.005f), // 3: Deltoid insertion / Bicep peak
            glm::vec3(sX * 0.298f, 1.165f, -0.008f), // 4: Mid bicep
            glm::vec3(sX * 0.312f, 1.110f, -0.010f), // 5: Lower bicep above elbow
            glm::vec3(sX * 0.320f, 1.060f, -0.012f), // 6: Elbow crease / olecranon point
            glm::vec3(sX * 0.325f, 1.000f, -0.008f), // 7: Forearm proximal head
            glm::vec3(sX * 0.330f, 0.940f, -0.002f), // 8: Brachioradialis forearm bulge
            glm::vec3(sX * 0.335f, 0.870f,  0.002f), // 9: Mid forearm
            glm::vec3(sX * 0.338f, 0.810f,  0.005f), // 10: Lower forearm
            glm::vec3(sX * 0.340f, 0.770f,  0.008f)  // 11: Wrist sleeve cuff
        };

        std::vector<glm::vec2> armRadii = {
            glm::vec2(0.056f, 0.054f), // 0: Clavicle socket
            glm::vec2(0.060f, 0.058f), // 1: Deltoid peak
            glm::vec2(0.058f, 0.056f), // 2: Deltoid belly
            glm::vec2(0.056f, 0.054f), // 3: Bicep peak
            glm::vec2(0.054f, 0.052f), // 4: Mid bicep
            glm::vec2(0.050f, 0.048f), // 5: Lower bicep
            glm::vec2(0.046f, 0.044f), // 6: Elbow point
            glm::vec2(0.046f, 0.044f), // 7: Proximal forearm
            glm::vec2(0.048f, 0.046f), // 8: Brachioradialis
            glm::vec2(0.045f, 0.043f), // 9: Mid forearm
            glm::vec2(0.042f, 0.040f), // 10: Lower forearm
            glm::vec2(0.043f, 0.041f)  // 11: Wrist cuff
        };

        std::vector<uint8_t> armBones0 = {
            BONE_SPINE_CHEST, // 0: Clavicle socket
            BONE_SPINE_CHEST, // 1: Deltoid peak
            BONE_SPINE_CHEST, // 2: Deltoid belly
            upperBone,        // 3: Deltoid insertion / Bicep peak
            upperBone,        // 4: Mid bicep
            upperBone,        // 5: Lower bicep
            upperBone,        // 6: Elbow crease
            upperBone,        // 7: Proximal forearm
            foreBone,         // 8: Brachioradialis
            foreBone,         // 9: Mid forearm
            foreBone,         // 10: Lower forearm
            foreBone          // 11: Wrist cuff
        };

        std::vector<uint8_t> armBones1 = {
            upperBone, // 0
            upperBone, // 1
            upperBone, // 2
            upperBone, // 3
            upperBone, // 4
            foreBone,  // 5
            foreBone,  // 6
            foreBone,  // 7
            foreBone,  // 8
            foreBone,  // 9
            foreBone,  // 10
            foreBone   // 11
        };

        std::vector<float> armWeights0 = {
            0.80f, // 0: 80% Chest, 20% UpperArm
            0.50f, // 1: 50% Chest, 50% UpperArm
            0.20f, // 2: 20% Chest, 80% UpperArm
            1.00f, // 3: 100% UpperArm
            1.00f, // 4: 100% UpperArm
            0.85f, // 5: 85% UpperArm, 15% Forearm
            0.50f, // 6: 50% UpperArm, 50% Forearm (Elbow crease)
            0.15f, // 7: 15% UpperArm, 85% Forearm
            1.00f, // 8: 100% Forearm
            1.00f, // 9: 100% Forearm
            1.00f, // 10: 100% Forearm
            1.00f  // 11: 100% Forearm
        };

        std::vector<float> armBulge = {
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.006f, 0.012f, 0.006f, 0.0f, 0.0f, 0.0f, 0.0f
        };

        appendSkinnedTube(armCenters, armRadii, armWeights0,
                          upperBone, upperBone,
                          glm::vec2(0.05f, 0.05f), glm::vec2(0.45f, 0.45f),
                          uniformMat, 32, armBulge, PART_BODY, false,
                          armBones0, armBones1);

        // Smooth rounded deltoid cap tucked cleanly into torso shoulder socket
        uint32_t shRing0Base = static_cast<uint32_t>(m_bindVertices.size()) - static_cast<uint32_t>(armCenters.size() * 32);
        uint32_t shApexIdx = static_cast<uint32_t>(m_bindVertices.size());
        SkinnedVertex shApex;
        shApex.bindPos = glm::vec3(sX * 0.168f, 1.368f, 0.002f);
        shApex.bindNormal = glm::normalize(glm::vec3(-sX * 0.35f, 0.90f, 0.0f));
        shApex.uv = glm::vec2(0.25f, 0.25f);
        shApex.color = uniformMat;
        shApex.bone0 = BONE_SPINE_CHEST;
        shApex.bone1 = upperBone;
        shApex.weight0 = 0.85f;
        shApex.weight1 = 0.15f;
        shApex.part = PART_BODY;
        m_bindVertices.push_back(shApex);

        for (int s = 0; s < 32; ++s) {
            int nextS = (s + 1) % 32;
            m_indices.push_back(shApexIdx);
            m_indices.push_back(shRing0Base + s);
            m_indices.push_back(shRing0Base + nextS);
        }
    }
}

// ----------------------------------------------------------------------------
// 3. TACTICAL GLOVES & 5-FINGER ARTICULATION (Oakley Factory Pilot / Mechanix M-Pact)
// ----------------------------------------------------------------------------
void SkinnedSoldier::buildTacticalGlovesAndFingers() {

    glm::vec4 gloveMat(0.24f, 0.24f, 0.24f, 0.60f);   // Mat Tag 0.60 = Combat Leather / Mechanix Wear

    glm::vec4 knuckleMat(0.12f, 0.12f, 0.12f, 0.80f); // Mat Tag 0.80 = Carbon Fiber Molded Knuckles

    glm::vec4 seamMat(0.18f, 0.18f, 0.18f, 0.60f);    // Reinforced Clarino leather fingertip pad

    for (int side = 0; side < 2; ++side) {

        float sX = (side == 0) ? -1.0f : 1.0f;

        uint8_t foreBone = (side == 0) ? BONE_L_FOREARM : BONE_R_FOREARM;

        uint8_t handBone = (side == 0) ? BONE_L_HAND : BONE_R_HAND;

        glm::vec3 handRoot = m_bindJoints[handBone];

        // ---------------------------------------------------------------------

        // 1. ANATOMICAL PALM & WRIST CUFF (Dual-Bone LBS blend with forearm cuff)

        // ---------------------------------------------------------------------

        // Smoothly bridges from sleeve cuff (Y=0.77m) down to metacarpal knuckles (Y=0.706m)

        std::vector<glm::vec3> palmRings = {

            handRoot + glm::vec3(0.0f,  0.014f, -0.002f), // Ring 0: Glove elastic wrist cuff

            handRoot + glm::vec3(0.0f, -0.006f,  0.000f), // Ring 1: Carpal joint / wrist flex crease

            handRoot + glm::vec3(0.0f, -0.028f,  0.004f), // Ring 2: Mid-palm (thenar / hypothenar mound)

            handRoot + glm::vec3(0.0f, -0.054f,  0.006f)  // Ring 3: Metacarpophalangeal (MCP) knuckle arch

        };

        std::vector<glm::vec2> palmRadii = {

            glm::vec2(0.038f, 0.030f), // Wrist cuff

            glm::vec2(0.037f, 0.027f), // Carpal crease

            glm::vec2(0.041f, 0.024f), // Mid palm

            glm::vec2(0.044f, 0.022f)  // Knuckle arch

        };

        // Dual-bone weight distribution:

        // Ring 0: 35% Forearm, 65% Hand (seamlessly bridges sleeve cuff without tearing!)

        // Ring 1: 10% Forearm, 90% Hand

        // Ring 2: 0% Forearm, 100% Hand

        // Ring 3: 0% Forearm, 100% Hand

        std::vector<float> palmWeights = { 0.35f, 0.10f, 0.0f, 0.0f };

        appendSkinnedTube(palmRings, palmRadii, palmWeights,

                          foreBone, handBone,

                          glm::vec2(0.25f, 0.55f), glm::vec2(0.45f, 0.95f),

                          gloveMat, 12);

        // ---------------------------------------------------------------------

        // 2. MOLDED HARD-KNUCKLE CARBON FIBER ARMOR (Oakley / Mechanix M-Pact)

        // ---------------------------------------------------------------------

        // Contoured protective plate curved across the 4 MCP knuckles on the dorsum (Z < 0)

        uint32_t knBase = static_cast<uint32_t>(m_bindVertices.size());

        glm::vec3 knCenter = handRoot + glm::vec3(0.0f, -0.046f, -0.025f);

        // 8-point faceted carbon fiber shell with dual airflow vents

        for (int k = 0; k < 8; ++k) {

            float a = (static_cast<float>(k) / 8.0f) * 2.0f * glm::pi<float>();

            float rx = 0.036f;

            float ry = 0.016f;

            glm::vec3 kp = knCenter + glm::vec3(std::sin(a) * rx, std::cos(a) * ry, -0.006f);

            SkinnedVertex ksv;

            ksv.bindPos = kp;

            ksv.bindNormal = glm::normalize(glm::vec3(std::sin(a) * 0.35f, std::cos(a) * 0.25f, -0.90f));

            ksv.uv = glm::vec2(0.35f, 0.75f);

            ksv.color = knuckleMat;

            ksv.bone0 = handBone;

            ksv.bone1 = handBone;

            ksv.weight0 = 1.0f;

            ksv.weight1 = 0.0f;

            ksv.part = PART_BODY;

            m_bindVertices.push_back(ksv);

        }

        for (int k = 1; k < 7; ++k) {

            m_indices.push_back(knBase);

            m_indices.push_back(knBase + k);

            m_indices.push_back(knBase + k + 1);

        }

        // ---------------------------------------------------------------------

        // 3. ARTICULATED DIGITS (3-Segment Jointed Phalanges with Organic Curvature)

        // ---------------------------------------------------------------------

        // Generates an anatomically jointed finger with Proximal, Intermediate & Distal

        // phalanges, natural flexion curves, tapering fingertip pads, and PBR seams.

        auto appendArticulatedFinger = [&](const glm::vec3& mcpRoot,

                                          const glm::vec3& dir0, float len0,

                                          const glm::vec3& dir1, float len1,

                                          const glm::vec3& dir2, float len2,

                                          float radius, uint8_t boneId)

        {

            const int radialSegments = 8;

            uint32_t fingerStartVertex = static_cast<uint32_t>(m_bindVertices.size());

            // 4 ring centers representing:

            // 0: MCP Joint (Knuckle base)

            // 1: PIP Joint (Proximal interphalangeal)

            // 2: DIP Joint (Distal interphalangeal)

            // 3: Fingertip pad apex

            glm::vec3 jointCenters[4];

            jointCenters[0] = mcpRoot;

            jointCenters[1] = jointCenters[0] + dir0 * len0;

            jointCenters[2] = jointCenters[1] + dir1 * len1;

            jointCenters[3] = jointCenters[2] + dir2 * len2;

            float jointRadii[4] = {

                radius,

                radius * 0.88f,

                radius * 0.74f,

                radius * 0.52f

            };

            for (int j = 0; j < 4; ++j) {

                glm::vec3 segDir = (j < 3) ? ((j == 0) ? dir0 : ((j == 1) ? dir1 : dir2)) : dir2;

                glm::vec3 upRef(0.0f, 0.0f, 1.0f);

                if (std::abs(glm::dot(segDir, upRef)) > 0.90f) {

                    upRef = glm::vec3(1.0f, 0.0f, 0.0f);

                }

                glm::vec3 rgt = glm::normalize(glm::cross(segDir, upRef));

                glm::vec3 up  = glm::normalize(glm::cross(rgt, segDir));

                float r = jointRadii[j];

                float tV = static_cast<float>(j) / 3.0f;

                for (int s = 0; s < radialSegments; ++s) {

                    float tU = static_cast<float>(s) / static_cast<float>(radialSegments);

                    float theta = tU * 2.0f * glm::pi<float>();

                    glm::vec3 norm = rgt * std::cos(theta) + up * std::sin(theta);

                    glm::vec3 pos = jointCenters[j] + norm * r;

                    SkinnedVertex fsv;

                    fsv.bindPos = pos;

                    fsv.bindNormal = norm;

                    fsv.uv = glm::vec2(0.30f + tV * 0.15f, 0.60f + tU * 0.25f);

                    fsv.color = gloveMat;

                    fsv.bone0 = boneId;

                    fsv.bone1 = handBone;

                    fsv.weight0 = 1.0f;

                    fsv.weight1 = 0.0f;

                    fsv.part = PART_BODY;

                    m_bindVertices.push_back(fsv);

                }

            }

            // Connect 3 segment rings with quad-triangle strips

            for (int j = 0; j < 3; ++j) {

                for (int s = 0; s < radialSegments; ++s) {

                    int nextS = (s + 1) % radialSegments;

                    uint32_t i00 = fingerStartVertex + j * radialSegments + s;

                    uint32_t i01 = fingerStartVertex + j * radialSegments + nextS;

                    uint32_t i10 = fingerStartVertex + (j + 1) * radialSegments + s;

                    uint32_t i11 = fingerStartVertex + (j + 1) * radialSegments + nextS;

                    // Outward-facing CCW quad triangles in (rgt, up, segDir) basis
                    m_indices.push_back(i00);
                    m_indices.push_back(i01);
                    m_indices.push_back(i10);

                    m_indices.push_back(i01);
                    m_indices.push_back(i11);
                    m_indices.push_back(i10);

                }

            }

            // Rounded hemispherical fingertip cap

            uint32_t capTipIndex = static_cast<uint32_t>(m_bindVertices.size());

            glm::vec3 tipDir = dir2;

            glm::vec3 capCenter = jointCenters[3] + tipDir * (jointRadii[3] * 0.65f);

            SkinnedVertex tipVertex;

            tipVertex.bindPos = capCenter;

            tipVertex.bindNormal = tipDir;

            tipVertex.uv = glm::vec2(0.40f, 0.75f);

            tipVertex.color = seamMat; // Reinforced Clarino leather fingertip pad

            tipVertex.bone0 = boneId;

            tipVertex.bone1 = handBone;

            tipVertex.weight0 = 1.0f;

            tipVertex.weight1 = 0.0f;

            tipVertex.part = PART_BODY;

            m_bindVertices.push_back(tipVertex);

            uint32_t lastRingStart = fingerStartVertex + 3 * radialSegments;

            for (int s = 0; s < radialSegments; ++s) {

                int nextS = (s + 1) % radialSegments;

                m_indices.push_back(capTipIndex);

                m_indices.push_back(lastRingStart + s);

                m_indices.push_back(lastRingStart + nextS);

            }

        };

        // Medial direction factor is -sX, Lateral is +sX

        // ---------------------------------------------------------------------

        // 1. OPPOSABLE THUMB

        // ---------------------------------------------------------------------

        // Rooted lower on the medial side of the palm (Thenar eminence),

        // angled inward and wrapping naturally around weapon grip

        glm::vec3 thumbRoot = handRoot + glm::vec3(-sX * 0.026f, -0.025f, 0.012f);

        glm::vec3 thumbDir0 = glm::normalize(glm::vec3(-sX * 0.28f, -0.68f, 0.67f));

        glm::vec3 thumbDir1 = glm::normalize(glm::vec3(-sX * 0.15f, -0.55f, 0.82f));

        glm::vec3 thumbDir2 = glm::normalize(glm::vec3( sX * 0.10f, -0.35f, 0.93f));

        appendArticulatedFinger(thumbRoot, thumbDir0, 0.020f, thumbDir1, 0.018f, thumbDir2, 0.015f, 0.0125f, handBone);

        // ---------------------------------------------------------------------

        // 2. INDEX FINGER (Trigger Finger on Right Hand)

        // ---------------------------------------------------------------------

        // On Right hand, rigged to BONE_R_TRIGGER_FINGER with exact joint origin!

        uint8_t indexBone = (side == 1) ? BONE_R_TRIGGER_FINGER : handBone;

        glm::vec3 indexRoot = handRoot + glm::vec3(-sX * 0.016f, -0.054f, 0.002f);

        glm::vec3 indexDir0 = glm::normalize(glm::vec3(-sX * 0.04f, -0.82f, 0.57f));

        glm::vec3 indexDir1 = glm::normalize(glm::vec3( 0.0f,       -0.52f, 0.85f));

        glm::vec3 indexDir2 = glm::normalize(glm::vec3( sX * 0.06f, -0.25f, 0.96f));

        appendArticulatedFinger(indexRoot, indexDir0, 0.022f, indexDir1, 0.018f, indexDir2, 0.014f, 0.0110f, indexBone);

        // ---------------------------------------------------------------------

        // 3. MIDDLE FINGER (Curled firmly onto Weapon Grip)

        // ---------------------------------------------------------------------

        glm::vec3 midRoot = handRoot + glm::vec3(-sX * 0.004f, -0.057f, -0.002f);

        glm::vec3 midDir0 = glm::normalize(glm::vec3(0.0f,        -0.78f, 0.62f));

        glm::vec3 midDir1 = glm::normalize(glm::vec3(0.0f,        -0.42f, 0.90f));

        glm::vec3 midDir2 = glm::normalize(glm::vec3(-sX * 0.05f, -0.15f, 0.98f));

        appendArticulatedFinger(midRoot, midDir0, 0.024f, midDir1, 0.020f, midDir2, 0.015f, 0.0112f, handBone);

        // ---------------------------------------------------------------------

        // 4. RING FINGER

        // ---------------------------------------------------------------------

        glm::vec3 ringRoot = handRoot + glm::vec3(sX * 0.010f, -0.055f, -0.004f);

        glm::vec3 ringDir0 = glm::normalize(glm::vec3(sX * 0.03f, -0.76f, 0.64f));

        glm::vec3 ringDir1 = glm::normalize(glm::vec3(0.0f,       -0.40f, 0.91f));

        glm::vec3 ringDir2 = glm::normalize(glm::vec3(-sX * 0.06f,-0.12f, 0.99f));

        appendArticulatedFinger(ringRoot, ringDir0, 0.022f, ringDir1, 0.018f, ringDir2, 0.014f, 0.0105f, handBone);

        // ---------------------------------------------------------------------

        // 5. LITTLE FINGER (Pinky)

        // ---------------------------------------------------------------------

        glm::vec3 pinkyRoot = handRoot + glm::vec3(sX * 0.024f, -0.050f, -0.006f);

        glm::vec3 pinkyDir0 = glm::normalize(glm::vec3(sX * 0.06f, -0.72f, 0.69f));

        glm::vec3 pinkyDir1 = glm::normalize(glm::vec3(sX * 0.02f, -0.38f, 0.92f));

        glm::vec3 pinkyDir2 = glm::normalize(glm::vec3(-sX * 0.06f,-0.10f, 0.99f));

        appendArticulatedFinger(pinkyRoot, pinkyDir0, 0.018f, pinkyDir1, 0.015f, pinkyDir2, 0.012f, 0.0095f, handBone);

    }

}

// ----------------------------------------------------------------------------

// 4. TACTICAL COMBAT BOOTS (LOWA ZEPHYR STYLE)

// ----------------------------------------------------------------------------

void SkinnedSoldier::buildCombatBoots() {

    glm::vec4 leatherMat(0.20f, 0.18f, 0.15f, 0.60f); // Leather Vamp (roughness 0.48, metallic 0.0)

    glm::vec4 soleMat(0.10f, 0.10f, 0.10f, 0.75f);    // Vulcanized Rubber Lug Sole (roughness 0.75)

    glm::vec4 frameMat(0.26f, 0.24f, 0.18f, 0.80f);   // PU Monowrap Exoskeleton Frame

    for (int side = 0; side < 2; ++side) {

        float sX = (side == 0) ? -0.13f : 0.13f;

        uint8_t footBone = (side == 0) ? BONE_L_FOOT : BONE_R_FOOT;

        glm::vec3 ankle = m_bindJoints[footBone];

        // Ankle Collar, Tongue & Heel Counter (24 radial segments for high anatomical fidelity)

        std::vector<glm::vec3> bootRings = {

            ankle + glm::vec3(0.0f,  0.07f,  0.002f), // Upper collar at pant blousing

            ankle + glm::vec3(0.0f,  0.04f,  0.008f), // Mid collar / speed-lace hooks

            ankle + glm::vec3(0.0f,  0.01f,  0.016f), // Lower collar / instep crease

            ankle + glm::vec3(0.0f, -0.02f,  0.026f)  // Heel counter & ankle bone swell

        };

        std::vector<glm::vec2> bootRadii = {

            glm::vec2(0.055f, 0.053f),

            glm::vec2(0.053f, 0.058f),

            glm::vec2(0.052f, 0.066f),

            glm::vec2(0.051f, 0.078f)

        };

        std::vector<float> bootWeights = { 1.0f, 1.0f, 1.0f, 1.0f };

        appendSkinnedTube(bootRings, bootRadii, bootWeights,

                          footBone, footBone,

                          glm::vec2(0.05f, 0.55f), glm::vec2(0.25f, 0.85f),

                          leatherMat, 24);

        // Sculpted Toe Vamp & Forefoot Box (12 vertices with rounded toe cap)

        uint32_t toeBase = static_cast<uint32_t>(m_bindVertices.size());

        glm::vec3 toeBox[12] = {

            // Upper instep & tongue

            ankle + glm::vec3(-0.046f, -0.015f,  0.025f),

            ankle + glm::vec3( 0.046f, -0.015f,  0.025f),

            ankle + glm::vec3( 0.044f, -0.028f,  0.095f),

            ankle + glm::vec3(-0.044f, -0.028f,  0.095f),

            // Toe cap peak

            ankle + glm::vec3( 0.038f, -0.042f,  0.155f),

            ankle + glm::vec3(-0.038f, -0.042f,  0.155f),

            // Lower welt line

            ankle + glm::vec3(-0.053f, -0.060f, -0.070f),

            ankle + glm::vec3( 0.053f, -0.060f, -0.070f),

            ankle + glm::vec3( 0.052f, -0.060f,  0.095f),

            ankle + glm::vec3(-0.052f, -0.060f,  0.095f),

            ankle + glm::vec3( 0.048f, -0.060f,  0.165f),

            ankle + glm::vec3(-0.048f, -0.060f,  0.165f)

        };

        for (int i = 0; i < 12; ++i) {

            SkinnedVertex tv;

            tv.bindPos = toeBox[i];

            tv.bindNormal = glm::normalize(toeBox[i] - (ankle + glm::vec3(0, -0.04f, 0.04f)));

            tv.uv = glm::vec2(0.12f, 0.70f);

            tv.color = leatherMat;

            tv.bone0 = footBone;

            tv.bone1 = footBone;

            tv.weight0 = 1.0f;

            tv.weight1 = 0.0f;

            tv.part = PART_BODY;

            m_bindVertices.push_back(tv);

        }

        // Top vamp & toe cap quads (strictly CCW outward-facing)
        m_indices.push_back(toeBase + 0); m_indices.push_back(toeBase + 3); m_indices.push_back(toeBase + 2);
        m_indices.push_back(toeBase + 0); m_indices.push_back(toeBase + 2); m_indices.push_back(toeBase + 1);

        m_indices.push_back(toeBase + 3); m_indices.push_back(toeBase + 5); m_indices.push_back(toeBase + 4);
        m_indices.push_back(toeBase + 3); m_indices.push_back(toeBase + 4); m_indices.push_back(toeBase + 2);

        // Sides connecting to welt (strictly CCW outward-facing)
        // Rear wall
        m_indices.push_back(toeBase + 0); m_indices.push_back(toeBase + 1); m_indices.push_back(toeBase + 7);
        m_indices.push_back(toeBase + 0); m_indices.push_back(toeBase + 7); m_indices.push_back(toeBase + 6);

        // Right side
        m_indices.push_back(toeBase + 1); m_indices.push_back(toeBase + 2); m_indices.push_back(toeBase + 8);
        m_indices.push_back(toeBase + 1); m_indices.push_back(toeBase + 8); m_indices.push_back(toeBase + 7);

        // Front right
        m_indices.push_back(toeBase + 2); m_indices.push_back(toeBase + 4); m_indices.push_back(toeBase + 10);
        m_indices.push_back(toeBase + 2); m_indices.push_back(toeBase + 10); m_indices.push_back(toeBase + 8);

        // Front toe
        m_indices.push_back(toeBase + 4); m_indices.push_back(toeBase + 5); m_indices.push_back(toeBase + 11);
        m_indices.push_back(toeBase + 4); m_indices.push_back(toeBase + 11); m_indices.push_back(toeBase + 10);

        // Front left
        m_indices.push_back(toeBase + 5); m_indices.push_back(toeBase + 3); m_indices.push_back(toeBase + 9);
        m_indices.push_back(toeBase + 5); m_indices.push_back(toeBase + 9); m_indices.push_back(toeBase + 11);

        // Left side
        m_indices.push_back(toeBase + 3); m_indices.push_back(toeBase + 0); m_indices.push_back(toeBase + 6);
        m_indices.push_back(toeBase + 3); m_indices.push_back(toeBase + 6); m_indices.push_back(toeBase + 9);

        // Lowa Zephyr PU Monowrap Exoskeleton Webbing Ribs (Signature triangular lateral stabilizers)
        uint32_t monoBase = static_cast<uint32_t>(m_bindVertices.size());
        float mSide = (side == 0) ? -1.0f : 1.0f;
        glm::vec3 mPts[6] = {
            ankle + glm::vec3(mSide * 0.054f, -0.058f, -0.040f),
            ankle + glm::vec3(mSide * 0.055f, -0.022f,  0.010f), // Raised skeletal triangle apex
            ankle + glm::vec3(mSide * 0.054f, -0.058f,  0.060f),
            ankle + glm::vec3(mSide * 0.054f, -0.058f,  0.070f),
            ankle + glm::vec3(mSide * 0.055f, -0.024f,  0.110f), // Second triangle apex
            ankle + glm::vec3(mSide * 0.050f, -0.058f,  0.150f)
        };

        for (int p = 0; p < 6; ++p) {
            SkinnedVertex mv;
            mv.bindPos = mPts[p];
            mv.bindNormal = glm::vec3(mSide, 0.2f, 0);
            mv.uv = glm::vec2(0.20f, 0.80f);
            mv.color = frameMat;
            mv.bone0 = footBone;
            mv.bone1 = footBone;
            mv.weight0 = 1.0f;
            mv.weight1 = 0.0f;
            mv.part = PART_BODY;
            m_bindVertices.push_back(mv);
        }

        if (side == 0) {
            // Left foot: invert triangle index order to face outward (-X)
            m_indices.push_back(monoBase + 0); m_indices.push_back(monoBase + 2); m_indices.push_back(monoBase + 1);
            m_indices.push_back(monoBase + 3); m_indices.push_back(monoBase + 5); m_indices.push_back(monoBase + 4);
        } else {
            // Right foot: face outward (+X)
            m_indices.push_back(monoBase + 0); m_indices.push_back(monoBase + 1); m_indices.push_back(monoBase + 2);
            m_indices.push_back(monoBase + 3); m_indices.push_back(monoBase + 4); m_indices.push_back(monoBase + 5);
        }

        // Vulcanized Vibram Deep-Lug Tread Sole with Heel Block
        uint32_t soleBase = static_cast<uint32_t>(m_bindVertices.size());
        glm::vec3 soleCorn[8] = {
            ankle + glm::vec3(-0.055f, -0.060f, -0.078f),
            ankle + glm::vec3( 0.055f, -0.060f, -0.078f),
            ankle + glm::vec3( 0.053f, -0.060f,  0.168f),
            ankle + glm::vec3(-0.053f, -0.060f,  0.168f),
            ankle + glm::vec3(-0.058f, -0.086f, -0.082f),
            ankle + glm::vec3( 0.058f, -0.086f, -0.082f),
            ankle + glm::vec3( 0.055f, -0.086f,  0.174f),
            ankle + glm::vec3(-0.055f, -0.086f,  0.174f)
        };

        for (int i = 0; i < 8; ++i) {
            SkinnedVertex sv;
            sv.bindPos = soleCorn[i];
            sv.bindNormal = (i < 4) ? glm::vec3(0, 1.0f, 0) : glm::vec3(0, -1.0f, 0);
            sv.uv = glm::vec2(0.15f, 0.85f);
            sv.color = soleMat;
            sv.bone0 = footBone;
            sv.bone1 = footBone;
            sv.weight0 = 1.0f;
            sv.weight1 = 0.0f;
            sv.part = PART_BODY;
            m_bindVertices.push_back(sv);
        }

        // Sole bottom tread (strictly CCW facing downwards -Y)
        m_indices.push_back(soleBase + 4); m_indices.push_back(soleBase + 5); m_indices.push_back(soleBase + 6);
        m_indices.push_back(soleBase + 4); m_indices.push_back(soleBase + 6); m_indices.push_back(soleBase + 7);

        // Sole perimeter sidewalls (strictly CCW facing outwards)
        // Back wall
        m_indices.push_back(soleBase + 0); m_indices.push_back(soleBase + 1); m_indices.push_back(soleBase + 5);
        m_indices.push_back(soleBase + 0); m_indices.push_back(soleBase + 5); m_indices.push_back(soleBase + 4);

        // Right wall
        m_indices.push_back(soleBase + 1); m_indices.push_back(soleBase + 2); m_indices.push_back(soleBase + 6);
        m_indices.push_back(soleBase + 1); m_indices.push_back(soleBase + 6); m_indices.push_back(soleBase + 5);

        // Front wall
        m_indices.push_back(soleBase + 2); m_indices.push_back(soleBase + 3); m_indices.push_back(soleBase + 7);
        m_indices.push_back(soleBase + 2); m_indices.push_back(soleBase + 7); m_indices.push_back(soleBase + 6);

        // Left wall
        m_indices.push_back(soleBase + 3); m_indices.push_back(soleBase + 0); m_indices.push_back(soleBase + 4);
        m_indices.push_back(soleBase + 3); m_indices.push_back(soleBase + 4); m_indices.push_back(soleBase + 7);

    }

}

// ----------------------------------------------------------------------------

// 5. CRYE PRECISION JPC 2.0 PLATE CARRIER & TACTICAL GEAR

// ----------------------------------------------------------------------------

void SkinnedSoldier::buildPlateCarrierAndGear() {

    glm::vec4 corduraMat(0.42f, 0.40f, 0.32f, 0.90f); // 500D Cordura Nylon (roughness 0.65, metallic 0.02)

    glm::vec4 darkPoly(0.12f, 0.12f, 0.12f, 0.80f);   // Ballistic Polymer

    glm::vec4 catOrange(0.95f, 0.42f, 0.12f, 0.90f);  // CAT Tourniquet Orange Windlass

    glm::vec4 magMat(0.18f, 0.18f, 0.18f, 0.80f);     // Magpul PMAG 30

    glm::vec4 brassMat(0.85f, 0.70f, 0.25f, 0.70f);    // 5.56x45mm NATO Brass Cartridge

    glm::vec4 molleMat(0.36f, 0.34f, 0.26f, 0.90f);   // Laser-Cut Molle Slots

    glm::vec3 chest = m_bindJoints[BONE_SPINE_CHEST];

    // --- FRONT SAPI BALLISTIC ARMOR PLATE BAG (Multi-Curve Ergonomic Geometry with 3D Depth) ---

    uint32_t fPlateBase = static_cast<uint32_t>(m_bindVertices.size());

    // 10-point multi-curve front plate contour with shooter's cut top corners

    glm::vec3 fPlate[10] = {

        chest + glm::vec3(-0.10f,  0.17f,  0.138f), // 0: Top left collar cut

        chest + glm::vec3( 0.10f,  0.17f,  0.138f), // 1: Top right collar cut

        chest + glm::vec3( 0.16f,  0.08f,  0.146f), // 2: Upper right shooter's cut

        chest + glm::vec3( 0.16f, -0.16f,  0.136f), // 3: Bottom right corner

        chest + glm::vec3( 0.00f, -0.16f,  0.142f), // 4: Bottom center crown

        chest + glm::vec3(-0.16f, -0.16f,  0.136f), // 5: Bottom left corner

        chest + glm::vec3(-0.16f,  0.08f,  0.146f), // 6: Upper left shooter's cut

        chest + glm::vec3( 0.00f,  0.08f,  0.158f), // 7: Upper crown peak

        chest + glm::vec3( 0.00f, -0.04f,  0.156f), // 8: Mid crown peak

        chest + glm::vec3( 0.00f,  0.17f,  0.144f)  // 9: Top center

    };

    for (int i = 0; i < 10; ++i) {

        SkinnedVertex fsv;

        fsv.bindPos = fPlate[i];

        fsv.bindNormal = glm::normalize(glm::vec3(fPlate[i].x * 0.4f, 0.0f, 1.0f));

        fsv.uv = glm::vec2(0.65f, 0.25f);

        fsv.color = corduraMat;

        fsv.bone0 = BONE_SPINE_CHEST;

        fsv.bone1 = BONE_SPINE_CHEST;

        fsv.weight0 = 1.0f;

        fsv.weight1 = 0.0f;

        fsv.part = PART_VEST;

        m_bindVertices.push_back(fsv);

    }

    // Front plate faceted fan

    m_indices.push_back(fPlateBase + 7); m_indices.push_back(fPlateBase + 9); m_indices.push_back(fPlateBase + 0);

    m_indices.push_back(fPlateBase + 7); m_indices.push_back(fPlateBase + 1); m_indices.push_back(fPlateBase + 9);

    m_indices.push_back(fPlateBase + 7); m_indices.push_back(fPlateBase + 2); m_indices.push_back(fPlateBase + 1);

    m_indices.push_back(fPlateBase + 8); m_indices.push_back(fPlateBase + 2); m_indices.push_back(fPlateBase + 7);

    m_indices.push_back(fPlateBase + 8); m_indices.push_back(fPlateBase + 3); m_indices.push_back(fPlateBase + 2);

    m_indices.push_back(fPlateBase + 8); m_indices.push_back(fPlateBase + 4); m_indices.push_back(fPlateBase + 3);

    m_indices.push_back(fPlateBase + 8); m_indices.push_back(fPlateBase + 5); m_indices.push_back(fPlateBase + 4);

    m_indices.push_back(fPlateBase + 8); m_indices.push_back(fPlateBase + 6); m_indices.push_back(fPlateBase + 5);

    m_indices.push_back(fPlateBase + 7); m_indices.push_back(fPlateBase + 6); m_indices.push_back(fPlateBase + 8);

    m_indices.push_back(fPlateBase + 7); m_indices.push_back(fPlateBase + 0); m_indices.push_back(fPlateBase + 6);

    // --- REAR SAPI BALLISTIC ARMOR PLATE BAG & REINFORCED DRAG HANDLE ---

    uint32_t rPlateBase = static_cast<uint32_t>(m_bindVertices.size());

    glm::vec3 rPlate[8] = {

        chest + glm::vec3(-0.11f,  0.17f, -0.134f),

        chest + glm::vec3( 0.11f,  0.17f, -0.134f),

        chest + glm::vec3( 0.16f,  0.07f, -0.138f),

        chest + glm::vec3( 0.16f, -0.16f, -0.132f),

        chest + glm::vec3(-0.16f, -0.16f, -0.132f),

        chest + glm::vec3(-0.16f,  0.07f, -0.138f),

        chest + glm::vec3( 0.00f,  0.02f, -0.144f), // Rear center crown

        chest + glm::vec3( 0.00f, -0.10f, -0.140f)

    };

    for (int i = 0; i < 8; ++i) {

        SkinnedVertex rsv;

        rsv.bindPos = rPlate[i];

        rsv.bindNormal = glm::vec3(0, 0, -1.0f);

        rsv.uv = glm::vec2(0.65f, 0.25f);

        rsv.color = corduraMat;

        rsv.bone0 = BONE_SPINE_CHEST;

        rsv.bone1 = BONE_SPINE_CHEST;

        rsv.weight0 = 1.0f;

        rsv.weight1 = 0.0f;

        rsv.part = PART_VEST;

        m_bindVertices.push_back(rsv);

    }

    for (int i = 0; i < 6; ++i) {

        m_indices.push_back(rPlateBase + 6);

        m_indices.push_back(rPlateBase + i);

        m_indices.push_back(rPlateBase + ((i + 1) % 6));

    }

    // --- PADDED ERGONOMIC SHOULDER STRAPS (Left & Right) ---

    for (int s = 0; s < 2; ++s) {

        float sX = (s == 0) ? -0.11f : 0.11f;

        uint32_t shBase = static_cast<uint32_t>(m_bindVertices.size());

        glm::vec3 shQuad[4] = {

            chest + glm::vec3(sX - 0.024f, 0.17f,  0.135f),

            chest + glm::vec3(sX + 0.024f, 0.17f,  0.135f),

            chest + glm::vec3(sX + 0.024f, 0.17f, -0.132f),

            chest + glm::vec3(sX - 0.024f, 0.17f, -0.132f)

        };

        for (int i = 0; i < 4; ++i) {

            SkinnedVertex ssv;

            ssv.bindPos = shQuad[i];

            ssv.bindNormal = glm::vec3(0, 1.0f, 0);

            ssv.uv = glm::vec2(0.65f, 0.25f);

            ssv.color = corduraMat * 1.05f;

            ssv.bone0 = BONE_SPINE_CHEST;

            ssv.bone1 = BONE_SPINE_CHEST;

            ssv.weight0 = 1.0f;

            ssv.weight1 = 0.0f;

            ssv.part = PART_VEST;

            m_bindVertices.push_back(ssv);

        }

        m_indices.push_back(shBase + 0); m_indices.push_back(shBase + 1); m_indices.push_back(shBase + 2);

        m_indices.push_back(shBase + 0); m_indices.push_back(shBase + 2); m_indices.push_back(shBase + 3);

    }

    // --- TRIPLE 5.56 KANGAROO MAGAZINE POUCH WITH 3D CURVED PMAGS ---

    for (int m = -1; m <= 1; ++m) {

        float mX = m * 0.076f;

        uint32_t magBase = static_cast<uint32_t>(m_bindVertices.size());

        glm::vec3 pCenter = chest + glm::vec3(mX, -0.06f, 0.170f);

        // PMAG 30 Curved Body with Distinct Waffle Texturing

        glm::vec3 mBox[8] = {

            pCenter + glm::vec3(-0.032f, -0.080f, -0.022f),

            pCenter + glm::vec3( 0.032f, -0.080f, -0.022f),

            pCenter + glm::vec3( 0.032f,  0.080f, -0.016f),

            pCenter + glm::vec3(-0.032f,  0.080f, -0.016f),

            pCenter + glm::vec3(-0.032f, -0.080f,  0.022f),

            pCenter + glm::vec3( 0.032f, -0.080f,  0.022f),

            pCenter + glm::vec3( 0.032f,  0.080f,  0.026f),

            pCenter + glm::vec3(-0.032f,  0.080f,  0.026f)

        };

        for (int i = 0; i < 8; ++i) {

            SkinnedVertex msv;

            msv.bindPos = mBox[i];

            msv.bindNormal = (i >= 4) ? glm::vec3(0, 0, 1.0f) : glm::vec3(0, 0, -1.0f);

            msv.uv = glm::vec2(0.85f, 0.15f);

            msv.color = magMat;

            msv.bone0 = BONE_SPINE_CHEST;

            msv.bone1 = BONE_SPINE_CHEST;

            msv.weight0 = 1.0f;

            msv.weight1 = 0.0f;

            msv.part = PART_VEST;

            m_bindVertices.push_back(msv);

        }

        // Front Face

        m_indices.push_back(magBase + 4); m_indices.push_back(magBase + 5); m_indices.push_back(magBase + 6);

        m_indices.push_back(magBase + 4); m_indices.push_back(magBase + 6); m_indices.push_back(magBase + 7);

        // Baseplate

        m_indices.push_back(magBase + 0); m_indices.push_back(magBase + 4); m_indices.push_back(magBase + 5);

        m_indices.push_back(magBase + 0); m_indices.push_back(magBase + 5); m_indices.push_back(magBase + 1);

        // Top Face

        m_indices.push_back(magBase + 3); m_indices.push_back(magBase + 2); m_indices.push_back(magBase + 6);

        m_indices.push_back(magBase + 3); m_indices.push_back(magBase + 6); m_indices.push_back(magBase + 7);

        // Left & Right Sides (clean solid closed 3D magazine)

        m_indices.push_back(magBase + 0); m_indices.push_back(magBase + 3); m_indices.push_back(magBase + 7);

        m_indices.push_back(magBase + 0); m_indices.push_back(magBase + 7); m_indices.push_back(magBase + 4);

        m_indices.push_back(magBase + 1); m_indices.push_back(magBase + 5); m_indices.push_back(magBase + 6);

        m_indices.push_back(magBase + 1); m_indices.push_back(magBase + 6); m_indices.push_back(magBase + 2);

        // Visible Brass Bullet Tip

        uint32_t bTip = static_cast<uint32_t>(m_bindVertices.size());

        glm::vec3 bPos = pCenter + glm::vec3(0.0f, 0.092f, 0.005f);

        SkinnedVertex bsv;

        bsv.bindPos = bPos;

        bsv.bindNormal = glm::vec3(0, 1.0f, 0);

        bsv.uv = glm::vec2(0.90f, 0.10f);

        bsv.color = brassMat;

        bsv.bone0 = BONE_SPINE_CHEST;

        bsv.bone1 = BONE_SPINE_CHEST;

        bsv.weight0 = 1.0f;

        bsv.weight1 = 0.0f;

        bsv.part = PART_VEST;

        m_bindVertices.push_back(bsv);

    }

    // --- CAT (COMBAT APPLICATION TOURNIQUET) ON LEFT SHOULDER STRAP ---

    uint32_t catBase = static_cast<uint32_t>(m_bindVertices.size());

    glm::vec3 catCenter = chest + glm::vec3(-0.11f, 0.16f, 0.110f);

    glm::vec3 catBox[4] = {

        catCenter + glm::vec3(-0.020f, -0.055f, 0),

        catCenter + glm::vec3( 0.020f, -0.055f, 0),

        catCenter + glm::vec3( 0.020f,  0.055f, 0),

        catCenter + glm::vec3(-0.020f,  0.055f, 0)

    };

    for (int i = 0; i < 4; ++i) {

        SkinnedVertex csv;

        csv.bindPos = catBox[i];

        csv.bindNormal = glm::vec3(-0.2f, 0.2f, 0.95f);

        csv.uv = glm::vec2(0.92f, 0.40f);

        csv.color = (i >= 2) ? catOrange : darkPoly; // High-vis orange windlass tip!

        csv.bone0 = BONE_SPINE_CHEST;

        csv.bone1 = BONE_SPINE_CHEST;

        csv.weight0 = 1.0f;

        csv.weight1 = 0.0f;

        csv.part = PART_VEST;

        m_bindVertices.push_back(csv);

    }

    m_indices.push_back(catBase + 0); m_indices.push_back(catBase + 1); m_indices.push_back(catBase + 2);

    m_indices.push_back(catBase + 0); m_indices.push_back(catBase + 2); m_indices.push_back(catBase + 3);

    // --- HARRIS PRC-152 TACTICAL RADIO WITH EXTENDED WHIP ANTENNA ---

    uint32_t radBase = static_cast<uint32_t>(m_bindVertices.size());

    glm::vec3 radCenter = chest + glm::vec3(-0.18f, 0.02f, 0.080f);

    // Radio chassis

    glm::vec3 radBox[4] = {

        radCenter + glm::vec3(-0.025f, -0.075f, 0.025f),

        radCenter + glm::vec3( 0.025f, -0.075f, 0.025f),

        radCenter + glm::vec3( 0.025f,  0.075f, 0.025f),

        radCenter + glm::vec3(-0.025f,  0.075f, 0.025f)

    };

    for (int i = 0; i < 4; ++i) {

        SkinnedVertex rsv;

        rsv.bindPos = radBox[i];

        rsv.bindNormal = glm::vec3(-0.7f, 0, 0.7f);

        rsv.uv = glm::vec2(0.70f, 0.35f);

        rsv.color = darkPoly;

        rsv.bone0 = BONE_SPINE_CHEST;

        rsv.bone1 = BONE_SPINE_CHEST;

        rsv.weight0 = 1.0f;

        rsv.weight1 = 0.0f;

        rsv.part = PART_VEST;

        m_bindVertices.push_back(rsv);

    }

    m_indices.push_back(radBase + 0); m_indices.push_back(radBase + 1); m_indices.push_back(radBase + 2);

    m_indices.push_back(radBase + 0); m_indices.push_back(radBase + 2); m_indices.push_back(radBase + 3);

    // Flexible 30-inch whip blade antenna extending up along shoulder

    uint32_t antBase = static_cast<uint32_t>(m_bindVertices.size());

    glm::vec3 antBottom = radCenter + glm::vec3(0, 0.08f, 0);

    glm::vec3 antTop    = chest + glm::vec3(-0.16f, 0.38f, 0.02f);

    glm::vec3 antSeg[4] = {

        antBottom + glm::vec3(-0.004f, 0, 0),

        antBottom + glm::vec3( 0.004f, 0, 0),

        antTop    + glm::vec3( 0.003f, 0, 0),

        antTop    + glm::vec3(-0.003f, 0, 0)

    };

    for (int i = 0; i < 4; ++i) {

        SkinnedVertex asv;

        asv.bindPos = antSeg[i];

        asv.bindNormal = glm::vec3(0, 0, 1.0f);

        asv.uv = glm::vec2(0.70f, 0.35f);

        asv.color = darkPoly;

        asv.bone0 = BONE_SPINE_CHEST;

        asv.bone1 = BONE_SPINE_CHEST;

        asv.weight0 = 1.0f;

        asv.weight1 = 0.0f;

        asv.part = PART_VEST;

        m_bindVertices.push_back(asv);

    }

    m_indices.push_back(antBase + 0); m_indices.push_back(antBase + 1); m_indices.push_back(antBase + 2);

    m_indices.push_back(antBase + 0); m_indices.push_back(antBase + 2); m_indices.push_back(antBase + 3);

    // --- DUTY BATTLE BELT & SAFARILAND KYDEX HOLSTER (Rigged to PELVIS) ---

    glm::vec3 pelvis = m_bindJoints[BONE_PELVIS];

    uint32_t holBase = static_cast<uint32_t>(m_bindVertices.size());

    glm::vec3 holCenter = pelvis + glm::vec3(0.205f, -0.06f, 0.020f);

    glm::vec3 holBox[4] = {

        holCenter + glm::vec3(0,  0.080f, -0.040f),

        holCenter + glm::vec3(0,  0.080f,  0.040f),

        holCenter + glm::vec3(0, -0.090f,  0.035f),

        holCenter + glm::vec3(0, -0.090f, -0.035f)

    };

    for (int i = 0; i < 4; ++i) {

        SkinnedVertex hsv;

        hsv.bindPos = holBox[i];

        hsv.bindNormal = glm::vec3(1.0f, 0, 0);

        hsv.uv = glm::vec2(0.75f, 0.35f);

        hsv.color = darkPoly;

        hsv.bone0 = BONE_PELVIS;

        hsv.bone1 = BONE_PELVIS;

        hsv.weight0 = 1.0f;

        hsv.weight1 = 0.0f;

        hsv.part = PART_BODY;

        m_bindVertices.push_back(hsv);

    }

    m_indices.push_back(holBase + 0); m_indices.push_back(holBase + 1); m_indices.push_back(holBase + 2);

    m_indices.push_back(holBase + 0); m_indices.push_back(holBase + 2); m_indices.push_back(holBase + 3);

}

// ----------------------------------------------------------------------------

// 6. OPS-CORE FAST HIGH-CUT HELMET & TACTICAL BALLISTIC EYEWEAR

// ----------------------------------------------------------------------------

void SkinnedSoldier::buildBallisticHelmetAndEyewear() {
    glm::vec4 helmMat(0.38f, 0.40f, 0.30f, 0.80f);   // Ballistic Shell (roughness 0.38, metallic 0.08)
    glm::vec4 shroudMat(0.22f, 0.22f, 0.24f, 0.70f); // CNC Aluminum Wilcox Shroud (roughness 0.22, metallic 0.88)
    glm::vec4 darkPoly(0.12f, 0.12f, 0.12f, 0.80f);  // ARC Rails & Headset
    glm::vec4 glassMat(0.18f, 0.22f, 0.26f, 0.40f);  // Smoked Polycarbonate Lens (roughness 0.06, metallic 0.22)

    glm::vec3 head = m_bindJoints[BONE_HEAD]; // (0, 1.48f, 0)

    // --- 1. OPS-CORE FAST HIGH-CUT BALLISTIC HELMET SHELL ---
    // Snugly hugs cranial vault, rim resting 1.2cm above eyebrows (1.615m)
    std::vector<glm::vec3> helmRings = {
        head + glm::vec3(0.0f,  0.135f,  0.003f), // 1.615m: Flared high-cut rim lip
        head + glm::vec3(0.0f,  0.155f,  0.001f), // 1.635m: Lower crown / ARC rail line
        head + glm::vec3(0.0f,  0.175f, -0.002f), // 1.655m: Mid crown / velcro loop field
        head + glm::vec3(0.0f,  0.195f, -0.005f), // 1.675m: Upper parietal dome
        head + glm::vec3(0.0f,  0.212f, -0.008f), // 1.692m: Apex dome
        head + glm::vec3(0.0f,  0.224f, -0.010f)  // 1.704m: Crown peak
    };

    std::vector<glm::vec2> helmRadii = {
        glm::vec2(0.086f, 0.096f), // Rim: snug 4mm clearance over skull
        glm::vec2(0.088f, 0.098f), // Lower crown
        glm::vec2(0.086f, 0.096f), // Mid crown
        glm::vec2(0.078f, 0.088f), // Upper dome
        glm::vec2(0.062f, 0.070f), // Apex dome
        glm::vec2(0.035f, 0.040f)  // Peak
    };

    std::vector<float> helmWeights = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    appendSkinnedTube(helmRings, helmRadii, helmWeights,
                      BONE_HEAD, BONE_HEAD,
                      glm::vec2(0.05f, 0.55f), glm::vec2(0.45f, 0.95f),
                      helmMat, 32, {}, PART_HELMET_FAST, true);

    // --- 2. WILCOX L4 G24 NVG MOUNTING SHROUD ON FOREHEAD ---
    uint32_t nvgBase = static_cast<uint32_t>(m_bindVertices.size());
    glm::vec3 nvgCenter = head + glm::vec3(0.0f, 0.145f, 0.098f);
    glm::vec3 nvgPlate[6] = {
        nvgCenter + glm::vec3(-0.020f, -0.018f, 0.000f),
        nvgCenter + glm::vec3( 0.020f, -0.018f, 0.000f),
        nvgCenter + glm::vec3( 0.022f,  0.018f, 0.000f),
        nvgCenter + glm::vec3(-0.022f,  0.018f, 0.000f),
        nvgCenter + glm::vec3( 0.000f,  0.000f, 0.012f),
        nvgCenter + glm::vec3( 0.000f, -0.008f, 0.016f)
    };

    for (int i = 0; i < 6; ++i) {
        SkinnedVertex nsv;
        nsv.bindPos = nvgPlate[i];
        nsv.bindNormal = glm::vec3(0, 0.15f, 0.98f);
        nsv.uv = glm::vec2(0.12f, 0.90f);
        nsv.color = shroudMat;
        nsv.bone0 = BONE_HEAD;
        nsv.bone1 = BONE_HEAD;
        nsv.weight0 = 1.0f;
        nsv.weight1 = 0.0f;
        nsv.part = PART_HELMET_FAST;
        m_bindVertices.push_back(nsv);
    }
    m_indices.push_back(nvgBase + 4); m_indices.push_back(nvgBase + 0); m_indices.push_back(nvgBase + 1);
    m_indices.push_back(nvgBase + 4); m_indices.push_back(nvgBase + 1); m_indices.push_back(nvgBase + 2);
    m_indices.push_back(nvgBase + 4); m_indices.push_back(nvgBase + 2); m_indices.push_back(nvgBase + 3);
    m_indices.push_back(nvgBase + 4); m_indices.push_back(nvgBase + 3); m_indices.push_back(nvgBase + 0);

    // --- 3. ARC ACCESSORY RAILS (Left & Right sides) ---
    for (int side = 0; side < 2; ++side) {
        float sX = (side == 0) ? -0.088f : 0.088f;
        uint32_t arcBase = static_cast<uint32_t>(m_bindVertices.size());
        glm::vec3 arcCenter = head + glm::vec3(sX, 0.145f, 0.006f);
        glm::vec3 arcBar[4] = {
            arcCenter + glm::vec3(0, -0.008f, -0.045f),
            arcCenter + glm::vec3(0, -0.008f,  0.045f),
            arcCenter + glm::vec3(0,  0.008f,  0.038f),
            arcCenter + glm::vec3(0,  0.008f, -0.038f)
        };
        for (int i = 0; i < 4; ++i) {
            SkinnedVertex asv;
            asv.bindPos = arcBar[i];
            asv.bindNormal = glm::vec3((side == 0) ? -1.0f : 1.0f, 0, 0);
            asv.uv = glm::vec2(0.20f, 0.90f);
            asv.color = darkPoly;
            asv.bone0 = BONE_HEAD;
            asv.bone1 = BONE_HEAD;
            asv.weight0 = 1.0f;
            asv.weight1 = 0.0f;
            asv.part = PART_HELMET_FAST;
            m_bindVertices.push_back(asv);
        }
        if (side == 0) {
            // Left ARC rail: invert winding to face outward (-X)
            m_indices.push_back(arcBase + 0); m_indices.push_back(arcBase + 2); m_indices.push_back(arcBase + 1);
            m_indices.push_back(arcBase + 0); m_indices.push_back(arcBase + 3); m_indices.push_back(arcBase + 2);
        } else {
            // Right ARC rail: face outward (+X)
            m_indices.push_back(arcBase + 0); m_indices.push_back(arcBase + 1); m_indices.push_back(arcBase + 2);
            m_indices.push_back(arcBase + 0); m_indices.push_back(arcBase + 2); m_indices.push_back(arcBase + 3);
        }
    }

    // --- 4. COMTAC DUAL-COMM TACTICAL HEADSET (High-cut ear clearance) ---
    for (int side = 0; side < 2; ++side) {
        float sX = (side == 0) ? -0.078f : 0.078f;
        uint32_t comtacBase = static_cast<uint32_t>(m_bindVertices.size());
        glm::vec3 cupCenter = head + glm::vec3(sX, 0.102f, 0.002f);
        glm::vec3 cup[6] = {
            cupCenter + glm::vec3(0, -0.022f, -0.016f),
            cupCenter + glm::vec3(0, -0.022f,  0.016f),
            cupCenter + glm::vec3(0,  0.022f,  0.016f),
            cupCenter + glm::vec3(0,  0.022f, -0.016f),
            cupCenter + glm::vec3((side == 0) ? -0.014f : 0.014f, 0.0f, 0.0f),
            cupCenter + glm::vec3((side == 0) ? -0.009f : 0.009f, 0.012f, -0.014f)
        };
        for (int i = 0; i < 6; ++i) {
            SkinnedVertex csv;
            csv.bindPos = cup[i];
            csv.bindNormal = glm::vec3((side == 0) ? -1.0f : 1.0f, 0, 0);
            csv.uv = glm::vec2(0.20f, 0.85f);
            csv.color = darkPoly;
            csv.bone0 = BONE_HEAD;
            csv.bone1 = BONE_HEAD;
            csv.weight0 = 1.0f;
            csv.weight1 = 0.0f;
            csv.part = PART_HELMET_FAST;
            m_bindVertices.push_back(csv);
        }
        if (side == 0) {
            // Left ComTac headset: invert fan winding to face outward (-X)
            m_indices.push_back(comtacBase + 4); m_indices.push_back(comtacBase + 1); m_indices.push_back(comtacBase + 0);
            m_indices.push_back(comtacBase + 4); m_indices.push_back(comtacBase + 2); m_indices.push_back(comtacBase + 1);
            m_indices.push_back(comtacBase + 4); m_indices.push_back(comtacBase + 3); m_indices.push_back(comtacBase + 2);
            m_indices.push_back(comtacBase + 4); m_indices.push_back(comtacBase + 0); m_indices.push_back(comtacBase + 3);
        } else {
            // Right ComTac headset: face outward (+X)
            m_indices.push_back(comtacBase + 4); m_indices.push_back(comtacBase + 0); m_indices.push_back(comtacBase + 1);
            m_indices.push_back(comtacBase + 4); m_indices.push_back(comtacBase + 1); m_indices.push_back(comtacBase + 2);
            m_indices.push_back(comtacBase + 4); m_indices.push_back(comtacBase + 2); m_indices.push_back(comtacBase + 3);
            m_indices.push_back(comtacBase + 4); m_indices.push_back(comtacBase + 3); m_indices.push_back(comtacBase + 0);
        }
    }

    // --- 5. ESS PROFILE NVG BALLISTIC SMOKED GOGGLES (Placed directly over eyes) ---
    uint32_t gogBase = static_cast<uint32_t>(m_bindVertices.size());
    glm::vec3 gogCenter = head + glm::vec3(0.0f, 0.105f, 0.086f); // 1.585m: exact eye plane
    glm::vec3 lens[8] = {
        gogCenter + glm::vec3(-0.062f, -0.014f,  0.000f),
        gogCenter + glm::vec3(-0.012f, -0.014f,  0.003f),
        gogCenter + glm::vec3( 0.012f, -0.014f,  0.003f),
        gogCenter + glm::vec3( 0.062f, -0.014f,  0.000f),
        gogCenter + glm::vec3( 0.062f,  0.014f,  0.000f),
        gogCenter + glm::vec3( 0.012f,  0.014f,  0.003f),
        gogCenter + glm::vec3(-0.012f,  0.014f,  0.003f),
        gogCenter + glm::vec3(-0.062f,  0.014f,  0.000f)
    };
    for (int i = 0; i < 8; ++i) {
        SkinnedVertex esv;
        esv.bindPos = lens[i];
        esv.bindNormal = glm::vec3(0, 0, 1.0f);
        esv.uv = glm::vec2(0.25f, 0.65f);
        esv.color = glassMat;
        esv.bone0 = BONE_HEAD;
        esv.bone1 = BONE_HEAD;
        esv.weight0 = 1.0f;
        esv.weight1 = 0.0f;
        esv.part = PART_HELMET_FAST;
        m_bindVertices.push_back(esv);
    }
    m_indices.push_back(gogBase + 0); m_indices.push_back(gogBase + 1); m_indices.push_back(gogBase + 6);
    m_indices.push_back(gogBase + 0); m_indices.push_back(gogBase + 6); m_indices.push_back(gogBase + 7);
    m_indices.push_back(gogBase + 2); m_indices.push_back(gogBase + 3); m_indices.push_back(gogBase + 4);
    m_indices.push_back(gogBase + 2); m_indices.push_back(gogBase + 4); m_indices.push_back(gogBase + 5);
}

void SkinnedSoldier::buildAnatomicalHeadAndFace() {
    glm::vec4 skinMat(0.88f, 0.74f, 0.62f, 0.50f); // PBR Mat Tag 0.50 = Human Skin SSS

    struct HeadRingDef {
        glm::vec3 center;
        glm::vec2 radii;
        float weightChest; // Bone 0 = SPINE_CHEST, Bone 1 = HEAD
        float vCoord;      // Exact vertical alignment with texture atlas
        float noseProtrusion;
        float lipProtrusion;
        float chinProtrusion;
        float orbitRecess;
        float browOverhang;
        float earWidth;
    };

    // Leonardo Da Vinci & Gold Standard Human Facial Canon (1.82m Athletic Military Operator)
    std::vector<HeadRingDef> ringDefs = {
        // 0: Mandarin collar junction (Y=1.370m) - securely nested inside jacket collar
        { glm::vec3(0.0f, 1.370f,  0.002f), glm::vec2(0.056f, 0.058f), 1.00f, 0.530f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        // 1: Lower neck / trapezius slope (Y=1.395m)
        { glm::vec3(0.0f, 1.395f,  0.001f), glm::vec2(0.059f, 0.061f), 0.70f, 0.550f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        // 2: Upper neck / thyroid prominence (Adam's apple, Y=1.420m)
        { glm::vec3(0.0f, 1.420f,  0.000f), glm::vec2(0.057f, 0.060f), 0.35f, 0.575f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        // 3: Submental crease & gonial angles of mandible (Y=1.440m - jaw angle undercut!)
        { glm::vec3(0.0f, 1.440f,  0.000f), glm::vec2(0.054f, 0.060f), 0.05f, 0.600f, 0.0f, 0.0f, 0.002f, 0.0f, 0.0f, 0.004f },
        // 4: Chin (Mental protuberance, Y=1.455m - 100% Head Bone, athletic masculine chin)
        { glm::vec3(0.0f, 1.455f,  0.002f), glm::vec2(0.050f, 0.067f), 0.00f, 0.625f, 0.0f, 0.0f, 0.005f, 0.0f, 0.0f, 0.008f },
        // 5: Mental sulcus (crease above chin, Y=1.472m)
        { glm::vec3(0.0f, 1.472f,  0.003f), glm::vec2(0.056f, 0.069f), 0.00f, 0.655f, 0.0f, 0.004f, 0.002f, 0.0f, 0.0f, 0.010f },
        // 6: Lower lip pout (Y=1.484m)
        { glm::vec3(0.0f, 1.484f,  0.004f), glm::vec2(0.059f, 0.071f), 0.00f, 0.665f, 0.0f, 0.005f, 0.0f, 0.0f, 0.0f, 0.011f },
        // 7: Upper lip Cupid's bow & oral slit (Y=1.496m, v=0.675 matches texture lips!)
        { glm::vec3(0.0f, 1.496f,  0.005f), glm::vec2(0.063f, 0.073f), 0.00f, 0.675f, 0.0f, 0.005f, 0.0f, 0.0f, 0.0f, 0.012f },
        // 8: Nasal base, nostrils & philtrum (Y=1.516m, v=0.722 matches texture nostrils!)
        { glm::vec3(0.0f, 1.516f,  0.005f), glm::vec2(0.067f, 0.076f), 0.00f, 0.722f, 0.016f, 0.0f, 0.0f, 0.0f, 0.0f, 0.014f },
        // 9: Rhinion / nose bridge & cheekbones (Y=1.540m, v=0.745 matches texture nose bridge!)
        { glm::vec3(0.0f, 1.540f,  0.004f), glm::vec2(0.072f, 0.082f), 0.00f, 0.745f, 0.012f, 0.0f, 0.0f, 0.0f, 0.0f, 0.014f },
        // 10: Orbital eye cavities & nasion (Y=1.560m, v=0.770 matches texture eyes!)
        { glm::vec3(0.0f, 1.560f,  0.003f), glm::vec2(0.075f, 0.086f), 0.00f, 0.770f, 0.005f, 0.0f, 0.0f, -0.006f, 0.0f, 0.012f },
        // 11: Supraorbital brow ridge (Y=1.580m, v=0.805 matches texture eyebrows!)
        { glm::vec3(0.0f, 1.580f,  0.001f), glm::vec2(0.076f, 0.088f), 0.00f, 0.805f, 0.0f, 0.0f, 0.0f, 0.0f, 0.006f, 0.006f },
        // 12: Temples & lower forehead (Y=1.600m)
        { glm::vec3(0.0f, 1.600f, -0.002f), glm::vec2(0.075f, 0.087f), 0.00f, 0.830f, 0.0f, 0.0f, 0.0f, 0.0f, 0.002f, 0.0f },
        // 13: Frontal hairline / upper forehead (Y=1.620m, v=0.855 matches texture hairline!)
        { glm::vec3(0.0f, 1.620f, -0.005f), glm::vec2(0.074f, 0.087f), 0.00f, 0.855f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        // 14: Parietal cranial vault (broad rounded human skull dome, Y=1.645m)
        { glm::vec3(0.0f, 1.645f, -0.008f), glm::vec2(0.072f, 0.084f), 0.00f, 0.900f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        // 15: Upper cranial crown dome (Y=1.668m)
        { glm::vec3(0.0f, 1.668f, -0.012f), glm::vec2(0.064f, 0.074f), 0.00f, 0.940f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        // 16: Scalp apex dome (Y=1.685m, broad rounded dome)
        { glm::vec3(0.0f, 1.685f, -0.015f), glm::vec2(0.040f, 0.046f), 0.00f, 0.980f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f }
    };

    const int radialSegments = 48;
    const int cols = radialSegments + 1; // Duplicate seam column prevents texture wrapping seam
    size_t numRings = ringDefs.size();
    uint32_t baseVertex = static_cast<uint32_t>(m_bindVertices.size());

    std::vector<std::vector<glm::vec3>> gridPositions(numRings, std::vector<glm::vec3>(cols));
    std::vector<std::vector<glm::vec2>> gridUVs(numRings, std::vector<glm::vec2>(cols));
    std::vector<std::vector<std::pair<float, float>>> gridWeights(numRings, std::vector<std::pair<float, float>>(cols));

    for (size_t r = 0; r < numRings; ++r) {
        const auto& rd = ringDefs[r];
        float v = rd.vCoord;
        float w0 = rd.weightChest;
        float w1 = 1.0f - w0;

        for (int s = 0; s <= radialSegments; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(radialSegments);
            float theta = (t - 0.5f) * 2.0f * glm::pi<float>();
            float sinT = std::sin(theta); // Horizontal lateral: -1 (left) to +1 (right)
            float cosT = std::cos(theta); // Sagittal depth: +1 (front) to -1 (back)

            float radX = rd.radii.x;
            float radZ = rd.radii.y;
            float extraZ = 0.0f;
            float extraX = 0.0f;

            // 1. Nose & Nostril Wings (protrude strictly on front face cosT > 0.60)
            if (rd.noseProtrusion > 0.001f && cosT > 0.60f) {
                float frontProfile = std::pow(std::max(0.0f, cosT), 12.0f);
                extraZ += rd.noseProtrusion * frontProfile;
                float alarProfile = std::pow(std::max(0.0f, cosT), 6.0f) * std::abs(sinT);
                extraX += 0.006f * alarProfile * ((sinT >= 0.0f) ? 1.0f : -1.0f);
            }

            // 2. Lips & Philtrum
            if (rd.lipProtrusion > 0.001f && cosT > 0.68f) {
                float lipProfile = std::pow(std::max(0.0f, cosT), 10.0f);
                extraZ += rd.lipProtrusion * lipProfile;
                if (std::abs(sinT) < 0.06f) extraZ -= 0.001f;
            }

            // 3. Masculine Chin (front face only)
            if (rd.chinProtrusion > 0.001f && cosT > 0.60f) {
                float chinProfile = std::pow(std::max(0.0f, cosT), 8.0f);
                extraZ += rd.chinProtrusion * chinProfile;
            }

            // 4. Orbital Eye Cavities
            if (rd.orbitRecess < -0.001f && cosT > 0.70f) {
                float eyeCenter = std::abs(sinT) - 0.35f;
                float eyeDepression = std::exp(-eyeCenter * eyeCenter * 35.0f);
                extraZ += rd.orbitRecess * eyeDepression;
            }

            // 5. Brow Ridge Overhang
            if (rd.browOverhang > 0.001f && cosT > 0.65f) {
                float browProfile = std::pow(std::max(0.0f, cosT), 4.0f);
                extraZ += rd.browOverhang * browProfile;
            }

            // 6. Anatomical Ear Projection (lateral at cosT ~ 0.0)
            if (rd.earWidth > 0.001f && std::abs(sinT) > 0.85f && cosT > -0.25f && cosT < 0.20f) {
                float earProfile = (std::abs(sinT) - 0.85f) / 0.15f;
                extraX += rd.earWidth * earProfile * ((sinT >= 0.0f) ? 1.0f : -1.0f);
                extraZ -= 0.004f * earProfile;
            }

            gridPositions[r][s] = rd.center + glm::vec3(sinT * radX + extraX, 0.0f, cosT * radZ + extraZ);

            // UV mapping: 100% Symmetric unwrap centered at u = 0.750 (the exact center of Q1)
            // theta = 0 (front, s=24) -> u = 0.750
            // theta = +pi/2 (char right, +X, s=36) -> u = 0.630
            // theta = -pi/2 (char left, -X, s=12) -> u = 0.870
            // theta = +pi (back right, s=48) -> u = 0.510
            // theta = -pi (back left, s=0) -> u = 0.990
            // UV mapping: Symmetric unwrap centered directly on facial features at u = 0.750
            // theta = 0 (front of face, +Z, s=24) -> u = 0.750 (exact center of eyes, nose, mouth)
            // theta = +pi (back right, s=48) -> u = 0.550 (dark brown hair)
            // theta = -pi (back left, s=0) -> u = 0.950 (dark brown hair)
            float u = 0.750f - (theta / glm::pi<float>()) * 0.200f;
            u = glm::clamp(u, 0.505f, 0.995f);

            gridUVs[r][s] = glm::vec2(u, v);
            gridWeights[r][s] = { w0, w1 };
        }
    }

    // Compute smooth surface normals and push vertices
    for (size_t r = 0; r < numRings; ++r) {
        for (int s = 0; s <= radialSegments; ++s) {
            int nextS = (s + 1 <= radialSegments) ? s + 1 : 1;
            int prevS = (s > 0) ? s - 1 : radialSegments - 1;

            glm::vec3 tanU = gridPositions[r][nextS] - gridPositions[r][prevS];
            glm::vec3 pPrev = (r > 0) ? gridPositions[r - 1][s] : gridPositions[r][s];
            glm::vec3 pNext = (r + 1 < numRings) ? gridPositions[r + 1][s] : gridPositions[r][s];
            glm::vec3 tanV = pNext - pPrev;

            glm::vec3 norm = glm::cross(tanV, tanU);
            if (glm::length(norm) > 0.0001f) norm = glm::normalize(norm);
            else norm = glm::vec3(0, 1, 0);

            glm::vec3 radialDir = gridPositions[r][s] - ringDefs[r].center;
            radialDir.y = 0.0f;
            if (glm::length(radialDir) > 0.0001f) {
                radialDir = glm::normalize(radialDir);
                if (glm::dot(norm, radialDir) < 0.0f) norm = -norm;
            }

            SkinnedVertex sv;
            sv.bindPos = gridPositions[r][s];
            sv.bindNormal = norm;
            sv.uv = gridUVs[r][s];
            sv.color = skinMat;
            sv.bone0 = BONE_SPINE_CHEST;
            sv.bone1 = BONE_HEAD;
            sv.weight0 = gridWeights[r][s].first;
            sv.weight1 = gridWeights[r][s].second;
            sv.part = PART_FACE;

            m_bindVertices.push_back(sv);
        }
    }

    // Connect rings with quad triangles
    for (size_t r = 0; r < numRings - 1; ++r) {
        for (int s = 0; s < radialSegments; ++s) {
            uint32_t i00 = baseVertex + static_cast<uint32_t>(r * cols + s);
            uint32_t i01 = baseVertex + static_cast<uint32_t>(r * cols + s + 1);
            uint32_t i10 = baseVertex + static_cast<uint32_t>((r + 1) * cols + s);
            uint32_t i11 = baseVertex + static_cast<uint32_t>((r + 1) * cols + s + 1);

            m_indices.push_back(i00); m_indices.push_back(i01); m_indices.push_back(i10);
            m_indices.push_back(i01); m_indices.push_back(i11); m_indices.push_back(i10);
        }
    }

    // Cap the cranial crown apex smoothly with rounded fan
    uint32_t apexIdx = static_cast<uint32_t>(m_bindVertices.size());
    SkinnedVertex apexV;
    apexV.bindPos = ringDefs.back().center + glm::vec3(0.0f, 0.008f, 0.0f);
    apexV.bindNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    apexV.uv = glm::vec2(0.750f, 0.985f);
    apexV.color = skinMat;
    apexV.bone0 = BONE_SPINE_CHEST;
    apexV.bone1 = BONE_HEAD;
    apexV.weight0 = 0.0f;
    apexV.weight1 = 1.0f;
    apexV.part = PART_FACE;
    m_bindVertices.push_back(apexV);

    uint32_t topRingBase = baseVertex + static_cast<uint32_t>((numRings - 1) * cols);
    for (int s = 0; s < radialSegments; ++s) {
        m_indices.push_back(apexIdx);
        m_indices.push_back(topRingBase + s);
        m_indices.push_back(topRingBase + s + 1);
    }
}

void SkinnedSoldier::buildHairstyles() {
    glm::vec3 head = m_bindJoints[BONE_HEAD]; // (0, 1.455f, 0)
    glm::vec4 baseHairMat(0.20f, 0.16f, 0.12f, 0.85f); // Mat tag 0.85 = PBR hair anisotropic sheen

    // 1. Buzzcut (hairId == 0): form-fitting scalp cap on cranial crown flush with skull
    std::vector<glm::vec3> buzzRings = {
        head + glm::vec3(0.000f, 0.150f, -0.002f), // 1.605m: Hairline root flush with skull
        head + glm::vec3(0.000f, 0.165f, -0.004f), // 1.620m
        head + glm::vec3(0.000f, 0.185f, -0.006f), // 1.640m
        head + glm::vec3(0.000f, 0.205f, -0.009f), // 1.660m
        head + glm::vec3(0.000f, 0.218f, -0.011f), // 1.673m
        head + glm::vec3(0.000f, 0.224f, -0.012f)  // 1.679m
    };
    std::vector<glm::vec2> buzzRadii = {
        glm::vec2(0.0745f, 0.0872f), // Seamlessly seals against scalp!
        glm::vec2(0.0760f, 0.0880f),
        glm::vec2(0.0740f, 0.0860f),
        glm::vec2(0.0660f, 0.0760f),
        glm::vec2(0.0480f, 0.0540f),
        glm::vec2(0.0240f, 0.0260f)
    };
    std::vector<float> buzzWeights = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    appendSkinnedTube(buzzRings, buzzRadii, buzzWeights,
                      BONE_HEAD, BONE_HEAD,
                      glm::vec2(0.55f, 0.85f), glm::vec2(0.95f, 0.98f),
                      baseHairMat, 32, {}, PART_HAIR_BUZZCUT, true);

    // 2. Operator Undercut / Modern Pompadour (hairId == 1)
    std::vector<glm::vec3> underRings = {
        head + glm::vec3(0.000f, 0.150f, -0.002f), // Seamlessly seals against scalp
        head + glm::vec3(0.000f, 0.165f, -0.003f),
        head + glm::vec3(0.002f, 0.186f, -0.005f),
        head + glm::vec3(0.004f, 0.208f, -0.008f),
        head + glm::vec3(0.003f, 0.222f, -0.010f),
        head + glm::vec3(0.001f, 0.230f, -0.012f)
    };
    std::vector<glm::vec2> underRadii = {
        glm::vec2(0.0745f, 0.0872f), // Seamlessly seals against scalp!
        glm::vec2(0.0780f, 0.0900f),
        glm::vec2(0.0760f, 0.0880f),
        glm::vec2(0.0680f, 0.0780f),
        glm::vec2(0.0500f, 0.0560f),
        glm::vec2(0.0260f, 0.0280f)
    };
    std::vector<float> underWeights = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    appendSkinnedTube(underRings, underRadii, underWeights,
                      BONE_HEAD, BONE_HEAD,
                      glm::vec2(0.55f, 0.85f), glm::vec2(0.95f, 0.98f),
                      baseHairMat, 32, {}, PART_HAIR_UNDERCUT, true);

    // 3. Tactical Crew Cut (hairId == 2)
    std::vector<glm::vec3> crewRings = {
        head + glm::vec3(0.0f, 0.150f, -0.002f), // Seamlessly seals against scalp
        head + glm::vec3(0.0f, 0.165f, -0.003f),
        head + glm::vec3(0.0f, 0.186f, -0.005f),
        head + glm::vec3(0.0f, 0.206f, -0.008f),
        head + glm::vec3(0.0f, 0.220f, -0.010f),
        head + glm::vec3(0.0f, 0.226f, -0.012f)
    };
    std::vector<glm::vec2> crewRadii = {
        glm::vec2(0.0745f, 0.0872f), // Seamlessly seals against scalp!
        glm::vec2(0.0770f, 0.0890f),
        glm::vec2(0.0750f, 0.0870f),
        glm::vec2(0.0670f, 0.0770f),
        glm::vec2(0.0490f, 0.0550f),
        glm::vec2(0.0250f, 0.0270f)
    };
    std::vector<float> crewWeights = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    appendSkinnedTube(crewRings, crewRadii, crewWeights,
                      BONE_HEAD, BONE_HEAD,
                      glm::vec2(0.55f, 0.85f), glm::vec2(0.95f, 0.98f),
                      baseHairMat, 32, {}, PART_HAIR_CREWCUT, true);
}

void SkinnedSoldier::buildFacialHair() {
    glm::vec3 head = m_bindJoints[BONE_HEAD]; // (0, 1.48f, 0)
    glm::vec4 baseHairMat(0.20f, 0.16f, 0.12f, 0.85f);

    auto appendFacialPatch = [&](
        const std::vector<glm::vec3>& ringCenters,
        const std::vector<glm::vec2>& ringRadii,
        float minAngle, float maxAngle, int angularSegments,
        uint8_t partTag
    ) {
        size_t numRings = std::min(ringCenters.size(), ringRadii.size());
        if (numRings < 2) return;
        uint32_t baseVertex = static_cast<uint32_t>(m_bindVertices.size());
        int cols = angularSegments + 1;

        for (size_t r = 0; r < numRings; ++r) {
            float v = static_cast<float>(r) / static_cast<float>(numRings - 1);
            float radX = ringRadii[r].x;
            float radZ = ringRadii[r].y;

            for (int s = 0; s <= angularSegments; ++s) {
                float t = static_cast<float>(s) / static_cast<float>(angularSegments);
                float angle = minAngle + t * (maxAngle - minAngle);
                float sinA = std::sin(angle);
                float cosA = std::cos(angle);

                // Snug form-fitting anterior surface (+1.8mm offset prevents z-fighting, strictly zero mouth penetration)
                glm::vec3 pos = ringCenters[r] + glm::vec3(sinA * radX, 0.0f, cosA * radZ);
                glm::vec3 norm = glm::normalize(glm::vec3(sinA, 0.12f, cosA));

                SkinnedVertex sv;
                sv.bindPos = pos + norm * 0.0018f;
                sv.bindNormal = norm;
                sv.uv = glm::vec2(0.65f + t * 0.20f, 0.91f + v * 0.06f);
                sv.color = baseHairMat;
                sv.bone0 = BONE_HEAD;
                sv.bone1 = BONE_HEAD;
                sv.weight0 = 0.0f;
                sv.weight1 = 1.0f;
                sv.part = partTag;

                m_bindVertices.push_back(sv);
            }
        }

        for (size_t r = 0; r < numRings - 1; ++r) {
            for (int s = 0; s < angularSegments; ++s) {
                uint32_t i00 = baseVertex + static_cast<uint32_t>(r * cols + s);
                uint32_t i01 = baseVertex + static_cast<uint32_t>(r * cols + s + 1);
                uint32_t i10 = baseVertex + static_cast<uint32_t>((r + 1) * cols + s);
                uint32_t i11 = baseVertex + static_cast<uint32_t>((r + 1) * cols + s + 1);

                // Front faces (CCW)
                m_indices.push_back(i00); m_indices.push_back(i10); m_indices.push_back(i01);
                m_indices.push_back(i01); m_indices.push_back(i10); m_indices.push_back(i11);
                // Back faces (CW) for double-sided thin hair shell
                m_indices.push_back(i00); m_indices.push_back(i01); m_indices.push_back(i10);
                m_indices.push_back(i01); m_indices.push_back(i11); m_indices.push_back(i10);
            }
        }
    };

    // 1. Full Tactical Volumetric Beard (beardId == 0)
    // Contoured jawline beard from ear to ear across chin, cheekline, and moustache
    std::vector<glm::vec3> fullBeardRings = {
        head + glm::vec3(0.0f, -0.015f, -0.005f), // Submental crease (1.465m)
        head + glm::vec3(0.0f,  0.000f,  0.000f), // Chin mental protuberance (1.480m)
        head + glm::vec3(0.0f,  0.018f,  0.002f), // Mental sulcus (1.498m)
        head + glm::vec3(0.0f,  0.038f,  0.004f), // Upper jaw / lower cheek (1.518m)
        head + glm::vec3(0.0f,  0.055f,  0.005f)  // Moustache & cheek trim (1.535m)
    };
    std::vector<glm::vec2> fullBeardRadii = {
        glm::vec2(0.052f, 0.058f),
        glm::vec2(0.048f, 0.068f),
        glm::vec2(0.056f, 0.070f),
        glm::vec2(0.065f, 0.075f),
        glm::vec2(0.068f, 0.077f)
    };
    appendFacialPatch(fullBeardRings, fullBeardRadii, -0.55f * glm::pi<float>(), 0.55f * glm::pi<float>(), 20, PART_BEARD_FULL);

    // 2. Sculpted Tactical Goatee (beardId == 2)
    // Snug form-fitting chin goatee patch (anterior only: angle -0.22*pi to +0.22*pi, zero mouth penetration)
    std::vector<glm::vec3> goateeChinRings = {
        head + glm::vec3(0.0f, -0.010f, 0.000f), // Lower chin edge (1.470m)
        head + glm::vec3(0.0f,  0.002f, 0.001f), // Chin tip tuft (1.482m)
        head + glm::vec3(0.0f,  0.018f, 0.003f)  // Soul patch ending below lower lip (1.498m)
    };
    std::vector<glm::vec2> goateeChinRadii = {
        glm::vec2(0.047f, 0.067f),
        glm::vec2(0.049f, 0.069f),
        glm::vec2(0.055f, 0.070f)
    };
    appendFacialPatch(goateeChinRings, goateeChinRadii, -0.22f * glm::pi<float>(), 0.22f * glm::pi<float>(), 10, PART_BEARD_GOATEE);

    // Goatee Moustache trim (anterior under nose only, from 1.524m to 1.538m)
    std::vector<glm::vec3> goateeMoustacheRings = {
        head + glm::vec3(0.0f, 0.044f, 0.005f), // Above upper lip (1.524m)
        head + glm::vec3(0.0f, 0.056f, 0.005f)  // Under nostrils (1.536m)
    };
    std::vector<glm::vec2> goateeMoustacheRadii = {
        glm::vec2(0.063f, 0.074f),
        glm::vec2(0.066f, 0.077f)
    };
    appendFacialPatch(goateeMoustacheRings, goateeMoustacheRadii, -0.24f * glm::pi<float>(), 0.24f * glm::pi<float>(), 10, PART_BEARD_GOATEE);
}

void SkinnedSoldier::appendVolumetricArmband(
    const std::vector<glm::vec3>& ringCenters,
    const std::vector<glm::vec2>& outerRadii,
    const std::vector<glm::vec2>& innerRadii,
    const std::vector<float>& ringWeightsBone0,
    uint8_t bone0, uint8_t bone1,
    const glm::vec4& color, int radialSegments)
{
    size_t numRings = std::min({ringCenters.size(), outerRadii.size(), innerRadii.size(), ringWeightsBone0.size()});
    if (numRings < 2) return;

    // 1. Generate Outer Wall Vertices (facing outward)
    uint32_t outerBase = static_cast<uint32_t>(m_bindVertices.size());
    for (size_t r = 0; r < numRings; ++r) {
        float tV = static_cast<float>(r) / static_cast<float>(numRings - 1);
        float w0 = ringWeightsBone0[r];
        float w1 = 1.0f - w0;
        float rx = outerRadii[r].x;
        float rz = outerRadii[r].y;

        for (int s = 0; s < radialSegments; ++s) {
            float tU = static_cast<float>(s) / static_cast<float>(radialSegments);
            float theta = tU * 2.0f * glm::pi<float>();
            float cosT = std::cos(theta);
            float sinT = std::sin(theta);

            glm::vec3 p = ringCenters[r] + glm::vec3(cosT * rx, 0.0f, sinT * rz);
            glm::vec3 n = glm::normalize(glm::vec3(cosT, 0.0f, sinT));

            SkinnedVertex sv;
            sv.bindPos = p;
            sv.bindNormal = n;
            sv.uv = glm::vec2(0.55f + tU * 0.40f, 0.05f + tV * 0.40f);
            sv.color = color;
            sv.bone0 = bone0;
            sv.bone1 = bone1;
            sv.weight0 = w0;
            sv.weight1 = w1;
            sv.part = PART_ARMBAND;
            m_bindVertices.push_back(sv);
        }
    }

    // 2. Generate Inner Wall Vertices (facing inward towards arm cloth)
    uint32_t innerBase = static_cast<uint32_t>(m_bindVertices.size());
    for (size_t r = 0; r < numRings; ++r) {
        float tV = static_cast<float>(r) / static_cast<float>(numRings - 1);
        float w0 = ringWeightsBone0[r];
        float w1 = 1.0f - w0;
        float rx = innerRadii[r].x;
        float rz = innerRadii[r].y;

        for (int s = 0; s < radialSegments; ++s) {
            float tU = static_cast<float>(s) / static_cast<float>(radialSegments);
            float theta = tU * 2.0f * glm::pi<float>();
            float cosT = std::cos(theta);
            float sinT = std::sin(theta);

            glm::vec3 p = ringCenters[r] + glm::vec3(cosT * rx, 0.0f, sinT * rz);
            glm::vec3 n = glm::normalize(glm::vec3(-cosT, 0.0f, -sinT));

            SkinnedVertex sv;
            sv.bindPos = p;
            sv.bindNormal = n;
            sv.uv = glm::vec2(0.55f + tU * 0.40f, 0.05f + tV * 0.40f);
            sv.color = color;
            sv.bone0 = bone0;
            sv.bone1 = bone1;
            sv.weight0 = w0;
            sv.weight1 = w1;
            sv.part = PART_ARMBAND;
            m_bindVertices.push_back(sv);
        }
    }

    // 3. Connect Outer Wall (descending Y -> CCW outward winding is i00, i01, i10 and i01, i11, i10)
    for (size_t r = 0; r < numRings - 1; ++r) {
        for (int s = 0; s < radialSegments; ++s) {
            int nextS = (s + 1) % radialSegments;
            uint32_t i00 = outerBase + static_cast<uint32_t>(r * radialSegments + s);
            uint32_t i01 = outerBase + static_cast<uint32_t>(r * radialSegments + nextS);
            uint32_t i10 = outerBase + static_cast<uint32_t>((r + 1) * radialSegments + s);
            uint32_t i11 = outerBase + static_cast<uint32_t>((r + 1) * radialSegments + nextS);

            m_indices.push_back(i00); m_indices.push_back(i01); m_indices.push_back(i10);
            m_indices.push_back(i01); m_indices.push_back(i11); m_indices.push_back(i10);
        }
    }

    // 4. Connect Inner Wall (descending Y -> CCW inward winding is i00, i10, i01 and i01, i10, i11)
    for (size_t r = 0; r < numRings - 1; ++r) {
        for (int s = 0; s < radialSegments; ++s) {
            int nextS = (s + 1) % radialSegments;
            uint32_t i00 = innerBase + static_cast<uint32_t>(r * radialSegments + s);
            uint32_t i01 = innerBase + static_cast<uint32_t>(r * radialSegments + nextS);
            uint32_t i10 = innerBase + static_cast<uint32_t>((r + 1) * radialSegments + s);
            uint32_t i11 = innerBase + static_cast<uint32_t>((r + 1) * radialSegments + nextS);

            m_indices.push_back(i00); m_indices.push_back(i10); m_indices.push_back(i01);
            m_indices.push_back(i01); m_indices.push_back(i10); m_indices.push_back(i11);
        }
    }

    // 5. Connect Top Rim Cap (at r = 0, facing UP)
    for (int s = 0; s < radialSegments; ++s) {
        int nextS = (s + 1) % radialSegments;
        uint32_t out0 = outerBase + s;
        uint32_t out1 = outerBase + nextS;
        uint32_t in0  = innerBase + s;
        uint32_t in1  = innerBase + nextS;

        m_indices.push_back(out0); m_indices.push_back(in0);  m_indices.push_back(out1);
        m_indices.push_back(out1); m_indices.push_back(in0);  m_indices.push_back(in1);
    }

    // 6. Connect Bottom Rim Cap (at r = numRings - 1, facing DOWN)
    uint32_t lastOuter = outerBase + static_cast<uint32_t>((numRings - 1) * radialSegments);
    uint32_t lastInner = innerBase + static_cast<uint32_t>((numRings - 1) * radialSegments);
    for (int s = 0; s < radialSegments; ++s) {
        int nextS = (s + 1) % radialSegments;
        uint32_t out0 = lastOuter + s;
        uint32_t out1 = lastOuter + nextS;
        uint32_t in0  = lastInner + s;
        uint32_t in1  = lastInner + nextS;

        m_indices.push_back(out0); m_indices.push_back(out1); m_indices.push_back(in0);
        m_indices.push_back(out1); m_indices.push_back(in1);  m_indices.push_back(in0);
    }
}

void SkinnedSoldier::buildArmbands() {
    glm::vec4 armMat(0.92f, 0.20f, 0.20f, 0.95f); // PBR Tag 0.95 = IFF Armband

    for (int side = 0; side < 2; ++side) {
        float sX = (side == 0) ? -1.0f : 1.0f;
        uint8_t upperBone = (side == 0) ? BONE_L_UPPER_ARM : BONE_R_UPPER_ARM;

        // Positioned cleanly on bicep between Ring 3 (Y=1.225) and Ring 4 (Y=1.165)
        // 100% upperBone with exact concentric alignment
        float yTop = 1.220f;
        float yMid = 1.195f;
        float yBot = 1.170f;
        std::vector<float> yLevels = { yTop, yMid, yBot };

        std::vector<glm::vec3> bandCenters;
        std::vector<glm::vec2> outerRadii;
        std::vector<glm::vec2> innerRadii;

        glm::vec3 c3(sX * 0.280f, 1.225f, -0.005f);
        glm::vec3 c4(sX * 0.298f, 1.165f, -0.008f);
        glm::vec2 r3(0.056f, 0.054f);
        glm::vec2 r4(0.054f, 0.052f);

        for (float y : yLevels) {
            float t = (1.225f - y) / (1.225f - 1.165f);
            glm::vec3 c = glm::mix(c3, c4, t);
            glm::vec2 r = glm::mix(r3, r4, t);
            bandCenters.push_back(c);
            // Generous radial clearance (+7.8mm outer, +5.4mm inner) sits 100% above any sleeve cloth wrinkles
            outerRadii.push_back(r + glm::vec2(0.0105f, 0.0105f));
            innerRadii.push_back(r + glm::vec2(0.0035f, 0.0035f));
        }

        std::vector<float> bandWeights = { 1.0f, 1.0f, 1.0f };

        appendVolumetricArmband(bandCenters, outerRadii, innerRadii, bandWeights,
                                upperBone, upperBone, armMat, 32);
    }
}

// ----------------------------------------------------------------------------
// 8. DEFORM MASTER MESH TO SKELETON POSE VIA LINEAR & SPHERICAL BLEND SKINNING
// ----------------------------------------------------------------------------
void SkinnedSoldier::deform(const SkeletonPose& pose,

                           const CharacterCustomization& custom,

                           std::vector<Vertex>& outVertices,

                           std::vector<uint32_t>& outIndices,

                           uint32_t dismemberMask) const

{

    outVertices.clear();

    outIndices.clear();

    // Map each bind vertex to its new index in outVertices (-1 if excluded)

    m_remapCache.assign(m_bindVertices.size(), -1);

    outVertices.reserve(m_bindVertices.size());
    outIndices.reserve(m_indices.size());

    // Pre-convert 16 bone matrices to unit quaternions for spherical blending
    glm::quat boneQuats[NUM_CHARACTER_BONES];
    for (int b = 0; b < NUM_CHARACTER_BONES; ++b) {
        boneQuats[b] = glm::normalize(glm::quat_cast(pose.boneRotations[b]));
    }

    // Color Selection from persistent mercenary customization

    glm::vec4 skinTones[4] = {

        glm::vec4(0.85f, 0.72f, 0.60f, 1.0f), // Fair Light

        glm::vec4(0.75f, 0.60f, 0.48f, 1.0f), // Slavic Tan

        glm::vec4(0.68f, 0.52f, 0.38f, 1.0f), // Olive

        glm::vec4(0.45f, 0.32f, 0.22f, 1.0f)  // Deep Warm

    };

    glm::vec4 baseSkin = skinTones[std::clamp(custom.skinToneId, 0, 3)];

    // Continuous melanin spectrum blend from Fair Nordic (0.0) -> Slavic Tan (0.35) -> Deep Warm (1.0)

    glm::vec3 melaninSpectrum = glm::mix(glm::vec3(0.88f, 0.76f, 0.64f), glm::vec3(0.38f, 0.26f, 0.18f), std::clamp(custom.melanin, 0.0f, 1.0f));

    glm::vec4 skinCol = glm::vec4(glm::mix(glm::vec3(baseSkin), melaninSpectrum, 0.55f), 1.0f);

    glm::vec4 hairColors[4] = {

        glm::vec4(0.22f, 0.16f, 0.12f, 0.85f), // 0: Dark Brown

        glm::vec4(0.06f, 0.06f, 0.07f, 0.85f), // 1: Jet Black

        glm::vec4(0.58f, 0.46f, 0.30f, 0.85f), // 2: Dirty Blonde

        glm::vec4(0.68f, 0.68f, 0.72f, 0.85f)  // 3: Grizzled Silver

    };

    glm::vec4 hairCol = hairColors[std::clamp(custom.hairColorId, 0, 3)];

    glm::vec4 camoPatterns[4] = {

        glm::vec4(0.44f, 0.42f, 0.32f, 1.0f), // 0: MultiCam OCP (Mercenary)

        glm::vec4(0.38f, 0.36f, 0.28f, 1.0f), // 1: MM-14 Digital Pixel (Mercenary)

        glm::vec4(0.16f, 0.17f, 0.18f, 1.0f), // 2: Covert Charcoal (Crimson)

        glm::vec4(0.24f, 0.30f, 0.18f, 1.0f)  // 3: M81 Woodland (Crimson)

    };

    int effectiveCamo = std::clamp(custom.camoId, 0, 3);

    glm::vec4 uniformCamo = camoPatterns[effectiveCamo];

    glm::vec4 vestColors[3] = {

        glm::vec4(0.42f, 0.40f, 0.32f, 1.0f), // Crye Coyote Tan

        glm::vec4(0.28f, 0.35f, 0.22f, 1.0f), // Ratnik Olive

        glm::vec4(0.20f, 0.22f, 0.18f, 1.0f)  // Ranger Green

    };

    glm::vec4 vestCol = vestColors[std::clamp(custom.vestId, 0, 2)];

    glm::vec4 gloveColors[3] = {

        glm::vec4(0.45f, 0.40f, 0.30f, 1.0f), // Coyote Tan

        glm::vec4(0.12f, 0.12f, 0.12f, 1.0f), // Black Hard-Knuckle

        glm::vec4(0.25f, 0.32f, 0.20f, 1.0f)  // Olive Drab

    };

    glm::vec4 gloveCol = gloveColors[std::clamp(custom.gloveId, 0, 2)];

    bool includeHelmet = (custom.helmetId < 4);

    bool includeVest = (custom.vestId < 3);

    // Evaluate Linear Blend Skinning (LBS) and dynamic part filtering

    for (size_t i = 0; i < m_bindVertices.size(); ++i) {

        const auto& bv = m_bindVertices[i];

        // Dismemberment anatomical filtering
        if (dismemberMask != 0) {
            if (dismemberMask & DISMEMBER_EXPLODED) {
                continue;
            }
            if (dismemberMask & DISMEMBER_HEAD) {
                if (bv.part == PART_FACE || bv.part == PART_HELMET_FAST ||
                    bv.part == PART_HAIR_BUZZCUT || bv.part == PART_HAIR_UNDERCUT ||
                    bv.part == PART_HAIR_CREWCUT || bv.part == PART_BEARD_FULL ||
                    bv.part == PART_BEARD_GOATEE ||
                    bv.bone0 == BONE_HEAD || bv.bone1 == BONE_HEAD) {
                    continue;
                }
            }
            if (dismemberMask & DISMEMBER_L_ARM) {
                if (bv.bone0 == BONE_L_UPPER_ARM || bv.bone0 == BONE_L_FOREARM || bv.bone0 == BONE_L_HAND ||
                    bv.bone1 == BONE_L_UPPER_ARM || bv.bone1 == BONE_L_FOREARM || bv.bone1 == BONE_L_HAND) {
                    if (bv.part != PART_VEST) continue;
                }
            }
            if (dismemberMask & DISMEMBER_R_ARM) {
                if (bv.bone0 == BONE_R_UPPER_ARM || bv.bone0 == BONE_R_FOREARM || bv.bone0 == BONE_R_HAND || bv.bone0 == BONE_R_TRIGGER_FINGER ||
                    bv.bone1 == BONE_R_UPPER_ARM || bv.bone1 == BONE_R_FOREARM || bv.bone1 == BONE_R_HAND || bv.bone1 == BONE_R_TRIGGER_FINGER) {
                    if (bv.part != PART_VEST) continue;
                }
            }
            if (dismemberMask & DISMEMBER_L_LEG) {
                if (bv.bone0 == BONE_L_THIGH || bv.bone0 == BONE_L_CALF || bv.bone0 == BONE_L_FOOT ||
                    bv.bone1 == BONE_L_THIGH || bv.bone1 == BONE_L_CALF || bv.bone1 == BONE_L_FOOT) {
                    continue;
                }
            }
            if (dismemberMask & DISMEMBER_R_LEG) {
                if (bv.bone0 == BONE_R_THIGH || bv.bone0 == BONE_R_CALF || bv.bone0 == BONE_R_FOOT ||
                    bv.bone1 == BONE_R_THIGH || bv.bone1 == BONE_R_CALF || bv.bone1 == BONE_R_FOOT) {
                    continue;
                }
            }
        }

        bool keep = false;

        switch (bv.part) {

            case PART_BODY:

            case PART_FACE:

                keep = true;

                break;

            case PART_HELMET_FAST:

                keep = includeHelmet;

                break;

            case PART_HAIR_BUZZCUT:

                keep = (!includeHelmet && custom.hairId == 0);

                break;

            case PART_HAIR_UNDERCUT:

                keep = (!includeHelmet && custom.hairId == 1);

                break;

            case PART_HAIR_CREWCUT:

                keep = (!includeHelmet && custom.hairId == 2);

                break;

            case PART_BEARD_FULL:

                keep = (custom.beardId == 0);

                break;

            case PART_BEARD_GOATEE:

                keep = (custom.beardId == 2);

                break;

            case PART_VEST:

                keep = includeVest;

                break;

            case PART_ARMBAND:

                keep = (custom.armbandId < 5);

                break;

            default:

                keep = true;

                break;

        }

        if (!keep) continue;

        m_remapCache[i] = static_cast<int>(outVertices.size());

        glm::vec3 bindP = bv.bindPos;

        glm::vec3 bindN = bv.bindNormal;

        // Dynamic Real-time Face Morphology (Nomad, Razor, Viper, Titan + Parametric Sliders)
        if (bv.part == PART_FACE || bv.part == PART_HAIR_BUZZCUT || bv.part == PART_HAIR_UNDERCUT || 
            bv.part == PART_HAIR_CREWCUT || bv.part == PART_BEARD_FULL || bv.part == PART_BEARD_GOATEE) {
            glm::vec3 headCenter = m_bindJoints[BONE_HEAD];
            glm::vec3 rel = bindP - headCenter;

            // Only apply morphology from neck upwards (rel.y > -0.020m), smooth cubic blend strictly preserving neck topology
            if (rel.y > -0.020f) {
                float faceBlend = glm::smoothstep(-0.020f, 0.025f, rel.y);

                // Continuous Gaussian weight curves across vertical head coordinates:
                // Jaw area centered at rel.y = 0.015m
                float jawWeight = std::exp(-std::pow((rel.y - 0.015f) / 0.040f, 2.0f)) * faceBlend;

                // Cheekbone area centered at rel.y = 0.085m
                float cheekWeight = std::exp(-std::pow((rel.y - 0.085f) / 0.035f, 2.0f)) * faceBlend;

                // Chin prominence centered at rel.y = 0.000m, front-facing
                float chinFront = std::clamp(rel.z / 0.065f, 0.0f, 1.0f);
                float chinWeight = std::exp(-std::pow(rel.y / 0.025f, 2.0f)) * chinFront * faceBlend;

                // Nose area centered at rel.y = 0.070m, front-facing
                float noseFront = std::clamp((rel.z - 0.035f) / 0.045f, 0.0f, 1.0f);
                float noseWeight = std::exp(-std::pow((rel.y - 0.070f) / 0.025f, 2.0f)) * noseFront;

                // 1. Subtle, authentic military archetype presets (Nomad = baseline)
                if (custom.faceId == 1) {
                    // Razor: Chiseled tactical aesthetic (leaner jaw -3%, cheekbones +2%)
                    rel.x *= (1.0f - 0.03f * jawWeight + 0.02f * cheekWeight);
                } else if (custom.faceId == 2) {
                    // Viper: Sleek angular operator (tapered jaw -2%, refined nose +1.2mm)
                    rel.x *= (1.0f - 0.02f * jawWeight);
                    rel.z += 0.0012f * noseWeight;
                } else if (custom.faceId == 3) {
                    // Titan: Broad square athletic veteran (solid jaw +4%, chin +1mm)
                    rel.x *= (1.0f + 0.04f * jawWeight);
                    rel.z += 0.0010f * chinWeight;
                }

                // 2. Controlled Athletic Parametric Face Sliders (Strictly bounded)
                float clampedJaw = std::clamp(custom.jawWidth, 0.94f, 1.06f);
                float clampedChin = std::clamp(custom.chinLength, -0.004f, 0.005f);
                float clampedNose = std::clamp(custom.noseScale, 0.94f, 1.06f);

                // Jaw Width Slider
                rel.x *= (1.0f + (clampedJaw - 1.0f) * jawWeight);

                // Chin Length Slider
                rel.y += clampedChin * chinWeight;
                rel.z += clampedChin * 0.40f * chinWeight;

                // Nose Scale Slider
                rel.z += (clampedNose - 1.0f) * 0.006f * noseWeight;
                rel.x *= (1.0f + (clampedNose - 1.0f) * 0.15f * noseWeight);
            }

            bindP = headCenter + rel;
        }

        glm::vec3 finalPos;
        glm::vec3 finalNorm;

        if (bv.bone0 == bv.bone1 || bv.weight0 >= 0.999f) {
            glm::vec3 rel0 = bindP - m_bindJoints[bv.bone0];
            finalPos = pose.bonePositions[bv.bone0] + pose.boneRotations[bv.bone0] * rel0;
            finalNorm = pose.boneRotations[bv.bone0] * bindN;
        } else if (bv.weight1 >= 0.999f) {
            glm::vec3 rel1 = bindP - m_bindJoints[bv.bone1];
            finalPos = pose.bonePositions[bv.bone1] + pose.boneRotations[bv.bone1] * rel1;
            finalNorm = pose.boneRotations[bv.bone1] * bindN;
        } else {
            // Dual-bone spherical/quaternion blend (preserves cross-section volume, 0% candy-wrapper collapse)
            glm::quat q0 = boneQuats[bv.bone0];
            glm::quat q1 = boneQuats[bv.bone1];
            if (glm::dot(q0, q1) < 0.0f) {
                q1 = -q1;
            }
            glm::quat qBlend = glm::normalize(glm::slerp(q0, q1, bv.weight1));
            glm::mat3 rBlend = glm::mat3_cast(qBlend);

            glm::vec3 rel0 = bindP - m_bindJoints[bv.bone0];
            glm::vec3 rel1 = bindP - m_bindJoints[bv.bone1];
            glm::vec3 relBlend = rel0 * bv.weight0 + rel1 * bv.weight1;
            glm::vec3 posBlend = pose.bonePositions[bv.bone0] * bv.weight0 + pose.bonePositions[bv.bone1] * bv.weight1;

            finalPos = posBlend + rBlend * relBlend;
            finalNorm = rBlend * bindN;
        }

        // Apply Customization Tints while strictly preserving PBR material alpha tag!

        float matTag = bv.color.a;

        glm::vec4 finalColor = bv.color;

        if (bv.part == PART_ARMBAND) {

            glm::vec4 armbandColors[5] = {

                glm::vec4(0.12f, 0.52f, 0.95f, 1.0f), // 0: Mercenary Blue

                glm::vec4(0.92f, 0.18f, 0.18f, 1.0f), // 1: Crimson Red

                glm::vec4(0.95f, 0.85f, 0.12f, 1.0f), // 2: Yellow

                glm::vec4(0.95f, 0.95f, 0.95f, 1.0f), // 3: White

                glm::vec4(0.40f, 0.40f, 0.30f, 1.0f)

            };

            finalColor = armbandColors[std::clamp(custom.armbandId, 0, 4)];

            finalColor.a = 0.95f;

        } else if (bv.part == PART_HAIR_BUZZCUT || bv.part == PART_HAIR_UNDERCUT || bv.part == PART_HAIR_CREWCUT ||

                   bv.part == PART_BEARD_FULL || bv.part == PART_BEARD_GOATEE) {

            finalColor.r = hairCol.r;

            finalColor.g = hairCol.g;

            finalColor.b = hairCol.b;

            finalColor.a = 0.85f; // Anisotropic hair/fur response

        } else if (matTag > 0.95f) {

            // Uniform Fabric

            finalColor.r = uniformCamo.r;

            finalColor.g = uniformCamo.g;

            finalColor.b = uniformCamo.b;

            finalColor.a = matTag;

        } else if (matTag > 0.85f && matTag <= 0.95f) {

            // Tactical Gear / Plate Carrier

            finalColor.r = vestCol.r;

            finalColor.g = vestCol.g;

            finalColor.b = vestCol.b;

            finalColor.a = matTag;

        } else if (matTag > 0.55f && matTag <= 0.65f) {

            // Gloves & Leather

            finalColor.r = gloveCol.r;

            finalColor.g = gloveCol.g;

            finalColor.b = gloveCol.b;

            finalColor.a = matTag;

        } else if (matTag > 0.45f && matTag <= 0.55f) {

            // Human Skin SSS

            finalColor.r = skinCol.r;

            finalColor.g = skinCol.g;

            finalColor.b = skinCol.b;

            finalColor.a = matTag;

// Stubble shadow is painted with high-frequency micro-pores directly in diffuse atlas

        } else {

            finalColor.a = matTag;

        }

        Vertex v;

        v.position = finalPos;

        v.normal = finalNorm;

        v.texCoords = bv.uv;

        v.color = finalColor;

        outVertices.push_back(v);

    }

    // Safely re-index active triangles (excluding any triangles that reference culled parts)

    for (size_t t = 0; t + 2 < m_indices.size(); t += 3) {

        int i0 = m_remapCache[m_indices[t]];

        int i1 = m_remapCache[m_indices[t + 1]];

        int i2 = m_remapCache[m_indices[t + 2]];

        if (i0 != -1 && i1 != -1 && i2 != -1) {

            outIndices.push_back(static_cast<uint32_t>(i0));

            outIndices.push_back(static_cast<uint32_t>(i1));

            outIndices.push_back(static_cast<uint32_t>(i2));

        }

    }

    // Append anatomical stump caps at severance planes
    if (dismemberMask != 0 && !(dismemberMask & DISMEMBER_EXPLODED)) {
        auto appendStumpCap = [&](const glm::vec3& worldCenter, const glm::vec3& worldNormal, const glm::vec3& worldTangent, float radius, float boneRadius) {
            glm::vec3 bitangent = glm::normalize(glm::cross(worldNormal, worldTangent));
            uint32_t baseIdx = static_cast<uint32_t>(outVertices.size());

            // Center vertex: bone marrow
            Vertex centerV;
            centerV.position = worldCenter + worldNormal * 0.002f;
            centerV.normal = worldNormal;
            centerV.texCoords = glm::vec2(0.5f, 0.5f);
            centerV.color = glm::vec4(0.38f, 0.04f, 0.04f, 0.95f);
            outVertices.push_back(centerV);

            const int segs = 16;
            // Inner ring: ivory cortical bone
            for (int s = 0; s < segs; ++s) {
                float th = static_cast<float>(s) / static_cast<float>(segs) * 2.0f * glm::pi<float>();
                float c = std::cos(th), si = std::sin(th);
                Vertex v;
                v.position = worldCenter + (worldTangent * c + bitangent * si) * boneRadius + worldNormal * 0.003f;
                v.normal = worldNormal;
                v.texCoords = glm::vec2(0.5f + c * 0.1f, 0.5f + si * 0.1f);
                v.color = glm::vec4(0.92f, 0.88f, 0.82f, 0.80f);
                outVertices.push_back(v);
            }

            // Outer ring: shredded coagulated muscle and arterial tissue
            for (int s = 0; s < segs; ++s) {
                float th = static_cast<float>(s) / static_cast<float>(segs) * 2.0f * glm::pi<float>();
                float c = std::cos(th), si = std::sin(th);
                float ragged = 1.0f + 0.12f * std::sin(th * 5.0f);
                Vertex v;
                v.position = worldCenter + (worldTangent * c + bitangent * si) * (radius * ragged);
                v.normal = worldNormal;
                v.texCoords = glm::vec2(0.5f + c * 0.4f, 0.5f + si * 0.4f);
                v.color = glm::vec4(0.48f, 0.03f, 0.03f, 0.95f);
                outVertices.push_back(v);
            }

            // Bone marrow center fan: center -> inner ring
            for (int s = 0; s < segs; ++s) {
                int nextS = (s + 1) % segs;
                outIndices.push_back(baseIdx);
                outIndices.push_back(baseIdx + 1 + s);
                outIndices.push_back(baseIdx + 1 + nextS);
            }

            // Muscle ring quads: inner ring -> outer ring
            for (int s = 0; s < segs; ++s) {
                int nextS = (s + 1) % segs;
                uint32_t in0 = baseIdx + 1 + s;
                uint32_t in1 = baseIdx + 1 + nextS;
                uint32_t out0 = baseIdx + 1 + segs + s;
                uint32_t out1 = baseIdx + 1 + segs + nextS;

                outIndices.push_back(in0);
                outIndices.push_back(out0);
                outIndices.push_back(in1);

                outIndices.push_back(in1);
                outIndices.push_back(out0);
                outIndices.push_back(out1);
            }
        };

        if (dismemberMask & DISMEMBER_HEAD) {
            glm::vec3 neckPos = pose.bonePositions[BONE_SPINE_CHEST] + pose.boneRotations[BONE_SPINE_CHEST] * glm::vec3(0.0f, 0.14f, 0.0f);
            glm::vec3 neckNorm = pose.boneRotations[BONE_SPINE_CHEST] * glm::vec3(0.0f, 1.0f, 0.0f);
            glm::vec3 neckTan = pose.boneRotations[BONE_SPINE_CHEST] * glm::vec3(1.0f, 0.0f, 0.0f);
            appendStumpCap(neckPos, neckNorm, neckTan, 0.052f, 0.016f);
        }
        if (dismemberMask & DISMEMBER_L_ARM) {
            glm::vec3 lShoulderPos = pose.bonePositions[BONE_L_UPPER_ARM];
            glm::vec3 lShoulderNorm = pose.boneRotations[BONE_SPINE_CHEST] * glm::vec3(-1.0f, 0.0f, 0.0f);
            glm::vec3 lShoulderTan = pose.boneRotations[BONE_SPINE_CHEST] * glm::vec3(0.0f, 0.0f, 1.0f);
            appendStumpCap(lShoulderPos, lShoulderNorm, lShoulderTan, 0.054f, 0.016f);
        }
        if (dismemberMask & DISMEMBER_R_ARM) {
            glm::vec3 rShoulderPos = pose.bonePositions[BONE_R_UPPER_ARM];
            glm::vec3 rShoulderNorm = pose.boneRotations[BONE_SPINE_CHEST] * glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 rShoulderTan = pose.boneRotations[BONE_SPINE_CHEST] * glm::vec3(0.0f, 0.0f, 1.0f);
            appendStumpCap(rShoulderPos, rShoulderNorm, rShoulderTan, 0.054f, 0.016f);
        }
        if (dismemberMask & DISMEMBER_L_LEG) {
            glm::vec3 lHipPos = pose.bonePositions[BONE_L_THIGH];
            glm::vec3 lHipNorm = pose.boneRotations[BONE_PELVIS] * glm::vec3(0.0f, -1.0f, 0.0f);
            glm::vec3 lHipTan = pose.boneRotations[BONE_PELVIS] * glm::vec3(1.0f, 0.0f, 0.0f);
            appendStumpCap(lHipPos, lHipNorm, lHipTan, 0.068f, 0.020f);
        }
        if (dismemberMask & DISMEMBER_R_LEG) {
            glm::vec3 rHipPos = pose.bonePositions[BONE_R_THIGH];
            glm::vec3 rHipNorm = pose.boneRotations[BONE_PELVIS] * glm::vec3(0.0f, -1.0f, 0.0f);
            glm::vec3 rHipTan = pose.boneRotations[BONE_PELVIS] * glm::vec3(1.0f, 0.0f, 0.0f);
            appendStumpCap(rHipPos, rHipNorm, rHipTan, 0.068f, 0.020f);
        }
    }
}

} // namespace Mercenary
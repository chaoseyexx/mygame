#include "ProceduralMesh.hpp"
#include "game/World.hpp"
#include "SkinnedSoldier.hpp"
#include <cmath>
#include <algorithm>
#include <random>

namespace Mercenary {

void ProceduralMesh::appendBox(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                               const glm::vec3& center, const glm::vec3& size, const glm::vec4& color,
                               const glm::vec3& rotation) {
    uint32_t startIndex = static_cast<uint32_t>(vertices.size());
    glm::vec3 h = size * 0.5f;

    glm::vec3 localVerts[24] = {
        // Front (+Z)
        {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z}, { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z},
        // Back (-Z)
        { h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z}, {-h.x,  h.y, -h.z}, { h.x,  h.y, -h.z},
        // Top (+Y)
        {-h.x,  h.y,  h.z}, { h.x,  h.y,  h.z}, { h.x,  h.y, -h.z}, {-h.x,  h.y, -h.z},
        // Bottom (-Y)
        {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z}, { h.x, -h.y,  h.z}, {-h.x, -h.y,  h.z},
        // Right (+X)
        { h.x, -h.y,  h.z}, { h.x, -h.y, -h.z}, { h.x,  h.y, -h.z}, { h.x,  h.y,  h.z},
        // Left (-X)
        {-h.x, -h.y, -h.z}, {-h.x, -h.y,  h.z}, {-h.x,  h.y,  h.z}, {-h.x,  h.y, -h.z}
    };

    glm::vec3 normals[6] = {
        {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}
    };

    glm::vec2 faceUVs[6][4] = {
        // Front (+Z): X, Y
        { {0.0f, 0.0f}, {size.x, 0.0f}, {size.x, size.y}, {0.0f, size.y} },
        // Back (-Z): X, Y
        { {size.x, 0.0f}, {0.0f, 0.0f}, {0.0f, size.y}, {size.x, size.y} },
        // Top (+Y): X, Z
        { {0.0f, 0.0f}, {size.x, 0.0f}, {size.x, size.z}, {0.0f, size.z} },
        // Bottom (-Y): X, Z
        { {0.0f, 0.0f}, {size.x, 0.0f}, {size.x, size.z}, {0.0f, size.z} },
        // Right (+X): Z, Y
        { {0.0f, 0.0f}, {size.z, 0.0f}, {size.z, size.y}, {0.0f, size.y} },
        // Left (-X): Z, Y
        { {size.z, 0.0f}, {0.0f, 0.0f}, {0.0f, size.y}, {size.z, size.y} }
    };

    bool hasRot = (std::abs(rotation.x) > 0.001f || std::abs(rotation.y) > 0.001f || std::abs(rotation.z) > 0.001f);
    glm::mat4 rotMat(1.0f);
    if (hasRot) {
        if (std::abs(rotation.x) > 0.001f) rotMat = glm::rotate(rotMat, rotation.x, glm::vec3(1, 0, 0));
        if (std::abs(rotation.y) > 0.001f) rotMat = glm::rotate(rotMat, rotation.y, glm::vec3(0, 1, 0));
        if (std::abs(rotation.z) > 0.001f) rotMat = glm::rotate(rotMat, rotation.z, glm::vec3(0, 0, 1));
    }

    for (int face = 0; face < 6; ++face) {
        glm::vec3 n = hasRot ? glm::vec3(rotMat * glm::vec4(normals[face], 0.0f)) : normals[face];
        for (int v = 0; v < 4; ++v) {
            int idx = face * 4 + v;
            glm::vec3 pos = hasRot ? (center + glm::vec3(rotMat * glm::vec4(localVerts[idx], 1.0f))) : (center + localVerts[idx]);
            vertices.push_back({ pos, n, faceUVs[face][v], color });
        }

        uint32_t offset = startIndex + face * 4;
        indices.push_back(offset + 0);
        indices.push_back(offset + 1);
        indices.push_back(offset + 2);
        indices.push_back(offset + 2);
        indices.push_back(offset + 3);
        indices.push_back(offset + 0);
    }
}

void ProceduralMesh::appendCylinder(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                   const glm::vec3& center, float radius, float height, const glm::vec4& color,
                                   int segments, float matType, bool capTop, bool capBottom) {
    if (segments < 3) segments = 3;
    uint32_t startIndex = static_cast<uint32_t>(vertices.size());
    float halfH = height * 0.5f;
    float step = 6.2831853f / (float)segments;

    for (int i = 0; i <= segments; ++i) {
        float angle = i * step;
        float x = std::cos(angle) * radius;
        float z = std::sin(angle) * radius;
        glm::vec3 norm(std::cos(angle), 0.0f, std::sin(angle));

        vertices.push_back({ center + glm::vec3(x, -halfH, z), norm, {matType, 0}, color });
        vertices.push_back({ center + glm::vec3(x,  halfH, z), norm, {matType, 1}, color });
    }

    // Standard Counter-Clockwise (CCW) outward-facing triangle winding
    for (int i = 0; i < segments; ++i) {
        uint32_t idx = startIndex + i * 2;
        indices.push_back(idx);
        indices.push_back(idx + 2);
        indices.push_back(idx + 1);

        indices.push_back(idx + 1);
        indices.push_back(idx + 2);
        indices.push_back(idx + 3);
    }

    if (capTop) {
        uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size());
        glm::vec3 topCenter = center + glm::vec3(0, halfH, 0);
        glm::vec3 topNorm(0, 1, 0);
        vertices.push_back({ topCenter, topNorm, {matType, 0.5f}, color });
        for (int i = 0; i <= segments; ++i) {
            float angle = i * step;
            float x = std::cos(angle) * radius;
            float z = std::sin(angle) * radius;
            vertices.push_back({ topCenter + glm::vec3(x, 0, z), topNorm, {matType, 0.5f}, color });
        }
        for (int i = 0; i < segments; ++i) {
            indices.push_back(topCenterIdx);
            indices.push_back(topCenterIdx + 1 + i);
            indices.push_back(topCenterIdx + 1 + i + 1);
        }
    }

    if (capBottom) {
        uint32_t btmCenterIdx = static_cast<uint32_t>(vertices.size());
        glm::vec3 btmCenter = center + glm::vec3(0, -halfH, 0);
        glm::vec3 btmNorm(0, -1, 0);
        vertices.push_back({ btmCenter, btmNorm, {matType, 0.5f}, color });
        for (int i = 0; i <= segments; ++i) {
            float angle = i * step;
            float x = std::cos(angle) * radius;
            float z = std::sin(angle) * radius;
            vertices.push_back({ btmCenter + glm::vec3(x, 0, z), btmNorm, {matType, 0.5f}, color });
        }
        for (int i = 0; i < segments; ++i) {
            indices.push_back(btmCenterIdx);
            indices.push_back(btmCenterIdx + 1 + i + 1);
            indices.push_back(btmCenterIdx + 1 + i);
        }
    }
}

void ProceduralMesh::appendPyramid(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                  const glm::vec3& center, float baseWidth, float height, const glm::vec4& color, float matType) {
    uint32_t startIdx = static_cast<uint32_t>(vertices.size());
    float hw = baseWidth * 0.5f;
    glm::vec3 apex = center + glm::vec3(0, height, 0);

    glm::vec3 b0 = center + glm::vec3(-hw, 0, -hw);
    glm::vec3 b1 = center + glm::vec3( hw, 0, -hw);
    glm::vec3 b2 = center + glm::vec3( hw, 0,  hw);
    glm::vec3 b3 = center + glm::vec3(-hw, 0,  hw);

    auto addTri = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2) {
        glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        uint32_t idx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({ p0, n, {matType, 0}, color });
        vertices.push_back({ p1, n, {matType, 1}, color });
        vertices.push_back({ p2, n, {matType, 0.5f}, color });
        indices.push_back(idx);
        indices.push_back(idx + 1);
        indices.push_back(idx + 2);
    };

    // 4 Triangular Slopes
    addTri(b0, b1, apex);
    addTri(b1, b2, apex);
    addTri(b2, b3, apex);
    addTri(b3, b0, apex);

    // Bottom Base
    glm::vec3 downNorm(0, -1, 0);
    uint32_t baseStart = static_cast<uint32_t>(vertices.size());
    vertices.push_back({ b0, downNorm, {matType, 0}, color });
    vertices.push_back({ b1, downNorm, {matType, 1}, color });
    vertices.push_back({ b2, downNorm, {matType, 1}, color });
    vertices.push_back({ b3, downNorm, {matType, 0}, color });
    indices.push_back(baseStart + 0);
    indices.push_back(baseStart + 2);
    indices.push_back(baseStart + 1);
    indices.push_back(baseStart + 0);
    indices.push_back(baseStart + 3);
    indices.push_back(baseStart + 2);
}

// =========================================================================
// ANATOMICAL METAHUMAN-GRADE PROCEDURAL GEOMETRY BUILDERS
// =========================================================================

void ProceduralMesh::appendSmoothEllipsoid(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                          const glm::vec3& center, const glm::vec3& radii, const glm::vec4& color,
                                          float matType, int segments, int rings) {
    if (segments < 3) segments = 3;
    if (rings < 2) rings = 2;
    glm::vec3 safeRadii = glm::max(radii, glm::vec3(0.0001f));
    uint32_t startIdx = static_cast<uint32_t>(vertices.size());
    const float PI = 3.14159265f;

    for (int r = 0; r <= rings; ++r) {
        float phi = PI * (float)r / (float)rings;
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int s = 0; s <= segments; ++s) {
            float theta = 2.0f * PI * (float)s / (float)segments;
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            glm::vec3 unitDir(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
            glm::vec3 pos = center + unitDir * safeRadii;
            glm::vec3 unnorm = unitDir / safeRadii;
            float len = glm::length(unnorm);
            glm::vec3 norm = (len > 0.0001f) ? (unnorm / len) : glm::vec3(0, 1, 0);

            vertices.push_back({ pos, norm, { matType, 0.0f }, color });
        }
    }

    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            uint32_t cur = startIdx + r * (segments + 1) + s;
            uint32_t nxt = cur + segments + 1;

            indices.push_back(cur);
            indices.push_back(nxt);
            indices.push_back(cur + 1);

            indices.push_back(cur + 1);
            indices.push_back(nxt);
            indices.push_back(nxt + 1);
        }
    }
}

void ProceduralMesh::appendSmoothCapsule(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                        const glm::vec3& start, const glm::vec3& end, float radiusStart, float radiusEnd,
                                        const glm::vec4& color, float matType, int segments) {
    uint32_t startIdx = static_cast<uint32_t>(vertices.size());
    const float PI = 3.14159265f;

    glm::vec3 axis = end - start;
    float len = glm::length(axis);
    glm::vec3 dir = (len > 0.0001f) ? glm::normalize(axis) : glm::vec3(0, 1, 0);

    glm::vec3 up = (std::abs(dir.y) < 0.99f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 side = glm::normalize(glm::cross(dir, up));
    glm::vec3 fwd = glm::normalize(glm::cross(side, dir));

    int heightRings = 3;
    for (int h = 0; h <= heightRings; ++h) {
        float t = (float)h / (float)heightRings;
        glm::vec3 center = glm::mix(start, end, t);
        float radius = glm::mix(radiusStart, radiusEnd, t);

        for (int s = 0; s <= segments; ++s) {
            float angle = 2.0f * PI * (float)s / (float)segments;
            float c = std::cos(angle);
            float sn = std::sin(angle);
            glm::vec3 radial = side * c + fwd * sn;
            glm::vec3 pos = center + radial * radius;
            glm::vec3 norm = glm::normalize(radial);

            vertices.push_back({ pos, norm, { matType, t }, color });
        }
    }

    for (int h = 0; h < heightRings; ++h) {
        for (int s = 0; s < segments; ++s) {
            uint32_t cur = startIdx + h * (segments + 1) + s;
            uint32_t nxt = cur + segments + 1;

            indices.push_back(cur);
            indices.push_back(nxt);
            indices.push_back(cur + 1);

            indices.push_back(cur + 1);
            indices.push_back(nxt);
            indices.push_back(nxt + 1);
        }
    }

    appendSmoothEllipsoid(vertices, indices, start, glm::vec3(radiusStart), color, matType, segments, 4);
    appendSmoothEllipsoid(vertices, indices, end, glm::vec3(radiusEnd), color, matType, segments, 4);
}

void ProceduralMesh::appendAnatomicalHead(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                         const glm::vec3& headCenter, const glm::vec4& skinCol,
                                         int faceId, int hairId, const glm::vec4& hairCol, int beardId) {
    // 1. Cranium & Skull: Smooth anatomical ellipsoid with MetaHuman PBR Skin material tag (1.0f)
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0, 0.015f, -0.015f), glm::vec3(0.095f, 0.112f, 0.105f), skinCol, 1.0f, 24, 16);

    // 2. Tapered Mandible (Jawline) and Chin
    appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.068f, -0.025f, -0.01f), headCenter + glm::vec3(0.0f, -0.088f, 0.054f), 0.036f, 0.028f, skinCol, 1.0f, 14);
    appendSmoothCapsule(vertices, indices, headCenter + glm::vec3( 0.068f, -0.025f, -0.01f), headCenter + glm::vec3(0.0f, -0.088f, 0.054f), 0.036f, 0.028f, skinCol, 1.0f, 14);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, -0.088f, 0.056f), glm::vec3(0.032f, 0.026f, 0.032f), skinCol, 1.0f, 14, 8);

    // 3. Cheekbones (Zygomatic prominence)
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(-0.064f, -0.012f, 0.038f), glm::vec3(0.024f, 0.022f, 0.026f), skinCol, 1.0f, 12, 8);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3( 0.064f, -0.012f, 0.038f), glm::vec3(0.024f, 0.022f, 0.026f), skinCol, 1.0f, 12, 8);

    // 4. Sculpted Nose Bridge & Tip
    appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(0.0f, 0.032f, 0.095f), headCenter + glm::vec3(0.0f, -0.026f, 0.118f), 0.015f, 0.018f, skinCol * 0.98f, 1.0f, 10);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, -0.028f, 0.122f), glm::vec3(0.015f, 0.013f, 0.014f), skinCol * 0.96f, 1.0f, 10, 8);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(-0.016f, -0.032f, 0.108f), glm::vec3(0.010f, 0.008f, 0.010f), skinCol * 0.94f, 1.0f, 8, 6);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3( 0.016f, -0.032f, 0.108f), glm::vec3(0.010f, 0.008f, 0.010f), skinCol * 0.94f, 1.0f, 8, 6);

    // 5. Orbital Sockets, Eye Whites and Irises
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(-0.036f, 0.012f, 0.090f), glm::vec3(0.012f, 0.008f, 0.008f), glm::vec4(0.92f, 0.92f, 0.94f, 1.0f), 1.0f, 8, 6);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(-0.036f, 0.012f, 0.096f), glm::vec3(0.005f, 0.005f, 0.003f), glm::vec4(0.12f, 0.14f, 0.16f, 1.0f), 1.0f, 8, 6);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3( 0.036f, 0.012f, 0.090f), glm::vec3(0.012f, 0.008f, 0.008f), glm::vec4(0.92f, 0.92f, 0.94f, 1.0f), 1.0f, 8, 6);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3( 0.036f, 0.012f, 0.096f), glm::vec3(0.005f, 0.005f, 0.003f), glm::vec4(0.12f, 0.14f, 0.16f, 1.0f), 1.0f, 8, 6);
    appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.052f, 0.028f, 0.088f), headCenter + glm::vec3(0.052f, 0.028f, 0.088f), 0.012f, 0.012f, skinCol * 0.95f, 1.0f, 8);

    // 6. Sculpted Lips
    glm::vec4 lipCol = glm::mix(skinCol, glm::vec4(0.72f, 0.38f, 0.34f, 1.0f), 0.45f);
    appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.022f, -0.054f, 0.094f), headCenter + glm::vec3(0.022f, -0.054f, 0.094f), 0.007f, 0.007f, lipCol, 1.0f, 8);
    appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.020f, -0.066f, 0.090f), headCenter + glm::vec3(0.020f, -0.066f, 0.090f), 0.008f, 0.008f, lipCol, 1.0f, 8);

    // 7. Anatomical Ears
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(-0.096f, -0.008f, -0.015f), glm::vec3(0.012f, 0.026f, 0.016f), skinCol * 0.96f, 1.0f, 8, 6);
    appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3( 0.096f, -0.008f, -0.015f), glm::vec3(0.012f, 0.026f, 0.016f), skinCol * 0.96f, 1.0f, 8, 6);

    // 8. Volumetric Styled Hair (Sculpted over skull crown)
    if (hairId == 0) {
        // High & Tight Buzzcut
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.034f, -0.008f), glm::vec3(0.100f, 0.120f, 0.110f), hairCol, 0.0f, 20, 12);
    } else if (hairId == 1) {
        // Operator Modern Undercut / Pompadour
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.042f, 0.005f), glm::vec3(0.098f, 0.124f, 0.114f), hairCol, 0.0f, 20, 12);
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.048f, 0.048f), glm::vec3(0.072f, 0.036f, 0.042f), hairCol * 1.05f, 0.0f, 12, 8);
    } else if (hairId == 2) {
        // Military Crew Cut
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.036f, -0.006f), glm::vec3(0.101f, 0.122f, 0.112f), hairCol, 0.0f, 20, 12);
    }

    // 9. Sculpted Facial Hair
    if (beardId == 0) {
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.068f, -0.025f, -0.01f), headCenter + glm::vec3(0.0f, -0.092f, 0.058f), 0.042f, 0.038f, hairCol, 0.0f, 10);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3( 0.068f, -0.025f, -0.01f), headCenter + glm::vec3(0.0f, -0.092f, 0.058f), 0.042f, 0.038f, hairCol, 0.0f, 10);
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, -0.095f, 0.062f), glm::vec3(0.036f, 0.032f, 0.038f), hairCol, 0.0f, 10, 8);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.024f, -0.045f, 0.098f), headCenter + glm::vec3(0.024f, -0.045f, 0.098f), 0.010f, 0.010f, hairCol, 0.0f, 8);
    } else if (beardId == 1) {
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, -0.082f, 0.050f), glm::vec3(0.042f, 0.034f, 0.040f), glm::mix(skinCol, hairCol, 0.40f), 1.0f, 10, 8);
    } else if (beardId == 2) {
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, -0.090f, 0.058f), glm::vec3(0.028f, 0.028f, 0.032f), hairCol, 0.0f, 10, 8);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.020f, -0.046f, 0.096f), headCenter + glm::vec3(0.020f, -0.046f, 0.096f), 0.009f, 0.009f, hairCol, 0.0f, 8);
    }
}

void ProceduralMesh::appendModernTacticalHelmet(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                               const glm::vec3& headCenter, const glm::vec4& camoCol, int helmetId) {
    if (helmetId >= 4 || helmetId < 0) {
        // None: Full facial identity and hairstyle completely uncovered
        return;
    }

    const glm::vec4 darkPolymer(0.12f, 0.12f, 0.12f, 1.0f);
    const glm::vec4 metalBracket(0.24f, 0.24f, 0.25f, 1.0f);
    const glm::vec4 balaclavaCol(0.16f, 0.16f, 0.17f, 1.0f);
    const glm::vec4 goggleFrame(0.10f, 0.10f, 0.10f, 1.0f);
    const glm::vec4 goggleLens(0.18f, 0.22f, 0.26f, 0.95f); // Smoked polycarbonate glass (matType 4.0f)

    // 0. High-detail Tactical Balaclava (worn beneath helmets)
    if (helmetId != 2) {
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, -0.015f, 0.005f), glm::vec3(0.098f, 0.114f, 0.108f), balaclavaCol, 0.0f, 14, 10);
    }

    if (helmetId == 0) {
        // FAST MT High-Cut (Ukrainian Mercenary / Modern NATO Operator):
        // 1. Ballistic curved helmet shell
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.044f, -0.006f), glm::vec3(0.116f, 0.110f, 0.126f), camoCol, 3.0f, 16, 12);
        // Helmet rim lip
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.092f, 0.026f, 0.082f), headCenter + glm::vec3(0.092f, 0.026f, 0.082f), 0.014f, 0.014f, camoCol, 3.0f, 8);

        // 2. Wilcox L4 G24 NVG breakaway shroud on front brow
        appendBox(vertices, indices, headCenter + glm::vec3(0.0f, 0.052f, 0.122f), glm::vec3(0.042f, 0.046f, 0.018f), darkPolymer);
        appendBox(vertices, indices, headCenter + glm::vec3(0.0f, 0.052f, 0.131f), glm::vec3(0.024f, 0.028f, 0.008f), metalBracket);

        // 3. Left & Right ARC accessory rails
        appendBox(vertices, indices, headCenter + glm::vec3(-0.118f, 0.038f, 0.01f), glm::vec3(0.012f, 0.024f, 0.115f), darkPolymer);
        appendBox(vertices, indices, headCenter + glm::vec3( 0.118f, 0.038f, 0.01f), glm::vec3(0.012f, 0.024f, 0.115f), darkPolymer);

        // 4. Peltor ComTac tactical comms headset with headband & boom mic
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(-0.112f, -0.012f, -0.016f), glm::vec3(0.022f, 0.040f, 0.028f), darkPolymer, 3.0f, 8, 6);
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3( 0.112f, -0.012f, -0.016f), glm::vec3(0.022f, 0.040f, 0.028f), darkPolymer, 3.0f, 8, 6);
        // Flexible gooseneck boom mic curving to mouth
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.110f, -0.025f, 0.01f), headCenter + glm::vec3(-0.042f, -0.058f, 0.094f), 0.007f, 0.009f, darkPolymer, 2.0f, 6);
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(-0.038f, -0.058f, 0.098f), glm::vec3(0.010f, 0.008f, 0.012f), darkPolymer, 0.0f, 6, 4);

        // 5. Ballistic Eye Protection Goggles (ESS NVG Profile)
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.070f, 0.016f, 0.090f), headCenter + glm::vec3(0.070f, 0.016f, 0.090f), 0.018f, 0.018f, goggleFrame, 3.0f, 8);
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.016f, 0.106f), glm::vec3(0.064f, 0.018f, 0.014f), goggleLens, 4.0f, 10, 6);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.082f, 0.016f, 0.06f), headCenter + glm::vec3(-0.118f, 0.016f, -0.04f), 0.012f, 0.012f, goggleFrame, 0.0f, 6);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3( 0.082f, 0.016f, 0.06f), headCenter + glm::vec3( 0.118f, 0.016f, -0.04f), 0.012f, 0.012f, goggleFrame, 0.0f, 6);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.118f, 0.016f, -0.04f), headCenter + glm::vec3(0.118f, 0.016f, -0.04f), 0.012f, 0.012f, goggleFrame, 0.0f, 8);

        // 6. Top velcro & Hel-Star IR strobe beacon
        appendBox(vertices, indices, headCenter + glm::vec3(0.0f, 0.148f, -0.01f), glm::vec3(0.038f, 0.016f, 0.055f), darkPolymer);
        appendBox(vertices, indices, headCenter + glm::vec3(0.0f, 0.036f, -0.130f), glm::vec3(0.076f, 0.048f, 0.024f), darkPolymer);

        // 7. Mercenary Blue IFF flag / velcro tape
        glm::vec4 uaBlue(0.15f, 0.55f, 0.95f, 1.0f);
        appendBox(vertices, indices, headCenter + glm::vec3(0.0f, 0.082f, 0.102f), glm::vec3(0.055f, 0.020f, 0.010f), uaBlue);
    }
    else if (helmetId == 1) {
        // 6B47 Ratnik Helmet (Russian Crimson / Ratnik Infantry):
        // 1. Heavy Russian rounded ballistic dome
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.046f, -0.008f), glm::vec3(0.120f, 0.116f, 0.130f), camoCol, 3.0f, 16, 12);
        // Protective ear flares
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(-0.120f, -0.008f, -0.012f), glm::vec3(0.020f, 0.054f, 0.078f), camoCol, 3.0f, 8, 6);
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3( 0.120f, -0.008f, -0.012f), glm::vec3(0.020f, 0.054f, 0.078f), camoCol, 3.0f, 8, 6);

        // 2. Russian front NVG mount plate & side rail
        appendBox(vertices, indices, headCenter + glm::vec3(0.0f, 0.048f, 0.126f), glm::vec3(0.046f, 0.048f, 0.020f), darkPolymer);
        appendBox(vertices, indices, headCenter + glm::vec3(0.0f, 0.048f, 0.136f), glm::vec3(0.026f, 0.028f, 0.008f), metalBracket);

        // 3. St. George / Crimson Red IFF Identification band wrapping around helmet
        glm::vec4 ruRed(0.92f, 0.20f, 0.20f, 1.0f);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.122f, 0.022f, 0.0f), headCenter + glm::vec3(0.122f, 0.022f, 0.0f), 0.014f, 0.014f, ruRed, 0.0f, 10);
        appendBox(vertices, indices, headCenter + glm::vec3(0.0f, 0.022f, 0.128f), glm::vec3(0.080f, 0.024f, 0.008f), ruRed);

        // 4. Russian 6B51 Ballistic Goggles
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.068f, 0.014f, 0.092f), headCenter + glm::vec3(0.068f, 0.014f, 0.092f), 0.018f, 0.018f, darkPolymer, 3.0f, 8);
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.014f, 0.108f), glm::vec3(0.062f, 0.018f, 0.014f), goggleLens, 4.0f, 10, 6);
    }
    else if (helmetId == 2) {
        // Tactical Boonie Hat
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.046f, -0.005f), glm::vec3(0.108f, 0.072f, 0.115f), camoCol, 0.0f, 14, 8);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.145f, 0.010f, 0.0f), headCenter + glm::vec3(0.145f, 0.010f, 0.0f), 0.038f, 0.038f, camoCol, 0.0f, 10);
        appendSmoothCapsule(vertices, indices, headCenter + glm::vec3(-0.112f, 0.028f, 0.0f), headCenter + glm::vec3(0.112f, 0.028f, 0.0f), 0.008f, 0.008f, darkPolymer, 0.0f, 8);
    }
    else if (helmetId == 3) {
        // Balaclava Only
        appendSmoothEllipsoid(vertices, indices, headCenter + glm::vec3(0.0f, 0.020f, -0.010f), glm::vec3(0.102f, 0.122f, 0.112f), balaclavaCol, 0.0f, 14, 10);
    }
}

void ProceduralMesh::appendModernPlateCarrier(std::vector<Vertex>& vertices, std::vector<uint32_t>& indices,
                                             const glm::vec3& torsoCenter, const glm::vec4& vestCol, int vestId) {
    if (vestId >= 3 || vestId < 0) {
        // None: Default combat uniform shirt only! No extra armor vest
        return;
    }

    const glm::vec4 darkPolymer(0.12f, 0.12f, 0.12f, 1.0f);
    const glm::vec4 magMetal(0.16f, 0.16f, 0.18f, 1.0f);
    const glm::vec4 brassBullet(0.85f, 0.70f, 0.25f, 1.0f);
    const glm::vec4 catOrange(0.95f, 0.42f, 0.12f, 1.0f);
    const glm::vec4 radioBlack(0.10f, 0.10f, 0.10f, 1.0f);
    glm::vec4 pouchCol = vestCol * 0.90f;

    if (vestId == 0) {
        // Crye Precision JPC 2.0 (Modern NATO / Mercenary PMC):
        // 1. Ergonomic chest & back ballistic plate bags
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(0.0f, -0.08f, 0.032f), torsoCenter + glm::vec3(0.0f, 0.08f, 0.032f), 0.185f, 0.165f, vestCol, 0.0f, 10);
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(0.0f, -0.08f, -0.032f), torsoCenter + glm::vec3(0.0f, 0.08f, -0.032f), 0.185f, 0.165f, vestCol, 0.0f, 10);

        // 2. Skeletal cummerbund side straps
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(-0.16f, -0.04f, 0.0f), torsoCenter + glm::vec3(-0.16f, 0.04f, 0.0f), 0.035f, 0.035f, darkPolymer, 0.0f, 6);
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3( 0.16f, -0.04f, 0.0f), torsoCenter + glm::vec3( 0.16f, 0.04f, 0.0f), 0.035f, 0.035f, darkPolymer, 0.0f, 6);

        // 3. Triple Front Kangaroo Magazine Pouches with 3 visible PMAG magazines
        for (int i = -1; i <= 1; ++i) {
            float x = i * 0.075f;
            appendBox(vertices, indices, torsoCenter + glm::vec3(x, -0.03f, 0.145f), glm::vec3(0.068f, 0.15f, 0.05f), pouchCol);
            appendBox(vertices, indices, torsoCenter + glm::vec3(x, 0.07f, 0.145f), glm::vec3(0.060f, 0.07f, 0.045f), magMetal);
            appendBox(vertices, indices, torsoCenter + glm::vec3(x, 0.108f, 0.145f), glm::vec3(0.030f, 0.012f, 0.025f), brassBullet);
            appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(x, 0.11f, 0.145f), torsoCenter + glm::vec3(x, 0.13f, 0.145f), 0.006f, 0.006f, darkPolymer, 0.0f, 4);
        }

        // 4. CAT (Combat Application Tourniquet) on Left Shoulder Strap
        appendBox(vertices, indices, torsoCenter + glm::vec3(-0.11f, 0.14f, 0.09f), glm::vec3(0.042f, 0.11f, 0.030f), darkPolymer);
        appendBox(vertices, indices, torsoCenter + glm::vec3(-0.11f, 0.18f, 0.095f), glm::vec3(0.028f, 0.022f, 0.022f), catOrange);

        // 5. Harris PRC-152 Tactical Radio with flexible whip antenna on Left Chest
        appendBox(vertices, indices, torsoCenter + glm::vec3(-0.16f, 0.02f, 0.09f), glm::vec3(0.052f, 0.14f, 0.052f), radioBlack);
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(-0.16f, 0.09f, 0.09f), torsoCenter + glm::vec3(-0.15f, 0.38f, 0.05f), 0.006f, 0.004f, radioBlack, 2.0f, 4);

        // 6. Chest PTT button and MOLLE webbing rows
        appendBox(vertices, indices, torsoCenter + glm::vec3(-0.05f, 0.09f, 0.135f), glm::vec3(0.030f, 0.035f, 0.015f), darkPolymer);
        for (int r = 0; r < 3; ++r) {
            float y = 0.04f + r * 0.032f;
            appendBox(vertices, indices, torsoCenter + glm::vec3(0.04f, y, 0.132f), glm::vec3(0.12f, 0.012f, 0.008f), darkPolymer);
        }

        // 7. Padded shoulder straps
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(-0.11f, 0.08f, 0.08f), torsoCenter + glm::vec3(-0.11f, 0.16f, -0.08f), 0.026f, 0.026f, vestCol, 0.0f, 6);
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3( 0.11f, 0.08f, 0.08f), torsoCenter + glm::vec3( 0.11f, 0.16f, -0.08f), 0.026f, 0.026f, vestCol, 0.0f, 6);
    }
    else if (vestId == 1) {
        // 6B45 Heavy Ratnik Assault Armor (Russian Infantry):
        // 1. Heavy ballistic torso shell
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(0.0f, -0.09f, 0.022f), torsoCenter + glm::vec3(0.0f, 0.09f, 0.022f), 0.205f, 0.185f, vestCol, 0.0f, 10);
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(0.0f, -0.09f, -0.022f), torsoCenter + glm::vec3(0.0f, 0.09f, -0.022f), 0.205f, 0.185f, vestCol, 0.0f, 10);

        // 2. High ballistic neck collar / throat shield
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(-0.09f, 0.165f, 0.0f), torsoCenter + glm::vec3(0.09f, 0.165f, 0.0f), 0.042f, 0.042f, vestCol, 0.0f, 8);

        // 3. Removable Groin Protection Armor Pad
        appendBox(vertices, indices, torsoCenter + glm::vec3(0.0f, -0.24f, 0.085f), glm::vec3(0.25f, 0.20f, 0.045f), vestCol);

        // 4. Quad AK-74 Magazine Pouches with visible ribbed bakelite magazines
        glm::vec4 bakeliteMag(0.32f, 0.18f, 0.12f, 1.0f);
        for (int i = 0; i < 4; ++i) {
            float x = -0.12f + i * 0.08f;
            appendBox(vertices, indices, torsoCenter + glm::vec3(x, -0.04f, 0.155f), glm::vec3(0.068f, 0.15f, 0.055f), pouchCol);
            appendBox(vertices, indices, torsoCenter + glm::vec3(x, 0.06f, 0.155f), glm::vec3(0.060f, 0.07f, 0.048f), bakeliteMag);
            appendBox(vertices, indices, torsoCenter + glm::vec3(x, 0.098f, 0.155f), glm::vec3(0.028f, 0.012f, 0.022f), brassBullet);
        }

        // 5. Dual RGD-5 / F-1 grenade pouches on flanks
        appendBox(vertices, indices, torsoCenter + glm::vec3(-0.19f, -0.06f, 0.06f), glm::vec3(0.055f, 0.085f, 0.055f), pouchCol);
        appendBox(vertices, indices, torsoCenter + glm::vec3( 0.19f, -0.06f, 0.06f), glm::vec3(0.055f, 0.085f, 0.055f), pouchCol);

        // 6. Ratnik radio pouch & antenna
        appendBox(vertices, indices, torsoCenter + glm::vec3(-0.17f, 0.05f, 0.07f), glm::vec3(0.050f, 0.12f, 0.050f), radioBlack);
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(-0.17f, 0.11f, 0.07f), torsoCenter + glm::vec3(-0.16f, 0.40f, 0.03f), 0.006f, 0.004f, radioBlack, 2.0f, 4);

        // 7. Ballistic shoulder pauldrons
        appendSmoothEllipsoid(vertices, indices, torsoCenter + glm::vec3(-0.21f, 0.12f, 0.0f), glm::vec3(0.065f, 0.055f, 0.075f), vestCol, 0.0f, 8, 6);
        appendSmoothEllipsoid(vertices, indices, torsoCenter + glm::vec3( 0.21f, 0.12f, 0.0f), glm::vec3(0.065f, 0.055f, 0.075f), vestCol, 0.0f, 8, 6);
    }
    else if (vestId == 2) {
        // D3CRX Chest Rig:
        appendBox(vertices, indices, torsoCenter + glm::vec3(0.0f, -0.02f, 0.125f), glm::vec3(0.33f, 0.18f, 0.06f), vestCol);
        for (int i = 0; i < 4; ++i) {
            float x = -0.11f + i * 0.072f;
            appendBox(vertices, indices, torsoCenter + glm::vec3(x, 0.06f, 0.135f), glm::vec3(0.055f, 0.06f, 0.045f), magMetal);
        }
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3(-0.12f, 0.05f, 0.08f), torsoCenter + glm::vec3(-0.12f, 0.14f, -0.08f), 0.016f, 0.016f, darkPolymer, 0.0f, 6);
        appendSmoothCapsule(vertices, indices, torsoCenter + glm::vec3( 0.12f, 0.05f, 0.08f), torsoCenter + glm::vec3( 0.12f, 0.14f, -0.08f), 0.016f, 0.016f, darkPolymer, 0.0f, 6);
    }
}

std::shared_ptr<Mesh> ProceduralMesh::createDetailedSoldier(bool isTeamA) {
    CharacterCustomization c;
    if (isTeamA) {
        c.camoId = 1;     // Ukrainian MM-14
        c.armbandId = 0;  // Blue
        c.helmetId = 0;   // FAST MT
        c.vestId = 0;     // JPC 2.0
    } else {
        c.camoId = 2;     // Russian EMR
        c.armbandId = 1;  // Red
        c.helmetId = 1;   // 6B47
        c.vestId = 1;     // 6B45
    }
    return createCustomSoldier(c);
}

// =========================================================================
// RUSSIAN AI INFANTRY (FACTION RU - CONTEMPORARY EMR/RATNIK EQUIPMENT)
// =========================================================================
std::shared_ptr<Mesh> ProceduralMesh::createRussianInfantry(CombatRole role, int lod) {
    CharacterCustomization c;
    c.camoId = 2;        // Russian EMR Digital Flora
    c.armbandId = 1;     // Crimson Red IFF
    c.helmetId = 1;      // 6B47 Ratnik Helmet with Ear Flares
    c.vestId = 1;        // 6B45 Heavy Ratnik Assault Armor with Groin Flap
    c.gloveId = 2;       // Olive Drab
    c.skinToneId = 1;    // Slavic Tan

    if (role == CombatRole::AutomaticRifleman) c.backpackId = 0;
    else if (role == CombatRole::Medic) c.backpackId = 1;
    else if (role == CombatRole::SquadLeader) c.backpackId = 0;
    else c.backpackId = 2;

    // Both near & mid range render full-fidelity procedural operators
    if (lod <= 1) {
        return createCustomSoldier(c);
    }

    // LOD 2 (> 160m): Clean anatomical silhouette with Ratnik helmet, armor & rifle
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const glm::vec4 emrCamo(0.24f, 0.30f, 0.20f, 1.0f);
    const glm::vec4 ratnikVest(0.28f, 0.35f, 0.22f, 1.0f);
    const glm::vec4 ratnikHelmet(0.26f, 0.32f, 0.22f, 1.0f);
    const glm::vec4 armTape(0.92f, 0.20f, 0.20f, 1.0f);
    const glm::vec4 gunColor(0.12f, 0.12f, 0.12f, 1.0f);

    appendSmoothCapsule(vertices, indices, {-0.14f, 0.70f, 0}, {-0.14f, 0.08f, 0}, 0.08f, 0.06f, emrCamo, 0.0f, 8);
    appendSmoothCapsule(vertices, indices, { 0.14f, 0.70f, 0}, { 0.14f, 0.08f, 0}, 0.08f, 0.06f, emrCamo, 0.0f, 8);
    appendSmoothCapsule(vertices, indices, {0, 0.75f, 0}, {0, 1.20f, 0}, 0.19f, 0.21f, ratnikVest, 0.0f, 8);
    appendSmoothCapsule(vertices, indices, {-0.24f, 1.12f, 0}, {-0.12f, 0.95f, 0.24f}, 0.055f, 0.048f, emrCamo, 0.0f, 6);
    appendSmoothCapsule(vertices, indices, { 0.24f, 1.12f, 0}, { 0.12f, 0.95f, 0.20f}, 0.055f, 0.048f, emrCamo, 0.0f, 6);
    appendBox(vertices, indices, {0.0f, 0.95f, 0.30f}, {0.06f, 0.10f, 0.65f}, gunColor);
    appendSmoothCapsule(vertices, indices, {-0.25f, 1.06f, 0}, {-0.25f, 0.98f, 0}, 0.060f, 0.058f, armTape, 0.0f, 6);
    appendSmoothCapsule(vertices, indices, { 0.25f, 1.06f, 0}, { 0.25f, 0.98f, 0}, 0.060f, 0.058f, armTape, 0.0f, 6);
    appendSmoothEllipsoid(vertices, indices, {0, 1.34f, 0}, glm::vec3(0.095f, 0.11f, 0.10f), glm::vec4(0.75f, 0.60f, 0.48f, 1.0f), 1.0f, 8, 6);
    appendSmoothEllipsoid(vertices, indices, {0, 1.42f, 0}, glm::vec3(0.12f, 0.12f, 0.13f), ratnikHelmet, 3.0f, 10, 8);

    return std::make_shared<Mesh>(vertices, indices);
}






// =========================================================================
// UKRAINIAN AI INFANTRY (FACTION UA - CONTEMPORARY MM-14/MULTICAM & FAST)
// =========================================================================
std::shared_ptr<Mesh> ProceduralMesh::createUkrainianInfantry(CombatRole role, int lod) {
    CharacterCustomization c;
    c.camoId = 1;        // Ukrainian MM-14 Digital Pixel
    c.armbandId = 0;     // Mercenary Blue
    c.helmetId = 0;      // FAST MT High-Cut Helmet
    c.vestId = 0;        // Crye JPC 2.0 Plate Carrier
    c.gloveId = 0;       // Coyote Tan Mechanix
    c.skinToneId = 1;    // Slavic Tan

    if (role == CombatRole::AutomaticRifleman) c.backpackId = 0;
    else if (role == CombatRole::Medic) c.backpackId = 1;
    else if (role == CombatRole::SquadLeader) c.backpackId = 0;
    else c.backpackId = 2; // None

    // Both near & mid range render full-fidelity procedural operators
    if (lod <= 1) {
        return createCustomSoldier(c);
    }

    // LOD 2 (> 160m): Clean anatomical silhouette with FAST helmet, JPC vest & rifle
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    const glm::vec4 mm14Camo(0.38f, 0.36f, 0.28f, 1.0f);
    const glm::vec4 jpcVest(0.42f, 0.40f, 0.32f, 1.0f);
    const glm::vec4 fastHelmet(0.35f, 0.38f, 0.30f, 1.0f);
    const glm::vec4 armTape(0.15f, 0.55f, 0.95f, 1.0f);
    const glm::vec4 gunColor(0.12f, 0.12f, 0.12f, 1.0f);

    appendSmoothCapsule(vertices, indices, {-0.14f, 0.70f, 0}, {-0.14f, 0.08f, 0}, 0.075f, 0.055f, mm14Camo, 0.0f, 8);
    appendSmoothCapsule(vertices, indices, { 0.14f, 0.70f, 0}, { 0.14f, 0.08f, 0}, 0.075f, 0.055f, mm14Camo, 0.0f, 8);
    appendSmoothCapsule(vertices, indices, {0, 0.75f, 0}, {0, 1.20f, 0}, 0.18f, 0.20f, jpcVest, 0.0f, 8);
    appendSmoothCapsule(vertices, indices, {-0.24f, 1.12f, 0}, {-0.12f, 0.95f, 0.24f}, 0.052f, 0.045f, mm14Camo, 0.0f, 6);
    appendSmoothCapsule(vertices, indices, { 0.24f, 1.12f, 0}, { 0.12f, 0.95f, 0.20f}, 0.052f, 0.045f, mm14Camo, 0.0f, 6);
    appendBox(vertices, indices, {0.0f, 0.95f, 0.30f}, {0.06f, 0.10f, 0.65f}, gunColor);
    appendSmoothCapsule(vertices, indices, {-0.25f, 1.06f, 0}, {-0.25f, 0.98f, 0}, 0.058f, 0.056f, armTape, 0.0f, 6);
    appendSmoothCapsule(vertices, indices, { 0.25f, 1.06f, 0}, { 0.25f, 0.98f, 0}, 0.058f, 0.056f, armTape, 0.0f, 6);
    appendSmoothEllipsoid(vertices, indices, {0, 1.34f, 0}, glm::vec3(0.09f, 0.10f, 0.095f), glm::vec4(0.75f, 0.60f, 0.48f, 1.0f), 1.0f, 8, 6);
    appendSmoothEllipsoid(vertices, indices, {0, 1.42f, 0}, glm::vec3(0.115f, 0.11f, 0.122f), fastHelmet, 3.0f, 10, 8);

    return std::make_shared<Mesh>(vertices, indices);
}






// =========================================================================
// PLAYER MERCENARY: MODULAR PROCEDURAL CUSTOMIZER (PERSISTENT APPEARANCE)
// =========================================================================
std::shared_ptr<Mesh> ProceduralMesh::createCustomSoldier(const CharacterCustomization& custom) {
    SkinnedSoldier model;
    SkeletonPose pose;
    for (int b = 0; b < NUM_CHARACTER_BONES; ++b) {
        pose.boneRotations[b] = glm::mat3(1.0f);
        pose.bonePositions[b] = model.getBindJoint(static_cast<CharacterBone>(b));
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    model.deform(pose, custom, vertices, indices);

    // Primary weapon held in operator's hands in static pose
    glm::vec3 rifleCenter(-0.01f, 0.98f, 0.35f);
    glm::vec4 rifleSteel(0.12f, 0.13f, 0.14f, 0.70f);
    glm::vec4 riflePolymer(0.18f, 0.18f, 0.18f, 0.80f);
    glm::vec4 holoGlass(0.95f, 0.25f, 0.25f, 0.40f);

    appendBox(vertices, indices, rifleCenter, {0.05f, 0.09f, 0.34f}, rifleSteel);
    appendBox(vertices, indices, rifleCenter + glm::vec3(0, 0.055f, 0.0f), {0.038f, 0.018f, 0.26f}, rifleSteel);
    appendBox(vertices, indices, rifleCenter + glm::vec3(0, 0.02f, 0.28f), {0.025f, 0.025f, 0.24f}, rifleSteel);
    appendBox(vertices, indices, rifleCenter + glm::vec3(0, 0.02f, 0.41f), {0.035f, 0.035f, 0.05f}, rifleSteel);
    appendBox(vertices, indices, rifleCenter + glm::vec3(0, -0.11f, 0.06f), {0.035f, 0.15f, 0.08f}, riflePolymer, glm::vec3(-0.25f, 0, 0));
    appendBox(vertices, indices, rifleCenter + glm::vec3(0, 0.082f, -0.02f), {0.045f, 0.055f, 0.09f}, rifleSteel);
    appendBox(vertices, indices, rifleCenter + glm::vec3(0, 0.082f, 0.025f), {0.032f, 0.040f, 0.008f}, holoGlass);
    appendBox(vertices, indices, rifleCenter + glm::vec3(0.04f, 0.01f, -0.22f), {0.045f, 0.095f, 0.16f}, riflePolymer);

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createDetailedRifle() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 gunMetal(0.12f, 0.13f, 0.14f, 0.70f);     // Matte black steel (PBR tag 0.70)
    const glm::vec4 furniture(0.20f, 0.22f, 0.18f, 0.80f);    // Tactical olive / dark polymer (PBR tag 0.80)
    const glm::vec4 opticLens(0.95f, 0.25f, 0.25f, 0.40f);    // Red dot glass (PBR tag 0.40)
    const glm::vec4 magColor(0.18f, 0.18f, 0.18f, 0.80f);     // Magpul PMAG (PBR tag 0.80)
    const glm::vec4 peqLaser(0.24f, 0.22f, 0.18f, 0.80f);     // PEQ-15 Tan module (PBR tag 0.80)

    // 1. Receiver & Top Picatinny Rail
    appendBox(vertices, indices, {0.0f, 0.0f, 0.0f}, {0.06f, 0.10f, 0.32f}, gunMetal);
    appendBox(vertices, indices, {0.0f, 0.06f, -0.02f}, {0.04f, 0.02f, 0.26f}, gunMetal);

    // 2. Holographic Reflex Sight (EOTech)
    appendBox(vertices, indices, {0.0f, 0.10f, -0.02f}, {0.05f, 0.06f, 0.10f}, gunMetal);
    appendBox(vertices, indices, {0.0f, 0.10f, 0.03f}, {0.035f, 0.045f, 0.01f}, opticLens);

    // 3. PEQ-15 IR Laser/Illuminator mounted on top rail
    appendBox(vertices, indices, {0.0f, 0.08f, 0.20f}, {0.05f, 0.03f, 0.09f}, peqLaser);

    // 4. Barrel, Gas Block & Muzzle Flash Hider
    appendBox(vertices, indices, {0.0f, 0.02f, 0.32f}, {0.03f, 0.03f, 0.36f}, gunMetal);
    appendBox(vertices, indices, {0.0f, 0.02f, 0.52f}, {0.04f, 0.04f, 0.06f}, gunMetal);

    // 5. Quad Rail Handguard with Vertical Foregrip
    appendBox(vertices, indices, {0.0f, 0.02f, 0.24f}, {0.06f, 0.08f, 0.22f}, furniture);
    appendBox(vertices, indices, {0.0f, -0.07f, 0.28f}, {0.035f, 0.10f, 0.04f}, furniture); // Vertical grip

    // 6. Curved 30-Round PMAG Magazine
    appendBox(vertices, indices, {0.0f, -0.12f, 0.06f}, {0.04f, 0.16f, 0.09f}, magColor, glm::vec3(-0.25f, 0.0f, 0.0f));

    // 7. Pistol Grip & Adjustable Buttstock
    appendBox(vertices, indices, {0.0f, -0.08f, -0.08f}, {0.04f, 0.12f, 0.06f}, furniture, glm::vec3(0.35f, 0.0f, 0.0f));
    appendBox(vertices, indices, {0.0f, 0.01f, -0.26f}, {0.05f, 0.11f, 0.22f}, furniture);
    appendBox(vertices, indices, {0.0f, 0.01f, -0.37f}, {0.055f, 0.12f, 0.02f}, gunMetal);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createM4A1() {
    return createDetailedRifle();
}

std::shared_ptr<Mesh> ProceduralMesh::createAK74M() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 steelMetal(0.13f, 0.14f, 0.15f, 0.70f);    // Stamped military steel (PBR 0.70)
    const glm::vec4 blackPoly(0.11f, 0.11f, 0.12f, 0.80f);     // Black polymer stock & handguard (PBR 0.80)
    const glm::vec4 plumBakelite(0.36f, 0.16f, 0.11f, 0.80f);  // Iconic Russian 5.45x39 plum/bakelite mag
    const glm::vec4 opticGlass(0.20f, 0.85f, 0.35f, 0.40f);    // Kobra green/cyan reticle glass

    // 1. Stamped Steel Receiver & Ribbed Dust Cover
    appendBox(vertices, indices, {0.0f, 0.01f, 0.02f}, {0.055f, 0.09f, 0.30f}, steelMetal);
    appendBox(vertices, indices, {0.0f, 0.065f, 0.01f}, {0.048f, 0.035f, 0.28f}, steelMetal); // Ribbed dust cover

    // 2. Charging Handle & Fire Selector Lever (Right side)
    appendBox(vertices, indices, {0.038f, 0.045f, 0.06f}, {0.025f, 0.015f, 0.02f}, steelMetal);
    appendBox(vertices, indices, {0.032f, 0.01f, -0.04f}, {0.010f, 0.020f, 0.09f}, steelMetal);

    // 3. Side-Mounted Kobra EKP-1S-03 Collimator Sight
    appendBox(vertices, indices, {-0.035f, 0.05f, -0.02f}, {0.018f, 0.08f, 0.07f}, steelMetal); // Dovetail bracket
    appendBox(vertices, indices, {0.0f, 0.105f, -0.02f}, {0.050f, 0.05f, 0.09f}, steelMetal);   // Sight body
    appendBox(vertices, indices, {0.0f, 0.105f, 0.025f}, {0.038f, 0.04f, 0.01f}, opticGlass);  // Optic lens

    // 4. Gas Tube, Lower Handguard & Cleaning Rod
    appendBox(vertices, indices, {0.0f, 0.025f, 0.26f}, {0.056f, 0.075f, 0.20f}, blackPoly);  // Lower handguard
    appendBox(vertices, indices, {0.0f, 0.055f, 0.26f}, {0.042f, 0.035f, 0.18f}, blackPoly);  // Upper handguard
    appendBox(vertices, indices, {0.0f, -0.015f, 0.32f}, {0.010f, 0.010f, 0.32f}, steelMetal); // Cleaning rod

    // 5. Long Barrel, Front Sight Block & Cylindrical AK-74 Muzzle Brake
    appendBox(vertices, indices, {0.0f, 0.032f, 0.44f}, {0.026f, 0.026f, 0.28f}, steelMetal);
    appendBox(vertices, indices, {0.0f, 0.062f, 0.52f}, {0.024f, 0.065f, 0.035f}, steelMetal); // Front sight post
    appendBox(vertices, indices, {0.0f, 0.032f, 0.60f}, {0.036f, 0.036f, 0.085f}, steelMetal); // AK-74 muzzle brake

    // 6. Distinctive Curved 30-Round 5.45x39mm Plum/Bakelite Magazine
    appendBox(vertices, indices, {0.0f, -0.125f, 0.10f}, {0.038f, 0.18f, 0.085f}, plumBakelite, glm::vec3(-0.38f, 0.0f, 0.0f));

    // 7. Polymer Pistol Grip & Solid Side-Folding Buttstock
    appendBox(vertices, indices, {0.0f, -0.08f, -0.06f}, {0.036f, 0.12f, 0.055f}, blackPoly, glm::vec3(0.32f, 0.0f, 0.0f));
    appendBox(vertices, indices, {0.0f, 0.015f, -0.25f}, {0.044f, 0.10f, 0.24f}, blackPoly); // Buttstock
    appendBox(vertices, indices, {0.0f, 0.015f, -0.375f}, {0.048f, 0.11f, 0.02f}, steelMetal); // Steel buttplate

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createMosinNagant() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 walnutWood(0.44f, 0.26f, 0.14f, 0.75f);   // Polished Russian walnut (PBR 0.75)
    const glm::vec4 bluedSteel(0.14f, 0.15f, 0.16f, 0.70f);   // Blued Imperial steel (PBR 0.70)
    const glm::vec4 scopeGlass(0.25f, 0.30f, 0.35f, 0.40f);   // PU 3.5x optical scope glass

    // 1. Continuous Classic Walnut Rifle Stock & Fore-end (Slender & Elegant)
    appendBox(vertices, indices, {0.0f, 0.005f, 0.12f}, {0.052f, 0.075f, 0.54f}, walnutWood);
    appendBox(vertices, indices, {0.0f, 0.015f, 0.46f}, {0.042f, 0.050f, 0.36f}, walnutWood); // Upper handguard
    // Barrel retention brass/steel barrel bands
    appendBox(vertices, indices, {0.0f, 0.015f, 0.34f}, {0.046f, 0.056f, 0.020f}, bluedSteel);
    appendBox(vertices, indices, {0.0f, 0.015f, 0.54f}, {0.044f, 0.054f, 0.020f}, bluedSteel);

    // 2. Long Exposed Slender Steel Barrel (730mm) with Front Hooded Post
    appendBox(vertices, indices, {0.0f, 0.022f, 0.68f}, {0.024f, 0.024f, 0.36f}, bluedSteel);
    appendBox(vertices, indices, {0.0f, 0.045f, 0.82f}, {0.018f, 0.038f, 0.025f}, bluedSteel); // Front sight

    // 3. Hex/Round Receiver, Straight Bolt Handle & 5-Round Internal Magazine
    appendBox(vertices, indices, {0.0f, 0.030f, -0.04f}, {0.048f, 0.045f, 0.22f}, bluedSteel);
    // Straight bolt handle protruding to the right
    appendBox(vertices, indices, {0.048f, 0.032f, -0.06f}, {0.075f, 0.016f, 0.016f}, bluedSteel);
    appendBox(vertices, indices, {0.088f, 0.032f, -0.06f}, {0.024f, 0.024f, 0.024f}, bluedSteel); // Ball knob
    // Fixed internal 5-round box magazine underneath
    appendBox(vertices, indices, {0.0f, -0.055f, -0.02f}, {0.034f, 0.065f, 0.11f}, bluedSteel);

    // 4. PU 3.5x Sniper Optical Scope on High Steel Side Bracket
    appendBox(vertices, indices, {-0.028f, 0.055f, -0.06f}, {0.014f, 0.075f, 0.08f}, bluedSteel); // Side mount
    appendBox(vertices, indices, {0.0f, 0.092f, -0.06f}, {0.032f, 0.032f, 0.20f}, bluedSteel);   // Scope tube
    appendBox(vertices, indices, {0.0f, 0.092f,  0.042f}, {0.038f, 0.038f, 0.04f}, bluedSteel);  // Objective bell
    appendBox(vertices, indices, {0.0f, 0.092f, -0.155f}, {0.036f, 0.036f, 0.04f}, bluedSteel);  // Ocular bell
    appendBox(vertices, indices, {0.0f, 0.092f,  0.061f}, {0.032f, 0.032f, 0.005f}, scopeGlass);  // Front glass
    appendBox(vertices, indices, {0.0f, 0.092f, -0.174f}, {0.030f, 0.030f, 0.005f}, scopeGlass);  // Rear glass

    // 5. Classic Semi-Pistol Grip Wrist & Buttstock with Steel Buttplate
    appendBox(vertices, indices, {0.0f, -0.030f, -0.18f}, {0.046f, 0.075f, 0.20f}, walnutWood, glm::vec3(0.22f, 0.0f, 0.0f));
    appendBox(vertices, indices, {0.0f, -0.035f, -0.36f}, {0.048f, 0.125f, 0.24f}, walnutWood);
    appendBox(vertices, indices, {0.0f, -0.035f, -0.485f}, {0.050f, 0.130f, 0.018f}, bluedSteel); // Steel buttplate

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createBerettaM9() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 slideMetal(0.14f, 0.15f, 0.16f, 0.70f);    // Anodized black Bruniton slide (PBR 0.70)
    const glm::vec4 barrelSteel(0.22f, 0.23f, 0.24f, 0.70f);   // Exposed stainless barrel (PBR 0.70)
    const glm::vec4 gripPoly(0.08f, 0.08f, 0.09f, 0.85f);      // Checkered black grip panels (PBR 0.85)
    const glm::vec4 whiteDot(0.92f, 0.92f, 0.92f, 0.90f);      // 3-dot combat sight posts

    // 1. Characteristic Open-Top Slide (Exposed Barrel)
    appendBox(vertices, indices, {0.0f, 0.035f, 0.03f}, {0.034f, 0.038f, 0.21f}, slideMetal);
    // Open top slide cutout
    appendBox(vertices, indices, {0.0f, 0.035f, 0.06f}, {0.024f, 0.025f, 0.10f}, barrelSteel); // Exposed barrel
    // Slide serrations at rear
    appendBox(vertices, indices, {0.0f, 0.035f, -0.05f}, {0.036f, 0.036f, 0.05f}, slideMetal);

    // 2. 3-Dot Combat Sights
    appendBox(vertices, indices, {0.0f, 0.058f, 0.115f}, {0.008f, 0.014f, 0.012f}, slideMetal); // Front sight
    appendBox(vertices, indices, {0.0f, 0.059f, 0.110f}, {0.004f, 0.004f, 0.004f}, whiteDot);
    appendBox(vertices, indices, {0.0f, 0.058f, -0.070f}, {0.024f, 0.014f, 0.012f}, slideMetal); // Rear notch
    appendBox(vertices, indices, {-0.009f, 0.059f, -0.072f}, {0.004f, 0.004f, 0.004f}, whiteDot);
    appendBox(vertices, indices, { 0.009f, 0.059f, -0.072f}, {0.004f, 0.004f, 0.004f}, whiteDot);

    // 3. Frame with Accessory Rail & Combat Trigger Guard
    appendBox(vertices, indices, {0.0f, 0.012f, 0.03f}, {0.032f, 0.026f, 0.19f}, slideMetal); // Frame dust cover
    appendBox(vertices, indices, {0.0f, -0.018f, 0.02f}, {0.022f, 0.036f, 0.055f}, slideMetal); // Trigger guard
    appendBox(vertices, indices, {0.0f, -0.012f, 0.015f}, {0.010f, 0.022f, 0.015f}, slideMetal, glm::vec3(-0.35f, 0.0f, 0.0f)); // Trigger
    appendBox(vertices, indices, {0.0f, 0.038f, -0.082f}, {0.012f, 0.022f, 0.016f}, slideMetal, glm::vec3(-0.40f, 0.0f, 0.0f)); // Hammer

    // 4. Ergonomic Combat Grip & 15-Round Magazine Baseplate
    appendBox(vertices, indices, {0.0f, -0.065f, -0.032f}, {0.032f, 0.115f, 0.054f}, gripPoly, glm::vec3(0.28f, 0.0f, 0.0f));
    appendBox(vertices, indices, {0.0f, -0.125f, -0.018f}, {0.034f, 0.018f, 0.058f}, slideMetal); // Mag baseplate

    return std::make_shared<Mesh>(vertices, indices);
}







std::shared_ptr<Mesh> ProceduralMesh::createDetailedSandbagWall() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 sandbagKhaki(0.68f, 0.62f, 0.46f, 1.0f);
    const glm::vec4 sandbagDark(0.56f, 0.50f, 0.36f, 1.0f);
    const glm::vec4 woodTimber(0.38f, 0.25f, 0.14f, 1.0f);

    // 1. Wooden timber support revetment behind sandbags (Exact 2.40m modular pitch)
    appendBox(vertices, indices, {-1.02f, 0.50f, -0.22f}, {0.12f, 1.00f, 0.12f}, woodTimber);
    appendBox(vertices, indices, { 0.00f, 0.50f, -0.22f}, {0.12f, 1.00f, 0.12f}, woodTimber);
    appendBox(vertices, indices, { 1.02f, 0.50f, -0.22f}, {0.12f, 1.00f, 0.12f}, woodTimber);
    // Backing timber retaining planks
    appendBox(vertices, indices, { 0.00f, 0.42f, -0.20f}, {2.28f, 0.78f, 0.05f}, woodTimber);

    // 2. 3 Staggered Rows of burlap sandbags (Symmetric: X in [-1.20m, +1.20m], Depth Z in [-0.35m, +0.35m])
    // Bottom Course: 4 full bags + 2 edge caps
    {
        float y = 0.14f;
        for (int i = 0; i < 4; ++i) {
            float x = -0.75f + i * 0.50f;
            glm::vec4 c = (i % 2 == 0) ? sandbagKhaki : sandbagDark;
            appendBox(vertices, indices, {x, y, 0.04f}, {0.48f, 0.24f, 0.38f}, c);
        }
        appendBox(vertices, indices, {-1.08f, y, 0.04f}, {0.22f, 0.24f, 0.36f}, sandbagDark);
        appendBox(vertices, indices, { 1.08f, y, 0.04f}, {0.22f, 0.24f, 0.36f}, sandbagKhaki);
    }

    // Mid Course (staggered offset by 0.25m): 5 full bags
    {
        float y = 0.38f;
        for (int i = 0; i < 5; ++i) {
            float x = -0.96f + i * 0.48f;
            glm::vec4 c = (i % 2 == 1) ? sandbagKhaki : sandbagDark;
            appendBox(vertices, indices, {x, y, 0.02f}, {0.46f, 0.24f, 0.38f}, c);
        }
    }

    // Top Crest Course (with central low firing notch for tactical cover): 4 bags + 2 caps
    {
        float y = 0.62f;
        for (int i = 0; i < 4; ++i) {
            float x = -0.75f + i * 0.50f;
            glm::vec4 c = (i % 2 == 0) ? sandbagDark : sandbagKhaki;
            appendBox(vertices, indices, {x, y, 0.02f}, {0.47f, 0.22f, 0.36f}, c);
        }
        appendBox(vertices, indices, {-1.08f, y, 0.02f}, {0.22f, 0.22f, 0.34f}, sandbagKhaki);
        appendBox(vertices, indices, { 1.08f, y, 0.02f}, {0.22f, 0.22f, 0.34f}, sandbagDark);

        // Firing Embrasures / Lip crenelation
        appendBox(vertices, indices, {-0.65f, 0.80f, 0.00f}, {0.42f, 0.16f, 0.32f}, sandbagKhaki);
        appendBox(vertices, indices, { 0.65f, 0.80f, 0.00f}, {0.42f, 0.16f, 0.32f}, sandbagDark);
    }

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createDetailedCrate() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 woodOlive(0.30f, 0.42f, 0.26f, 1.0f);
    const glm::vec4 metalBracket(0.18f, 0.20f, 0.22f, 1.0f);
    const glm::vec4 yellowStencil(0.92f, 0.82f, 0.25f, 1.0f);

    appendBox(vertices, indices, {0, 0.45f, 0}, {1.4f, 0.9f, 1.1f}, woodOlive);

    // Steel corner brackets
    float hx = 0.68f, hy = 0.45f, hz = 0.53f;
    for (int x = -1; x <= 1; x += 2) {
        for (int y = -1; y <= 1; y += 2) {
            for (int z = -1; z <= 1; z += 2) {
                appendBox(vertices, indices, {x * hx, 0.45f + y * 0.38f, z * hz}, {0.12f, 0.18f, 0.12f}, metalBracket);
            }
        }
    }

    appendBox(vertices, indices, {0, 0.65f, 0.56f}, {0.16f, 0.12f, 0.04f}, metalBracket);
    appendBox(vertices, indices, {0, 0.91f, 0}, {0.85f, 0.02f, 0.35f}, yellowStencil);

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createPineTree() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 barkBrown(0.24f, 0.16f, 0.10f, 1.0f);
    const glm::vec4 barkHighlight(0.32f, 0.22f, 0.14f, 1.0f);
    const glm::vec4 needleSkirtCol(0.28f, 0.18f, 0.12f, 1.0f);
    const glm::vec4 needleDark(0.10f, 0.20f, 0.09f, 1.0f);
    const glm::vec4 needleMid(0.14f, 0.28f, 0.12f, 1.0f);
    const glm::vec4 needleTip(0.20f, 0.36f, 0.16f, 1.0f);

    // 1. Fallen pine needle bed skirt at base of tree (matType = 5.0f foliage)
    appendCylinder(vertices, indices, {0.0f, 0.04f, 0.0f}, 1.8f, 0.08f, needleSkirtCol, 12, 5.0f);

    // 2. Flared root buttresses anchoring into soil
    for (int r = 0; r < 4; ++r) {
        float angle = (float)r * 1.5707963f + 0.3f;
        glm::vec3 dir(std::cos(angle), 0.0f, std::sin(angle));
        appendSmoothCapsule(vertices, indices, dir * 0.15f + glm::vec3(0, 0.35f, 0), dir * 0.75f + glm::vec3(0, 0.05f, 0), 0.22f, 0.09f, barkBrown, 0.0f, 6);
    }

    // 3. Realistic Tapered Trunk (height 7.5m)
    appendSmoothCapsule(vertices, indices, {0.0f, 0.1f, 0.0f}, {0.0f, 7.2f, 0.0f}, 0.32f, 0.08f, barkHighlight, 0.0f, 10);

    // 4. Multi-Tier Branched Foliage Canopy (7 tiers from Y=1.8m to Y=7.2m)
    struct PineTier {
        float y;
        float radius;
        int branchCount;
        float droop;
    };

    PineTier tiers[7] = {
        { 1.8f, 2.6f, 8, -0.22f },
        { 2.6f, 2.3f, 8, -0.18f },
        { 3.5f, 2.0f, 7, -0.15f },
        { 4.4f, 1.6f, 6, -0.12f },
        { 5.2f, 1.3f, 6, -0.09f },
        { 6.0f, 0.95f, 5, -0.06f },
        { 6.7f, 0.60f, 4, -0.02f }
    };

    for (int t = 0; t < 7; ++t) {
        const auto& tier = tiers[t];
        float angleStep = 6.2831853f / (float)tier.branchCount;
        float tierOffset = (float)t * 0.45f;

        for (int b = 0; b < tier.branchCount; ++b) {
            float angle = (float)b * angleStep + tierOffset;
            float cosA = std::cos(angle);
            float sinA = std::sin(angle);
            glm::vec3 branchDir(cosA, tier.droop, sinA);

            // Wood branch armature
            glm::vec3 bStart(0.0f, tier.y, 0.0f);
            glm::vec3 bEnd = bStart + branchDir * (tier.radius * 0.85f);
            appendCylinder(vertices, indices, (bStart + bEnd) * 0.5f, 0.06f - (float)t * 0.006f, glm::length(bEnd - bStart), barkBrown, 5);

            // Dense foliage needle planes (matType = 5.0f for wind sway & leaf transmission!)
            glm::vec3 sideDir(-sinA, 0.0f, cosA);
            float w = tier.radius * 0.45f;
            float len = tier.radius;

            uint32_t baseIdx = static_cast<uint32_t>(vertices.size());
            glm::vec3 v0 = bStart - sideDir * (w * 0.2f);
            glm::vec3 v1 = bStart + sideDir * (w * 0.2f);
            glm::vec3 v2 = bEnd + sideDir * (w * 0.65f);
            glm::vec3 v3 = bEnd - sideDir * (w * 0.65f);
            glm::vec3 tip = bStart + branchDir * len;

            glm::vec3 fn = glm::normalize(glm::cross(v1 - v0, v2 - v0));

            glm::vec4 cBase = (b % 2 == 0) ? needleDark : needleMid;
            glm::vec4 cTip = needleTip;

            // Two-sided foliage quads
            vertices.push_back({ v0, fn, {5.0f, 0.0f}, cBase });
            vertices.push_back({ v1, fn, {5.0f, 0.0f}, cBase });
            vertices.push_back({ v2, fn, {5.0f, 1.0f}, cTip });
            vertices.push_back({ v3, fn, {5.0f, 1.0f}, cTip });
            vertices.push_back({ tip, fn, {5.0f, 1.0f}, cTip });

            // Triangle 1
            indices.push_back(baseIdx + 0); indices.push_back(baseIdx + 1); indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 0); indices.push_back(baseIdx + 2); indices.push_back(baseIdx + 3);
            indices.push_back(baseIdx + 3); indices.push_back(baseIdx + 2); indices.push_back(baseIdx + 4);

            // Back face
            indices.push_back(baseIdx + 2); indices.push_back(baseIdx + 1); indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 3); indices.push_back(baseIdx + 2); indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 4); indices.push_back(baseIdx + 2); indices.push_back(baseIdx + 3);
        }
    }

    // Top crown pyramid (matType = 5.0f foliage)
    appendPyramid(vertices, indices, {0.0f, 7.1f, 0.0f}, 0.75f, 0.95f, needleTip, 5.0f);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createHescoBastion() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 fabricTan(0.46f, 0.40f, 0.30f, 1.0f);
    const glm::vec4 steelWire(0.30f, 0.32f, 0.34f, 1.0f);
    const glm::vec4 soilCap(0.26f, 0.22f, 0.18f, 1.0f);

    // 1. Main Geotextile Earth-filled Bastion Body (1.4m x 1.35m x 1.4m)
    appendBox(vertices, indices, {0.0f, 0.675f, 0.0f}, {1.4f, 1.35f, 1.4f}, fabricTan);

    // 2. 4 Galvanized Spiral Steel Corner Hinges
    float half = 0.69f;
    appendCylinder(vertices, indices, {-half, 0.70f, -half}, 0.045f, 1.40f, steelWire, 8);
    appendCylinder(vertices, indices, { half, 0.70f, -half}, 0.045f, 1.40f, steelWire, 8);
    appendCylinder(vertices, indices, { half, 0.70f,  half}, 0.045f, 1.40f, steelWire, 8);
    appendCylinder(vertices, indices, {-half, 0.70f,  half}, 0.045f, 1.40f, steelWire, 8);

    // 3. Top Packed Crushed Rock / Earth Mound Fill Cap
    appendBox(vertices, indices, {0.0f, 1.39f, 0.0f}, {1.32f, 0.12f, 1.32f}, soilCap);
    appendPyramid(vertices, indices, {0.0f, 1.44f, 0.0f}, 1.15f, 0.18f, soilCap);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createCommandTent() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 tentCanvas(0.14f, 0.18f, 0.12f, 1.0f);
    const glm::vec4 steelPole(0.18f, 0.19f, 0.20f, 1.0f);
    const glm::vec4 camoGarnish(0.18f, 0.22f, 0.13f, 1.0f);
    const glm::vec4 interiorTable(0.35f, 0.25f, 0.18f, 1.0f);

    float len = 6.8f;
    float width = 4.6f;
    float ridgeH = 2.85f;
    float eaveH = 1.35f;
    float halfL = len * 0.5f;
    float halfW = width * 0.5f;

    // 1. Structural Tubular Steel Frame (Front, Center, Rear A-Frames)
    for (float z : {-halfL, 0.0f, halfL}) {
        // Vertical side poles
        appendCylinder(vertices, indices, {-halfW, eaveH * 0.5f, z}, 0.04f, eaveH, steelPole, 6);
        appendCylinder(vertices, indices, { halfW, eaveH * 0.5f, z}, 0.04f, eaveH, steelPole, 6);
        // Slanted rafters
        glm::vec3 ridge(0.0f, ridgeH, z);
        glm::vec3 leftEave(-halfW, eaveH, z);
        glm::vec3 rightEave(halfW, eaveH, z);
        appendCylinder(vertices, indices, (leftEave + ridge) * 0.5f, 0.04f, glm::length(ridge - leftEave), steelPole, 6);
        appendCylinder(vertices, indices, (rightEave + ridge) * 0.5f, 0.04f, glm::length(ridge - rightEave), steelPole, 6);
    }
    // Horizontal ridge beam
    appendCylinder(vertices, indices, {0.0f, ridgeH, 0.0f}, 0.045f, len, steelPole, 6);

    // 2. Heavy Military Canvas Roof Slopes (Gabled pitched roof)
    float slopeLen = std::sqrt(halfW * halfW + (ridgeH - eaveH) * (ridgeH - eaveH));
    float slopeAngle = std::atan2(ridgeH - eaveH, halfW);
    appendBox(vertices, indices, {-halfW * 0.5f, (ridgeH + eaveH) * 0.5f, 0.0f}, {slopeLen, 0.04f, len}, tentCanvas, {0.0f, 0.0f, slopeAngle});
    appendBox(vertices, indices, { halfW * 0.5f, (ridgeH + eaveH) * 0.5f, 0.0f}, {slopeLen, 0.04f, len}, tentCanvas, {0.0f, 0.0f, -slopeAngle});

    // Camouflage garnish strips directly on roof slopes
    appendBox(vertices, indices, {-halfW * 0.5f, (ridgeH + eaveH) * 0.5f + 0.03f, 0.0f}, {slopeLen * 0.85f, 0.02f, len * 0.90f}, camoGarnish, {0.0f, 0.0f, slopeAngle});
    appendBox(vertices, indices, { halfW * 0.5f, (ridgeH + eaveH) * 0.5f + 0.03f, 0.0f}, {slopeLen * 0.85f, 0.02f, len * 0.90f}, camoGarnish, {0.0f, 0.0f, -slopeAngle});

    // 3. Vertical Wall Skirts
    appendBox(vertices, indices, {-halfW, eaveH * 0.5f, 0.0f}, {0.04f, eaveH, len}, tentCanvas);
    appendBox(vertices, indices, { halfW, eaveH * 0.5f, 0.0f}, {0.04f, eaveH, len}, tentCanvas);
    // Rear wall
    appendBox(vertices, indices, {0.0f, eaveH * 0.5f, -halfL}, {width, eaveH, 0.04f}, tentCanvas);

    // 4. Interior Operations Planning Table
    appendBox(vertices, indices, {0.0f, 0.45f, 0.0f}, {2.2f, 0.85f, 1.2f}, interiorTable);
    appendBox(vertices, indices, {0.0f, 0.90f, 0.0f}, {1.8f, 0.04f, 0.9f}, glm::vec4(0.85f, 0.82f, 0.65f, 1.0f)); // Topo map sheet

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createFuelDrum() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 drumGreen(0.24f, 0.32f, 0.20f, 1.0f);
    const glm::vec4 steelRim(0.35f, 0.36f, 0.38f, 1.0f);
    const glm::vec4 yellowBand(0.82f, 0.70f, 0.16f, 1.0f);

    // 1. Drum Body (16-sided cylinder with top and bottom solid caps)
    appendCylinder(vertices, indices, {0.0f, 0.46f, 0.0f}, 0.29f, 0.92f, drumGreen, 16, 0.0f, true, true);

    // 2. Top & Bottom Chime Flanges
    appendCylinder(vertices, indices, {0.0f, 0.91f, 0.0f}, 0.31f, 0.035f, steelRim, 16, 0.0f, true, false);
    appendCylinder(vertices, indices, {0.0f, 0.02f, 0.0f}, 0.31f, 0.035f, steelRim, 16, 0.0f, false, true);

    // 3. Middle Rolling Expansion Ribs
    appendCylinder(vertices, indices, {0.0f, 0.33f, 0.0f}, 0.305f, 0.035f, steelRim, 16);
    appendCylinder(vertices, indices, {0.0f, 0.61f, 0.0f}, 0.305f, 0.035f, steelRim, 16);

    // 4. Yellow Fuel Stencil Band around midsection
    appendCylinder(vertices, indices, {0.0f, 0.47f, 0.0f}, 0.292f, 0.14f, yellowBand, 16);

    // 5. Stamped Head Bung Plugs (solid capped plugs)
    appendCylinder(vertices, indices, {0.14f, 0.93f, 0.0f}, 0.032f, 0.02f, steelRim, 8, 0.0f, true, false);
    appendCylinder(vertices, indices, {-0.14f, 0.93f, 0.0f}, 0.020f, 0.02f, steelRim, 8, 0.0f, true, false);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createCargoPallet() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 timberWood(0.36f, 0.28f, 0.18f, 1.0f);
    const glm::vec4 ammoCanGreen(0.16f, 0.22f, 0.13f, 1.0f);
    const glm::vec4 steelLatch(0.24f, 0.26f, 0.28f, 1.0f);

    // 1. Euro-Pallet 3 Bottom Runners (along Z)
    for (float x : {-0.48f, 0.0f, 0.48f}) {
        appendBox(vertices, indices, {x, 0.05f, 0.0f}, {0.10f, 0.10f, 1.10f}, timberWood);
    }

    // 2. 7 Top Deck Boards (along X)
    for (float z : {-0.48f, -0.32f, -0.16f, 0.0f, 0.16f, 0.32f, 0.48f}) {
        appendBox(vertices, indices, {0.0f, 0.115f, z}, {1.18f, 0.025f, 0.12f}, timberWood);
    }

    // 3. Stacked Heavy Munitions Spam Cans (2 rows of 2 cans)
    for (float x : {-0.24f, 0.24f}) {
        for (float z : {-0.24f, 0.24f}) {
            // Ammo can body
            appendBox(vertices, indices, {x, 0.29f, z}, {0.28f, 0.28f, 0.38f}, ammoCanGreen);
            // Handle and top lid rim
            appendBox(vertices, indices, {x, 0.44f, z}, {0.30f, 0.03f, 0.40f}, ammoCanGreen);
            appendCylinder(vertices, indices, {x, 0.465f, z}, 0.025f, 0.14f, steelLatch, 6);
        }
    }

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createFieldGenerator() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 trailerSteel(0.18f, 0.20f, 0.22f, 1.0f);
    const glm::vec4 genCabinet(0.24f, 0.30f, 0.20f, 1.0f);
    const glm::vec4 wheelRubber(0.12f, 0.12f, 0.12f, 1.0f);
    const glm::vec4 floodLamp(1.0f, 0.96f, 0.82f, 1.0f);

    // 1. Skid Trailer Frame & Tongue
    appendBox(vertices, indices, {0.0f, 0.18f, 0.0f}, {1.2f, 0.12f, 1.8f}, trailerSteel);
    appendBox(vertices, indices, {0.0f, 0.18f, 1.2f}, {0.14f, 0.10f, 0.8f}, trailerSteel); // Tow hitch

    // 2. Wheels (Left & Right)
    appendCylinder(vertices, indices, {-0.65f, 0.28f, 0.0f}, 0.28f, 0.14f, wheelRubber, 12);
    appendCylinder(vertices, indices, { 0.65f, 0.28f, 0.0f}, 0.28f, 0.14f, wheelRubber, 12);

    // 3. Diesel Generator Engine Enclosure
    appendBox(vertices, indices, {0.0f, 0.68f, -0.1f}, {0.95f, 0.85f, 1.35f}, genCabinet);

    // Radiator mesh & exhaust muffler pipe
    appendBox(vertices, indices, {0.0f, 0.68f, 0.58f}, {0.70f, 0.65f, 0.04f}, trailerSteel);
    appendCylinder(vertices, indices, {-0.30f, 1.35f, -0.5f}, 0.045f, 0.50f, trailerSteel, 8);

    // 4. Telescoping Floodlight Mast
    appendCylinder(vertices, indices, {0.35f, 1.50f, -0.55f}, 0.04f, 2.4f, trailerSteel, 8);
    // Mast crossarm
    appendBox(vertices, indices, {0.35f, 2.70f, -0.55f}, {0.90f, 0.06f, 0.06f}, trailerSteel);

    // 5. Dual Halogen Floodlights
    for (float xOff : {-0.35f, 0.35f}) {
        glm::vec3 lampPos(0.35f + xOff, 2.65f, -0.55f);
        appendBox(vertices, indices, lampPos, {0.28f, 0.22f, 0.18f}, trailerSteel, {0.35f, 0.0f, 0.0f});
        appendBox(vertices, indices, lampPos + glm::vec3(0, -0.02f, 0.10f), {0.24f, 0.18f, 0.02f}, floodLamp, {0.35f, 0.0f, 0.0f});
    }

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createMilitaryTruck() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 truckCamo(0.18f, 0.22f, 0.16f, 1.0f);
    const glm::vec4 chassisSteel(0.12f, 0.13f, 0.14f, 1.0f);
    const glm::vec4 canvasCanopy(0.20f, 0.24f, 0.17f, 1.0f);
    const glm::vec4 tireRubber(0.12f, 0.12f, 0.12f, 1.0f);
    const glm::vec4 glassWindow(0.18f, 0.24f, 0.26f, 0.90f);
    const glm::vec4 headlightLens(0.95f, 0.92f, 0.75f, 1.0f);

    // 1. Heavy Channel Steel Chassis Frame Rails (Length 7.2m)
    appendBox(vertices, indices, {-0.48f, 0.65f, 0.0f}, {0.14f, 0.22f, 7.2f}, chassisSteel);
    appendBox(vertices, indices, { 0.48f, 0.65f, 0.0f}, {0.14f, 0.22f, 7.2f}, chassisSteel);
    // Front Winch Bumper & Tow Shackles
    appendBox(vertices, indices, {0.0f, 0.65f, 3.55f}, {2.3f, 0.25f, 0.28f}, chassisSteel);
    appendCylinder(vertices, indices, {0.0f, 0.72f, 3.45f}, 0.12f, 0.55f, chassisSteel, 8); // Winch drum

    // 2. 6 Heavy All-Terrain Mud Wheels (Front axle at Z=+2.3m, Tandem rear bogie at Z=-1.2m, -2.5m)
    float axleZ[3] = { 2.3f, -1.2f, -2.5f };
    for (int a = 0; a < 3; ++a) {
        float z = axleZ[a];
        // Axle housing
        appendBox(vertices, indices, {0.0f, 0.55f, z}, {2.2f, 0.12f, 0.12f}, chassisSteel);

        for (float side : {-1.12f, 1.12f}) {
            // Mud tire
            appendCylinder(vertices, indices, {side, 0.55f, z}, 0.56f, 0.35f, tireRubber, 16);
            // 10-hole steel wheel rim hub
            appendCylinder(vertices, indices, {side * 1.03f, 0.55f, z}, 0.32f, 0.36f, chassisSteel, 12);
        }
    }

    // 3. Angular Forward Cab (Length 2.2m, Width 2.4m, Height 1.75m, Y=1.75m)
    appendBox(vertices, indices, {0.0f, 1.70f, 2.25f}, {2.35f, 1.65f, 2.1f}, truckCamo);
    // Front Hood / Engine Cowl (Sloping downward)
    appendBox(vertices, indices, {0.0f, 1.35f, 3.05f}, {2.15f, 0.95f, 0.85f}, truckCamo);
    // Front Radiator Grille
    appendBox(vertices, indices, {0.0f, 1.25f, 3.49f}, {1.65f, 0.65f, 0.04f}, chassisSteel);

    // Protected Headlights
    appendBox(vertices, indices, {-0.88f, 1.15f, 3.48f}, {0.20f, 0.20f, 0.04f}, headlightLens);
    appendBox(vertices, indices, { 0.88f, 1.15f, 3.48f}, {0.20f, 0.20f, 0.04f}, headlightLens);

    // Split Dual-Pane Ballistic Windshield (matType = 4.0f for glass reflection)
    appendBox(vertices, indices, {-0.55f, 2.05f, 3.20f}, {0.92f, 0.62f, 0.03f}, glassWindow);
    appendBox(vertices, indices, { 0.55f, 2.05f, 3.20f}, {0.92f, 0.62f, 0.03f}, glassWindow);

    // Cab Side Windows
    appendBox(vertices, indices, {-1.18f, 2.05f, 2.25f}, {0.03f, 0.55f, 0.85f}, glassWindow);
    appendBox(vertices, indices, { 1.18f, 2.05f, 2.25f}, {0.03f, 0.55f, 0.85f}, glassWindow);

    // Snorkel Air Intake Pipe along right A-pillar
    appendCylinder(vertices, indices, {1.22f, 2.20f, 2.45f}, 0.055f, 1.4f, chassisSteel, 8);

    // 4. Rear Tactical Cargo Bed (Length 4.6m, Width 2.45m, Height 1.95m)
    // Dropside bed floor & sides
    appendBox(vertices, indices, {0.0f, 1.05f, -1.25f}, {2.45f, 0.35f, 4.6f}, truckCamo);
    // Arched Heavy Military Canvas Cargo Canopy
    appendBox(vertices, indices, {0.0f, 2.25f, -1.25f}, {2.40f, 1.70f, 4.5f}, canvasCanopy);

    // 5. Vertically-Mounted Spare Tire behind cab
    appendCylinder(vertices, indices, {0.0f, 1.45f, 1.12f}, 0.55f, 0.32f, tireRubber, 14);

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createDragonTooth() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 concreteGrey(0.48f, 0.50f, 0.48f, 1.0f);
    const glm::vec4 weatheredConcrete(0.40f, 0.42f, 0.40f, 1.0f);

    // Concrete Plinth Base
    appendBox(vertices, indices, {0.0f, 0.15f, 0.0f}, {1.3f, 0.3f, 1.3f}, weatheredConcrete);
    // Anti-Tank Concrete Pyramid
    appendPyramid(vertices, indices, {0.0f, 0.3f, 0.0f}, 1.2f, 1.2f, concreteGrey);

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createBarbedWire() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 timber(0.36f, 0.24f, 0.14f, 1.0f);
    const glm::vec4 steelWire(0.30f, 0.32f, 0.34f, 1.0f);

    // X-Frame Wooden Pickets (Crossed posts)
    for (float x : {-1.5f, 0.0f, 1.5f}) {
        appendBox(vertices, indices, {x, 0.6f, 0.0f}, {0.10f, 1.4f, 0.10f}, timber, {0.0f, 0.0f, 0.5f});
        appendBox(vertices, indices, {x, 0.6f, 0.0f}, {0.10f, 1.4f, 0.10f}, timber, {0.0f, 0.0f, -0.5f});
    }

    // Coiled Razor Wire Strands
    for (float y : {0.35f, 0.65f, 0.95f}) {
        for (float z : {-0.25f, 0.25f}) {
            appendBox(vertices, indices, {0.0f, y, z}, {3.6f, 0.04f, 0.04f}, steelWire);
        }
    }

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createTankWreck() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 scorchedSteel(0.18f, 0.16f, 0.15f, 1.0f);  // Charred hull
    const glm::vec4 rustedTracks(0.26f, 0.20f, 0.16f, 1.0f);   // Rusty tracks
    const glm::vec4 darkArmor(0.14f, 0.13f, 0.12f, 1.0f);

    // 1. Lower Hull & Tracks (Left & Right)
    appendBox(vertices, indices, {-1.15f, 0.45f, 0.0f}, {0.45f, 0.8f, 4.8f}, rustedTracks);
    appendBox(vertices, indices, { 1.15f, 0.45f, 0.0f}, {0.45f, 0.8f, 4.8f}, rustedTracks);

    // 2. Main Tank Hull Chassis
    appendBox(vertices, indices, {0.0f, 0.65f, 0.0f}, {1.9f, 0.65f, 4.6f}, scorchedSteel);
    // Slanted Front Glacis Armor
    appendBox(vertices, indices, {0.0f, 0.75f, 2.1f}, {1.8f, 0.5f, 0.8f}, scorchedSteel, {-0.45f, 0, 0});

    // 3. Blown Turret (Cocked off-center from catastrophic ammo explosion)
    appendBox(vertices, indices, {0.2f, 1.25f, -0.2f}, {1.6f, 0.6f, 1.8f}, darkArmor, {0.12f, 0.35f, 0.08f});
    // Blown-open Crew Hatch
    appendBox(vertices, indices, {0.45f, 1.58f, -0.2f}, {0.55f, 0.06f, 0.55f}, darkArmor, {0.0f, 0.0f, 1.1f});

    // 4. Main Cannon Barrel (Bent & drooping)
    appendCylinder(vertices, indices, {0.35f, 1.30f, 1.8f}, 0.09f, 2.6f, darkArmor, 8);

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createRuinedWall() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 brickRed(0.48f, 0.36f, 0.30f, 1.0f);
    const glm::vec4 concretePillar(0.42f, 0.44f, 0.42f, 1.0f);
    const glm::vec4 steelRebar(0.18f, 0.18f, 0.18f, 1.0f);

    // Main Left & Right Wall Pillars
    appendBox(vertices, indices, {-2.2f, 1.6f, 0.0f}, {1.4f, 3.2f, 0.45f}, brickRed);
    appendBox(vertices, indices, { 2.2f, 1.4f, 0.0f}, {1.4f, 2.8f, 0.45f}, brickRed);

    // Lower wall below blown window
    appendBox(vertices, indices, {0.0f, 0.6f, 0.0f}, {3.0f, 1.2f, 0.45f}, brickRed);

    // Window lintel beam (cracked)
    appendBox(vertices, indices, {0.0f, 2.4f, 0.0f}, {3.0f, 0.3f, 0.50f}, concretePillar);

    // Jagged exposed steel rebar rods sticking out of collapsed upper wall
    appendBox(vertices, indices, {-2.2f, 3.4f, 0.0f}, {0.04f, 0.6f, 0.04f}, steelRebar, {0.15f, 0, 0.2f});
    appendBox(vertices, indices, {-1.8f, 3.3f, 0.0f}, {0.04f, 0.5f, 0.04f}, steelRebar, {-0.1f, 0, -0.15f});
    appendBox(vertices, indices, { 1.9f, 3.0f, 0.0f}, {0.04f, 0.5f, 0.04f}, steelRebar, {0.2f, 0, 0.1f});

    // Piles of brick and concrete rubble at the base
    appendBox(vertices, indices, {-0.5f, 0.2f,  0.4f}, {1.2f, 0.38f, 0.8f}, brickRed);
    appendBox(vertices, indices, { 0.8f, 0.15f, -0.4f}, {1.0f, 0.30f, 0.7f}, concretePillar);

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createRiverBridge() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 timberPlanks(0.38f, 0.30f, 0.20f, 1.0f);
    const glm::vec4 steelGirder(0.22f, 0.24f, 0.26f, 1.0f);
    const glm::vec4 concreteAbutment(0.44f, 0.45f, 0.44f, 1.0f);

    // 1. Heavy Wooden Roadway Deck
    appendBox(vertices, indices, {0.0f, 0.0f, 0.0f}, {6.5f, 0.4f, 22.0f}, timberPlanks);

    // 2. Steel I-Beam Support Girders beneath roadway
    appendBox(vertices, indices, {-2.8f, -0.45f, 0.0f}, {0.4f, 0.6f, 22.0f}, steelGirder);
    appendBox(vertices, indices, { 2.8f, -0.45f, 0.0f}, {0.4f, 0.6f, 22.0f}, steelGirder);

    // 3. Side Guardrails & Trusses
    appendBox(vertices, indices, {-3.1f, 0.55f, 0.0f}, {0.15f, 0.8f, 22.0f}, steelGirder);
    appendBox(vertices, indices, { 3.1f, 0.55f, 0.0f}, {0.15f, 0.8f, 22.0f}, steelGirder);

    // 4. Concrete Support Pillars in riverbed
    appendBox(vertices, indices, {0.0f, -1.8f, -4.0f}, {5.8f, 3.2f, 1.2f}, concreteAbutment);
    appendBox(vertices, indices, {0.0f, -1.8f,  4.0f}, {5.8f, 3.2f, 1.2f}, concreteAbutment);

    return std::make_shared<Mesh>(vertices, indices);
}






std::shared_ptr<Mesh> ProceduralMesh::createRiverWater(float width, float length) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    int segX = 260; // Continuous curved waterway across 2600m
    int segZ = 16;
    vertices.reserve((segX + 1) * (segZ + 1));
    indices.reserve(segX * segZ * 6);

    // Translucent deep river water with subtle wave tint
    const glm::vec4 waterCol(0.08f, 0.18f, 0.22f, 0.12f);
    glm::vec3 norm(0.0f, 1.0f, 0.0f);
    float stepX = width / (float)segX;
    float stepZ = length / (float)segZ;
    float halfW = width * 0.5f;
    float halfL = length * 0.5f;

    for (int z = 0; z <= segZ; ++z) {
        for (int x = 0; x <= segX; ++x) {
            float px = -halfW + x * stepX;
            float pz = -halfL + z * stepZ;
            // Snaking river channel curve matching World::sampleTerrainHeight
            float riverCurveZ = std::sin(px * 0.0032f) * 38.0f;
            float worldZ = pz + riverCurveZ;
            // Calibrated water elevation sitting naturally inside river trough
            float py = -0.32f + std::sin(px * 0.08f + worldZ * 0.12f) * 0.035f;

            vertices.push_back({ {px, py, worldZ}, norm, {px * 0.05f, worldZ * 0.05f}, waterCol });
        }
    }

    for (int z = 0; z < segZ; ++z) {
        for (int x = 0; x < segX; ++x) {
            uint32_t r1 = z * (segX + 1) + x;
            uint32_t r2 = (z + 1) * (segX + 1) + x;

            indices.push_back(r1);
            indices.push_back(r2);
            indices.push_back(r1 + 1);

            indices.push_back(r1 + 1);
            indices.push_back(r2);
            indices.push_back(r2 + 1);
        }
    }

    return std::make_shared<Mesh>(vertices, indices);
}


// =========================================================================
// PROCEDURAL WARZONE MULTI-ELEVATION TERRAIN (ROLLING STEPPE & RIVERBED)
// =========================================================================
std::shared_ptr<Mesh> ProceduralMesh::createWarzoneTerrain(float sizeX, float sizeZ, int subdivisionsX, int subdivisionsZ) {
    if (subdivisionsX <= 0) subdivisionsX = 1;
    if (subdivisionsZ <= 0) subdivisionsZ = subdivisionsX;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve((subdivisionsX + 1) * (subdivisionsZ + 1));
    indices.reserve(subdivisionsX * subdivisionsZ * 6);

    float halfX = sizeX * 0.5f;
    float halfZ = sizeZ * 0.5f;
    float stepX = sizeX / (float)subdivisionsX;
    float stepZ = sizeZ / (float)subdivisionsZ;

    auto sampleHeight = [](float x, float z) -> float {
        return World::sampleTerrainHeight(x, z);
    };

    // Terrain grid vertices
    for (int z = 0; z <= subdivisionsZ; ++z) {
        for (int x = 0; x <= subdivisionsX; ++x) {
            float px = -halfX + x * stepX;
            float pz = -halfZ + z * stepZ;
            float py = sampleHeight(px, pz);

            // Calculate finite difference surface normal for realistic slope lighting
            float eps = 0.5f;
            float hL = sampleHeight(px - eps, pz);
            float hR = sampleHeight(px + eps, pz);
            float hD = sampleHeight(px, pz - eps);
            float hU = sampleHeight(px, pz + eps);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));

            // Curving River Siversky channel distance
            float riverCenterZ = std::sin(px * 0.0032f) * 38.0f;
            float riverDist = std::abs(pz - riverCenterZ);
            glm::vec4 color;

            if (riverDist < 26.0f) {
                // Damp riverbank silt, mud & alluvial dark soil
                float t = std::clamp(riverDist / 26.0f, 0.0f, 1.0f);
                color = glm::mix(glm::vec4(0.18f, 0.15f, 0.12f, 1.0f), glm::vec4(0.23f, 0.19f, 0.15f, 1.0f), t);
            } else if (riverDist >= 26.0f && riverDist < 60.0f) {
                // Churned frontline earth, vehicle ruts & trench soil
                float t = std::clamp((riverDist - 26.0f) / 34.0f, 0.0f, 1.0f);
                color = glm::mix(glm::vec4(0.23f, 0.19f, 0.15f, 1.0f), glm::vec4(0.28f, 0.30f, 0.20f, 1.0f), t);
            } else {
                // Wild Ukrainian steppe meadow (tactical olive-drab earth)
                color = glm::vec4(0.28f, 0.30f, 0.20f, 1.0f);
            }

            vertices.push_back({ {px, py, pz}, normal, {px * 0.08f, pz * 0.08f}, color });
        }
    }

    // Grid triangle indices
    for (int z = 0; z < subdivisionsZ; ++z) {
        for (int x = 0; x < subdivisionsX; ++x) {
            uint32_t r1 = z * (subdivisionsX + 1) + x;
            uint32_t r2 = (z + 1) * (subdivisionsX + 1) + x;

            indices.push_back(r1);
            indices.push_back(r2);
            indices.push_back(r1 + 1);

            indices.push_back(r1 + 1);
            indices.push_back(r2);
            indices.push_back(r2 + 1);
        }
    }

    return std::make_shared<Mesh>(vertices, indices);
}


// =========================================================================
// 3D PROCEDURAL WIND-ANIMATED GRASS FIELD (WILD STEPPE MEADOWS)
// =========================================================================
std::shared_ptr<Mesh> ProceduralMesh::createGrassField(float fieldSize, int numTufts) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(numTufts * 12);
    indices.reserve(numTufts * 36);

    std::mt19937 rng(4321);
    std::uniform_real_distribution<float> distCoord(-fieldSize * 0.5f, fieldSize * 0.5f);
    std::uniform_real_distribution<float> distHeight(0.38f, 0.68f);
    std::uniform_real_distribution<float> distWidth(0.26f, 0.44f);
    std::uniform_real_distribution<float> distYaw(0.0f, 3.14159f);
    std::uniform_real_distribution<float> distColor(0.88f, 1.12f);
    std::uniform_real_distribution<float> distRoll(0.0f, 1.0f);

    auto sampleHeight = [](float x, float z) -> float {
        return World::sampleTerrainHeight(x, z);
    };

    // Physically calibrated linear PBR albedo palette (matches 2048x2048 terrain sRGB textures)
    glm::vec4 rootColor(0.024f, 0.035f, 0.012f, 1.0f); // Deep soil earth root
    glm::vec4 tipGrass(0.052f, 0.082f, 0.024f, 1.0f);  // Lush Ukrainian meadow green
    glm::vec4 tipWheat(0.075f, 0.078f, 0.034f, 1.0f);  // Sun-cured steppe thatch
    glm::vec4 tipClover(0.042f, 0.072f, 0.020f, 1.0f); // Deep dark clover
    glm::vec4 tipAmber(0.068f, 0.065f, 0.028f, 1.0f);  // Amber reed thatch

    int placed = 0;
    for (int i = 0; i < numTufts * 4 && placed < numTufts; ++i) {
        float gx = distCoord(rng);
        float gz = distCoord(rng);

        // Do not spawn grass inside the deep river channel (Z = 0)
        if (std::abs(gz) < 12.0f) continue;

        // Curving military road corridor:
        float roadCenter = std::sin(gz * 0.018f) * 3.2f;
        float distToRoad = std::abs(gx - roadCenter);

        // Road wheel ruts (compacted tire tracks): fully clear
        if (distToRoad > 0.8f && distToRoad < 2.6f) continue;
        // Road center hump between wheel tracks: sparse weeds
        if (distToRoad <= 0.8f && distRoll(rng) > 0.15f) continue;
        // Road gravel shoulders: natural organic feathered transition
        if (distToRoad >= 2.6f && distToRoad < 4.2f && distRoll(rng) > 0.45f) continue;

        // East village branch road exclusion
        if (gx > -10.0f && gx < 160.0f && gz > -65.0f && gz < 25.0f) {
            float eastBranch = std::abs((gz + 18.0f) - (gx * 0.28f));
            if (eastBranch < 2.4f) continue;
            if (eastBranch < 3.8f && distRoll(rng) > 0.35f) continue;
        }

        // Do not spawn inside FOB fortified staging compounds
        if (std::abs(gx) < 32.0f && (std::abs(gz + 220.0f) < 30.0f || std::abs(gz - 220.0f) < 30.0f)) continue;

        float gy = sampleHeight(gx, gz);
        float h = distHeight(rng);
        float w = distWidth(rng);
        float yaw = distYaw(rng);

        float colVar = distColor(rng);
        int paletteRoll = rng() % 100;
        glm::vec4 tipColor;
        if (paletteRoll < 45) tipColor = tipGrass * colVar;
        else if (paletteRoll < 72) tipColor = tipWheat * colVar;
        else if (paletteRoll < 86) tipColor = tipClover * colVar;
        else tipColor = tipAmber * colVar;
        tipColor.a = 1.0f;

        // 3 crossed vertical quads (60-degree star formation) with natural outward tip flare
        for (int q = 0; q < 3; ++q) {
            float quadYaw = yaw + (float)q * 1.04719755f; // 60 degrees
            float cy = std::cos(quadYaw) * w * 0.5f;
            float sy = std::sin(quadYaw) * w * 0.5f;

            // Outward tip flare creates lush, natural volumetric tuft silhouette
            float flare = 1.25f;

            glm::vec3 v0 = { gx - cy, gy, gz - sy };
            glm::vec3 v1 = { gx + cy, gy, gz + sy };
            glm::vec3 v2 = { gx + cy * flare, gy + h, gz + sy * flare };
            glm::vec3 v3 = { gx - cy * flare, gy + h, gz - sy * flare };

            // Upward-softened surface normal for smooth hemispherical dome lighting
            glm::vec3 faceNorm = glm::cross(v1 - v0, v3 - v0);
            glm::vec3 normal = glm::normalize(glm::vec3(faceNorm.x * 0.20f, 0.95f, faceNorm.z * 0.20f));
            uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

            // TexCoords.x = 5.0f marks foliage for vertex wind sway shader!
            vertices.push_back({ v0, normal, {5.0f, 0.0f}, rootColor });
            vertices.push_back({ v1, normal, {5.0f, 0.0f}, rootColor });
            vertices.push_back({ v2, normal, {5.0f, 1.0f}, tipColor });
            vertices.push_back({ v3, normal, {5.0f, 1.0f}, tipColor });

            // Front face
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);

            // Back face
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 3);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 0);
        }
        placed++;
    }

    return std::make_shared<Mesh>(vertices, indices);
}



std::shared_ptr<Mesh> ProceduralMesh::createGrassClump() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 grassCol(0.34f, 0.42f, 0.20f, 1.0f);
    glm::vec4 tipCol(0.48f, 0.54f, 0.26f, 1.0f);

    float angles[4] = { 0.0f, 45.0f, 90.0f, 135.0f };
    for (float ang : angles) {
        float rad = glm::radians(ang);
        float dx = std::cos(rad) * 0.45f;
        float dz = std::sin(rad) * 0.45f;
        float h = 0.85f;

        uint32_t base = (uint32_t)vertices.size();
        glm::vec3 n(-dz, 0.0f, dx);

        vertices.push_back({ {-dx, 0.0f, -dz}, n, {0.0f, 0.0f}, grassCol });
        vertices.push_back({ { dx, 0.0f,  dz}, n, {1.0f, 0.0f}, grassCol });
        vertices.push_back({ { dx * 0.9f, h,  dz * 0.9f}, n, {1.0f, 1.0f}, tipCol });
        vertices.push_back({ {-dx * 0.9f, h, -dz * 0.9f}, n, {0.0f, 1.0f}, tipCol });

        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 3);
        indices.push_back(base + 2); indices.push_back(base + 1); indices.push_back(base + 0);
        indices.push_back(base + 3); indices.push_back(base + 2); indices.push_back(base + 0);
    }

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createBushFoliage() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 foliageCol(0.22f, 0.32f, 0.16f, 1.0f);
    glm::vec4 foliageLight(0.28f, 0.38f, 0.20f, 1.0f);

    // Central core foliage cluster
    appendSmoothEllipsoid(vertices, indices, glm::vec3(0.0f, 0.65f, 0.0f), glm::vec3(0.75f, 0.60f, 0.75f), foliageCol, 0.0f, 14, 10);
    // Surrounding natural leafy mounds
    appendSmoothEllipsoid(vertices, indices, glm::vec3( 0.35f, 0.50f,  0.25f), glm::vec3(0.55f, 0.45f, 0.55f), foliageLight, 0.0f, 12, 8);
    appendSmoothEllipsoid(vertices, indices, glm::vec3(-0.35f, 0.48f, -0.20f), glm::vec3(0.50f, 0.42f, 0.50f), foliageCol, 0.0f, 12, 8);
    appendSmoothEllipsoid(vertices, indices, glm::vec3(-0.20f, 0.52f,  0.35f), glm::vec3(0.52f, 0.44f, 0.52f), foliageLight, 0.0f, 12, 8);

    // Gnarled wood base stems
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.20f, 0.0f), 0.06f, 0.40f, glm::vec4(0.24f, 0.18f, 0.12f, 1.0f), 8);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createConcreteBarrier() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 concreteCol(0.66f, 0.65f, 0.62f, 1.0f);
    glm::vec4 steelFoot(0.40f, 0.40f, 0.42f, 1.0f);

    // Heavy Jersey Highway Barrier: 3.0m long, 0.95m high, stepped cross-section
    appendBox(vertices, indices, glm::vec3(0.0f, 0.15f, 0.0f), glm::vec3(3.0f, 0.30f, 0.65f), concreteCol);
    appendBox(vertices, indices, glm::vec3(0.0f, 0.55f, 0.0f), glm::vec3(3.0f, 0.50f, 0.38f), concreteCol);
    appendBox(vertices, indices, glm::vec3(0.0f, 0.88f, 0.0f), glm::vec3(3.0f, 0.16f, 0.22f), concreteCol);

    // Steel lifting rebar loops on top
    appendCylinder(vertices, indices, glm::vec3(-0.85f, 0.98f, 0.0f), 0.015f, 0.12f, steelFoot, 8);
    appendCylinder(vertices, indices, glm::vec3( 0.85f, 0.98f, 0.0f), 0.015f, 0.12f, steelFoot, 8);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createCampfireSmoke() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Scorched metal burning drum base
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.42f, 0.0f), 0.32f, 0.84f, glm::vec4(0.14f, 0.13f, 0.12f, 1.0f), 12);
    // Glowing red burning embers at top
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.82f, 0.0f), 0.30f, 0.08f, glm::vec4(0.95f, 0.38f, 0.05f, 1.0f), 12);

    // 4 rising smoke billow discs expanding upwards
    glm::vec4 smokeCol(0.22f, 0.22f, 0.24f, 0.75f);
    appendSmoothEllipsoid(vertices, indices, glm::vec3( 0.02f, 1.25f,  0.02f), glm::vec3(0.45f, 0.35f, 0.45f), smokeCol, 0.0f, 12, 8);
    appendSmoothEllipsoid(vertices, indices, glm::vec3(-0.06f, 1.85f,  0.05f), glm::vec3(0.65f, 0.48f, 0.65f), smokeCol, 0.0f, 12, 8);
    appendSmoothEllipsoid(vertices, indices, glm::vec3( 0.10f, 2.65f, -0.08f), glm::vec3(0.92f, 0.62f, 0.92f), smokeCol, 0.0f, 12, 8);
    appendSmoothEllipsoid(vertices, indices, glm::vec3(-0.14f, 3.65f,  0.12f), glm::vec3(1.25f, 0.85f, 1.25f), smokeCol, 0.0f, 12, 8);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createSignalFlare() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Flare casing stick
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.18f, 0.0f), 0.035f, 0.36f, glm::vec4(0.48f, 0.46f, 0.42f, 1.0f), 8);
    // Blinding red flare spark core
    appendSmoothEllipsoid(vertices, indices, glm::vec3(0.0f, 0.40f, 0.0f), glm::vec3(0.12f, 0.16f, 0.12f), glm::vec4(1.0f, 0.22f, 0.12f, 1.0f), 0.0f, 12, 8);
    // Incandescent illumination flare corona
    appendSmoothEllipsoid(vertices, indices, glm::vec3(0.0f, 0.40f, 0.0f), glm::vec3(0.24f, 0.28f, 0.24f), glm::vec4(1.0f, 0.65f, 0.25f, 0.65f), 0.0f, 10, 6);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createMountedHMG() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    glm::vec4 gunmetal(0.18f, 0.19f, 0.20f, 1.0f);
    glm::vec4 darkSteel(0.12f, 0.12f, 0.13f, 1.0f);
    glm::vec4 oliveAmmoCan(0.26f, 0.30f, 0.20f, 1.0f);
    glm::vec4 brassLink(0.82f, 0.68f, 0.22f, 1.0f);
    glm::vec4 rubberGrips(0.08f, 0.08f, 0.08f, 1.0f);

    // 1. Heavy Tripod Base (Pintle Mount)
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.38f, 0.0f), 0.055f, 0.76f, darkSteel, 10);
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.65f, 0.0f), 0.09f, 0.12f, gunmetal, 10);

    // 3 Angled Tubular Tripod Legs
    appendBox(vertices, indices, glm::vec3(0.0f, 0.30f, 0.42f), glm::vec3(0.045f, 0.045f, 0.78f), darkSteel, glm::vec3(-0.62f, 0.0f, 0.0f));
    appendBox(vertices, indices, glm::vec3(-0.38f, 0.30f, -0.32f), glm::vec3(0.045f, 0.045f, 0.75f), darkSteel, glm::vec3(0.55f, -0.80f, 0.0f));
    appendBox(vertices, indices, glm::vec3(0.38f, 0.30f, -0.32f), glm::vec3(0.045f, 0.045f, 0.75f), darkSteel, glm::vec3(0.55f, 0.80f, 0.0f));

    // Foot ground spade claws
    appendBox(vertices, indices, glm::vec3(0.0f, 0.02f, 0.75f), glm::vec3(0.12f, 0.04f, 0.14f), darkSteel);
    appendBox(vertices, indices, glm::vec3(-0.62f, 0.02f, -0.58f), glm::vec3(0.12f, 0.04f, 0.14f), darkSteel);
    appendBox(vertices, indices, glm::vec3(0.62f, 0.02f, -0.58f), glm::vec3(0.12f, 0.04f, 0.14f), darkSteel);

    // 2. Gun Cradle & Elevation Yoke
    appendBox(vertices, indices, glm::vec3(0.0f, 0.78f, 0.0f), glm::vec3(0.18f, 0.14f, 0.14f), gunmetal);
    appendBox(vertices, indices, glm::vec3(-0.09f, 0.88f, 0.0f), glm::vec3(0.035f, 0.20f, 0.12f), gunmetal);
    appendBox(vertices, indices, glm::vec3(0.09f, 0.88f, 0.0f), glm::vec3(0.035f, 0.20f, 0.12f), gunmetal);

    // 3. DShK 12.7mm Heavy Receiver Body
    appendBox(vertices, indices, glm::vec3(0.0f, 0.96f, 0.08f), glm::vec3(0.14f, 0.15f, 0.62f), gunmetal);
    appendBox(vertices, indices, glm::vec3(0.0f, 1.05f, 0.12f), glm::vec3(0.13f, 0.045f, 0.38f), darkSteel);

    // 4. Heavy 12.7x108mm Fluted Barrel
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.96f, 0.55f), 0.048f, 0.40f, darkSteel, 12);
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.96f, 1.05f), 0.032f, 0.70f, gunmetal, 10);
    appendCylinder(vertices, indices, glm::vec3(0.0f, 0.96f, 1.45f), 0.065f, 0.16f, darkSteel, 12);
    appendBox(vertices, indices, glm::vec3(0.0f, 0.96f, 1.45f), glm::vec3(0.15f, 0.03f, 0.12f), darkSteel);

    // 5. Spade Butterfly Grips & Thumb Trigger
    appendBox(vertices, indices, glm::vec3(0.0f, 0.96f, -0.26f), glm::vec3(0.24f, 0.035f, 0.035f), darkSteel);
    appendCylinder(vertices, indices, glm::vec3(-0.11f, 0.96f, -0.26f), 0.022f, 0.16f, rubberGrips, 8);
    appendCylinder(vertices, indices, glm::vec3(0.11f, 0.96f, -0.26f), 0.022f, 0.16f, rubberGrips, 8);
    appendBox(vertices, indices, glm::vec3(0.0f, 0.96f, -0.24f), glm::vec3(0.04f, 0.06f, 0.02f), brassLink);

    // 6. Side Ammo Box & Brass Cartridge Belt
    appendBox(vertices, indices, glm::vec3(-0.18f, 0.94f, 0.10f), glm::vec3(0.14f, 0.22f, 0.26f), oliveAmmoCan);
    appendBox(vertices, indices, glm::vec3(-0.09f, 0.99f, 0.10f), glm::vec3(0.08f, 0.025f, 0.14f), brassLink);

    // 7. AA Spider Sight & Rear Aperture
    appendCylinder(vertices, indices, glm::vec3(0.0f, 1.09f, 0.38f), 0.055f, 0.015f, darkSteel, 12);
    appendBox(vertices, indices, glm::vec3(0.0f, 1.08f, -0.15f), glm::vec3(0.025f, 0.06f, 0.02f), darkSteel);

    return std::make_shared<Mesh>(vertices, indices);
}



// =========================================================================
// PROCEDURAL TRAINING BASE TERRAIN & COMPOUND GROUNDS
// =========================================================================
std::shared_ptr<Mesh> ProceduralMesh::createTrainingTerrain(float sizeX, float sizeZ, int subdivisions) {
    if (subdivisions <= 0) subdivisions = 1;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve((subdivisions + 1) * (subdivisions + 1));
    indices.reserve(subdivisions * subdivisions * 6);

    float halfX = sizeX * 0.5f;
    float halfZ = sizeZ * 0.5f;
    float stepX = sizeX / (float)subdivisions;
    float stepZ = sizeZ / (float)subdivisions;

    auto sampleBaseHeight = [](float x, float z) -> float {
        return World::sampleTerrainHeight(x, z);
    };

    for (int z = 0; z <= subdivisions; ++z) {
        for (int x = 0; x <= subdivisions; ++x) {
            float px = -halfX + x * stepX;
            float pz = -halfZ + z * stepZ;
            float py = sampleBaseHeight(px, pz);

            float eps = 0.5f;
            float hL = sampleBaseHeight(px - eps, pz);
            float hR = sampleBaseHeight(px + eps, pz);
            float hD = sampleBaseHeight(px, pz - eps);
            float hU = sampleBaseHeight(px, pz + eps);
            glm::vec3 normal = glm::normalize(glm::vec3(hL - hR, 2.0f * eps, hD - hU));

            glm::vec4 color(0.28f, 0.30f, 0.20f, 1.0f);
            vertices.push_back({ {px, py, pz}, normal, {px * 0.08f, pz * 0.08f}, color });
        }
    }

    for (int z = 0; z < subdivisions; ++z) {
        for (int x = 0; x < subdivisions; ++x) {
            uint32_t r1 = z * (subdivisions + 1) + x;
            uint32_t r2 = (z + 1) * (subdivisions + 1) + x;

            indices.push_back(r1);
            indices.push_back(r2);
            indices.push_back(r1 + 1);

            indices.push_back(r1 + 1);
            indices.push_back(r2);
            indices.push_back(r2 + 1);
        }
    }

    return std::make_shared<Mesh>(vertices, indices);
}


// =========================================================================
// 3D PROCEDURAL WIND-ANIMATED GRASS FIELD FOR MILITARY BASE
// =========================================================================
// =========================================================================
// FLAT TACTICAL ARENA GROUND MESH (FOR TDM / CQB INDUSTRIAL YARDS)
// =========================================================================
std::shared_ptr<Mesh> ProceduralMesh::createArenaTerrain(float sizeX, float sizeZ, int subdivisions) {
    if (subdivisions <= 0) subdivisions = 1;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve((subdivisions + 1) * (subdivisions + 1));
    indices.reserve(subdivisions * subdivisions * 6);

    float halfX = sizeX * 0.5f;
    float halfZ = sizeZ * 0.5f;
    float stepX = sizeX / (float)subdivisions;
    float stepZ = sizeZ / (float)subdivisions;
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    glm::vec4 color(0.40f, 0.40f, 0.42f, 1.0f);

    for (int z = 0; z <= subdivisions; ++z) {
        for (int x = 0; x <= subdivisions; ++x) {
            float px = -halfX + x * stepX;
            float pz = -halfZ + z * stepZ;
            float py = 0.0f; // Exact 0.0f surface height for player feet & crates

            glm::vec2 uv(px * 0.25f, pz * 0.25f);
            vertices.push_back({ {px, py, pz}, normal, uv, color });
        }
    }

    for (int z = 0; z < subdivisions; ++z) {
        for (int x = 0; x < subdivisions; ++x) {
            uint32_t r1 = z * (subdivisions + 1) + x;
            uint32_t r2 = (z + 1) * (subdivisions + 1) + x;

            indices.push_back(r1);
            indices.push_back(r2);
            indices.push_back(r1 + 1);

            indices.push_back(r1 + 1);
            indices.push_back(r2);
            indices.push_back(r2 + 1);
        }
    }

    return std::make_shared<Mesh>(vertices, indices);
}

std::shared_ptr<Mesh> ProceduralMesh::createTrainingGrassField(float fieldSize, int numTufts) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(numTufts * 12);
    indices.reserve(numTufts * 36);

    std::mt19937 rng(98765);
    std::uniform_real_distribution<float> distCoord(-fieldSize * 0.5f, fieldSize * 0.5f);
    std::uniform_real_distribution<float> distHeight(0.38f, 0.68f);
    std::uniform_real_distribution<float> distWidth(0.26f, 0.44f);
    std::uniform_real_distribution<float> distYaw(0.0f, 3.14159f);
    std::uniform_real_distribution<float> distColor(0.88f, 1.12f);

    auto sampleBaseHeight = [](float x, float z) -> float {
        if (x >= -85.0f && x <= 235.0f && z >= -120.0f && z <= 95.0f) {
            if (x > 215.0f && std::abs(z) < 25.0f) {
                float t = (x - 215.0f) / 20.0f;
                return glm::smoothstep(0.0f, 1.0f, t) * 4.5f;
            }
            return 0.0f;
        }
        float distEdgeX = 0.0f;
        if (x < -85.0f) distEdgeX = -85.0f - x;
        else if (x > 235.0f) distEdgeX = x - 235.0f;

        float distEdgeZ = 0.0f;
        if (z < -120.0f) distEdgeZ = -120.0f - z;
        else if (z > 95.0f) distEdgeZ = z - 95.0f;

        float distEdge = std::max(distEdgeX, distEdgeZ);
        float berm = glm::smoothstep(0.0f, 35.0f, distEdge) * 4.0f;
        float roll = (std::sin(x * 0.02f) * std::cos(z * 0.02f)) * 0.8f;
        return berm + roll;
    };

    glm::vec4 rootColor(0.024f, 0.035f, 0.012f, 1.0f);
    glm::vec4 tipGrass(0.052f, 0.082f, 0.024f, 1.0f);
    glm::vec4 tipWheat(0.075f, 0.078f, 0.034f, 1.0f);
    glm::vec4 tipClover(0.042f, 0.072f, 0.020f, 1.0f);
    glm::vec4 tipAmber(0.068f, 0.065f, 0.028f, 1.0f);

    int placed = 0;
    for (int i = 0; i < numTufts * 4 && placed < numTufts; ++i) {
        float gx = distCoord(rng);
        float gz = distCoord(rng);

        if (std::abs(gx) < 19.0f && std::abs(gz) < 19.0f) continue;
        if (gx >= 12.0f && gx <= 235.0f && std::abs(gz) <= 24.0f) continue;
        if (gx >= -66.0f && gx <= -28.0f && std::abs(gz) <= 20.0f) continue;
        if (std::abs(gx) <= 38.0f && gz >= 28.0f && gz <= 82.0f) continue;
        if (std::abs(gx) <= 5.5f && gz >= -115.0f && gz <= 80.0f) continue;
        if (std::abs(gx) <= 32.0f && gz >= -112.0f && gz <= -68.0f) continue;

        float gy = sampleBaseHeight(gx, gz);
        float h = distHeight(rng);
        float w = distWidth(rng);
        float yaw = distYaw(rng);

        float colVar = distColor(rng);
        int paletteRoll = rng() % 100;
        glm::vec4 tipColor;
        if (paletteRoll < 45) tipColor = tipGrass * colVar;
        else if (paletteRoll < 72) tipColor = tipWheat * colVar;
        else if (paletteRoll < 86) tipColor = tipClover * colVar;
        else tipColor = tipAmber * colVar;
        tipColor.a = 1.0f;

        for (int q = 0; q < 3; ++q) {
            float quadYaw = yaw + (float)q * 1.04719755f;
            float cy = std::cos(quadYaw) * w * 0.5f;
            float sy = std::sin(quadYaw) * w * 0.5f;
            float flare = 1.25f;

            glm::vec3 v0 = { gx - cy, gy, gz - sy };
            glm::vec3 v1 = { gx + cy, gy, gz + sy };
            glm::vec3 v2 = { gx + cy * flare, gy + h, gz + sy * flare };
            glm::vec3 v3 = { gx - cy * flare, gy + h, gz - sy * flare };

            glm::vec3 faceNorm = glm::cross(v1 - v0, v3 - v0);
            glm::vec3 normal = glm::normalize(glm::vec3(faceNorm.x * 0.20f, 0.95f, faceNorm.z * 0.20f));
            uint32_t baseIdx = static_cast<uint32_t>(vertices.size());

            vertices.push_back({ v0, normal, {5.0f, 0.0f}, rootColor });
            vertices.push_back({ v1, normal, {5.0f, 0.0f}, rootColor });
            vertices.push_back({ v2, normal, {5.0f, 1.0f}, tipColor });
            vertices.push_back({ v3, normal, {5.0f, 1.0f}, tipColor });

            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 1);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 0);
            indices.push_back(baseIdx + 2);
            indices.push_back(baseIdx + 3);
        }
        placed++;
    }

    return std::make_shared<Mesh>(vertices, indices);
}



// =========================================================================
// MODULAR URBAN & REGIONAL MMO ARCHITECTURE
// =========================================================================

std::shared_ptr<Mesh> ProceduralMesh::createUrbanApartment() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 concreteFacade(0.56f, 0.55f, 0.52f, 1.0f);
    const glm::vec4 concreteTrim(0.72f, 0.72f, 0.70f, 1.0f);
    const glm::vec4 windowGlass(0.12f, 0.16f, 0.22f, 1.0f);
    const glm::vec4 windowFrame(0.85f, 0.85f, 0.83f, 1.0f);
    const glm::vec4 balconySteel(0.28f, 0.30f, 0.32f, 1.0f);
    const glm::vec4 roofGravel(0.38f, 0.38f, 0.36f, 1.0f);
    const glm::vec4 steelDoor(0.22f, 0.25f, 0.28f, 1.0f);

    float bW = 18.0f;
    float bD = 11.5f;
    float bH = 15.0f; // 5 floors (3.0m per floor)

    // 1. Primary Structural Concrete Mass
    appendBox(vertices, indices, {0.0f, bH * 0.5f, 0.0f}, {bW, bH, bD}, concreteFacade);

    // 2. Rooftop Parapet Wall & Gravel Insulation Layer
    appendBox(vertices, indices, {0.0f, bH + 0.05f, 0.0f}, {bW - 0.4f, 0.1f, bD - 0.4f}, roofGravel);
    appendBox(vertices, indices, {0.0f, bH + 0.45f, bD * 0.5f - 0.15f}, {bW, 0.9f, 0.3f}, concreteTrim);
    appendBox(vertices, indices, {0.0f, bH + 0.45f, -bD * 0.5f + 0.15f}, {bW, 0.9f, 0.3f}, concreteTrim);
    appendBox(vertices, indices, {bW * 0.5f - 0.15f, bH + 0.45f, 0.0f}, {0.3f, 0.9f, bD}, concreteTrim);
    appendBox(vertices, indices, {-bW * 0.5f + 0.15f, bH + 0.45f, 0.0f}, {0.3f, 0.9f, bD}, concreteTrim);

    // Elevator & Ventilation Penthouse on Roof
    appendBox(vertices, indices, {-2.5f, bH + 1.25f, 0.0f}, {4.2f, 2.5f, 3.8f}, concreteFacade);
    appendBox(vertices, indices, {-2.5f, bH + 2.55f, 0.0f}, {4.4f, 0.1f, 4.0f}, concreteTrim);

    // 3. Facade Window Rows & Balconies across 5 Floors
    for (int floor = 0; floor < 5; ++floor) {
        float floorY = 1.6f + floor * 2.85f;

        // Front (+Z) & Back (-Z) Windows
        for (float wx = -6.5f; wx <= 6.5f; wx += 3.25f) {
            bool hasBalcony = (floor >= 1 && (std::abs(wx) > 5.0f || std::abs(wx) < 1.0f));

            if (hasBalcony) {
                // Cantilever Balcony Slab
                appendBox(vertices, indices, {wx, floorY - 0.95f, bD * 0.5f + 0.75f}, {2.2f, 0.22f, 1.5f}, concreteTrim);
                // Balcony Front & Side Guardrails
                appendBox(vertices, indices, {wx, floorY - 0.45f, bD * 0.5f + 1.45f}, {2.2f, 0.85f, 0.06f}, balconySteel);
                appendBox(vertices, indices, {wx - 1.07f, floorY - 0.45f, bD * 0.5f + 0.75f}, {0.06f, 0.85f, 1.4f}, balconySteel);
                appendBox(vertices, indices, {wx + 1.07f, floorY - 0.45f, bD * 0.5f + 0.75f}, {0.06f, 0.85f, 1.4f}, balconySteel);
                // Balcony Glass Doorway
                appendBox(vertices, indices, {wx, floorY + 0.15f, bD * 0.5f + 0.02f}, {1.4f, 2.0f, 0.04f}, windowGlass);
            } else {
                // Recessed Double-Pane Window
                appendBox(vertices, indices, {wx, floorY, bD * 0.5f + 0.02f}, {1.6f, 1.4f, 0.04f}, windowFrame);
                appendBox(vertices, indices, {wx, floorY, bD * 0.5f + 0.04f}, {1.4f, 1.2f, 0.04f}, windowGlass);
                // Window Sill
                appendBox(vertices, indices, {wx, floorY - 0.72f, bD * 0.5f + 0.08f}, {1.75f, 0.12f, 0.22f}, concreteTrim);
            }

            // Back Windows
            appendBox(vertices, indices, {wx, floorY, -bD * 0.5f - 0.02f}, {1.6f, 1.4f, 0.04f}, windowFrame);
            appendBox(vertices, indices, {wx, floorY, -bD * 0.5f - 0.04f}, {1.4f, 1.2f, 0.04f}, windowGlass);
            appendBox(vertices, indices, {wx, floorY - 0.72f, -bD * 0.5f - 0.08f}, {1.75f, 0.12f, 0.22f}, concreteTrim);
        }
    }

    // 4. Ground Floor Main Entrance
    appendBox(vertices, indices, {0.0f, 1.15f, bD * 0.5f + 0.05f}, {1.8f, 2.3f, 0.10f}, steelDoor);
    appendBox(vertices, indices, {0.0f, 2.50f, bD * 0.5f + 0.90f}, {2.6f, 0.22f, 1.80f}, concreteTrim); // Awning
    appendBox(vertices, indices, {-1.15f, 1.25f, bD * 0.5f + 1.65f}, {0.10f, 2.4f, 0.10f}, balconySteel); // Support post
    appendBox(vertices, indices, { 1.15f, 1.25f, bD * 0.5f + 1.65f}, {0.10f, 2.4f, 0.10f}, balconySteel);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createIndustrialWarehouse() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 corrugatedSteel(0.40f, 0.44f, 0.48f, 1.0f);
    const glm::vec4 frameBeams(0.20f, 0.22f, 0.25f, 1.0f);
    const glm::vec4 roofMetal(0.32f, 0.35f, 0.38f, 1.0f);
    const glm::vec4 skylightGlass(0.22f, 0.32f, 0.40f, 1.0f);
    const glm::vec4 freightDoor(0.26f, 0.28f, 0.30f, 1.0f);
    const glm::vec4 safetyYellow(0.85f, 0.72f, 0.15f, 1.0f);

    float wW = 24.0f;
    float wD = 16.0f;
    float wH = 7.5f;

    // 1. Warehouse Main Enclosure Walls
    appendBox(vertices, indices, {0.0f, wH * 0.5f, 0.0f}, {wW, wH, wD}, corrugatedSteel);

    // 2. Structural External I-Beam Columns along Perimeter
    for (float x = -wW * 0.5f; x <= wW * 0.5f + 0.1f; x += 6.0f) {
        appendBox(vertices, indices, {x, wH * 0.5f,  wD * 0.5f + 0.20f}, {0.45f, wH, 0.40f}, frameBeams);
        appendBox(vertices, indices, {x, wH * 0.5f, -wD * 0.5f - 0.20f}, {0.45f, wH, 0.40f}, frameBeams);
    }
    for (float z = -wD * 0.5f; z <= wD * 0.5f + 0.1f; z += 5.33f) {
        appendBox(vertices, indices, { wW * 0.5f + 0.20f, wH * 0.5f, z}, {0.40f, wH, 0.45f}, frameBeams);
        appendBox(vertices, indices, {-wW * 0.5f - 0.20f, wH * 0.5f, z}, {0.40f, wH, 0.45f}, frameBeams);
    }

    // 3. Pitched Gable Roof with Skylights
    float roofRidgeH = 2.4f;
    appendBox(vertices, indices, {0.0f, wH + roofRidgeH * 0.5f, 0.0f}, {wW + 0.8f, roofRidgeH, wD + 0.8f}, roofMetal);
    // Ridge beam
    appendBox(vertices, indices, {0.0f, wH + roofRidgeH + 0.15f, 0.0f}, {wW + 1.0f, 0.3f, 0.5f}, frameBeams);
    // Roof Skylight Glass strips
    for (float x = -8.0f; x <= 8.0f; x += 4.0f) {
        appendBox(vertices, indices, {x, wH + roofRidgeH * 0.65f, 3.2f}, {1.8f, 0.25f, 3.5f}, skylightGlass);
        appendBox(vertices, indices, {x, wH + roofRidgeH * 0.65f, -3.2f}, {1.8f, 0.25f, 3.5f}, skylightGlass);
    }

    // 4. Large Roll-Up Freight Loading Doors
    appendBox(vertices, indices, {-5.5f, 2.1f, wD * 0.5f + 0.05f}, {4.2f, 4.2f, 0.15f}, freightDoor);
    appendBox(vertices, indices, { 5.5f, 2.1f, wD * 0.5f + 0.05f}, {4.2f, 4.2f, 0.15f}, freightDoor);
    // Hazard Stripes & Door Bumpers
    appendBox(vertices, indices, {-5.5f, 4.35f, wD * 0.5f + 0.12f}, {4.6f, 0.35f, 0.12f}, safetyYellow);
    appendBox(vertices, indices, { 5.5f, 4.35f, wD * 0.5f + 0.12f}, {4.6f, 0.35f, 0.12f}, safetyYellow);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createTownChurch() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 brickWall(0.68f, 0.66f, 0.62f, 1.0f);
    const glm::vec4 stoneTrim(0.80f, 0.78f, 0.74f, 1.0f);
    const glm::vec4 roofTiles(0.44f, 0.26f, 0.22f, 1.0f);
    const glm::vec4 steepleCopper(0.26f, 0.38f, 0.34f, 1.0f);
    const glm::vec4 timberDoor(0.32f, 0.22f, 0.14f, 1.0f);
    const glm::vec4 stainedGlass(0.24f, 0.32f, 0.44f, 1.0f);

    float nW = 10.5f;
    float nL = 17.5f;
    float nH = 6.8f;

    // 1. Church Nave Mass & Foundation Plinth
    appendBox(vertices, indices, {0.0f, 0.25f, 0.0f}, {nW + 0.6f, 0.5f, nL + 0.6f}, stoneTrim);
    appendBox(vertices, indices, {0.0f, nH * 0.5f, 0.0f}, {nW, nH, nL}, brickWall);

    // 2. Steep Pitched Gable Roof
    float rH = 3.6f;
    appendBox(vertices, indices, {0.0f, nH + rH * 0.5f, 0.0f}, {nW + 0.8f, rH, nL + 0.8f}, roofTiles);

    // 3. Tall Square Bell Tower & Steeple
    float tW = 4.8f;
    float tH = 17.5f;
    float tZ = -nL * 0.5f + tW * 0.5f;
    appendBox(vertices, indices, {0.0f, tH * 0.5f, tZ}, {tW, tH, tW}, brickWall);
    appendBox(vertices, indices, {0.0f, tH + 0.25f, tZ}, {tW + 0.6f, 0.5f, tW + 0.6f}, stoneTrim);

    // Pyramidal Copper Steeple rising to 22.5m
    appendPyramid(vertices, indices, {0.0f, tH + 0.5f, tZ}, tW + 0.4f, 5.5f, steepleCopper);

    // Belfry Arched Openings on 4 sides of tower
    appendBox(vertices, indices, {0.0f, 14.5f, tZ + tW * 0.5f + 0.02f}, {1.4f, 2.5f, 0.1f}, stainedGlass);
    appendBox(vertices, indices, {0.0f, 14.5f, tZ - tW * 0.5f - 0.02f}, {1.4f, 2.5f, 0.1f}, stainedGlass);
    appendBox(vertices, indices, {tW * 0.5f + 0.02f, 14.5f, tZ}, {0.1f, 2.5f, 1.4f}, stainedGlass);
    appendBox(vertices, indices, {-tW * 0.5f - 0.02f, 14.5f, tZ}, {0.1f, 2.5f, 1.4f}, stainedGlass);

    // 4. Arched Nave Windows along Sides
    for (float z = -4.5f; z <= 6.5f; z += 3.6f) {
        appendBox(vertices, indices, { nW * 0.5f + 0.02f, 3.8f, z}, {0.1f, 2.4f, 1.2f}, stainedGlass);
        appendBox(vertices, indices, {-nW * 0.5f - 0.02f, 3.8f, z}, {0.1f, 2.4f, 1.2f}, stainedGlass);
    }

    // 5. Heavy Arched Timber Main Entry Portal
    appendBox(vertices, indices, {0.0f, 1.8f, nL * 0.5f + 0.05f}, {2.2f, 3.2f, 0.15f}, timberDoor);
    appendBox(vertices, indices, {0.0f, 3.5f, nL * 0.5f + 0.20f}, {2.6f, 0.4f, 0.40f}, stoneTrim);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createFreightBoxcar() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 rustRed(0.45f, 0.25f, 0.20f, 1.0f);
    const glm::vec4 steelBlack(0.14f, 0.14f, 0.16f, 1.0f);
    const glm::vec4 roofMetal(0.30f, 0.32f, 0.34f, 1.0f);
    const glm::vec4 railWheels(0.20f, 0.20f, 0.22f, 1.0f);

    float cL = 13.5f;
    float cW = 3.1f;
    float cH = 3.2f;

    // 1. Heavy Boxcar Main Body
    appendBox(vertices, indices, {0.0f, 2.2f, 0.0f}, {cL, cH, cW}, rustRed);
    // Roof Arch
    appendBox(vertices, indices, {0.0f, 3.85f, 0.0f}, {cL + 0.2f, 0.25f, cW + 0.15f}, roofMetal);

    // 2. Heavy Steel Underframe & Center Sill
    appendBox(vertices, indices, {0.0f, 0.52f, 0.0f}, {cL + 0.8f, 0.35f, 2.2f}, steelBlack);

    // 3. Rail Wheel Bogies & Flanged Train Wheels (Front & Rear)
    for (float bx : {-4.8f, 4.8f}) {
        appendBox(vertices, indices, {bx, 0.38f, 0.0f}, {2.2f, 0.25f, 2.4f}, steelBlack);
        appendCylinder(vertices, indices, {bx - 0.65f, 0.38f,  1.15f}, 0.38f, 0.18f, railWheels, 12);
        appendCylinder(vertices, indices, {bx + 0.65f, 0.38f,  1.15f}, 0.38f, 0.18f, railWheels, 12);
        appendCylinder(vertices, indices, {bx - 0.65f, 0.38f, -1.15f}, 0.38f, 0.18f, railWheels, 12);
        appendCylinder(vertices, indices, {bx + 0.65f, 0.38f, -1.15f}, 0.38f, 0.18f, railWheels, 12);
    }

    // 4. Sliding Cargo Doors & Lock Bars
    appendBox(vertices, indices, {0.0f, 2.1f,  cW * 0.5f + 0.06f}, {2.8f, 2.5f, 0.10f}, rustRed * 0.88f);
    appendBox(vertices, indices, {0.0f, 2.1f, -cW * 0.5f - 0.06f}, {2.8f, 2.5f, 0.10f}, rustRed * 0.88f);
    appendBox(vertices, indices, {0.0f, 2.1f,  cW * 0.5f + 0.12f}, {0.08f, 2.6f, 0.06f}, steelBlack);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createStreetLamp() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 steelGrey(0.24f, 0.26f, 0.28f, 1.0f);
    const glm::vec4 lampLight(0.98f, 0.94f, 0.72f, 1.0f);

    // Base plinth
    appendBox(vertices, indices, {0.0f, 0.25f, 0.0f}, {0.45f, 0.5f, 0.45f}, steelGrey);
    // Vertical mast pole
    appendCylinder(vertices, indices, {0.0f, 3.8f, 0.0f}, 0.10f, 6.8f, steelGrey, 8);
    // Curved cantilever arm
    appendBox(vertices, indices, {0.8f, 7.2f, 0.0f}, {1.6f, 0.12f, 0.12f}, steelGrey);
    // Downward luminaire housing & lens
    appendBox(vertices, indices, {1.6f, 7.05f, 0.0f}, {0.55f, 0.22f, 0.35f}, steelGrey);
    appendBox(vertices, indices, {1.6f, 6.92f, 0.0f}, {0.45f, 0.06f, 0.28f}, lampLight);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createCommunicationTower() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 steelGrey(0.48f, 0.50f, 0.52f, 1.0f);
    const glm::vec4 aviationOrange(0.88f, 0.32f, 0.12f, 1.0f);
    const glm::vec4 whiteBand(0.90f, 0.90f, 0.92f, 1.0f);
    const glm::vec4 beaconRed(1.0f, 0.15f, 0.15f, 1.0f);

    float tH = 32.0f;
    int numBands = 8;
    float bandH = tH / (float)numBands;

    // 4 Corner Lattice Legs tapering from 4.8m base to 1.2m summit
    for (int b = 0; b < numBands; ++b) {
        float y0 = b * bandH;
        float yCenter = y0 + bandH * 0.5f;
        glm::vec4 bandCol = (b % 2 == 0) ? aviationOrange : whiteBand;

        float spread = glm::mix(4.8f, 1.2f, (float)b / (float)numBands);
        float hS = spread * 0.5f;

        // 4 Vertical leg posts
        appendBox(vertices, indices, {-hS, yCenter, -hS}, {0.18f, bandH, 0.18f}, bandCol);
        appendBox(vertices, indices, { hS, yCenter, -hS}, {0.18f, bandH, 0.18f}, bandCol);
        appendBox(vertices, indices, { hS, yCenter,  hS}, {0.18f, bandH, 0.18f}, bandCol);
        appendBox(vertices, indices, {-hS, yCenter,  hS}, {0.18f, bandH, 0.18f}, bandCol);

        // Horizontal braces
        appendBox(vertices, indices, {0.0f, y0 + bandH, -hS}, {spread, 0.12f, 0.12f}, steelGrey);
        appendBox(vertices, indices, {0.0f, y0 + bandH,  hS}, {spread, 0.12f, 0.12f}, steelGrey);
        appendBox(vertices, indices, {-hS, y0 + bandH, 0.0f}, {0.12f, 0.12f, spread}, steelGrey);
        appendBox(vertices, indices, { hS, y0 + bandH, 0.0f}, {0.12f, 0.12f, spread}, steelGrey);
    }

    // Work Platforms at 16m and 28m
    for (float py : {16.0f, 28.0f}) {
        appendBox(vertices, indices, {0.0f, py, 0.0f}, {3.2f, 0.15f, 3.2f}, steelGrey);
        appendBox(vertices, indices, {0.0f, py + 0.5f, 1.55f}, {3.2f, 0.85f, 0.08f}, steelGrey);
        appendBox(vertices, indices, {0.0f, py + 0.5f, -1.55f}, {3.2f, 0.85f, 0.08f}, steelGrey);
    }

    // Parabolic Microwave Dish Antenna
    appendCylinder(vertices, indices, {1.2f, 24.0f, 0.0f}, 1.2f, 0.35f, whiteBand, 16, 0.0f, true, true);

    // Flashing Hazard Beacon at peak
    appendBox(vertices, indices, {0.0f, tH + 0.45f, 0.0f}, {0.35f, 0.70f, 0.35f}, beaconRed);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createRefineryTank() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 tankSteel(0.58f, 0.60f, 0.62f, 1.0f);
    const glm::vec4 roofMetal(0.48f, 0.50f, 0.52f, 1.0f);
    const glm::vec4 safetyYellow(0.88f, 0.74f, 0.15f, 1.0f);
    const glm::vec4 pipingSteel(0.24f, 0.25f, 0.26f, 1.0f);

    float tRadius = 6.5f;
    float tHeight = 8.5f;

    // 1. Heavy Cylindrical Steel Storage Vessel
    appendCylinder(vertices, indices, {0.0f, tHeight * 0.5f, 0.0f}, tRadius, tHeight, tankSteel, 24, 0.0f, true, true);

    // 2. Conical Welded Steel Roof
    appendPyramid(vertices, indices, {0.0f, tHeight, 0.0f}, tRadius * 1.95f, 1.4f, roofMetal);

    // 3. Safety Yellow Rooftop Perimeter Guardrail
    appendBox(vertices, indices, {0.0f, tHeight + 0.45f,  tRadius - 0.2f}, {tRadius * 1.4f, 0.9f, 0.08f}, safetyYellow);
    appendBox(vertices, indices, {0.0f, tHeight + 0.45f, -tRadius + 0.2f}, {tRadius * 1.4f, 0.9f, 0.08f}, safetyYellow);

    // 4. Ground Level Intake / Outlet Valve Manifold
    appendCylinder(vertices, indices, {tRadius + 0.6f, 0.6f, 0.0f}, 0.28f, 1.6f, pipingSteel, 10);
    appendBox(vertices, indices, {tRadius + 1.2f, 0.6f, 0.0f}, {0.45f, 0.45f, 0.45f}, safetyYellow); // Valve body

    return std::make_shared<Mesh>(vertices, indices);
}





// =========================================================================
// TRAINING, PROVING GROUNDS & DEFENSE ASSETS
// =========================================================================

std::shared_ptr<Mesh> ProceduralMesh::createShootingTarget() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 darkSteel(0.18f, 0.20f, 0.22f, 1.0f);
    const glm::vec4 postSteel(0.28f, 0.30f, 0.32f, 1.0f);
    const glm::vec4 targetOlive(0.28f, 0.36f, 0.24f, 1.0f);
    const glm::vec4 yellowTag(0.92f, 0.82f, 0.22f, 1.0f);
    const glm::vec4 redBullseye(0.85f, 0.20f, 0.15f, 1.0f);

    // 1. Steel base plate
    appendBox(vertices, indices, {0.0f, 0.04f, 0.0f}, {0.65f, 0.08f, 0.65f}, darkSteel);

    // 2. Upright steel support post
    appendBox(vertices, indices, {0.0f, 0.50f, 0.0f}, {0.08f, 0.92f, 0.08f}, postSteel);

    // 3. Silhouette target torso plate
    appendBox(vertices, indices, {0.0f, 1.15f, 0.0f}, {0.46f, 0.56f, 0.04f}, targetOlive);
    // Silhouette target head plate
    appendBox(vertices, indices, {0.0f, 1.50f, 0.0f}, {0.22f, 0.22f, 0.04f}, targetOlive);

    // 4. Center bullseye zone
    appendBox(vertices, indices, {0.0f, 1.18f, 0.025f}, {0.18f, 0.20f, 0.02f}, redBullseye);

    // 5. Distance marker signboard
    appendBox(vertices, indices, {0.0f, 0.25f, -0.42f}, {0.06f, 0.45f, 0.06f}, postSteel);
    appendBox(vertices, indices, {0.0f, 0.42f, -0.42f}, {0.08f, 0.24f, 0.38f}, yellowTag);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createWatchtower() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 timberBrown(0.34f, 0.26f, 0.18f, 1.0f);
    const glm::vec4 timberDark(0.24f, 0.18f, 0.12f, 1.0f);
    const glm::vec4 metalRoof(0.28f, 0.30f, 0.32f, 1.0f);
    const glm::vec4 steelGrey(0.38f, 0.40f, 0.42f, 1.0f);

    float tH = 4.8f;
    float pW = 2.8f;

    // 4 Corner structural timber legs
    for (float x : {-pW * 0.45f, pW * 0.45f}) {
        for (float z : {-pW * 0.45f, pW * 0.45f}) {
            appendBox(vertices, indices, {x, tH * 0.5f, z}, {0.18f, tH, 0.18f}, timberDark);
            appendBox(vertices, indices, {x, tH + 1.1f, z}, {0.12f, 2.2f, 0.12f}, timberDark);
        }
    }

    // Cross bracing struts
    appendBox(vertices, indices, {0.0f, tH * 0.45f, pW * 0.45f}, {pW * 0.85f, 0.10f, 0.10f}, timberBrown);
    appendBox(vertices, indices, {0.0f, tH * 0.45f, -pW * 0.45f}, {pW * 0.85f, 0.10f, 0.10f}, timberBrown);
    appendBox(vertices, indices, {pW * 0.45f, tH * 0.45f, 0.0f}, {0.10f, 0.10f, pW * 0.85f}, timberBrown);
    appendBox(vertices, indices, {-pW * 0.45f, tH * 0.45f, 0.0f}, {0.10f, 0.10f, pW * 0.85f}, timberBrown);

    // Platform deck floor
    appendBox(vertices, indices, {0.0f, tH, 0.0f}, {pW, 0.14f, pW}, timberBrown);

    // Safety railings
    appendBox(vertices, indices, {0.0f, tH + 0.55f, pW * 0.48f}, {pW, 0.08f, 0.06f}, timberBrown);
    appendBox(vertices, indices, {0.0f, tH + 0.55f, -pW * 0.48f}, {pW, 0.08f, 0.06f}, timberBrown);
    appendBox(vertices, indices, {pW * 0.48f, tH + 0.55f, 0.0f}, {0.06f, 0.08f, pW}, timberBrown);
    appendBox(vertices, indices, {-pW * 0.48f, tH + 0.55f, 0.4f}, {0.06f, 0.08f, pW * 0.65f}, timberBrown);

    // Pitched corrugated metal roof
    appendBox(vertices, indices, {0.0f, tH + 2.25f, 0.0f}, {pW + 0.6f, 0.08f, pW + 0.6f}, metalRoof);
    appendPyramid(vertices, indices, {0.0f, tH + 2.30f, 0.0f}, pW + 0.7f, 0.65f, metalRoof);

    // Access ladder
    for (float ly = 0.4f; ly < tH; ly += 0.45f) {
        appendBox(vertices, indices, {-pW * 0.55f, ly, -pW * 0.25f}, {0.06f, 0.04f, 0.55f}, steelGrey);
    }
    appendBox(vertices, indices, {-pW * 0.55f, tH * 0.5f, -pW * 0.52f}, {0.06f, tH, 0.06f}, steelGrey);
    appendBox(vertices, indices, {-pW * 0.55f, tH * 0.5f, 0.02f}, {0.06f, tH, 0.06f}, steelGrey);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createGuardBooth() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 concreteGrey(0.55f, 0.54f, 0.52f, 1.0f);
    const glm::vec4 boothOlive(0.32f, 0.38f, 0.28f, 1.0f);
    const glm::vec4 windowTint(0.14f, 0.18f, 0.24f, 1.0f);
    const glm::vec4 roofGrey(0.25f, 0.27f, 0.29f, 1.0f);
    const glm::vec4 gateRed(0.88f, 0.22f, 0.18f, 1.0f);
    const glm::vec4 gateWhite(0.92f, 0.92f, 0.90f, 1.0f);

    // 1. Concrete curb pad
    appendBox(vertices, indices, {0.0f, 0.10f, 0.0f}, {2.4f, 0.20f, 2.4f}, concreteGrey);

    // 2. Cabin body
    appendBox(vertices, indices, {0.0f, 1.35f, 0.0f}, {1.9f, 2.30f, 1.9f}, boothOlive);

    // 3. Observation windows
    appendBox(vertices, indices, {0.0f, 1.55f, 0.96f}, {1.4f, 0.85f, 0.04f}, windowTint);
    appendBox(vertices, indices, {0.96f, 1.55f, 0.0f}, {0.04f, 0.85f, 1.4f}, windowTint);
    appendBox(vertices, indices, {-0.96f, 1.55f, 0.0f}, {0.04f, 0.85f, 1.4f}, windowTint);

    // 4. Overhanging flat roof
    appendBox(vertices, indices, {0.0f, 2.55f, 0.0f}, {2.3f, 0.15f, 2.3f}, roofGrey);

    // 5. Security boom gate pedestal & barrier arm
    appendBox(vertices, indices, {1.55f, 0.55f, 0.0f}, {0.35f, 0.90f, 0.35f}, boothOlive);
    for (float seg = 0.0f; seg < 3.2f; seg += 0.8f) {
        glm::vec4 c = ((int)(seg / 0.8f) % 2 == 0) ? gateRed : gateWhite;
        appendBox(vertices, indices, {1.70f + seg * 0.5f, 0.85f, 0.0f}, {0.40f, 0.08f, 0.06f}, c);
    }

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createPerimeterFence() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 steelPost(0.40f, 0.42f, 0.44f, 1.0f);
    const glm::vec4 wireMesh(0.55f, 0.56f, 0.58f, 0.85f);
    const glm::vec4 barbWire(0.35f, 0.36f, 0.38f, 1.0f);

    float fW = 3.6f;
    float fH = 2.4f;

    // 2 Tubular Steel Fence Posts
    appendCylinder(vertices, indices, {-fW * 0.5f, fH * 0.5f, 0.0f}, 0.05f, fH, steelPost, 8);
    appendCylinder(vertices, indices, { fW * 0.5f, fH * 0.5f, 0.0f}, 0.05f, fH, steelPost, 8);

    // Top, middle, bottom horizontal tension rails
    appendBox(vertices, indices, {0.0f, 0.15f, 0.0f}, {fW, 0.04f, 0.04f}, steelPost);
    appendBox(vertices, indices, {0.0f, fH * 0.5f, 0.0f}, {fW, 0.04f, 0.04f}, steelPost);
    appendBox(vertices, indices, {0.0f, fH - 0.15f, 0.0f}, {fW, 0.04f, 0.04f}, steelPost);

    // Semi-transparent diamond chain-link wire mesh infill panel
    appendBox(vertices, indices, {0.0f, fH * 0.5f, 0.0f}, {fW - 0.1f, fH - 0.3f, 0.02f}, wireMesh);

    // Angled outrigger arms with 3 razor wire strands
    for (float x : {-fW * 0.5f, fW * 0.5f}) {
        appendBox(vertices, indices, {x, fH + 0.20f, 0.15f}, {0.04f, 0.40f, 0.04f}, steelPost, glm::vec3(glm::radians(30.0f), 0.0f, 0.0f));
    }
    appendBox(vertices, indices, {0.0f, fH + 0.15f, 0.10f}, {fW, 0.02f, 0.02f}, barbWire);
    appendBox(vertices, indices, {0.0f, fH + 0.28f, 0.20f}, {fW, 0.02f, 0.02f}, barbWire);
    appendBox(vertices, indices, {0.0f, fH + 0.38f, 0.30f}, {fW, 0.02f, 0.02f}, barbWire);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createGunRack() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 woodFrame(0.36f, 0.28f, 0.18f, 1.0f);
    const glm::vec4 rifleMetal(0.15f, 0.16f, 0.18f, 1.0f);
    const glm::vec4 rifleStock(0.30f, 0.22f, 0.15f, 1.0f);

    // Wooden A-Frame Base & End Uprights
    appendBox(vertices, indices, {0.0f, 0.08f, 0.0f}, {1.6f, 0.16f, 0.55f}, woodFrame);
    appendBox(vertices, indices, {-0.76f, 0.70f, 0.0f}, {0.08f, 1.25f, 0.45f}, woodFrame);
    appendBox(vertices, indices, { 0.76f, 0.70f, 0.0f}, {0.08f, 1.25f, 0.45f}, woodFrame);

    // Barrel rest bar at top
    appendBox(vertices, indices, {0.0f, 1.15f, 0.0f}, {1.55f, 0.06f, 0.12f}, woodFrame);

    // 4 Racked Assault Rifles
    for (float rx : {-0.48f, -0.16f, 0.16f, 0.48f}) {
        appendBox(vertices, indices, {rx, 0.72f, 0.0f}, {0.05f, 0.88f, 0.08f}, rifleMetal);
        appendBox(vertices, indices, {rx, 0.22f, -0.04f}, {0.05f, 0.26f, 0.12f}, rifleStock);
        appendCylinder(vertices, indices, {rx, 1.24f, 0.0f}, 0.015f, 0.22f, rifleMetal, 8);
    }

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createObstacleWall() {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    const glm::vec4 timberWall(0.38f, 0.30f, 0.20f, 1.0f);
    const glm::vec4 timberPost(0.26f, 0.20f, 0.14f, 1.0f);
    const glm::vec4 redTopBar(0.85f, 0.24f, 0.18f, 1.0f);

    float wW = 3.4f;
    float wH = 2.2f;

    // 2 Heavy timber vertical end posts
    appendBox(vertices, indices, {-wW * 0.48f, wH * 0.5f, 0.0f}, {0.20f, wH + 0.1f, 0.20f}, timberPost);
    appendBox(vertices, indices, { wW * 0.48f, wH * 0.5f, 0.0f}, {0.20f, wH + 0.1f, 0.20f}, timberPost);

    // Horizontal timber plank wall
    appendBox(vertices, indices, {0.0f, wH * 0.5f, 0.0f}, {wW - 0.25f, wH, 0.14f}, timberWall);

    // 2 Diagonal rear buttress legs
    appendBox(vertices, indices, {-wW * 0.30f, wH * 0.45f, -0.75f}, {0.12f, 0.12f, 1.6f}, timberPost, glm::vec3(glm::radians(-40.0f), 0.0f, 0.0f));
    appendBox(vertices, indices, { wW * 0.30f, wH * 0.45f, -0.75f}, {0.12f, 0.12f, 1.6f}, timberPost, glm::vec3(glm::radians(-40.0f), 0.0f, 0.0f));

    // Top red grab beam
    appendBox(vertices, indices, {0.0f, wH + 0.06f, 0.0f}, {wW + 0.1f, 0.12f, 0.18f}, redTopBar);

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createSeveredCylinder(float radius, float height, const glm::vec4& color, bool hasLampHead) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Cylinder centered at local (0, 0, 0), extending from -height/2 to +height/2
    appendCylinder(vertices, indices, {0.0f, 0.0f, 0.0f}, radius, height, color, 8, 2.0f, true, false);

    // Jagged metal fracture rim at bottom (-height/2)
    float bY = -height * 0.5f;
    for (int i = 0; i < 8; ++i) {
        float angle = (float)i / 8.0f * 6.283185f;
        float tearH = (i % 2 == 0) ? 0.08f : -0.05f;
        glm::vec3 p(std::cos(angle) * (radius * 0.95f), bY + tearH, std::sin(angle) * (radius * 0.95f));
        appendBox(vertices, indices, p, {0.04f, 0.08f, 0.04f}, color);
    }

    if (hasLampHead) {
        float tY = height * 0.5f;
        const glm::vec4 lampLight(0.98f, 0.94f, 0.72f, 1.0f);
        appendBox(vertices, indices, {0.8f, tY, 0.0f}, {1.6f, 0.12f, 0.12f}, color);
        appendBox(vertices, indices, {1.6f, tY - 0.15f, 0.0f}, {0.55f, 0.22f, 0.35f}, color);
        appendBox(vertices, indices, {1.6f, tY - 0.28f, 0.0f}, {0.45f, 0.06f, 0.28f}, lampLight);
    }

    return std::make_shared<Mesh>(vertices, indices);
}


std::shared_ptr<Mesh> ProceduralMesh::createStumpCylinder(float radius, float height, const glm::vec4& color) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // Base plinth if height is tall enough
    if (height >= 0.5f) {
        appendBox(vertices, indices, {0.0f, 0.25f, 0.0f}, {0.45f, 0.5f, 0.45f}, color);
        float cylH = height - 0.5f;
        appendCylinder(vertices, indices, {0.0f, 0.5f + cylH * 0.5f, 0.0f}, radius, cylH, color, 8, 2.0f, true, false);
    } else {
        appendBox(vertices, indices, {0.0f, height * 0.5f, 0.0f}, {0.45f, height, 0.45f}, color);
    }

    // Jagged metal fracture rim at top
    for (int i = 0; i < 8; ++i) {
        float angle = (float)i / 8.0f * 6.283185f;
        float tearH = (i % 2 == 1) ? 0.08f : -0.04f;
        glm::vec3 p(std::cos(angle) * (radius * 0.95f), height + tearH, std::sin(angle) * (radius * 0.95f));
        appendBox(vertices, indices, p, {0.04f, 0.08f, 0.04f}, color);
    }

    return std::make_shared<Mesh>(vertices, indices);
}


} // namespace Mercenary

#include <array>
#include "game/PunctureSystem.hpp"
#include <iostream>
#include <random>
#include <algorithm>

namespace Mercenary {

void PunctureSystem::init() {
    m_punctures.reserve(MAX_STORED_PUNCTURES);
}

void PunctureSystem::clear() {
    m_punctures.clear();
}

void PunctureSystem::addPuncture(const glm::vec3& hitPoint, const glm::vec3& hitNormal, const glm::vec3& rayDir, int surfaceType, bool isThroughHole, float caliberSize, int weaponType) {
    static std::mt19937 randGen(4242);
    std::uniform_real_distribution<float> seedDist(0.1f, 99.0f);

    glm::vec3 dir = glm::length(rayDir) > 0.001f ? glm::normalize(rayDir) : glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 norm = glm::length(hitNormal) > 0.001f ? glm::normalize(hitNormal) : -dir;

    // Caliber-accurate military ballistics damage scale (in meters)
    // 9mm Pistol: 1.8cm aperture / 5.0cm impact crater
    // 5.45/5.56mm Rifle: 3.2cm open through-aperture / 9.6cm kinetic destruction rim
    // 7.62x54R Sniper: 4.8cm open through-aperture / 14.4cm kinetic destruction rim
    // 12.7mm (.50 BMG) HMG: 9.0cm gaping blast perforation / 24.0cm kinetic destruction rim
    float baseInner = 0.016f; // 5.56mm / 5.45mm rifle: 3.2cm through-hole
    float baseOuter = 0.048f; // 9.6cm kinetic scorch & spall rim

    if (weaponType == 1) {
        // 7.62x54R Mosin Sniper
        baseInner = 0.024f;   // 4.8cm hole
        baseOuter = 0.072f;   // 14.4cm rim
    } else if (weaponType == 2) {
        // 9x19mm M9 Pistol
        baseInner = 0.009f;   // 1.8cm hole
        baseOuter = 0.026f;   // 5.2cm rim
    } else if (weaponType == 3) {
        // 12.7mm (.50 BMG) Heavy MG
        baseInner = 0.045f;   // 9.0cm hole
        baseOuter = 0.120f;   // 24.0cm rim
    }

    if (caliberSize > 0.01f) {
        float calScale = std::clamp(caliberSize / 0.08f, 0.75f, 1.45f);
        baseInner *= calScale;
        baseOuter *= calScale;
    }

    AssetPuncture puncture;
    puncture.position = hitPoint;
    puncture.innerRadius = baseInner;
    puncture.rayDir = dir;
    puncture.outerRadius = baseOuter;
    puncture.surfaceType = surfaceType;
    puncture.isThroughHole = isThroughHole ? 1 : 0;
    puncture.seed = seedDist(randGen);
    puncture.normal = norm;
    puncture.age = 0.0f;
    puncture.maxAge = 240.0f;

    if (m_punctures.size() >= MAX_STORED_PUNCTURES) {
        m_punctures.erase(m_punctures.begin());
    }
    m_punctures.push_back(puncture);

    std::cout << "[PunctureEngine] " << (isThroughHole ? "Through-Hole" : "Surface-Crater")
              << " at (" << hitPoint.x << ", " << hitPoint.y << ", " << hitPoint.z
              << ") Core: " << (baseInner * 200.0f) << "cm - Rim: " << (baseOuter * 200.0f)
              << "cm - Mat: " << surfaceType << "\n" << std::flush;
}

void PunctureSystem::update(float dt) {
    for (auto it = m_punctures.begin(); it != m_punctures.end();) {
        it->age += dt;
        if (it->age >= it->maxAge) {
            it = m_punctures.erase(it);
        } else {
            ++it;
        }
    }
}

void PunctureSystem::uploadToShader(Shader& shader, const glm::vec3& viewCenterPos) {
    if (m_punctures.empty()) {
        shader.setInt("uPunctureCount", 0);
        return;
    }

    // Collect up to MAX_SHADER_PUNCTURES nearest to the view center
    struct Candidate {
        float distSq;
        const AssetPuncture* puncture;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(m_punctures.size());

    for (const auto& p : m_punctures) {
        glm::vec3 diff = p.position - viewCenterPos;
        float dSq = glm::dot(diff, diff);
        candidates.push_back({dSq, &p});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.distSq < b.distSq;
    });

    int count = std::min(static_cast<int>(candidates.size()), MAX_SHADER_PUNCTURES);
    shader.setInt("uPunctureCount", count);

        struct PunctureUniformNames {
        std::string posR;
        std::string rayDir;
        std::string matSeed;
        std::string norm;
    };
    static const auto s_uniformNames = []() {
        std::array<PunctureUniformNames, MAX_SHADER_PUNCTURES> arr;
        for (int i = 0; i < MAX_SHADER_PUNCTURES; ++i) {
            std::string idx = std::to_string(i);
            arr[i].posR = "uPuncturePosR[" + idx + "]";
            arr[i].rayDir = "uPunctureRayDir[" + idx + "]";
            arr[i].matSeed = "uPunctureMatSeed[" + idx + "]";
            arr[i].norm = "uPunctureNorm[" + idx + "]";
        }
        return arr;
    }();

    for (int i = 0; i < count; ++i) {
        const auto* p = candidates[i].puncture;
        const auto& u = s_uniformNames[i];

        shader.setVec4(u.posR, glm::vec4(p->position, p->innerRadius));
        shader.setVec4(u.rayDir, glm::vec4(p->rayDir, p->outerRadius));
        shader.setVec4(u.matSeed, glm::vec4(static_cast<float>(p->surfaceType), static_cast<float>(p->isThroughHole), p->seed, 0.0f));
        shader.setVec4(u.norm, glm::vec4(p->normal, 0.0f));
    }
}

} // namespace Mercenary

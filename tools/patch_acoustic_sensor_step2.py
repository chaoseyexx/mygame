import sys

print("Patching Renderer.hpp and Renderer.cpp for 3D Acoustic Wavefront Rendering (Step 2)...")

with open('src/engine/Renderer.hpp', 'r', encoding='utf-8') as f:
    rhpp = f.read()

# 1. Forward declare struct AcousticWaveEvent and add methods to Renderer
old_reticle = "    void drawAimReticle(const glm::vec3& worldPos, float bloomRadius, const glm::vec4& color, bool withTickMarks = true);"
assert old_reticle in rhpp, "Could not find drawAimReticle in Renderer.hpp"

new_methods = """    void drawAimReticle(const glm::vec3& worldPos, float bloomRadius, const glm::vec4& color, bool withTickMarks = true);

    // AAA Acoustic Wavefront & Sensor Beacon Rendering (F3 Debug Visualizer)
    void drawAcousticWaveRing(const glm::vec3& center, float radius, const glm::vec4& color, int segments = 36);
    void drawAcousticBeacon(const glm::vec3& origin, const glm::vec4& color, float height = 1.8f);
    void renderAcousticWaves(const std::vector<AcousticWaveEvent>& waves);"""

rhpp = rhpp.replace(old_reticle, new_methods, 1)

# Forward declare AcousticWaveEvent
fwd_target = "struct WorldObstacle;"
assert fwd_target in rhpp, "Could not find struct WorldObstacle"
rhpp = rhpp.replace(fwd_target, "struct WorldObstacle;\nstruct AcousticWaveEvent;", 1)

with open('src/engine/Renderer.hpp', 'w', encoding='utf-8') as f:
    f.write(rhpp)

print("Renderer.hpp updated successfully.")

# 2. Add implementation to Renderer.cpp
with open('src/engine/Renderer.cpp', 'r', encoding='utf-8') as f:
    rcpp = f.read()

# Add include Audio.hpp
top_inc = '#include "Renderer.hpp"'
assert top_inc in rcpp, "Could not find top_inc"
rcpp = rcpp.replace(top_inc, top_inc + '\n#include "Audio.hpp"', 1)

# Add methods at the end of Renderer.cpp (before closing namespace if any)
render_impl = """
void Renderer::drawAcousticWaveRing(const glm::vec3& center, float radius, const glm::vec4& color, int segments) {
    if (radius < 0.2f || color.a <= 0.01f) return;
    const float step = 2.0f * 3.14159265f / static_cast<float>(segments);

    // Height offset slightly above floor to prevent z-fighting
    float y = center.y + 0.04f;

    std::vector<float> ringData;
    ringData.reserve(segments * 14);

    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;

        glm::vec3 p1(center.x + std::cos(a1) * radius, y, center.z + std::sin(a1) * radius);
        glm::vec3 p2(center.x + std::cos(a2) * radius, y, center.z + std::sin(a2) * radius);

        ringData.insert(ringData.end(), {p1.x, p1.y, p1.z, color.r, color.g, color.b, color.a});
        ringData.insert(ringData.end(), {p2.x, p2.y, p2.z, color.r, color.g, color.b, color.a});
    }

    m_lineShader->bind();
    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);

    size_t dataBytes = ringData.size() * sizeof(float);
    if (dataBytes > 1024 * sizeof(float) * 7) {
        glBufferData(GL_ARRAY_BUFFER, dataBytes, ringData.data(), GL_DYNAMIC_DRAW);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, dataBytes, ringData.data());
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.2f);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(segments * 2));

    glDisable(GL_BLEND);
    glLineWidth(1.0f);
    glBindVertexArray(0);
}

void Renderer::drawAcousticBeacon(const glm::vec3& origin, const glm::vec4& color, float height) {
    if (color.a <= 0.02f) return;

    // Vertical locator line from ground to beacon height
    drawLine(origin + glm::vec3(0.0f, 0.02f, 0.0f), origin + glm::vec3(0.0f, height, 0.0f), color);

    // Subtle 4-tick crosshair at beacon top
    float tick = 0.22f;
    glm::vec3 top = origin + glm::vec3(0.0f, height, 0.0f);
    drawLine(top - glm::vec3(tick, 0.0f, 0.0f), top + glm::vec3(tick, 0.0f, 0.0f), color);
    drawLine(top - glm::vec3(0.0f, 0.0f, tick), top + glm::vec3(0.0f, 0.0f, tick), color);
}

void Renderer::renderAcousticWaves(const std::vector<AcousticWaveEvent>& waves) {
    if (waves.empty()) return;

    for (const auto& w : waves) {
        float lifeFrac = w.currentRadius / (w.maxRadius > 0.0f ? w.maxRadius : 1.0f);
        if (lifeFrac >= 1.0f) continue;

        // Exponential perceptual alpha decay as wave radiates outward
        float alpha = std::pow(1.0f - lifeFrac, 1.25f) * w.color.a;
        if (alpha <= 0.02f) continue;

        glm::vec4 leadColor = w.color;
        leadColor.a = alpha;

        // 1. Primary leading wavefront ring
        drawAcousticWaveRing(w.origin, w.currentRadius, leadColor, 36);

        // 2. Trailing acoustic harmonic wake (secondary ripple at 82% radius)
        if (w.currentRadius > 3.0f) {
            glm::vec4 wakeColor = w.color;
            wakeColor.a = alpha * 0.45f;
            drawAcousticWaveRing(w.origin, w.currentRadius * 0.82f, wakeColor, 32);
        }

        // 3. Sound origin beacon pin (visible while sound wave is actively propagating)
        float beaconAlpha = std::clamp(1.0f - (w.lifetime / 1.2f), 0.0f, 1.0f) * w.color.a;
        if (beaconAlpha > 0.05f) {
            glm::vec4 beaconCol = w.color;
            beaconCol.a = beaconAlpha;
            drawAcousticBeacon(w.origin, beaconCol, 1.6f);
        }
    }
}
"""

# Find closing namespace Mercenary or end of file
idx_ns = rcpp.rfind("} // namespace Mercenary")
if idx_ns != -1:
    rcpp = rcpp[:idx_ns] + render_impl + "\n} // namespace Mercenary\n"
else:
    rcpp += render_impl

with open('src/engine/Renderer.cpp', 'w', encoding='utf-8') as f:
    f.write(rcpp)

print("Renderer.cpp updated successfully with Step 2!")

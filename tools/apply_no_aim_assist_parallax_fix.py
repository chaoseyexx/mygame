import re

# 1. Patch src/engine/Renderer.hpp
with open('src/engine/Renderer.hpp', 'r', encoding='utf-8') as f:
    ren_h = f.read()

target_h = "void drawAimReticle(const glm::vec3& worldPos, float bloomRadius, const glm::vec4& color);"
repl_h = "void drawAimReticle(const glm::vec3& worldPos, float bloomRadius, const glm::vec4& color, bool withTickMarks = true);"

if target_h in ren_h:
    ren_h = ren_h.replace(target_h, repl_h, 1)
    with open('src/engine/Renderer.hpp', 'w', encoding='utf-8', newline='') as f:
        f.write(ren_h)
    print("Patched Renderer.hpp successfully")
else:
    print("Renderer.hpp target not found")

# 2. Patch src/engine/Renderer.cpp
with open('src/engine/Renderer.cpp', 'r', encoding='utf-8') as f:
    ren_cpp = f.read()

target_cpp1 = "void Renderer::drawAimReticle(const glm::vec3& worldPos, float bloomRadius, const glm::vec4& color) {"
repl_cpp1 = "void Renderer::drawAimReticle(const glm::vec3& worldPos, float bloomRadius, const glm::vec4& color, bool withTickMarks) {"

target_cpp2 = """    // Four Cardinal Cross Tick Marks facing camera
    float tickStart = bloomRadius * 1.05f;
    float tickEnd   = bloomRadius * 1.45f;

    // Top tick
    glm::vec3 t0 = center + quadUp * tickStart;
    glm::vec3 t1 = center + quadUp * tickEnd;
    circleData.insert(circleData.end(), {t0.x, t0.y, t0.z, color.r, color.g, color.b, color.a});
    circleData.insert(circleData.end(), {t1.x, t1.y, t1.z, color.r, color.g, color.b, color.a});

    // Bottom tick
    glm::vec3 b0 = center - quadUp * tickStart;
    glm::vec3 b1 = center - quadUp * tickEnd;
    circleData.insert(circleData.end(), {b0.x, b0.y, b0.z, color.r, color.g, color.b, color.a});
    circleData.insert(circleData.end(), {b1.x, b1.y, b1.z, color.r, color.g, color.b, color.a});

    // Left tick
    glm::vec3 l0 = center - quadRight * tickStart;
    glm::vec3 l1 = center - quadRight * tickEnd;
    circleData.insert(circleData.end(), {l0.x, l0.y, l0.z, color.r, color.g, color.b, color.a});
    circleData.insert(circleData.end(), {l1.x, l1.y, l1.z, color.r, color.g, color.b, color.a});

    // Right tick
    glm::vec3 r0 = center + quadRight * tickStart;
    glm::vec3 r1 = center + quadRight * tickEnd;
    circleData.insert(circleData.end(), {r0.x, r0.y, r0.z, color.r, color.g, color.b, color.a});
    circleData.insert(circleData.end(), {r1.x, r1.y, r1.z, color.r, color.g, color.b, color.a});"""

repl_cpp2 = """    // Four Cardinal Cross Tick Marks facing camera (Only for actor / target confirmation, never on generic obstacles)
    if (withTickMarks) {
        float tickStart = bloomRadius * 1.05f;
        float tickEnd   = bloomRadius * 1.45f;

        // Top tick
        glm::vec3 t0 = center + quadUp * tickStart;
        glm::vec3 t1 = center + quadUp * tickEnd;
        circleData.insert(circleData.end(), {t0.x, t0.y, t0.z, color.r, color.g, color.b, color.a});
        circleData.insert(circleData.end(), {t1.x, t1.y, t1.z, color.r, color.g, color.b, color.a});

        // Bottom tick
        glm::vec3 b0 = center - quadUp * tickStart;
        glm::vec3 b1 = center - quadUp * tickEnd;
        circleData.insert(circleData.end(), {b0.x, b0.y, b0.z, color.r, color.g, color.b, color.a});
        circleData.insert(circleData.end(), {b1.x, b1.y, b1.z, color.r, color.g, color.b, color.a});

        // Left tick
        glm::vec3 l0 = center - quadRight * tickStart;
        glm::vec3 l1 = center - quadRight * tickEnd;
        circleData.insert(circleData.end(), {l0.x, l0.y, l0.z, color.r, color.g, color.b, color.a});
        circleData.insert(circleData.end(), {l1.x, l1.y, l1.z, color.r, color.g, color.b, color.a});

        // Right tick
        glm::vec3 r0 = center + quadRight * tickStart;
        glm::vec3 r1 = center + quadRight * tickEnd;
        circleData.insert(circleData.end(), {r0.x, r0.y, r0.z, color.r, color.g, color.b, color.a});
        circleData.insert(circleData.end(), {r1.x, r1.y, r1.z, color.r, color.g, color.b, color.a});
    }"""

target_cpp3 = """    // If ray hits an obstacle, cover, or target, render a subtle tactical 3D impact pip on the surface
    if (hitSurface) {
        glm::vec3 norm = (glm::length(hitNormal) > 0.1f) ? glm::normalize(hitNormal) : glm::vec3(0, 1, 0);
        glm::vec3 pipCenter = end + norm * 0.008f + jitterVec;
        float pipAlpha = std::clamp(0.85f - heat * 0.40f, 0.25f, 0.85f);
        glm::vec4 retColor = hitActor ? glm::vec4(1.0f, 0.32f, 0.32f, pipAlpha) : glm::vec4(0.85f, 0.88f, 0.92f, pipAlpha);
        drawAimReticle(pipCenter, 0.038f + spread * 0.030f + heat * 0.015f, retColor);
    }"""

repl_cpp3 = """    // If ray hits an obstacle, cover, or target, render a subtle tactical 3D impact pip on the surface
    if (hitSurface) {
        glm::vec3 norm = (glm::length(hitNormal) > 0.1f) ? glm::normalize(hitNormal) : glm::vec3(0, 1, 0);
        glm::vec3 pipCenter = end + norm * 0.008f + jitterVec;
        float pipAlpha = std::clamp(0.85f - heat * 0.40f, 0.25f, 0.85f);
        glm::vec4 retColor = hitActor ? glm::vec4(1.0f, 0.32f, 0.32f, pipAlpha) : glm::vec4(0.85f, 0.88f, 0.92f, pipAlpha);
        float pipRadius = hitActor ? (0.038f + spread * 0.030f + heat * 0.015f) : (0.018f + spread * 0.015f);
        drawAimReticle(pipCenter, pipRadius, retColor, hitActor);
    }"""

assert target_cpp1 in ren_cpp, "Renderer.cpp target 1 not found"
assert target_cpp2 in ren_cpp, "Renderer.cpp target 2 not found"
assert target_cpp3 in ren_cpp, "Renderer.cpp target 3 not found"

ren_cpp = ren_cpp.replace(target_cpp1, repl_cpp1, 1)
ren_cpp = ren_cpp.replace(target_cpp2, repl_cpp2, 1)
ren_cpp = ren_cpp.replace(target_cpp3, repl_cpp3, 1)

with open('src/engine/Renderer.cpp', 'w', encoding='utf-8', newline='') as f:
    f.write(ren_cpp)
print("Patched Renderer.cpp successfully")

# 3. Patch src/game/Player.cpp
with open('src/game/Player.cpp', 'r', encoding='utf-8') as f:
    player_cpp = f.read()

# Target in raycast evaluation
target_player1 = """    glm::vec3 adsAim = (glm::length(unifiedTarget - muzzle) > 0.001f)
        ? glm::normalize(unifiedTarget - muzzle)
        : gunFwd;"""

repl_player1 = """    glm::vec3 camDir = camera ? camera->getForwardVector() : glm::vec3(std::sin(m_yaw), 0.0f, std::cos(m_yaw));
    glm::vec3 camPos = camera ? camera->getPosition() : (m_position + glm::vec3(0.0f, 1.6f, 0.0f));

    // Smooth Battle-Zero Convergence: enforce minimum safe convergence distance (>= 35.0m) along camera view line
    // Eliminates near-field parallax collapse and prevents the weapon/laser from snapping onto close obstacles (crates, walls, barriers)
    float targetDist = glm::distance(camPos, unifiedTarget);
    float safeConvergenceDist = std::max(targetDist, 35.0f);
    glm::vec3 smoothAimTarget = camPos + camDir * safeConvergenceDist;

    glm::vec3 adsAim = (glm::length(smoothAimTarget - muzzle) > 0.001f)
        ? glm::normalize(smoothAimTarget - muzzle)
        : gunFwd;"""

# Target in weapon firing
target_player2 = """                glm::vec3 adsAim = (glm::length(unifiedTarget - muzzlePos) > 0.001f)
                    ? glm::normalize(unifiedTarget - muzzlePos)
                    : gunFwd;"""

repl_player2 = """                glm::vec3 camDir = camera ? camera->getForwardVector() : glm::vec3(std::sin(m_yaw), 0.0f, std::cos(m_yaw));
                glm::vec3 camPos = camera ? camera->getPosition() : (m_position + glm::vec3(0.0f, 1.6f, 0.0f));
                float targetDist = glm::distance(camPos, unifiedTarget);
                float safeConvergenceDist = std::max(targetDist, 35.0f);
                glm::vec3 smoothAimTarget = camPos + camDir * safeConvergenceDist;

                glm::vec3 adsAim = (glm::length(smoothAimTarget - muzzlePos) > 0.001f)
                    ? glm::normalize(smoothAimTarget - muzzlePos)
                    : gunFwd;"""

assert target_player1 in player_cpp, "Player.cpp target 1 not found"
assert target_player2 in player_cpp, "Player.cpp target 2 not found"

player_cpp = player_cpp.replace(target_player1, repl_player1, 1)
player_cpp = player_cpp.replace(target_player2, repl_player2, 1)

with open('src/game/Player.cpp', 'w', encoding='utf-8', newline='') as f:
    f.write(player_cpp)
print("Patched Player.cpp successfully")

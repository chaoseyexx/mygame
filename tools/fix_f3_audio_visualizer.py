import sys

print("Applying Comprehensive Fix for F3 Real-Time Acoustic Debug Visualizer...")

# -------------------------------------------------------------
# 1. Update Renderer.cpp (Fix uniform, depth test, and line thickness)
# -------------------------------------------------------------
with open('src/engine/Renderer.cpp', 'r', encoding='utf-8') as f:
    rcpp = f.read()

target_draw_ring = """void Renderer::drawAcousticWaveRing(const glm::vec3& center, float radius, const glm::vec4& color, int segments) {
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
}"""

assert target_draw_ring in rcpp, "Could not find target_draw_ring in Renderer.cpp"

replacement_draw_ring = """void Renderer::drawAcousticWaveRing(const glm::vec3& center, float radius, const glm::vec4& color, int segments) {
    if (radius < 0.2f || color.a <= 0.01f) return;
    const float step = 2.0f * 3.14159265f / static_cast<float>(segments);

    // Height offset slightly above floor to prevent z-fighting
    float y = center.y + 0.06f;

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
    m_lineShader->setMat4("uViewProj", m_viewProjMatrix);

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
    glDisable(GL_DEPTH_TEST); // Acoustic waves propagate through walls and terrain! Always visible!
    glDepthMask(GL_FALSE);
    glLineWidth(3.0f);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(segments * 2));

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
    glBindVertexArray(0);
}

void Renderer::drawAcousticBeacon(const glm::vec3& origin, const glm::vec4& color, float height) {
    if (color.a <= 0.02f) return;

    m_lineShader->bind();
    m_lineShader->setMat4("uViewProj", m_viewProjMatrix);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glLineWidth(3.0f);

    // Vertical locator line from ground to beacon height
    drawLine(origin + glm::vec3(0.0f, 0.02f, 0.0f), origin + glm::vec3(0.0f, height, 0.0f), color);

    // Subtle 4-tick crosshair at beacon top
    float tick = 0.35f;
    glm::vec3 top = origin + glm::vec3(0.0f, height, 0.0f);
    drawLine(top - glm::vec3(tick, 0.0f, 0.0f), top + glm::vec3(tick, 0.0f, 0.0f), color);
    drawLine(top - glm::vec3(0.0f, 0.0f, tick), top + glm::vec3(0.0f, 0.0f, tick), color);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glLineWidth(1.0f);
}"""

rcpp = rcpp.replace(target_draw_ring, replacement_draw_ring, 1)

with open('src/engine/Renderer.cpp', 'w', encoding='utf-8') as f:
    f.write(rcpp)

print("1. Renderer.cpp updated successfully.")

# -------------------------------------------------------------
# 2. Update Audio.cpp (Longer minimum wave duration for clear visual tracking)
# -------------------------------------------------------------
with open('src/engine/Audio.cpp', 'r', encoding='utf-8') as f:
    acpp = f.read()

target_record_wave = """    wave.speed = m_acousticWaveVisualSpeed;
    wave.lifetime = 0.0f;
    wave.maxLifetime = wave.maxRadius / wave.speed;"""

assert target_record_wave in acpp, "Could not find target_record_wave in Audio.cpp"

replacement_record_wave = """    wave.speed = m_acousticWaveVisualSpeed;
    wave.lifetime = 0.0f;
    // Guaranteed minimum visual duration of 2.0 seconds for footsteps, 3.5 seconds for gunfire!
    float minDuration = (type == SoundType::RifleShot || type == SoundType::SniperShot || type == SoundType::HeavyMGShot || type == SoundType::GrenadeExplosion) ? 3.5f : 2.0f;
    wave.maxLifetime = std::max(minDuration, wave.maxRadius / wave.speed);"""

acpp = acpp.replace(target_record_wave, replacement_record_wave, 1)

with open('src/engine/Audio.cpp', 'w', encoding='utf-8') as f:
    f.write(acpp)

print("2. Audio.cpp updated successfully.")

# -------------------------------------------------------------
# 3. Update UI.hpp (Add toast timer and toggle method)
# -------------------------------------------------------------
with open('src/game/UI.hpp', 'r', encoding='utf-8') as f:
    ui_hpp = f.read()

target_ui_toggle = """    bool m_audioDebugVisualizerEnabled = false;
    void toggleAudioDebugVisualizer() { m_audioDebugVisualizerEnabled = !m_audioDebugVisualizerEnabled; }"""

assert target_ui_toggle in ui_hpp, "Could not find target_ui_toggle in UI.hpp"

replacement_ui_toggle = """    bool m_audioDebugVisualizerEnabled = false;
    float m_audioVisualizerToastTimer = 0.0f;
    void toggleAudioDebugVisualizer() { 
        m_audioDebugVisualizerEnabled = !m_audioDebugVisualizerEnabled; 
        m_audioVisualizerToastTimer = 2.8f;
    }"""

ui_hpp = ui_hpp.replace(target_ui_toggle, replacement_ui_toggle, 1)

with open('src/game/UI.hpp', 'w', encoding='utf-8') as f:
    f.write(ui_hpp)

print("3. UI.hpp updated successfully.")

# -------------------------------------------------------------
# 4. Update UI.cpp (Ensure F3 visualizer renders in MainMenu, CharacterSelect, Deployment AND InGame)
# -------------------------------------------------------------
with open('src/game/UI.cpp', 'r', encoding='utf-8') as f:
    ui_cpp = f.read()

# In renderTacticalDeploymentScreen return
target_deploy_return = """    if (m_isDeploying) {
        renderTacticalDeploymentScreen(player, ProfileManager::get().getActiveProfile(), &world, frontline, renderer);
        return;
    }"""

assert target_deploy_return in ui_cpp, "Could not find target_deploy_return"

replacement_deploy_return = """    if (m_isDeploying) {
        renderTacticalDeploymentScreen(player, ProfileManager::get().getActiveProfile(), &world, frontline, renderer);
        if (m_audioDebugVisualizerEnabled && camera) {
            ImGuiIO& io = ImGui::GetIO();
            renderAcousticDebugVisualizer(player, *camera, aspectRatio, io.DisplaySize.x, io.DisplaySize.y);
        }
        return;
    }"""

ui_cpp = ui_cpp.replace(target_deploy_return, replacement_deploy_return, 1)

# In renderCharacterSelectScreen return
target_char_return = """    if (m_gameState == GameState::CharacterSelect) {
        renderCharacterSelectScreen(player);
        return;
    }"""

assert target_char_return in ui_cpp, "Could not find target_char_return"

replacement_char_return = """    if (m_gameState == GameState::CharacterSelect) {
        renderCharacterSelectScreen(player);
        if (m_audioDebugVisualizerEnabled && camera) {
            ImGuiIO& io = ImGui::GetIO();
            renderAcousticDebugVisualizer(player, *camera, aspectRatio, io.DisplaySize.x, io.DisplaySize.y);
        }
        return;
    }"""

ui_cpp = ui_cpp.replace(target_char_return, replacement_char_return, 1)

# In renderLobbyMainMenu return
target_menu_return = """    if (m_gameState == GameState::MainMenu) {
        renderLobbyMainMenu(player, world, missions);
        if (m_serverOfflineModalOpen) {
            renderServerOfflineModal(player, ProfileManager::get().getActiveProfile());
        }
        return;
    }"""

assert target_menu_return in ui_cpp, "Could not find target_menu_return"

replacement_menu_return = """    if (m_gameState == GameState::MainMenu) {
        renderLobbyMainMenu(player, world, missions);
        if (m_serverOfflineModalOpen) {
            renderServerOfflineModal(player, ProfileManager::get().getActiveProfile());
        }
        if (m_audioDebugVisualizerEnabled && camera) {
            ImGuiIO& io = ImGui::GetIO();
            renderAcousticDebugVisualizer(player, *camera, aspectRatio, io.DisplaySize.x, io.DisplaySize.y);
        }
        return;
    }"""

ui_cpp = ui_cpp.replace(target_menu_return, replacement_menu_return, 1)

with open('src/game/UI.cpp', 'w', encoding='utf-8') as f:
    f.write(ui_cpp)

print("4. UI.cpp updated successfully.")

# -------------------------------------------------------------
# 5. Update UIMenu.cpp (Add [F3 AUDIO: ON/OFF] clickable button to top navigation bar)
# -------------------------------------------------------------
with open('src/game/UIMenu.cpp', 'r', encoding='utf-8') as f:
    menu_cpp = f.read()

target_nav_right = """        // 3. RIGHT SECTION: Currencies & Quick Utility Actions
        float rightX = io.DisplaySize.x - 380.0f;"""

assert target_nav_right in menu_cpp, "Could not find target_nav_right in UIMenu.cpp"

replacement_nav_right = """        // 3. RIGHT SECTION: Currencies & Quick Utility Actions
        float rightX = io.DisplaySize.x - 380.0f;

        // F3 Acoustic Sensor Visualizer Toggle Pill Button
        {
            float sBtnX = rightX - 165.0f;
            ImVec2 sBtnMin(sBtnX, 13);
            ImVec2 sBtnMax(sBtnX + 155, 41);
            ImGui::SetCursorScreenPos(sBtnMin);
            ImGui::InvisibleButton("##F3SensorNavbarToggle", ImVec2(155, 28));
            bool sHover = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked()) {
                toggleAudioDebugVisualizer();
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImU32 sBgCol = m_audioDebugVisualizerEnabled ? IM_COL32(10, 42, 54, 230) : (sHover ? IM_COL32(24, 30, 40, 220) : IM_COL32(16, 20, 26, 200));
            ImU32 sBorderCol = m_audioDebugVisualizerEnabled ? IM_COL32(0, 235, 255, 240) : (sHover ? IM_COL32(80, 100, 120, 200) : IM_COL32(45, 55, 70, 180));
            dl->AddRectFilled(sBtnMin, sBtnMax, sBgCol, 4.0f);
            dl->AddRect(sBtnMin, sBtnMax, sBorderCol, 4.0f, 0, 1.4f);
            const char* sLabel = m_audioDebugVisualizerEnabled ? "[F3 AUDIO: ON]" : "[F3 AUDIO: OFF]";
            ImU32 sTxtCol = m_audioDebugVisualizerEnabled ? IM_COL32(0, 245, 255, 255) : IM_COL32(155, 170, 185, 220);
            dl->AddText(ImVec2(sBtnX + 18.0f, 18), sTxtCol, sLabel);
        }"""

menu_cpp = menu_cpp.replace(target_nav_right, replacement_nav_right, 1)

with open('src/game/UIMenu.cpp', 'w', encoding='utf-8') as f:
    f.write(menu_cpp)

print("5. UIMenu.cpp updated successfully.")

# -------------------------------------------------------------
# 6. Update UIHUD.cpp (Add instant Screen Toast notification and test buttons in F3 window)
# -------------------------------------------------------------
with open('src/game/UIHUD.cpp', 'r', encoding='utf-8') as f:
    hud_cpp = f.read()

# Add Toast Notification at the beginning of renderAcousticDebugVisualizer
target_hud_start = """    AudioEngine& audio = AudioEngine::get();
    std::vector<AcousticWaveEvent> activeWaves = audio.getActiveAcousticWaves();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    glm::vec3 playerPos = player.getPosition();
    float cameraYawRad = glm::radians(camera.getYaw());"""

assert target_hud_start in hud_cpp, "Could not find target_hud_start in UIHUD.cpp"

replacement_hud_start = """    AudioEngine& audio = AudioEngine::get();
    std::vector<AcousticWaveEvent> activeWaves = audio.getActiveAcousticWaves();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    glm::vec3 playerPos = player.getPosition();
    float cameraYawRad = glm::radians(camera.getYaw());

    // Big Visual Toast Notification when F3 is toggled
    if (m_audioVisualizerToastTimer > 0.0f) {
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 0.016f;
        m_audioVisualizerToastTimer -= dt;

        float alphaFrac = std::clamp(m_audioVisualizerToastTimer / 0.5f, 0.0f, 1.0f);
        uint32_t aByte = static_cast<uint32_t>(alphaFrac * 255.0f);

        float tW = 440.0f;
        float tH = 34.0f;
        float tX = (screenW - tW) * 0.5f;
        float tY = 82.0f;

        dl->AddRectFilled(ImVec2(tX, tY), ImVec2(tX + tW, tY + tH), IM_COL32(10, 16, 24, static_cast<uint32_t>(aByte * 0.92f)), 6.0f);
        dl->AddRect(ImVec2(tX, tY), ImVec2(tX + tW, tY + tH), IM_COL32(0, 230, 255, aByte), 6.0f, 0, 1.6f);
        dl->AddText(ImVec2(tX + 22.0f, tY + 8.0f), IM_COL32(0, 245, 255, aByte), "ACOUSTIC SENSOR & 3D PROPAGATION: ENABLED [F3]");
    }"""

hud_cpp = hud_cpp.replace(target_hud_start, replacement_hud_start, 1)

# Add Test Sound buttons in the F3 Telemetry window
target_f3_sliders = """        if (ImGui::Button("Reset Visual (80 m/s)")) {
            audio.setAcousticVisualSpeed(80.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("True Physical (343 m/s)")) {
            audio.setAcousticVisualSpeed(343.0f);
        }"""

assert target_f3_sliders in hud_cpp, "Could not find target_f3_sliders in UIHUD.cpp"

replacement_f3_sliders = """        if (ImGui::Button("Visual (65 m/s)")) {
            audio.setAcousticVisualSpeed(65.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Fast (120 m/s)")) {
            audio.setAcousticVisualSpeed(120.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Physical (343 m/s)")) {
            audio.setAcousticVisualSpeed(343.0f);
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.2f, 0.95f, 1.0f, 1.0f), "Instant Sound Wave Tests:");
        if (ImGui::Button("▶ Test Footstep (Cyan Ripple)")) {
            audio.recordAcousticWave(SoundType::FootstepConcrete, playerPos, 0.75f, 28.0f, AcousticEnvironment::OpenField, true);
            audio.playSound(SoundType::FootstepConcrete, 0.65f);
        }
        ImGui::SameLine();
        if (ImGui::Button("▶ Test Gunfire (Red Shockwave)")) {
            audio.recordAcousticWave(SoundType::RifleShot, playerPos, 1.0f, 180.0f, AcousticEnvironment::OpenField, true);
            audio.playSound(SoundType::RifleShot, 0.85f);
        }"""

hud_cpp = hud_cpp.replace(target_f3_sliders, replacement_f3_sliders, 1)

with open('src/game/UIHUD.cpp', 'w', encoding='utf-8') as f:
    f.write(hud_cpp)

print("6. UIHUD.cpp updated successfully.")

# -------------------------------------------------------------
# 7. Update GameApp.cpp (Listener updated before player.update + Support F3, F2, and O keys)
# -------------------------------------------------------------
with open('src/game/GameApp.cpp', 'r', encoding='utf-8') as f:
    app_cpp = f.read()

# Make sure listener is updated BEFORE player.update
target_update_order = """            player.update(gameDt, window, mouseWorldPos, world, ballistics, camera.getYaw(), &camera, &aimInfo, &frontline.getBots());
            AudioEngine::get().setListener(player.getPosition(), glm::radians(camera.getYaw()));
            AudioEngine::get().updateAcoustics(gameDt);"""

assert target_update_order in app_cpp, "Could not find target_update_order in GameApp.cpp"

replacement_update_order = """            AudioEngine::get().setListener(player.getPosition(), glm::radians(camera.getYaw()));
            AudioEngine::get().updateAcoustics(gameDt);
            player.update(gameDt, window, mouseWorldPos, world, ballistics, camera.getYaw(), &camera, &aimInfo, &frontline.getBots());"""

app_cpp = app_cpp.replace(target_update_order, replacement_update_order, 1)

# Add fallback keys: F3 OR F2 OR O (to bypass laptop Fn-lock)
target_f3_key = """        // F3: Toggle Real-Time Acoustic & Sensor Debug Visualizer
        static bool f3PressedLast = false;
        bool f3Pressed = window.isKeyPressed(GLFW_KEY_F3);
        if (f3Pressed && !f3PressedLast) {
            ui.toggleAudioDebugVisualizer();
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        f3PressedLast = f3Pressed;"""

assert target_f3_key in app_cpp, "Could not find target_f3_key in GameApp.cpp"

replacement_f3_key = """        // F3 (or F2 / O fallback if laptop Fn-lock is on): Toggle Real-Time Acoustic & Sensor Debug Visualizer
        static bool f3PressedLast = false;
        bool f3Pressed = window.isKeyPressed(GLFW_KEY_F3) || window.isKeyPressed(GLFW_KEY_F2) || (!ChatSystem::get().isChatOpen() && window.isKeyPressed(GLFW_KEY_O));
        if (f3Pressed && !f3PressedLast) {
            ui.toggleAudioDebugVisualizer();
            AudioEngine::get().playSound(SoundType::TacticalClick);
            std::cout << "[Acoustic Visualizer] Toggled: " << (ui.isAudioDebugVisualizerEnabled() ? "ENABLED" : "DISABLED") << std::endl;
        }
        f3PressedLast = f3Pressed;"""

app_cpp = app_cpp.replace(target_f3_key, replacement_f3_key, 1)

with open('src/game/GameApp.cpp', 'w', encoding='utf-8') as f:
    f.write(app_cpp)

print("7. GameApp.cpp updated successfully.")
print("All fixes applied successfully!")

import sys

print("Patching UI.cpp and GameApp.cpp for F3 Wiring (Step 4)...")

# 1. Update UI.cpp
with open('src/game/UI.cpp', 'r', encoding='utf-8') as f:
    ui_cpp = f.read()

target_ui = "renderTacticalPings(*camera, aspectRatio, player);"
assert target_ui in ui_cpp, "Could not find target_ui in UI.cpp"

replacement_ui = """renderTacticalPings(*camera, aspectRatio, player);
        if (m_audioDebugVisualizerEnabled) {
            ImGuiIO& io = ImGui::GetIO();
            renderAcousticDebugVisualizer(player, *camera, aspectRatio, io.DisplaySize.x, io.DisplaySize.y);
        }"""

ui_cpp = ui_cpp.replace(target_ui, replacement_ui, 1)

with open('src/game/UI.cpp', 'w', encoding='utf-8') as f:
    f.write(ui_cpp)

print("UI.cpp updated successfully.")

# 2. Update GameApp.cpp
with open('src/game/GameApp.cpp', 'r', encoding='utf-8') as f:
    app_cpp = f.read()

# Add F3 key detection
target_f11 = """        // F11 or Alt+Enter: Seamless Fullscreen / Windowed Toggle
        static bool f11PressedLast = false;
        bool altEnter = (window.isKeyPressed(GLFW_KEY_LEFT_ALT) || window.isKeyPressed(GLFW_KEY_RIGHT_ALT)) && window.isKeyPressed(GLFW_KEY_ENTER);
        bool f11Pressed = window.isKeyPressed(GLFW_KEY_F11) || altEnter;
        if (f11Pressed && !f11PressedLast) {
            window.toggleFullscreen();
            ui.setWindowMode(window.getDisplayMode() == WindowDisplayMode::ExclusiveFullscreen ? 1 : 0);
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        f11PressedLast = f11Pressed;"""

assert target_f11 in app_cpp, "Could not find target_f11 in GameApp.cpp"

replacement_f11_f3 = target_f11 + """

        // F3: Toggle Real-Time Acoustic & Sensor Debug Visualizer
        static bool f3PressedLast = false;
        bool f3Pressed = window.isKeyPressed(GLFW_KEY_F3);
        if (f3Pressed && !f3PressedLast) {
            ui.toggleAudioDebugVisualizer();
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        f3PressedLast = f3Pressed;"""

app_cpp = app_cpp.replace(target_f11, replacement_f11_f3, 1)

# Add listener update & acoustic update
target_player_update = "player.update(gameDt, window, mouseWorldPos, world, ballistics, camera.getYaw(), &camera, &aimInfo, &frontline.getBots());"
assert target_player_update in app_cpp, "Could not find target_player_update in GameApp.cpp"

replacement_player_update = """player.update(gameDt, window, mouseWorldPos, world, ballistics, camera.getYaw(), &camera, &aimInfo, &frontline.getBots());
            AudioEngine::get().setListener(player.getPosition(), glm::radians(camera.getYaw()));
            AudioEngine::get().updateAcoustics(gameDt);"""

app_cpp = app_cpp.replace(target_player_update, replacement_player_update, 1)

# Add 3D wave rendering before renderer.endFrame()
target_render_end = """        ballistics.render(renderer);
        renderer.endFrame();"""

assert target_render_end in app_cpp, "Could not find target_render_end in GameApp.cpp"

replacement_render_end = """        ballistics.render(renderer);
        if (ui.isAudioDebugVisualizerEnabled()) {
            renderer.renderAcousticWaves(AudioEngine::get().getActiveAcousticWaves());
        }
        renderer.endFrame();"""

app_cpp = app_cpp.replace(target_render_end, replacement_render_end, 1)

with open('src/game/GameApp.cpp', 'w', encoding='utf-8') as f:
    f.write(app_cpp)

print("GameApp.cpp updated successfully with Step 4!")

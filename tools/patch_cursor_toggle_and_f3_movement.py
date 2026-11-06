import sys

print("Applying fix for F3 movement lock and adding U keybind for cursor toggle...")

# -------------------------------------------------------------
# 1. Update Player.cpp to prevent ImGui overlays from blocking movement when cursor is captured
# -------------------------------------------------------------
with open('src/game/Player.cpp', 'r', encoding='utf-8') as f:
    player_cpp = f.read()

target_capture = """    bool isChatOpen = ChatSystem::get().isChatOpen();
    bool captureKb = isChatOpen || (ImGui::GetCurrentContext() && (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput));
    bool uiWantsMouse = isChatOpen || (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse);"""

assert target_capture in player_cpp, "Could not find target_capture in Player.cpp"

replacement_capture = """    bool isChatOpen = ChatSystem::get().isChatOpen();
    // Only capture keyboard if chat is active, text input is active, or cursor is free (not in-game aim mode)
    bool captureKb = isChatOpen || (ImGui::GetCurrentContext() && (ImGui::GetIO().WantTextInput || (ImGui::GetIO().WantCaptureKeyboard && !window.isCursorCaptured())));
    bool uiWantsMouse = isChatOpen || (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse && !window.isCursorCaptured());"""

player_cpp = player_cpp.replace(target_capture, replacement_capture, 1)

with open('src/game/Player.cpp', 'w', encoding='utf-8') as f:
    f.write(player_cpp)

print("1. Player.cpp updated successfully.")

# -------------------------------------------------------------
# 2. Update UI.hpp with cursor freed tracking
# -------------------------------------------------------------
with open('src/game/UI.hpp', 'r', encoding='utf-8') as f:
    ui_hpp = f.read()

target_ui_hpp = "    bool isAudioDebugVisualizerEnabled() const { return m_audioDebugVisualizerEnabled; }"
assert target_ui_hpp in ui_hpp, "Could not find target_ui_hpp in UI.hpp"

replacement_ui_hpp = """    bool isAudioDebugVisualizerEnabled() const { return m_audioDebugVisualizerEnabled; }
    bool m_cursorFreed = false;
    void setCursorFreed(bool freed) { m_cursorFreed = freed; }
    bool isCursorFreed() const { return m_cursorFreed; }"""

ui_hpp = ui_hpp.replace(target_ui_hpp, replacement_ui_hpp, 1)

with open('src/game/UI.hpp', 'w', encoding='utf-8') as f:
    f.write(ui_hpp)

print("2. UI.hpp updated successfully.")

# -------------------------------------------------------------
# 3. Update GameApp.cpp for 'U' keybind to toggle cursor mode
# -------------------------------------------------------------
with open('src/game/GameApp.cpp', 'r', encoding='utf-8') as f:
    app_cpp = f.read()

# Replace chat U binding with dedicated Cursor Toggle
target_chat_u = """        static bool enterPressedLast = false;
        static bool yPressedLast = false;
        static bool uPressedLast = false;
        bool enterPressed = window.isKeyPressed(GLFW_KEY_ENTER);
        bool yPressed = window.isKeyPressed(GLFW_KEY_Y);
        bool uPressed = window.isKeyPressed(GLFW_KEY_U);
        bool chatAlreadyOpen = ChatSystem::get().isChatOpen();
        if (!altEnter && ui.getGameState() == GameState::InGame && !ui.isOptionsOpen() && !ui.isEscapeMenuOpen() && !ui.isInventoryOpen()) {
            if (!chatAlreadyOpen) {
                if (enterPressed && !enterPressedLast) {
                    ChatSystem::get().openChat();
                } else if (yPressed && !yPressedLast) {
                    ChatSystem::get().setActiveChannel(ChatChannel::Global);
                    ChatSystem::get().openChat();
                } else if (uPressed && !uPressedLast) {
                    ChatSystem::get().setActiveChannel(ChatChannel::Team);
                    ChatSystem::get().openChat();
                }
            }
        }
        enterPressedLast = enterPressed;
        yPressedLast = yPressed;
        uPressedLast = uPressed;"""

assert target_chat_u in app_cpp, "Could not find target_chat_u in GameApp.cpp"

replacement_chat_u = """        static bool enterPressedLast = false;
        static bool yPressedLast = false;
        bool enterPressed = window.isKeyPressed(GLFW_KEY_ENTER);
        bool yPressed = window.isKeyPressed(GLFW_KEY_Y);
        bool chatAlreadyOpen = ChatSystem::get().isChatOpen();
        if (!altEnter && ui.getGameState() == GameState::InGame && !ui.isOptionsOpen() && !ui.isEscapeMenuOpen() && !ui.isInventoryOpen()) {
            if (!chatAlreadyOpen) {
                if (enterPressed && !enterPressedLast) {
                    ChatSystem::get().openChat();
                } else if (yPressed && !yPressedLast) {
                    ChatSystem::get().setActiveChannel(ChatChannel::Global);
                    ChatSystem::get().openChat();
                }
            }
        }
        enterPressedLast = enterPressed;
        yPressedLast = yPressed;

        // U Key: Dedicated Toggle for Free Cursor (UI Interaction vs Aim Lock)
        static bool s_manualCursorFree = false;
        static bool uPressedLast = false;
        bool uPressed = window.isKeyPressed(GLFW_KEY_U);
        if (uPressed && !uPressedLast && !chatAlreadyOpen && ui.getGameState() == GameState::InGame) {
            s_manualCursorFree = !s_manualCursorFree;
            ui.setCursorFreed(s_manualCursorFree);
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        uPressedLast = uPressed;"""

app_cpp = app_cpp.replace(target_chat_u, replacement_chat_u, 1)

# Update cursor capture logic in third-person camera section
target_cursor_capture = """            bool inMenu = ui.isOptionsOpen() || ui.isEscapeMenuOpen() || ui.isInventoryOpen() || 
                          ui.isMapOpen() || ui.isWeaponWheelOpen() || ChatSystem::get().isChatOpen() ||
                          (ui.getGameState() != GameState::InGame);

            if (inMenu || ui.isAlertWheelOpen() || ui.isWeaponWheelOpen()) {
                window.setCursorCaptured(false);
            } else {
                window.setCursorCaptured(true);
            }"""

assert target_cursor_capture in app_cpp, "Could not find target_cursor_capture in GameApp.cpp"

replacement_cursor_capture = """            bool inMenu = ui.isOptionsOpen() || ui.isEscapeMenuOpen() || ui.isInventoryOpen() || 
                          ui.isMapOpen() || ui.isWeaponWheelOpen() || ChatSystem::get().isChatOpen() ||
                          (ui.getGameState() != GameState::InGame);

            // Free cursor if in menu, or if manually toggled with 'U'
            if (inMenu || ui.isAlertWheelOpen() || ui.isWeaponWheelOpen() || s_manualCursorFree) {
                window.setCursorCaptured(false);
            } else {
                window.setCursorCaptured(true);
            }"""

app_cpp = app_cpp.replace(target_cursor_capture, replacement_cursor_capture, 1)

with open('src/game/GameApp.cpp', 'w', encoding='utf-8') as f:
    f.write(app_cpp)

print("3. GameApp.cpp updated successfully.")

# -------------------------------------------------------------
# 4. Update UIHUD.cpp to set non-stealing flags on F3 window and render HUD pill
# -------------------------------------------------------------
with open('src/game/UIHUD.cpp', 'r', encoding='utf-8') as f:
    hud_cpp = f.read()

target_f3_window = """    ImGui::SetNextWindowPos(ImVec2(24.0f, 100.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 320.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Tactical Acoustic Telemetry [F3]", &m_audioDebugVisualizerEnabled, ImGuiWindowFlags_NoCollapse)) {"""

assert target_f3_window in hud_cpp, "Could not find target_f3_window in UIHUD.cpp"

replacement_f3_window = """    // On-screen notification when Cursor is manually freed with U
    if (m_cursorFreed) {
        float bW = 280.0f;
        float bH = 26.0f;
        float bX = (screenW - bW) * 0.5f;
        float bY = 52.0f;
        dl->AddRectFilled(ImVec2(bX, bY), ImVec2(bX + bW, bY + bH), IM_COL32(14, 18, 26, 225), 4.0f);
        dl->AddRect(ImVec2(bX, bY), ImVec2(bX + bW, bY + bH), IM_COL32(255, 205, 45, 240), 4.0f, 0, 1.4f);
        dl->AddText(ImVec2(bX + 16.0f, bY + 5.0f), IM_COL32(255, 225, 75, 255), "CURSOR FREED [U] - Press U to Lock Aim");
    }

    ImGui::SetNextWindowPos(ImVec2(24.0f, 100.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 320.0f), ImGuiCond_FirstUseEver);

    // Prevent F3 window from stealing keyboard focus from movement unless cursor is freed
    ImGuiWindowFlags f3Flags = ImGuiWindowFlags_NoCollapse;
    if (!m_cursorFreed) {
        f3Flags |= ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavInputs;
    }

    if (ImGui::Begin("Tactical Acoustic Telemetry [F3]", &m_audioDebugVisualizerEnabled, f3Flags)) {
        if (!m_cursorFreed) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "[Press 'U' to Unlock Cursor for UI]");
        } else {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "[CURSOR UNLOCKED - Press 'U' to Lock Aim]");
        }
        ImGui::Separator();"""

hud_cpp = hud_cpp.replace(target_f3_window, replacement_f3_window, 1)

with open('src/game/UIHUD.cpp', 'w', encoding='utf-8') as f:
    f.write(hud_cpp)

print("4. UIHUD.cpp updated successfully.")
print("All patches applied successfully!")

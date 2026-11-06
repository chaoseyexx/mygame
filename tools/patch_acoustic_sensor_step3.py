import sys

print("Patching UI.hpp and UIHUD.cpp for Tactical Acoustic Sensor HUD (Step 3)...")

with open('src/game/UI.hpp', 'r', encoding='utf-8') as f:
    ui_hpp = f.read()

# Add F3 visualizer members to UIManager in UI.hpp
target_pings = "    const std::vector<TacticalPing>& getTacticalPings() const { return m_tacticalPings; }"
assert target_pings in ui_hpp, "Could not find target_pings in UI.hpp"

f3_decl = """    const std::vector<TacticalPing>& getTacticalPings() const { return m_tacticalPings; }

    // F3 Real-Time Acoustic & Sensor Debug Visualizer
    bool m_audioDebugVisualizerEnabled = false;
    void toggleAudioDebugVisualizer() { m_audioDebugVisualizerEnabled = !m_audioDebugVisualizerEnabled; }
    bool isAudioDebugVisualizerEnabled() const { return m_audioDebugVisualizerEnabled; }
    void setAudioDebugVisualizerEnabled(bool val) { m_audioDebugVisualizerEnabled = val; }
    void renderAcousticDebugVisualizer(const Player& player, const Camera& camera, float aspectRatio, float screenW, float screenH);"""

ui_hpp = ui_hpp.replace(target_pings, f3_decl, 1)

with open('src/game/UI.hpp', 'w', encoding='utf-8') as f:
    f.write(ui_hpp)

print("UI.hpp updated successfully.")

# Add implementation to UIHUD.cpp
with open('src/game/UIHUD.cpp', 'r', encoding='utf-8') as f:
    hud_cpp = f.read()

acoustic_hud_impl = """
void UIManager::renderAcousticDebugVisualizer(const Player& player, const Camera& camera, float aspectRatio, float screenW, float screenH) {
    if (!m_audioDebugVisualizerEnabled) return;

    AudioEngine& audio = AudioEngine::get();
    std::vector<AcousticWaveEvent> activeWaves = audio.getActiveAcousticWaves();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    glm::vec3 playerPos = player.getPosition();
    float cameraYawRad = glm::radians(camera.getYaw());

    // =========================================================================
    // 1. TACTICAL BOOMERANG POLAR ACOUSTIC RADAR (Top-Right HUD)
    // =========================================================================
    const float radarRadius = 88.0f;
    const ImVec2 radarCenter(screenW - radarRadius - 28.0f, radarRadius + 28.0f);

    // Radar Dark Outer Glass & Tactical Frame
    dl->AddCircleFilled(radarCenter, radarRadius + 4.0f, IM_COL32(10, 14, 22, 235));
    dl->AddCircle(radarCenter, radarRadius + 4.0f, IM_COL32(35, 180, 240, 220), 0, 2.0f);

    // Subtle Grid Range Rings: 15m, 45m, 90m, 150m
    float ringFracs[] = { 15.0f / 150.0f, 45.0f / 150.0f, 90.0f / 150.0f, 1.0f };
    for (float frac : ringFracs) {
        dl->AddCircle(radarCenter, radarRadius * frac, IM_COL32(35, 120, 170, 70), 0, 1.0f);
    }

    // Crosshair Axis Lines
    dl->AddLine(ImVec2(radarCenter.x - radarRadius, radarCenter.y), ImVec2(radarCenter.x + radarRadius, radarCenter.y), IM_COL32(35, 140, 190, 80), 1.0f);
    dl->AddLine(ImVec2(radarCenter.x, radarCenter.y - radarRadius), ImVec2(radarCenter.x, radarCenter.y + radarRadius), IM_COL32(35, 140, 190, 80), 1.0f);

    // Cardinal Compass Indicators
    dl->AddText(ImVec2(radarCenter.x - 4.0f, radarCenter.y - radarRadius + 3.0f), IM_COL32(80, 220, 255, 230), "N");
    dl->AddText(ImVec2(radarCenter.x - 4.0f, radarCenter.y + radarRadius - 16.0f), IM_COL32(60, 160, 200, 180), "S");
    dl->AddText(ImVec2(radarCenter.x + radarRadius - 14.0f, radarCenter.y - 7.0f), IM_COL32(60, 160, 200, 180), "E");
    dl->AddText(ImVec2(radarCenter.x - radarRadius + 4.0f, radarCenter.y - 7.0f), IM_COL32(60, 160, 200, 180), "W");

    // Center Player Operator Diamond
    dl->AddQuadFilled(
        ImVec2(radarCenter.x, radarCenter.y - 5.0f),
        ImVec2(radarCenter.x + 4.0f, radarCenter.y),
        ImVec2(radarCenter.x, radarCenter.y + 5.0f),
        ImVec2(radarCenter.x - 4.0f, radarCenter.y),
        IM_COL32(0, 255, 220, 255)
    );

    // Radar Header Badge
    dl->AddText(ImVec2(radarCenter.x - 70.0f, radarCenter.y - radarRadius - 22.0f), IM_COL32(0, 235, 255, 240), "[ACOUSTIC SENSOR F3]");

    // Track dynamic peak Sound Pressure Level (SPL) at player position
    float maxSplDb = 35.0f; // Ambient baseline
    float cosY = std::cos(-cameraYawRad);
    float sinY = std::sin(-cameraYawRad);

    // Plot Active Sound Waves on Radar
    for (const auto& wave : activeWaves) {
        glm::vec3 rel = wave.origin - playerPos;
        float dist = glm::length(rel);

        // Physical Decibel Calculation at listener position
        float spl = wave.initialDb - 20.0f * std::log10(std::max(dist, 1.0f)) - 0.005f * dist;
        if (spl > maxSplDb) maxSplDb = spl;

        // Rotate relative position to camera forward frame (UP = Forward)
        float localX = rel.x * cosY - rel.z * sinY;
        float localZ = rel.x * sinY + rel.z * cosY;

        float normDist = std::clamp(dist / 150.0f, 0.0f, 1.0f);
        float rDist = normDist * radarRadius;
        float angle = std::atan2(localX, localZ);

        ImVec2 blipPos(radarCenter.x + std::sin(angle) * rDist, radarCenter.y - std::cos(angle) * rDist);

        uint32_t aByte = static_cast<uint32_t>(std::clamp(1.0f - (wave.lifetime / wave.maxLifetime), 0.1f, 1.0f) * 255.0f);
        ImU32 blipCol = IM_COL32(
            static_cast<int>(wave.color.r * 255.0f),
            static_cast<int>(wave.color.g * 255.0f),
            static_cast<int>(wave.color.b * 255.0f),
            aByte
        );

        // Blip dot
        dl->AddCircleFilled(blipPos, 4.0f, blipCol);

        // Expanding radar ring pulse around blip
        float pulseFrac = fmodf(wave.lifetime * 2.5f, 1.0f);
        dl->AddCircle(blipPos, 4.0f + pulseFrac * 12.0f, (blipCol & 0x00FFFFFF) | (static_cast<uint32_t>((1.0f - pulseFrac) * aByte * 0.7f) << 24), 0, 1.2f);

        // Directional Threat Arrow on radar rim if sound is distant gunfire/explosion
        if (dist > 150.0f && (wave.soundType == SoundType::RifleShot || wave.soundType == SoundType::SniperShot || wave.soundType == SoundType::HeavyMGShot || wave.soundType == SoundType::GrenadeExplosion)) {
            ImVec2 rimPos(radarCenter.x + std::sin(angle) * (radarRadius - 2.0f), radarCenter.y - std::cos(angle) * (radarRadius - 2.0f));
            dl->AddTriangleFilled(
                rimPos,
                ImVec2(rimPos.x - std::cos(angle) * 5.0f - std::sin(angle) * 7.0f, rimPos.y - std::sin(angle) * 5.0f + std::cos(angle) * 7.0f),
                ImVec2(rimPos.x + std::cos(angle) * 5.0f - std::sin(angle) * 7.0f, rimPos.y + std::sin(angle) * 5.0f + std::cos(angle) * 7.0f),
                IM_COL32(255, 45, 30, aByte)
            );
        }
    }

    // Dynamic SPL Meter Under Radar
    char splBuf[64];
    snprintf(splBuf, sizeof(splBuf), "SPL: %.1f dB", maxSplDb);
    ImU32 splColor = (maxSplDb > 100.0f) ? IM_COL32(255, 60, 40, 255) : (maxSplDb > 75.0f) ? IM_COL32(255, 200, 30, 255) : IM_COL32(80, 230, 160, 240);
    dl->AddText(ImVec2(radarCenter.x - 35.0f, radarCenter.y + radarRadius + 8.0f), splColor, splBuf);

    // 10-Segment Audio Pressure LED Bar
    float barW = 120.0f;
    float barH = 5.0f;
    float barX = radarCenter.x - barW * 0.5f;
    float barY = radarCenter.y + radarRadius + 26.0f;
    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(18, 22, 30, 200));

    float splFrac = std::clamp((maxSplDb - 40.0f) / 90.0f, 0.0f, 1.0f);
    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * splFrac, barY + barH), splColor);

    // =========================================================================
    // 2. 3D-TO-2D PROJECTED FLOATING SOUND ORIGIN PINS
    // =========================================================================
    for (const auto& wave : activeWaves) {
        float dist = glm::distance(playerPos, wave.origin);
        if (dist > 160.0f) continue;

        // Project origin marker coordinate to 2D screen
        glm::vec3 beaconTop = wave.origin + glm::vec3(0.0f, 1.35f, 0.0f);
        glm::vec2 projected(0.0f);
        bool onScreen = camera.worldToScreen(beaconTop, aspectRatio, screenW, screenH, projected);

        bool inBounds = onScreen &&
                        projected.x >= 70.0f && projected.x <= screenW - 70.0f &&
                        projected.y >= 50.0f && projected.y <= screenH - 50.0f;

        if (inBounds) {
            float fadeFrac = std::clamp(1.0f - (wave.lifetime / 1.4f), 0.0f, 1.0f);
            if (fadeFrac <= 0.05f) continue;

            uint32_t aTag = static_cast<uint32_t>(fadeFrac * 255.0f);
            ImU32 tagColor = IM_COL32(
                static_cast<int>(wave.color.r * 255.0f),
                static_cast<int>(wave.color.g * 255.0f),
                static_cast<int>(wave.color.b * 255.0f),
                aTag
            );

            // Ground origin pulsing ring
            glm::vec2 groundProjected(0.0f);
            if (camera.worldToScreen(wave.origin + glm::vec3(0.0f, 0.05f, 0.0f), aspectRatio, screenW, screenH, groundProjected)) {
                dl->AddLine(ImVec2(groundProjected.x, groundProjected.y), ImVec2(projected.x, projected.y), (tagColor & 0x00FFFFFF) | (static_cast<uint32_t>(aTag * 0.45f) << 24), 1.2f);
                dl->AddCircleFilled(ImVec2(groundProjected.x, groundProjected.y), 3.5f, tagColor);
            }

            // Build Classification Badge Text
            const char* sndName = getSoundTypeName(wave.soundType);
            float splAtDist = wave.initialDb - 20.0f * std::log10(std::max(dist, 1.0f));
            char badgeLine1[64];
            char badgeLine2[64];
            snprintf(badgeLine1, sizeof(badgeLine1), "%s", sndName);
            snprintf(badgeLine2, sizeof(badgeLine2), "%.0f dB  |  %.1fm", splAtDist, dist);

            ImVec2 textSize1 = ImGui::CalcTextSize(badgeLine1);
            ImVec2 textSize2 = ImGui::CalcTextSize(badgeLine2);
            float badgeW = std::max(textSize1.x, textSize2.x) + 16.0f;
            float badgeH = textSize1.y + textSize2.y + 10.0f;

            ImVec2 bMin(projected.x - badgeW * 0.5f, projected.y - badgeH - 6.0f);
            ImVec2 bMax(projected.x + badgeW * 0.5f, projected.y - 6.0f);

            // Badge Background and Frame
            dl->AddRectFilled(bMin, bMax, IM_COL32(12, 16, 24, static_cast<uint32_t>(aTag * 0.88f)), 4.0f);
            dl->AddRect(bMin, bMax, tagColor, 4.0f, 0, 1.4f);

            // Badge Text
            dl->AddText(ImVec2(bMin.x + 8.0f, bMin.y + 3.0f), tagColor, badgeLine1);
            dl->AddText(ImVec2(bMin.x + 8.0f, bMin.y + textSize1.y + 5.0f), IM_COL32(200, 215, 230, aTag), badgeLine2);
        }
    }

    // =========================================================================
    // 3. F3 TACTICAL ACOUSTIC TELEMETRY & CONTROLS WINDOW (ImGui)
    // =========================================================================
    ImGui::SetNextWindowPos(ImVec2(24.0f, 100.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.0f, 320.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Tactical Acoustic Telemetry [F3]", &m_audioDebugVisualizerEnabled, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 1.0f, 1.0f), "MERCENARY ACOUSTIC SUBSYSTEM");
        ImGui::Separator();

        AudioEngine::VoiceTelemetrySnapshot snap = audio.getVoiceSnapshot();
        ImGui::Text("Active Voices: %d / %d", snap.activeCount, snap.maxVoices);
        ImGui::Text("Active Acoustic Waves: %d / 64", static_cast<int>(activeWaves.size()));
        ImGui::Text("Current Ear SPL: %.1f dB", maxSplDb);

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Wavefront Speed Tuning:");
        float curSpeed = audio.getAcousticVisualSpeed();
        if (ImGui::SliderFloat("Visual Speed", &curSpeed, 30.0f, 343.0f, "%.0f m/s")) {
            audio.setAcousticVisualSpeed(curSpeed);
        }
        if (ImGui::Button("Reset Visual (80 m/s)")) {
            audio.setAcousticVisualSpeed(80.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("True Physical (343 m/s)")) {
            audio.setAcousticVisualSpeed(343.0f);
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Recent Sound Events:");
        ImGui::BeginChild("WaveEventsList", ImVec2(0.0f, 120.0f), true);
        if (activeWaves.empty()) {
            ImGui::TextDisabled("No active acoustic wave emissions.");
        } else {
            for (int i = static_cast<int>(activeWaves.size()) - 1; i >= 0; --i) {
                const auto& w = activeWaves[i];
                float d = glm::distance(playerPos, w.origin);
                ImGui::TextColored(ImVec4(w.color.r, w.color.g, w.color.b, 1.0f),
                    "[%s]  r=%.1fm  d=%.1fm  (%.0f dB)",
                    getSoundTypeName(w.soundType), w.currentRadius, d, w.initialDb);
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
"""

# Append to UIHUD.cpp
idx_ns = hud_cpp.rfind("} // namespace Mercenary")
if idx_ns != -1:
    hud_cpp = hud_cpp[:idx_ns] + acoustic_hud_impl + "\n} // namespace Mercenary\n"
else:
    hud_cpp += acoustic_hud_impl

with open('src/game/UIHUD.cpp', 'w', encoding='utf-8') as f:
    f.write(hud_cpp)

print("UIHUD.cpp updated successfully with Step 3!")

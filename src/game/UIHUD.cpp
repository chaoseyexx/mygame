#include "UI.hpp"
#include "engine/InputConfig.hpp"
#include "ChatSystem.hpp"
#include "engine/VoiceChat.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include "engine/Audio.hpp"
#include "engine/SteamIntegration.hpp"
#include "engine/Network.hpp"
#include "engine/Camera.hpp"
#include "FrontlineAI.hpp"
#include "ExchangeMarket.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iomanip>

namespace Mercenary {
void UIManager::renderVoiceChatHUD(const Player& player, float screenW, float screenH) {
    (void)player;
    VoiceChatManager& vc = VoiceChatManager::get();
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // 1. TRANSMITTING (TX) BADGE
    if (vc.isTransmitting()) {
        bool isProx = vc.isProximityPttActive();
        float badgeW = 230.0f;
        float badgeH = 32.0f;
        float badgeX = 28.0f;
        float badgeY = screenH - 310.0f;

        ImU32 bgCol = IM_COL32(14, 18, 24, 235);
        ImU32 borderCol = isProx ? IM_COL32(65, 215, 125, 240) : IM_COL32(75, 185, 255, 240);
        ImU32 textCol = isProx ? IM_COL32(120, 240, 165, 255) : IM_COL32(130, 210, 255, 255);

        dl->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + badgeH), bgCol, 4.0f);
        dl->AddRect(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + badgeH), borderCol, 4.0f, 0, 1.5f);

        // Icon dot
        dl->AddCircleFilled(ImVec2(badgeX + 14.0f, badgeY + 16.0f), 4.5f, borderCol);

        // Text
        const char* label = isProx ? "TX // PROXIMITY VOICE [V]" : "TX // SQUAD RADIO [B]";
        dl->AddText(ImVec2(badgeX + 26.0f, badgeY + 8.0f), textCol, label);

        // Dynamic VU Equalizer Bars (5 bars)
        float amp = vc.getLocalMicAmplitude();
        float vuStartX = badgeX + badgeW - 46.0f;
        for (int b = 0; b < 5; ++b) {
            float barMaxH = 16.0f;
            float freqWeight = 0.5f + 0.5f * std::sin((float)b * 1.2f + ImGui::GetTime() * 14.0f);
            float h = std::clamp(amp * barMaxH * freqWeight + 2.0f, 2.0f, barMaxH);
            float bx = vuStartX + b * 7.0f;
            float by = badgeY + 24.0f - h;

            ImU32 barCol = (b >= 4) ? IM_COL32(245, 75, 65, 255) :
                          ((b >= 3) ? IM_COL32(245, 195, 45, 255) : IM_COL32(65, 215, 125, 255));
            dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + 4.5f, badgeY + 24.0f), barCol, 1.0f);
        }
    }

    // 2. RECEIVING (RX) ACTIVE SPEAKERS
    const auto& speakers = vc.getActiveSpeakers();
    float rxY = 75.0f;
    for (const auto& [id, spk] : speakers) {
        if (!spk.isSpeaking && spk.currentAmplitude < 0.05f) continue;

        float rxW = 200.0f;
        float rxH = 26.0f;
        float rxX = screenW - rxW - 28.0f;

        ImU32 bgCol = IM_COL32(12, 16, 22, 225);
        ImU32 borderCol = spk.isSquadRadio ? IM_COL32(75, 185, 255, 220) : IM_COL32(245, 195, 65, 220);
        ImU32 textCol = spk.isSquadRadio ? IM_COL32(130, 210, 255, 255) : IM_COL32(245, 220, 140, 255);

        dl->AddRectFilled(ImVec2(rxX, rxY), ImVec2(rxX + rxW, rxY + rxH), bgCol, 3.5f);
        dl->AddRect(ImVec2(rxX, rxY), ImVec2(rxX + rxW, rxY + rxH), borderCol, 3.5f, 0, 1.0f);

        // Radiating wave icon
        float cx = rxX + 14.0f;
        float cy = rxY + 13.0f;
        dl->AddCircleFilled(ImVec2(cx, cy), 3.0f, borderCol);
        float pulse = std::sin(ImGui::GetTime() * 12.0f) * 2.0f;
        dl->AddCircle(ImVec2(cx, cy), 6.0f + pulse, borderCol, 12, 1.0f);

        // Callsign
        std::string label = "RX // " + spk.callsign;
        dl->AddText(ImVec2(rxX + 26.0f, rxY + 5.0f), textCol, label.c_str());

        // 3-segment level meter
        float mX = rxX + rxW - 26.0f;
        for (int m = 0; m < 3; ++m) {
            float mh = (m + 1) * 4.0f;
            ImU32 mc = (spk.currentAmplitude * 3.0f >= m) ? borderCol : IM_COL32(40, 48, 60, 160);
            dl->AddRectFilled(ImVec2(mX + m * 6.0f, rxY + 20.0f - mh), ImVec2(mX + m * 6.0f + 4.0f, rxY + 20.0f), mc, 1.0f);
        }

        rxY += rxH + 6.0f;
    }
}

void UIManager::renderMinimalistHUD(Player& player, const World& world, const MissionSystem& missions) {
    ImGuiIO& io = ImGui::GetIO();
    ChatSystem::get().renderChatHUD(player, io.DisplaySize.x, io.DisplaySize.y);
    renderVoiceChatHUD(player, io.DisplaySize.x, io.DisplaySize.y);
    SurvivalStats& stats = player.getStats();

    ImGuiWindowFlags transparentHud = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    // Tactical Spawn Shield Indicator Badge
    if (player.hasSpawnProtection()) {
        float spTime = player.getSpawnProtectionTimer();
        float badgeW = 390.0f;
        float badgeH = 28.0f;
        float badgeX = io.DisplaySize.x * 0.5f - badgeW * 0.5f;
        float badgeY = 74.0f;
        drawList->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + badgeH), IM_COL32(18, 30, 48, 220), 4.0f);
        drawList->AddRect(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + badgeH), IM_COL32(56, 189, 248, 210), 4.0f, 0, 1.4f);

        char spBuf[80];
        std::snprintf(spBuf, sizeof(spBuf), "[ TACTICAL SHIELD: %.1fs ] (Firing Breaks Invulnerability)", spTime);
        ImVec2 sz = ImGui::CalcTextSize(spBuf);
        drawList->AddText(ImVec2(badgeX + (badgeW - sz.x) * 0.5f, badgeY + 6.0f), IM_COL32(210, 240, 255, 255), spBuf);
    }

    // Training Mode HUD indicator watermark badge
    if (m_isTrainingMode) {
        float badgeW = 340.0f;
        float badgeH = 26.0f;
        float badgeX = io.DisplaySize.x * 0.5f - badgeW * 0.5f;
        float badgeY = 38.0f;
        drawList->AddRectFilled(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + badgeH), IM_COL32(16, 20, 26, 210), 3.0f);
        drawList->AddRect(ImVec2(badgeX, badgeY), ImVec2(badgeX + badgeW, badgeY + badgeH), IM_COL32(219, 98, 41, 160), 3.0f, 0, 1.0f);
        drawList->AddText(ImVec2(badgeX + 14, badgeY + 5), IM_COL32(235, 130, 50, 255), "SIMULATION PROTOCOL // OFFLINE TRAINING");
        drawList->AddText(ImVec2(badgeX + 265, badgeY + 5), IM_COL32(140, 220, 150, 255), "[BOTS ACTIVE]");
    }

    // ================= 1. BOTTOM-LEFT: SLEEK VITALITY & COVER GAUGE =================
    float padX = 32.0f;
    float padY = io.DisplaySize.y - 48.0f;
    float barW = 200.0f;

    // Health Bar Track & Fill
    float hpNorm = std::clamp(stats.health / 100.0f, 0.0f, 1.0f);
    ImU32 hpColor = (hpNorm > 0.40f) ? IM_COL32(50, 205, 110, 230) :
                    ((hpNorm > 0.20f) ? IM_COL32(235, 175, 40, 230) : IM_COL32(235, 55, 45, 240));

    // Dark glass background backing
    drawList->AddRectFilled(ImVec2(padX - 8, padY - 22), ImVec2(padX + barW + 16, padY + 16), IM_COL32(12, 14, 18, 160), 4.0f);

    // Health bar (4px height)
    drawList->AddRectFilled(ImVec2(padX, padY - 14), ImVec2(padX + barW, padY - 10), IM_COL32(25, 30, 36, 200), 2.0f);
    drawList->AddRectFilled(ImVec2(padX, padY - 14), ImVec2(padX + barW * hpNorm, padY - 10), hpColor, 2.0f);

    // Stamina bar (3.5px height with fatigue warning color)
    float stamNorm = std::clamp(stats.stamina / 100.0f, 0.0f, 1.0f);
    ImU32 stamColor = (stats.isExhausted || stats.stamina < 30.0f) ? IM_COL32(245, 158, 11, 240) : IM_COL32(56, 189, 248, 230);
    drawList->AddRectFilled(ImVec2(padX, padY - 6), ImVec2(padX + barW, padY - 2.0f), IM_COL32(25, 30, 36, 180), 1.5f);
    drawList->AddRectFilled(ImVec2(padX, padY - 6), ImVec2(padX + barW * stamNorm, padY - 2.0f), stamColor, 1.5f);

    // Subtle tactical cover / stance status pills
    if (stats.cover == CoverType::Heavy) {
        drawList->AddText(ImVec2(padX, padY + 1), IM_COL32(80, 220, 120, 230), "IN COVER");
    } else if (stats.cover == CoverType::Light) {
        drawList->AddText(ImVec2(padX, padY + 1), IM_COL32(235, 195, 60, 230), "LIGHT COVER");
    }

    if (stats.stance == PlayerStance::Crouching) {
        drawList->AddText(ImVec2(padX + 90, padY + 1), IM_COL32(160, 175, 195, 210), "CROUCH");
    } else if (stats.stance == PlayerStance::Prone) {
        drawList->AddText(ImVec2(padX + 90, padY + 1), IM_COL32(160, 175, 195, 210), "PRONE");
    }

    if (stats.bleedCuts > 0 || player.getBody().getTotalBleedRate() > 0.0f) {
        drawList->AddText(ImVec2(padX + 145, padY + 1), IM_COL32(245, 60, 50, 240), "BLEEDING");
    }

    // Anatomical Organ & Limb Injury Status Badges
    float alertY = padY - 44.0f;
    const BodySystem& body = player.getBody();
    for (const auto& part : body.getBodyParts()) {
        if (part.isFractured) {
            std::string label = std::string(getBodyRegionName(part.region)) + " FRACTURE";
            drawList->AddRectFilled(ImVec2(padX, alertY), ImVec2(padX + 160, alertY + 16), IM_COL32(180, 45, 35, 200), 2.0f);
            drawList->AddText(ImVec2(padX + 6, alertY + 1), IM_COL32(255, 220, 220, 255), label.c_str());
            alertY -= 20.0f;
        }
        for (const auto& organ : part.organs) {
            if (!organ.isFunctioning || organ.health < 40.0f) {
                std::string label = std::string(getOrganName(organ.organ)) + " CRITICAL";
                drawList->AddRectFilled(ImVec2(padX, alertY), ImVec2(padX + 160, alertY + 16), IM_COL32(215, 35, 35, 220), 2.0f);
                drawList->AddText(ImVec2(padX + 6, alertY + 1), IM_COL32(255, 255, 255, 255), label.c_str());
                alertY -= 20.0f;
            }
        }
    }

    // ================= 2. BOTTOM-RIGHT: MINIMALIST WEAPON & AMMO GAUGE =================
    auto& equipped = player.getInventory().getEquipped(player.getActiveWeaponSlot());
    if (equipped.has_value()) {
        float rW = 210.0f;
        float rx = io.DisplaySize.x - rW - 32.0f;
        float ry = io.DisplaySize.y - 48.0f;

        // Dark glass background backing
        drawList->AddRectFilled(ImVec2(rx - 12, ry - 22), ImVec2(rx + rW + 8, ry + 16), IM_COL32(12, 14, 18, 160), 4.0f);

        int reserves = player.getInventory().getAmmoCount(equipped->weaponType);

        // Render clean ammo text via ImGui window
        ImGui::SetNextWindowPos(ImVec2(rx, ry - 22));
        ImGui::SetNextWindowSize(ImVec2(rW, 38));
        if (ImGui::Begin("##AmmoHud", nullptr, transparentHud)) {
            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextColored(ImVec4(0.96f, 0.97f, 0.98f, 1.0f), "%d", equipped->currentMag);
            ImGui::SameLine();
            ImGui::SetWindowFontScale(0.95f);
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "/ %d", reserves);

            ImGui::SameLine(130);
            ImGui::TextColored(ImVec4(0.90f, 0.65f, 0.15f, 0.9f), "%s", equipped->isAutomatic ? "AUTO" : "SEMI");

            ImGui::SetWindowFontScale(0.85f);
            ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 0.85f), "%s", equipped->name.c_str());
            ImGui::End();
        }
    }

    // ================= 3. TOP-CENTER: SLEEK COMPASS HEADING =================
    float deg = std::fmod(glm::degrees(player.getYaw()) + 360.0f, 360.0f);
    const char* heading = "N";
    if (deg >= 22.5f && deg < 67.5f) heading = "NE";
    else if (deg >= 67.5f && deg < 112.5f) heading = "E";
    else if (deg >= 112.5f && deg < 157.5f) heading = "SE";
    else if (deg >= 157.5f && deg < 202.5f) heading = "S";
    else if (deg >= 202.5f && deg < 247.5f) heading = "SW";
    else if (deg >= 247.5f && deg < 292.5f) heading = "W";
    else if (deg >= 292.5f && deg < 337.5f) heading = "NW";

    char compassBuf[64];
    snprintf(compassBuf, sizeof(compassBuf), "%s  %03.0f", heading, deg);
    float compW = 120.0f;
    float cx = io.DisplaySize.x * 0.5f - compW * 0.5f;
    drawList->AddRectFilled(ImVec2(cx, 10), ImVec2(cx + compW, 32), IM_COL32(12, 14, 18, 140), 3.0f);
    drawList->AddText(ImVec2(cx + 28, 13), IM_COL32(200, 215, 230, 210), compassBuf);

    // Small subtle objective indicator only when a contract is active (1 line, unobtrusive)
    if (missions.hasActiveContract()) {
        const auto& contract = missions.getActiveContract();
        float dist = glm::distance(player.getPosition(), contract.targetLocation);
        char objBuf[128];
        snprintf(objBuf, sizeof(objBuf), "> %s (%.0fm)", contract.title.c_str(), dist);
        drawList->AddText(ImVec2(32, 14), IM_COL32(235, 175, 45, 210), objBuf);
    }
}

// =========================================================================
// 5. IN-GAME ESCAPE POPUP MENU
// =========================================================================
void UIManager::renderInGameEscapePopup(Player& player, World* world, FrontlineWarfare* frontline) {
    if (!m_escapeMenuOpen) return;

    ImGuiIO& io = ImGui::GetIO();
    float winW = 400.0f;
    float winH = 475.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - winW * 0.5f, io.DisplaySize.y * 0.5f - winH * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar;

    ImGui::Begin("OPERATION PAUSED##EscMenu", &m_escapeMenuOpen, flags);

    ImGui::SetWindowFontScale(1.2f);
    ImGui::TextColored(ImVec4(0.92f, 0.58f, 0.18f, 1.0f), "GAME PAUSED");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Simulation continues live on the frontline.");
    ImGui::Spacing();

    // 1. Resume
    if (ImGui::Button("RESUME RAID [ESC]", ImVec2(-1, 32))) {
        m_escapeMenuOpen = false;
        AudioEngine::get().playSound(SoundType::TacticalClick);
    }

    // 2. Character / Inventory
    if (ImGui::Button("FIELD GEAR & INVENTORY [TAB]", ImVec2(-1, 30))) {
        m_inventoryOpen = true;
        m_escapeMenuOpen = false;
        AudioEngine::get().playSound(SoundType::TacticalClick);
    }

    // 3. Tactical Map
    if (ImGui::Button("TACTICAL FRONTLINE MAP [M]", ImVec2(-1, 30))) {
        m_mapOpen = true;
        m_escapeMenuOpen = false;
        AudioEngine::get().playSound(SoundType::TacticalClick);
    }

    // 4. Options
    if (ImGui::Button("SETTINGS & CONTROLS", ImVec2(-1, 30))) {
        m_optionsOpen = true;
        AudioEngine::get().playSound(SoundType::TacticalClick);
    }

    if (m_isTrainingMode) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.92f, 0.58f, 0.18f, 1.0f), "[ TRAINING SIMULATION CONTROLS ]");
        if (renderCompetitorButton("RESUPPLY AMMUNITION & MEDS", ImVec2(-1, 32), false, true)) {
            player.getStats().health = 100.0f;
            player.getStats().blood = 5000.0f;
            player.getStats().bleedCuts = 0;
            player.getInventory().addItem(Inventory::create556Ammo(120));
            player.getInventory().addItem(Inventory::create545Ammo(120));
            player.getInventory().addItem(Inventory::create9mmAmmo(60));
            player.getInventory().addItem(Inventory::createBandage(3));
            AudioEngine::get().playSound(SoundType::ItemEquip, 1.0f, 1.0f);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 5. Disconnect to Lobby
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.28f, 0.16f, 1.0f));
    const char* discLabel = m_isTrainingMode ? "END TRAINING // RETURN TO ARSENAL" : "DISCONNECT TO PMC HIDEOUT";
    if (ImGui::Button(discLabel, ImVec2(-1, 32))) {
        m_escapeMenuOpen = false;
        m_gameState = GameState::MainMenu;
        m_currentTab = MainMenuTab::Overview;
        m_isOnlineMultiplayer = false;
        m_isMatchmaking = false;
        NetworkEngine::get().disconnectClient();
        if (m_isTrainingMode) {
            m_isTrainingMode = false;
            if (world) world->generateWarzone();
            if (frontline) frontline->init(10);
        }
        AudioEngine::get().playSound(SoundType::TacticalClick);
        SteamIntegration::get().setRichPresence("status", "MERCENARY: PMC Hideout");
    }
    ImGui::PopStyleColor();

    // 6. Change Character
    if (ImGui::Button("CHANGE OPERATIVE DOSSIER", ImVec2(-1, 28))) {
        m_escapeMenuOpen = false;
        m_gameState = GameState::CharacterSelect;
        m_isOnlineMultiplayer = false;
        m_isMatchmaking = false;
        NetworkEngine::get().disconnectClient();
        AudioEngine::get().playSound(SoundType::TacticalClick);
    }

    // 7. Exit Game
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.48f, 0.20f, 0.20f, 1.0f));
    if (ImGui::Button("EXIT TO DESKTOP", ImVec2(-1, 28))) {
        exit(0);
    }
    ImGui::PopStyleColor();

    ImGui::End();
}

// =========================================================================
// 6. SETTINGS & CONTROLS MODAL (100% Real Engine Hardware & Render Controls)
// =========================================================================
// =========================================================================
// 8.4 PROCEDURAL HIGH-RES WEAPON & EQUIPMENT ICONS FOR RADIAL WHEEL
// =========================================================================
static void drawWheelVectorIcon(ImDrawList* dl, int sectorIndex, ImVec2 center, float scale, bool isSelected, bool hasItem, const std::string& name) {
    ImU32 priCol = isSelected ? IM_COL32(255, 215, 80, 255) : IM_COL32(215, 225, 238, 230);
    ImU32 secCol = isSelected ? IM_COL32(255, 175, 45, 240) : IM_COL32(145, 160, 180, 210);
    ImU32 darkCol = isSelected ? IM_COL32(110, 85, 30, 255) : IM_COL32(45, 52, 64, 255);
    ImU32 accCol = isSelected ? IM_COL32(255, 240, 160, 255) : IM_COL32(100, 200, 255, 240);

    float cx = center.x;
    float cy = center.y;

    switch (sectorIndex) {
        case 0: { // 01: Primary Weapon (M4A1 / AK-74 Assault Rifle)
            if (!hasItem) {
                // Dashed tactical watermark silhouette of rifle
                dl->AddLine(ImVec2(cx - 24.0f * scale, cy), ImVec2(cx + 26.0f * scale, cy), secCol, 1.8f * scale);
                dl->AddRect(ImVec2(cx - 10.0f * scale, cy - 4.0f * scale), ImVec2(cx + 12.0f * scale, cy + 4.0f * scale), secCol, 1.0f, 0, 1.2f * scale);
                dl->AddLine(ImVec2(cx + 2.0f * scale, cy + 4.0f * scale), ImVec2(cx - 2.0f * scale, cy + 14.0f * scale), secCol, 1.5f * scale);
                dl->AddLine(ImVec2(cx - 10.0f * scale, cy), ImVec2(cx - 24.0f * scale, cy + 7.0f * scale), secCol, 1.5f * scale);
                break;
            }
            if (name.find("AK") != std::string::npos || name.find("Kalashnikov") != std::string::npos) {
                // AK-74M: Curved dust cover, plum foregrip, curved banana mag, slant brake
                dl->AddRectFilled(ImVec2(cx - 12.0f * scale, cy - 4.5f * scale), ImVec2(cx + 6.0f * scale, cy + 3.0f * scale), darkCol, 1.0f);
                dl->AddRectFilled(ImVec2(cx + 6.0f * scale, cy - 3.5f * scale), ImVec2(cx + 18.0f * scale, cy + 2.5f * scale), priCol, 1.5f);
                dl->AddLine(ImVec2(cx + 18.0f * scale, cy - 1.0f * scale), ImVec2(cx + 28.0f * scale, cy - 1.0f * scale), priCol, 2.0f * scale);
                dl->AddLine(ImVec2(cx + 25.0f * scale, cy - 1.0f * scale), ImVec2(cx + 25.0f * scale, cy - 5.0f * scale), priCol, 1.5f * scale);
                ImVec2 m0(cx + 2.0f * scale, cy + 3.0f * scale), m1(cx + 6.0f * scale, cy + 3.0f * scale);
                ImVec2 m2(cx + 11.0f * scale, cy + 13.0f * scale), m3(cx + 7.0f * scale, cy + 13.0f * scale);
                dl->AddQuadFilled(m0, m1, m2, m3, secCol);
                dl->AddLine(ImVec2(cx - 6.0f * scale, cy + 3.0f * scale), ImVec2(cx - 10.0f * scale, cy + 10.0f * scale), darkCol, 2.5f * scale);
                dl->AddQuadFilled(ImVec2(cx - 12.0f * scale, cy - 3.0f * scale), ImVec2(cx - 12.0f * scale, cy + 2.0f * scale),
                                  ImVec2(cx - 26.0f * scale, cy + 5.0f * scale), ImVec2(cx - 26.0f * scale, cy - 4.0f * scale), priCol);
            } else {
                // M4A1 SOPMOD: Upper/lower receiver, quad rail, EOTech holo, STANAG mag, Crane stock
                dl->AddRectFilled(ImVec2(cx - 10.0f * scale, cy - 4.5f * scale), ImVec2(cx + 6.0f * scale, cy + 3.5f * scale), darkCol, 1.0f);
                dl->AddRectFilled(ImVec2(cx + 6.0f * scale, cy - 3.5f * scale), ImVec2(cx + 20.0f * scale, cy + 2.5f * scale), priCol, 1.0f);
                dl->AddLine(ImVec2(cx + 20.0f * scale, cy - 1.0f * scale), ImVec2(cx + 29.0f * scale, cy - 1.0f * scale), priCol, 2.2f * scale);
                dl->AddRectFilled(ImVec2(cx - 4.0f * scale, cy - 9.0f * scale), ImVec2(cx + 4.0f * scale, cy - 4.5f * scale), darkCol, 1.0f);
                dl->AddCircleFilled(ImVec2(cx, cy - 6.5f * scale), 1.2f * scale, accCol);
                ImVec2 m0(cx + 2.0f * scale, cy + 3.5f * scale), m1(cx + 6.5f * scale, cy + 3.5f * scale);
                ImVec2 m2(cx + 4.0f * scale, cy + 13.5f * scale), m3(cx - 0.5f * scale, cy + 13.0f * scale);
                dl->AddQuadFilled(m0, m1, m2, m3, secCol);
                dl->AddLine(ImVec2(cx - 6.0f * scale, cy + 3.5f * scale), ImVec2(cx - 11.0f * scale, cy + 11.0f * scale), darkCol, 2.8f * scale);
                dl->AddLine(ImVec2(cx - 10.0f * scale, cy - 1.0f * scale), ImVec2(cx - 24.0f * scale, cy - 1.0f * scale), darkCol, 2.5f * scale);
                dl->AddRectFilled(ImVec2(cx - 26.0f * scale, cy - 4.5f * scale), ImVec2(cx - 18.0f * scale, cy + 5.0f * scale), priCol, 1.5f);
            }
            break;
        }
        case 1: { // 02: Marksman / Sniper DMR (Mosin-Nagant / SVD)
            dl->AddLine(ImVec2(cx - 28.0f * scale, cy + 1.0f * scale), ImVec2(cx + 22.0f * scale, cy + 1.0f * scale), priCol, 3.5f * scale);
            dl->AddLine(ImVec2(cx + 12.0f * scale, cy - 0.8f * scale), ImVec2(cx + 32.0f * scale, cy - 0.8f * scale), secCol, 1.8f * scale);
            dl->AddLine(ImVec2(cx + 29.0f * scale, cy - 0.8f * scale), ImVec2(cx + 29.0f * scale, cy - 4.5f * scale), secCol, 1.2f * scale);
            dl->AddRectFilled(ImVec2(cx - 8.0f * scale, cy - 8.5f * scale), ImVec2(cx + 10.0f * scale, cy - 5.5f * scale), darkCol, 1.0f);
            dl->AddLine(ImVec2(cx - 4.0f * scale, cy - 5.5f * scale), ImVec2(cx - 4.0f * scale, cy - 1.0f * scale), darkCol, 1.8f * scale);
            dl->AddLine(ImVec2(cx + 6.0f * scale, cy - 5.5f * scale), ImVec2(cx + 6.0f * scale, cy - 1.0f * scale), darkCol, 1.8f * scale);
            dl->AddCircleFilled(ImVec2(cx + 9.0f * scale, cy - 7.0f * scale), 1.3f * scale, accCol);
            dl->AddLine(ImVec2(cx, cy - 1.5f * scale), ImVec2(cx + 3.0f * scale, cy + 3.0f * scale), secCol, 1.4f * scale);
            dl->AddQuadFilled(ImVec2(cx - 14.0f * scale, cy - 1.0f * scale), ImVec2(cx - 14.0f * scale, cy + 2.5f * scale),
                              ImVec2(cx - 29.0f * scale, cy + 5.5f * scale), ImVec2(cx - 29.0f * scale, cy - 2.5f * scale), priCol);
            break;
        }
        case 2: { // 03: Secondary / CQB Pistol (Beretta M9A3)
            dl->AddRectFilled(ImVec2(cx - 14.0f * scale, cy - 5.0f * scale), ImVec2(cx + 16.0f * scale, cy - 0.5f * scale), priCol, 1.2f);
            dl->AddLine(ImVec2(cx - 4.0f * scale, cy - 3.5f * scale), ImVec2(cx + 6.0f * scale, cy - 3.5f * scale), darkCol, 1.5f * scale);
            dl->AddRectFilled(ImVec2(cx - 11.0f * scale, cy - 0.5f * scale), ImVec2(cx + 8.0f * scale, cy + 2.5f * scale), secCol, 1.0f);
            dl->AddCircle(ImVec2(cx - 1.0f * scale, cy + 3.0f * scale), 3.5f * scale, darkCol, 8, 1.4f * scale);
            dl->AddLine(ImVec2(cx - 1.0f * scale, cy + 0.5f * scale), ImVec2(cx - 3.0f * scale, cy + 3.5f * scale), darkCol, 1.3f * scale);
            ImVec2 g0(cx - 12.0f * scale, cy + 1.5f * scale), g1(cx - 6.0f * scale, cy + 1.5f * scale);
            ImVec2 g2(cx - 9.0f * scale, cy + 14.0f * scale), g3(cx - 16.0f * scale, cy + 13.5f * scale);
            dl->AddQuadFilled(g0, g1, g2, g3, secCol);
            dl->AddRectFilled(ImVec2(cx - 17.0f * scale, cy + 13.5f * scale), ImVec2(cx - 8.0f * scale, cy + 15.5f * scale), darkCol, 0.8f);
            break;
        }
        case 3: { // 04: Fortification (Deployable Sandbag Wall)
            for (int b = -1; b <= 1; ++b) {
                float ox = cx + b * 13.0f * scale;
                dl->AddRectFilled(ImVec2(ox - 6.0f * scale, cy + 3.0f * scale), ImVec2(ox + 6.0f * scale, cy + 10.0f * scale), priCol, 2.5f);
                dl->AddRect(ImVec2(ox - 6.0f * scale, cy + 3.0f * scale), ImVec2(ox + 6.0f * scale, cy + 10.0f * scale), darkCol, 2.5f, 0, 1.0f);
                dl->AddLine(ImVec2(ox - 4.0f * scale, cy + 6.5f * scale), ImVec2(ox + 4.0f * scale, cy + 6.5f * scale), secCol, 1.0f);
            }
            for (int b = 0; b <= 1; ++b) {
                float ox = cx + (b - 0.5f) * 13.0f * scale;
                dl->AddRectFilled(ImVec2(ox - 6.0f * scale, cy - 4.5f * scale), ImVec2(ox + 6.0f * scale, cy + 2.5f * scale), priCol, 2.5f);
                dl->AddRect(ImVec2(ox - 6.0f * scale, cy - 4.5f * scale), ImVec2(ox + 6.0f * scale, cy + 2.5f * scale), darkCol, 2.5f, 0, 1.0f);
                dl->AddLine(ImVec2(ox - 4.0f * scale, cy - 1.0f * scale), ImVec2(ox + 4.0f * scale, cy - 1.0f * scale), secCol, 1.0f);
            }
            dl->AddLine(ImVec2(cx - 18.0f * scale, cy - 7.0f * scale), ImVec2(cx - 18.0f * scale, cy + 12.0f * scale), darkCol, 2.2f * scale);
            dl->AddLine(ImVec2(cx + 18.0f * scale, cy - 7.0f * scale), ImVec2(cx + 18.0f * scale, cy + 12.0f * scale), darkCol, 2.2f * scale);
            break;
        }
        case 4: { // 05: Sling Arms / Holster (Mobility Boost)
            dl->AddLine(ImVec2(cx - 16.0f * scale, cy - 12.0f * scale), ImVec2(cx + 16.0f * scale, cy + 12.0f * scale), priCol, 2.5f * scale);
            dl->AddLine(ImVec2(cx + 16.0f * scale, cy - 12.0f * scale), ImVec2(cx - 16.0f * scale, cy + 12.0f * scale), secCol, 2.5f * scale);
            dl->AddCircleFilled(ImVec2(cx, cy), 5.5f * scale, darkCol);
            dl->AddCircle(ImVec2(cx, cy), 5.5f * scale, priCol, 12, 1.5f * scale);
            dl->AddRectFilled(ImVec2(cx - 3.0f * scale, cy - 3.0f * scale), ImVec2(cx + 3.0f * scale, cy + 3.0f * scale), accCol, 1.0f);
            dl->AddLine(ImVec2(cx + 9.0f * scale, cy - 7.0f * scale), ImVec2(cx + 14.0f * scale, cy - 7.0f * scale), accCol, 1.8f * scale);
            dl->AddLine(ImVec2(cx + 11.0f * scale, cy - 4.0f * scale), ImVec2(cx + 16.0f * scale, cy - 4.0f * scale), accCol, 1.8f * scale);
            break;
        }
        case 5: { // 06: Melee Combat Blade (Ka-Bar Combat Tanto)
            ImVec2 b0(cx - 18.0f * scale, cy - 1.0f * scale);
            ImVec2 b1(cx + 4.0f * scale, cy - 4.0f * scale);
            ImVec2 b2(cx + 4.0f * scale, cy + 2.5f * scale);
            ImVec2 b3(cx - 12.0f * scale, cy + 2.5f * scale);
            dl->AddQuadFilled(b0, b1, b2, b3, priCol);
            dl->AddLine(b1, b0, accCol, 1.5f * scale);
            dl->AddLine(b0, b3, accCol, 1.5f * scale);
            dl->AddLine(ImVec2(cx - 8.0f * scale, cy - 0.8f * scale), ImVec2(cx + 2.0f * scale, cy - 0.8f * scale), darkCol, 1.2f * scale);
            dl->AddRectFilled(ImVec2(cx + 4.0f * scale, cy - 7.0f * scale), ImVec2(cx + 6.5f * scale, cy + 6.0f * scale), darkCol, 1.0f);
            dl->AddRectFilled(ImVec2(cx + 6.5f * scale, cy - 2.8f * scale), ImVec2(cx + 20.0f * scale, cy + 2.2f * scale), secCol, 1.5f);
            for (float g = cx + 9.0f * scale; g < cx + 19.0f * scale; g += 2.8f * scale) {
                dl->AddLine(ImVec2(g, cy - 2.8f * scale), ImVec2(g, cy + 2.2f * scale), darkCol, 1.0f);
            }
            dl->AddRectFilled(ImVec2(cx + 20.0f * scale, cy - 4.0f * scale), ImVec2(cx + 22.5f * scale, cy + 3.4f * scale), darkCol, 1.0f);
            break;
        }
        case 6: { // 07: Tactical First Aid IFAK (Medical Trauma Kit)
            ImVec2 p0(cx - 14.0f * scale, cy - 11.0f * scale);
            ImVec2 p1(cx + 14.0f * scale, cy + 11.0f * scale);
            dl->AddRectFilled(p0, p1, darkCol, 3.5f);
            dl->AddRect(p0, p1, secCol, 3.5f, 0, 1.4f * scale);
            dl->AddLine(ImVec2(p0.x + 2.0f * scale, p0.y + 4.0f * scale), ImVec2(p1.x - 2.0f * scale, p0.y + 4.0f * scale), priCol, 1.2f * scale);
            dl->AddRectFilled(ImVec2(cx - 2.5f * scale, p0.y + 2.0f * scale), ImVec2(cx + 2.5f * scale, p0.y + 6.0f * scale), accCol, 1.0f);
            ImU32 crossCol = IM_COL32(235, 65, 55, 255);
            dl->AddRectFilled(ImVec2(cx - 2.5f * scale, cy - 5.5f * scale), ImVec2(cx + 2.5f * scale, cy + 7.5f * scale), crossCol, 1.0f);
            dl->AddRectFilled(ImVec2(cx - 7.5f * scale, cy - 0.5f * scale), ImVec2(cx + 7.5f * scale, cy + 2.5f * scale), crossCol, 1.0f);
            break;
        }
        case 7: { // 08: Tactical Resupply (Combat Rig & Dual Magazines)
            for (int m = 0; m < 2; ++m) {
                float ox = cx + (m == 0 ? -7.0f : 5.0f) * scale;
                ImVec2 m0(ox - 4.5f * scale, cy - 10.0f * scale);
                ImVec2 m1(ox + 4.5f * scale, cy - 10.0f * scale);
                ImVec2 m2(ox + 3.0f * scale, cy + 10.0f * scale);
                ImVec2 m3(ox - 3.5f * scale, cy + 10.0f * scale);
                dl->AddQuadFilled(m0, m1, m2, m3, darkCol);
                dl->AddQuad(m0, m1, m2, m3, priCol, 1.2f * scale);
                dl->AddRectFilled(ImVec2(ox - 3.0f * scale, cy - 13.0f * scale), ImVec2(ox + 3.0f * scale, cy - 10.0f * scale), IM_COL32(245, 195, 65, 255), 1.0f);
                dl->AddLine(ImVec2(ox - 3.5f * scale, cy - 3.0f * scale), ImVec2(ox + 3.5f * scale, cy - 3.0f * scale), secCol, 1.0f);
                dl->AddLine(ImVec2(ox - 3.0f * scale, cy + 3.0f * scale), ImVec2(ox + 3.0f * scale, cy + 3.0f * scale), secCol, 1.0f);
            }
            dl->AddLine(ImVec2(cx - 14.0f * scale, cy - 2.0f * scale), ImVec2(cx + 14.0f * scale, cy - 2.0f * scale), accCol, 1.5f * scale);
            break;
        }
        default:
            break;
    }
}

// =========================================================================
// 8.5 GTA 5 STYLE 8-SLOT RADIAL WEAPON WHEEL & ATTRIBUTE DOSSIER
// =========================================================================


void UIManager::setWeaponWheelOpen(bool open, Player* player, World* world) {
    if (m_weaponWheelOpen == open) return;

    if (open) {
        m_weaponWheelOpen = true;
        m_wheelAnimTime = 0.0f;
        AudioEngine::get().playSound(SoundType::TacticalClick, 0.55f, 1.15f);
    } else {
        m_weaponWheelOpen = false;
        if (player) {
            switch (m_selectedWheelSector) {
                case 0: // Primary Weapon
                    player->switchWeapon(EquipSlot::PrimaryWeapon);
                    AudioEngine::get().playSound(SoundType::ItemEquip, 0.75f, 1.0f);
                    break;
                case 1: // Secondary / DMR
                    player->switchWeapon(EquipSlot::SecondaryWeapon);
                    AudioEngine::get().playSound(SoundType::ItemEquip, 0.75f, 1.0f);
                    break;
                case 2: // Sidearm / CQB
                    player->switchWeapon(EquipSlot::SecondaryWeapon);
                    AudioEngine::get().playSound(SoundType::ItemEquip, 0.75f, 1.05f);
                    break;
                case 3: // Deployable Fortification
                    if (world) {
                        if (player->deployFortification(*world)) {
                            AudioEngine::get().playSound(SoundType::FortifyBuild, 0.85f, 1.0f);
                        } else {
                            AudioEngine::get().playSound(SoundType::TacticalClick, 0.5f, 0.8f);
                        }
                    }
                    break;
                case 4: // Holster / Sling
                    AudioEngine::get().playSound(SoundType::GearRustle, 0.65f, 1.0f);
                    break;
                case 5: // Melee Blade
                    AudioEngine::get().playSound(SoundType::ItemEquip, 0.65f, 1.2f);
                    break;
                case 6: // First Aid IFAK
                    if (player->useQuickMedical()) {
                        // useQuickMedical handles item consumption, body treatment & audio
                    } else {
                        AudioEngine::get().playSound(SoundType::TacticalClick, 0.5f, 0.8f);
                    }
                    break;
                case 7: // Tactical Resupply / Reload
                    player->reloadCurrentWeapon();
                    AudioEngine::get().playSound(SoundType::Reload, 0.8f, 1.0f);
                    break;
                default:
                    break;
            }
        }
    }
}

void UIManager::renderWeaponWheel(Player& player, World& world) {
    if (!m_weaponWheelOpen) return;

    ImGuiIO& io = ImGui::GetIO();
    constexpr float PI_CONST = 3.14159265358979323846f;

    // Fullscreen borderless overlay for GTA 5 radial weapon wheel
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (!ImGui::Begin("##GTA5WeaponWheelOverlay", nullptr, flags)) {
        ImGui::End();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Dark cinematic vignette backdrop
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(8, 12, 16, 175));

    // Smooth entrance animation curve (Cubic ease-out)
    m_wheelAnimTime += io.DeltaTime * 6.5f;
    float animProgress = std::clamp(m_wheelAnimTime, 0.0f, 1.0f);
    float ease = 1.0f - std::pow(1.0f - animProgress, 3.0f);

    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    float innerR = 125.0f * ease;
    float outerR = 270.0f * ease;
    float hubR = 112.0f * ease;

    // Mouse pointer vector and angle detection
    ImVec2 mouseVec(io.MousePos.x - center.x, io.MousePos.y - center.y);
    float mouseDist = std::sqrt(mouseVec.x * mouseVec.x + mouseVec.y * mouseVec.y);

    if (mouseDist > 45.0f) {
        float mouseAngle = std::atan2(mouseVec.y, mouseVec.x);
        // GTA 5 layout: sector 0 is top (-PI/2), advancing clockwise
        float normAngle = mouseAngle + PI_CONST * 0.5f;
        while (normAngle < 0.0f) normAngle += 2.0f * PI_CONST;
        while (normAngle >= 2.0f * PI_CONST) normAngle -= 2.0f * PI_CONST;

        int hovered = static_cast<int>(std::floor((normAngle + PI_CONST / 8.0f) / (PI_CONST * 2.0f / 8.0f))) % 8;
        if (hovered != m_selectedWheelSector) {
            m_selectedWheelSector = hovered;
            AudioEngine::get().playSound(SoundType::TacticalClick, 0.35f, 1.25f);
        }
    }

    // Interactive mouse scroll wheel navigation
    if (io.MouseWheel > 0.1f) {
        m_selectedWheelSector = (m_selectedWheelSector + (m_invertScroll ? 7 : 1)) % 8;
        AudioEngine::get().playSound(SoundType::TacticalClick, 0.45f, 1.2f);
    } else if (io.MouseWheel < -0.1f) {
        m_selectedWheelSector = (m_selectedWheelSector + (m_invertScroll ? 1 : 7)) % 8;
        AudioEngine::get().playSound(SoundType::TacticalClick, 0.45f, 1.2f);
    }

    // Inspect inventory and equipped slots for all 8 sectors
    auto& eqPrimary = player.getInventory().getEquipped(EquipSlot::PrimaryWeapon);
    auto& eqSecondary = player.getInventory().getEquipped(EquipSlot::SecondaryWeapon);

    int medicalSuppliesCount = 0;
    for (const auto& it : player.getInventory().getItems()) {
        if (it.category == ItemCategory::Medical) medicalSuppliesCount += it.quantity;
    }

    struct WheelSectorData {
        const char* slotCode;
        const char* category;
        std::string name;
        std::string caliber;
        std::string ammoText;
        float damage;
        float fireRate;
        float accuracy;
        float range;
        bool hasItem;
        std::optional<Item> itemRef;
    };

    WheelSectorData sectors[8];

    // Sector 0: Primary Assault Rifle / Carbine (12 o'clock)
    sectors[0].slotCode = "01";
    sectors[0].category = "PRIMARY WEAPON";
    if (eqPrimary.has_value()) {
        const auto& w = eqPrimary.value();
        sectors[0].name = w.name;
        sectors[0].caliber = w.caliber.empty() ? "5.56x45mm NATO" : w.caliber;
        int res = player.getInventory().getAmmoCount(w.weaponType);
        sectors[0].ammoText = "MAG: " + std::to_string(w.currentMag) + " / " + std::to_string(res) + " RDS";
        sectors[0].damage = std::clamp(w.damage / 70.0f, 0.15f, 1.0f);
        sectors[0].fireRate = std::clamp(w.fireRateRpm / 900.0f, 0.15f, 1.0f);
        sectors[0].accuracy = std::clamp(w.ergonomics / 100.0f, 0.20f, 1.0f);
        sectors[0].range = std::clamp(w.effectiveRange / 600.0f, 0.15f, 1.0f);
        sectors[0].hasItem = true;
        sectors[0].itemRef = w;
    } else {
        sectors[0].name = "EMPTY PRIMARY SLOT";
        sectors[0].caliber = "NO FIREARM EQUIPPED";
        sectors[0].ammoText = "0 / 0 RDS";
        sectors[0].damage = 0.0f; sectors[0].fireRate = 0.0f; sectors[0].accuracy = 0.0f; sectors[0].range = 0.0f;
        sectors[0].hasItem = false;
    }

    // Sector 1: Marksman / Sniper Rifle (1:30 o'clock)
    sectors[1].slotCode = "02";
    sectors[1].category = "MARKSMAN / SNIPER";
    if (eqSecondary.has_value() && (eqSecondary->weaponType == WeaponType::Sniper || eqSecondary->name.find("Mosin") != std::string::npos || eqSecondary->name.find("SVD") != std::string::npos)) {
        const auto& w = eqSecondary.value();
        sectors[1].name = w.name;
        sectors[1].caliber = w.caliber.empty() ? "7.62x54mmR" : w.caliber;
        int res = player.getInventory().getAmmoCount(w.weaponType);
        sectors[1].ammoText = "MAG: " + std::to_string(w.currentMag) + " / " + std::to_string(res) + " RDS";
        sectors[1].damage = std::clamp(w.damage / 70.0f, 0.20f, 1.0f);
        sectors[1].fireRate = std::clamp(w.fireRateRpm / 900.0f, 0.10f, 1.0f);
        sectors[1].accuracy = std::clamp(w.ergonomics / 100.0f, 0.30f, 1.0f);
        sectors[1].range = std::clamp(w.effectiveRange / 600.0f, 0.30f, 1.0f);
        sectors[1].hasItem = true;
        sectors[1].itemRef = w;
    } else {
        sectors[1].name = "LONG-RANGE DMR";
        sectors[1].caliber = "7.62x54mmR / .308 WIN";
        sectors[1].ammoText = "STANDBY OPTIC";
        sectors[1].damage = 0.85f; sectors[1].fireRate = 0.35f; sectors[1].accuracy = 0.92f; sectors[1].range = 0.95f;
        sectors[1].hasItem = false;
    }

    // Sector 2: Secondary / CQB Firearm (3 o'clock)
    sectors[2].slotCode = "03";
    sectors[2].category = "SECONDARY / CQB";
    if (eqSecondary.has_value()) {
        const auto& w = eqSecondary.value();
        sectors[2].name = w.name;
        sectors[2].caliber = w.caliber.empty() ? "9x19mm Parabellum" : w.caliber;
        int res = player.getInventory().getAmmoCount(w.weaponType);
        sectors[2].ammoText = "MAG: " + std::to_string(w.currentMag) + " / " + std::to_string(res) + " RDS";
        sectors[2].damage = std::clamp(w.damage / 70.0f, 0.15f, 1.0f);
        sectors[2].fireRate = std::clamp(w.fireRateRpm / 900.0f, 0.15f, 1.0f);
        sectors[2].accuracy = std::clamp(w.ergonomics / 100.0f, 0.20f, 1.0f);
        sectors[2].range = std::clamp(w.effectiveRange / 600.0f, 0.15f, 1.0f);
        sectors[2].hasItem = true;
        sectors[2].itemRef = w;
    } else {
        sectors[2].name = "SIDEARM PISTOL";
        sectors[2].caliber = "9x19mm PARABELLUM";
        sectors[2].ammoText = "BACKUP READY";
        sectors[2].damage = 0.40f; sectors[2].fireRate = 0.65f; sectors[2].accuracy = 0.70f; sectors[2].range = 0.30f;
        sectors[2].hasItem = false;
    }

    // Sector 3: Fortifications & Tactical Barriers (4:30 o'clock)
    sectors[3].slotCode = "04";
    sectors[3].category = "FORTIFICATION";
    sectors[3].name = "T-SHAPED SANDBAG WALL";
    sectors[3].caliber = "MIL-SPEC COMBAT BARRICADE";
    sectors[3].ammoText = "READY TO BUILD [4]";
    sectors[3].damage = 0.90f; // Represents barrier durability 100HP
    sectors[3].fireRate = 0.20f;
    sectors[3].accuracy = 0.85f; // Heavy 55% cover
    sectors[3].range = 0.35f;
    sectors[3].hasItem = true;

    // Sector 4: Tactical Sling / Holster (6 o'clock)
    sectors[4].slotCode = "05";
    sectors[4].category = "SLING ARMS";
    sectors[4].name = "HOLSTER & COMBAT SPRINT";
    sectors[4].caliber = "FREE HANDS / LOW PROFILE";
    sectors[4].ammoText = "MOBILITY BOOST +15%";
    sectors[4].damage = 0.05f;
    sectors[4].fireRate = 0.05f;
    sectors[4].accuracy = 0.95f;
    sectors[4].range = 0.05f;
    sectors[4].hasItem = true;

    // Sector 5: Melee / Tactical Combat Blade (7:30 o'clock)
    sectors[5].slotCode = "06";
    sectors[5].category = "MELEE COMBAT";
    sectors[5].name = "KA-BAR COMBAT TANTO";
    sectors[5].caliber = "CARBON STEEL FIXED BLADE";
    sectors[5].ammoText = "SILENT EXECUTION";
    sectors[5].damage = 0.68f;
    sectors[5].fireRate = 0.50f;
    sectors[5].accuracy = 0.98f;
    sectors[5].range = 0.12f;
    sectors[5].hasItem = true;

    // Sector 6: Tactical First Aid & IFAK (9 o'clock)
    sectors[6].slotCode = "07";
    sectors[6].category = "TACTICAL FIRST AID";
    sectors[6].name = "IFAK TRAUMA MEDICAL PACK";
    sectors[6].caliber = "HEMOSTATIC GAUZE & TOURNIQUET";
    sectors[6].ammoText = (medicalSuppliesCount > 0) ? ("SUPPLIES AVAILABLE (" + std::to_string(medicalSuppliesCount) + ")") : "DEPLETED";
    sectors[6].damage = 0.75f; // Heal amount
    sectors[6].fireRate = 0.80f; // Bleed stop
    sectors[6].accuracy = 0.90f;
    sectors[6].range = 0.40f;
    sectors[6].hasItem = (medicalSuppliesCount > 0);

    // Sector 7: Combat Rig Ammo Resupply (10:30 o'clock)
    sectors[7].slotCode = "08";
    sectors[7].category = "TACTICAL RESUPPLY";
    sectors[7].name = "CHEST RIG AMMO CARRIER";
    sectors[7].caliber = "STANAG / PMAG COMBAT RELOAD";
    sectors[7].ammoText = "COMBAT READY [R]";
    sectors[7].damage = 0.45f;
    sectors[7].fireRate = 0.90f;
    sectors[7].accuracy = 0.85f;
    sectors[7].range = 0.60f;
    sectors[7].hasItem = true;

    // Draw 8 annular sectors
    for (int i = 0; i < 8; ++i) {
        float midAngle = -PI_CONST * 0.5f + i * (PI_CONST * 2.0f / 8.0f);
        float halfSector = (PI_CONST * 2.0f / 8.0f) * 0.5f;
        float margin = 0.025f; // Clean angular gap between wedges
        float sAngle = midAngle - halfSector + margin;
        float eAngle = midAngle + halfSector - margin;

        bool isSelected = (i == m_selectedWheelSector);

        // Selected wedge radially pops outward by 14px like GTA 5!
        float pop = isSelected ? (14.0f * ease) : 0.0f;
        ImVec2 popOff(std::cos(midAngle) * pop, std::sin(midAngle) * pop);

        ImU32 fillColor = isSelected ? IM_COL32(42, 32, 18, 240) : IM_COL32(16, 20, 26, 215);
        ImU32 borderColor = isSelected ? IM_COL32(255, 175, 42, 255) : IM_COL32(50, 62, 78, 170);

        // Build polygon mesh for the annular slice
        const int arcDivs = 14;
        std::vector<ImVec2> poly;
        poly.reserve(arcDivs * 2 + 2);

        for (int s = 0; s <= arcDivs; ++s) {
            float a = sAngle + (eAngle - sAngle) * static_cast<float>(s) / static_cast<float>(arcDivs);
            poly.push_back(ImVec2(center.x + popOff.x + std::cos(a) * outerR,
                                  center.y + popOff.y + std::sin(a) * outerR));
        }
        for (int s = arcDivs; s >= 0; --s) {
            float a = sAngle + (eAngle - sAngle) * static_cast<float>(s) / static_cast<float>(arcDivs);
            poly.push_back(ImVec2(center.x + popOff.x + std::cos(a) * innerR,
                                  center.y + popOff.y + std::sin(a) * innerR));
        }

        // Render filled quad strips for smooth visual fill
        for (int s = 0; s < arcDivs; ++s) {
            ImVec2 o0 = poly[s];
            ImVec2 o1 = poly[s + 1];
            ImVec2 i1 = poly[poly.size() - 2 - s];
            ImVec2 i0 = poly[poly.size() - 1 - s];
            dl->AddQuadFilled(i0, o0, o1, i1, fillColor);
        }

        // Sector outline
        dl->AddPolyline(poly.data(), static_cast<int>(poly.size()), borderColor, ImDrawFlags_Closed, isSelected ? 2.2f : 1.0f);

        // Glowing outer accent arc for selected wedge
        if (isSelected) {
            std::vector<ImVec2> outerGlow;
            outerGlow.reserve(arcDivs + 1);
            for (int s = 0; s <= arcDivs; ++s) {
                float a = sAngle + (eAngle - sAngle) * static_cast<float>(s) / static_cast<float>(arcDivs);
                outerGlow.push_back(ImVec2(center.x + popOff.x + std::cos(a) * (outerR + 2.0f),
                                           center.y + popOff.y + std::sin(a) * (outerR + 2.0f)));
            }
            dl->AddPolyline(outerGlow.data(), static_cast<int>(outerGlow.size()), IM_COL32(255, 215, 60, 255), 0, 3.5f);
        }

        // Sector label & item icon in wedge center
        float midR = (innerR + outerR) * 0.5f;
        ImVec2 wedgePos(center.x + popOff.x + std::cos(midAngle) * midR,
                        center.y + popOff.y + std::sin(midAngle) * midR);

        // Slot number code (top of wedge)
        ImVec2 codeSz = ImGui::CalcTextSize(sectors[i].slotCode);
        dl->AddText(ImVec2(wedgePos.x - codeSz.x * 0.5f, wedgePos.y - 30.0f),
                    isSelected ? IM_COL32(255, 195, 55, 255) : IM_COL32(140, 150, 165, 180),
                    sectors[i].slotCode);

        // High-resolution procedural vector icon in wedge center
        drawWheelVectorIcon(dl, i, ImVec2(wedgePos.x, wedgePos.y - 6.0f), 0.95f, isSelected, sectors[i].hasItem, sectors[i].name);

        // Clean, crisp category title at bottom of wedge
        const char* shortCat = "";
        switch (i) {
            case 0: shortCat = "PRIMARY"; break;
            case 1: shortCat = "SNIPER DMR"; break;
            case 2: shortCat = "SIDEARM"; break;
            case 3: shortCat = "FORTIFY"; break;
            case 4: shortCat = "SLING ARMS"; break;
            case 5: shortCat = "MELEE"; break;
            case 6: shortCat = "FIRST AID"; break;
            case 7: shortCat = "RESUPPLY"; break;
            default: shortCat = sectors[i].category; break;
        }
        ImVec2 catSz = ImGui::CalcTextSize(shortCat);
        dl->AddText(ImVec2(wedgePos.x - catSz.x * 0.5f, wedgePos.y + 16.0f),
                    isSelected ? IM_COL32(255, 255, 255, 255) : IM_COL32(165, 180, 198, 200),
                    shortCat);
    }

    // ================= CENTER HUB: GTA 5 ATTRIBUTE DOSSIER =================
    dl->AddCircleFilled(center, hubR, IM_COL32(12, 16, 22, 250));
    dl->AddCircle(center, hubR, IM_COL32(255, 175, 45, 220), 64, 2.0f);
    dl->AddCircle(center, hubR - 4.0f, IM_COL32(255, 175, 45, 60), 64, 1.0f);

    const auto& sel = sectors[m_selectedWheelSector];

    // 1. Large High-Resolution Graphical Weapon Silhouette / Icon in Center Hub
    drawWheelVectorIcon(dl, m_selectedWheelSector, ImVec2(center.x, center.y - 56.0f), 1.55f, true, sel.hasItem, sel.name);

    // 2. Weapon / Item Name
    ImVec2 nSz = ImGui::CalcTextSize(sel.name.c_str());
    dl->AddText(ImVec2(center.x - nSz.x * 0.5f, center.y - 32.0f), IM_COL32(255, 255, 255, 255), sel.name.c_str());

    // 3. Caliber / Ammo Status
    std::string detailSub = sel.caliber;
    if (detailSub.length() > 25) detailSub = detailSub.substr(0, 23) + "..";
    ImVec2 cSz = ImGui::CalcTextSize(detailSub.c_str());
    dl->AddText(ImVec2(center.x - cSz.x * 0.5f, center.y - 18.0f), IM_COL32(145, 200, 255, 220), detailSub.c_str());

    // Helper for GTA 5 attribute progress bars
    auto drawStatBar = [&](const char* label, float fraction, float yPos, ImU32 fillCol) {
        float barX = center.x - 75.0f;
        float barW = 150.0f;
        float barH = 4.5f;
        dl->AddText(ImVec2(barX, yPos - 9.0f), IM_COL32(165, 175, 190, 210), label);

        // Background slot
        dl->AddRectFilled(ImVec2(barX, yPos), ImVec2(barX + barW, yPos + barH), IM_COL32(24, 30, 38, 255), 2.0f);
        // Filled bar
        float filledW = barW * std::clamp(fraction, 0.04f, 1.0f);
        dl->AddRectFilled(ImVec2(barX, yPos), ImVec2(barX + filledW, yPos + barH), fillCol, 2.0f);
        // Subtle tick at right edge
        dl->AddLine(ImVec2(barX + filledW, yPos - 1.0f), ImVec2(barX + filledW, yPos + barH + 1.0f), IM_COL32(255, 255, 255, 220), 1.5f);
    };

    // 4 GTA 5 Weapon Stat Bars
    drawStatBar("DAMAGE", sel.damage, center.y + 4.0f, IM_COL32(255, 115, 45, 255));
    drawStatBar("FIRE RATE", sel.fireRate, center.y + 21.0f, IM_COL32(255, 175, 45, 255));
    drawStatBar("ACCURACY", sel.accuracy, center.y + 38.0f, IM_COL32(75, 210, 140, 255));
    drawStatBar("RANGE", sel.range, center.y + 55.0f, IM_COL32(70, 170, 255, 255));

    // Footer instruction
    const char* hint = "[ MOUSE / SCROLL ] SELECT   *   [ RELEASE TAB ] EQUIP";
    ImVec2 hintSz = ImGui::CalcTextSize(hint);
    dl->AddText(ImVec2(center.x - hintSz.x * 0.5f, center.y + 74.0f), IM_COL32(160, 170, 185, 200), hint);

    ImGui::End();
}

// =========================================================================
// 9. CASUALTY REPORT (FULL LOOT LOSS ON COMBAT DEATH)
// =========================================================================
void UIManager::renderInteractionPrompt(Player& player, World& world, const Camera* camera, float aspectRatio, FrontlineWarfare* frontline) {
    if (m_gameState != GameState::InGame) return;

    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // 0. Mounted Heavy Machine Gun (DShK 12.7mm) HUD
    if (player.isMounted()) {
        MountedTurret* turret = player.getActiveTurret();
        int ammo = turret ? turret->ammo : 0;
        int maxAmmo = turret ? turret->maxAmmo : 150;
        char hmgText[256];
        std::snprintf(hmgText, sizeof(hmgText), "[DShK 12.7mm HMG] %d / %d RDS   |   [LEFT-CLICK] FIRE AP ROUNDS   |   [F / SPACE] DISMOUNT", ammo, maxAmmo);
        ImVec2 hmgSz = ImGui::CalcTextSize(hmgText);
        float hmgW = hmgSz.x + 40.0f;
        float hmgH = 34.0f;
        float hmgX = (screenW - hmgW) * 0.5f;
        float hmgY = screenH - 165.0f;
        ImVec2 hMin(hmgX, hmgY);
        ImVec2 hMax(hmgX + hmgW, hmgY + hmgH);
        dl->AddRectFilled(hMin, hMax, IM_COL32(20, 16, 12, 235), 6.0f);
        dl->AddRect(hMin, hMax, IM_COL32(255, 140, 30, 240), 6.0f, 0, 1.8f);
        dl->AddText(ImVec2(hmgX + 20.0f, hmgY + (hmgH - hmgSz.y) * 0.5f), IM_COL32(255, 235, 200, 255), hmgText);
        return;
    }

    // 0B. Prompt to Mount Nearby Heavy Machine Gun
    MountedTurret* nearTurret = world.getNearbyTurret(player.getPosition(), 2.2f);
    if (nearTurret && !nearTurret->isOccupied) {
        char mntText[128];
        std::snprintf(mntText, sizeof(mntText), "[F] MOUNT %s (%d RDS)", nearTurret->name.c_str(), nearTurret->ammo);
        ImVec2 mntSz = ImGui::CalcTextSize(mntText);
        float mntW = mntSz.x + 36.0f;
        float mntH = 32.0f;
        float mntX = (screenW - mntW) * 0.5f;
        float mntY = screenH - 165.0f;
        ImVec2 mMin(mntX, mntY);
        ImVec2 mMax(mntX + mntW, mntY + mntH);
        dl->AddRectFilled(mMin, mMax, IM_COL32(18, 16, 14, 230), 5.0f);
        dl->AddRect(mMin, mMax, IM_COL32(255, 160, 40, 240), 5.0f, 0, 1.5f);
        dl->AddText(ImVec2(mntX + 18.0f, mntY + (mntH - mntSz.y) * 0.5f), IM_COL32(255, 240, 210, 255), mntText);
    }

    // 0B-2. Prompt for Base Garrison NPCs (SSG Reeves & SGT Bishop)
    if (frontline && m_isTrainingMode) {
        for (const auto& bot : frontline->getBots()) {
            if (bot.state == BotState::Dead) continue;
            float distToNpc = glm::distance(player.getPosition(), bot.position);
            if (distToNpc <= 3.5f) {
                char npcPrompt[160] = {0};
                if (bot.callsign.find("Reeves") != std::string::npos) {
                    std::snprintf(npcPrompt, sizeof(npcPrompt), "[F] SSG REEVES: RESUPPLY MUNITIONS & GEAR");
                } else if (bot.callsign.find("Bishop") != std::string::npos) {
                    std::snprintf(npcPrompt, sizeof(npcPrompt), "[F] SGT BISHOP: RESET TARGET LANES");
                } else if (!bot.dialogue.empty()) {
                    std::snprintf(npcPrompt, sizeof(npcPrompt), "[F] TALK: %s %s", bot.rank.c_str(), bot.callsign.c_str());
                }

                if (npcPrompt[0] != '\0') {
                    ImVec2 npSz = ImGui::CalcTextSize(npcPrompt);
                    float npW = npSz.x + 36.0f;
                    float npH = 32.0f;
                    float npX = (screenW - npW) * 0.5f;
                    float npY = screenH - 165.0f;
                    dl->AddRectFilled(ImVec2(npX, npY), ImVec2(npX + npW, npY + npH), IM_COL32(14, 20, 28, 235), 5.0f);
                    dl->AddRect(ImVec2(npX, npY), ImVec2(npX + npW, npY + npH), IM_COL32(80, 190, 255, 240), 5.0f, 0, 1.5f);
                    dl->AddText(ImVec2(npX + 18.0f, npY + (npH - npSz.y) * 0.5f), IM_COL32(230, 245, 255, 255), npcPrompt);
                    break;
                }
            }
        }
    }

    // 0B-3. Tactical Target Hit Confirmation Banner
    if (m_targetHitBannerTimer > 0.0f) {
        m_targetHitBannerTimer = std::max(0.0f, m_targetHitBannerTimer - io.DeltaTime);
        float alpha = std::clamp(m_targetHitBannerTimer / 0.35f, 0.0f, 1.0f);
        if (m_targetHitBannerTimer > 2.0f) alpha = std::clamp((2.4f - m_targetHitBannerTimer) / 0.4f, 0.0f, 1.0f);

        ImVec2 banSz = ImGui::CalcTextSize(m_targetHitBannerText.c_str());
        float banW = banSz.x + 44.0f;
        float banH = 34.0f;
        float banX = (screenW - banW) * 0.5f;
        float banY = 120.0f;

        dl->AddRectFilled(ImVec2(banX, banY), ImVec2(banX + banW, banY + banH), IM_COL32(24, 20, 10, (int)(235 * alpha)), 5.0f);
        dl->AddRect(ImVec2(banX, banY), ImVec2(banX + banW, banY + banH), IM_COL32(255, 200, 50, (int)(245 * alpha)), 5.0f, 0, 1.6f);
        dl->AddText(ImVec2(banX + 22.0f, banY + (banH - banSz.y) * 0.5f), IM_COL32(255, 235, 160, (int)(255 * alpha)), m_targetHitBannerText.c_str());
    }

    // 0C. Active Raid Extraction Zone HUD & Progress
    ExtractionZone* activeExtract = world.getActiveExtractZone(player.getPosition());
    for (auto& zone : world.getExtractionZones()) {
        if (&zone == activeExtract) {
            zone.countdown = std::max(0.0f, zone.countdown - io.DeltaTime);
        } else {
            zone.countdown = std::min(zone.maxCountdown, zone.countdown + io.DeltaTime * 1.5f);
        }
    }

    if (activeExtract) {
        float prog = std::clamp(1.0f - (activeExtract->countdown / activeExtract->maxCountdown), 0.0f, 1.0f);
        char exfilText[256];
        std::snprintf(exfilText, sizeof(exfilText), ">>> EXTRACTION IN PROGRESS: %.1fs <<<   [%s]", activeExtract->countdown, activeExtract->name.c_str());
        ImVec2 exSz = ImGui::CalcTextSize(exfilText);
        float exW = std::max(exSz.x + 40.0f, 420.0f);
        float exH = 48.0f;
        float exX = (screenW - exW) * 0.5f;
        float exY = 70.0f; // Prominently framed at top center

        ImVec2 eMin(exX, exY);
        ImVec2 eMax(exX + exW, exY + exH);
        dl->AddRectFilled(eMin, eMax, IM_COL32(10, 28, 16, 240), 6.0f);
        dl->AddRect(eMin, eMax, IM_COL32(40, 240, 110, 245), 6.0f, 0, 2.0f);
        dl->AddText(ImVec2(exX + 20.0f, exY + 8.0f), IM_COL32(220, 255, 230, 255), exfilText);

        // Green Progress Bar
        float barX = exX + 20.0f;
        float barY = exY + 30.0f;
        float barW = exW - 40.0f;
        float barH = 8.0f;
        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(20, 45, 25, 255), 3.0f);
        dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * prog, barY + barH), IM_COL32(50, 255, 120, 255), 3.0f);

        // Completion Check
        if (activeExtract->countdown <= 0.0f) {
            m_gameState = GameState::Extracted;
            AudioEngine::get().playSound(SoundType::DeployHorn, 0.95f, 1.0f);

            CharacterProfile* active = ProfileManager::get().getActiveProfile();
            if (active) {
                active->raidsSurvived++;
                active->raidsTotal++;
                active->cash += 1500; // +$1500 Exfil Reward
                active->savedInventory = player.getInventory();
                ProfileManager::get().saveProfiles();
                SteamIntegration::get().unlockAchievement("ACH_FIRST_CONTRACT");
            }
            return;
        }
    }

    // 1. GTA 5 / Ghost Recon Tactical Cover HUD Indicator
    if (player.isInCover()) {
        const char* covText = "[Q] LEAVE COVER   [SPACE] VAULT / CLIMB   [R-CLICK] PEEK & AIM   [A / D] STRAFE WALL";
        ImVec2 covSz = ImGui::CalcTextSize(covText);
        float covW = covSz.x + 36.0f;
        float covH = 30.0f;
        float covX = (screenW - covW) * 0.5f;
        float covY = screenH - 165.0f;
        ImVec2 cMin(covX, covY);
        ImVec2 cMax(covX + covW, covY + covH);
        dl->AddRectFilled(cMin, cMax, IM_COL32(10, 24, 18, 230), 5.0f);
        dl->AddRect(cMin, cMax, IM_COL32(40, 220, 130, 240), 5.0f, 0, 1.5f);
        dl->AddText(ImVec2(covX + 18.0f, covY + (covH - covSz.y) * 0.5f), IM_COL32(230, 255, 240, 255), covText);
    } else if (player.isNearCover()) {
        const char* covText = "[Q] TAKE COVER";
        ImVec2 covSz = ImGui::CalcTextSize(covText);
        float covW = covSz.x + 30.0f;
        float covH = 28.0f;
        float covX = (screenW - covW) * 0.5f;
        float covY = screenH - 160.0f;
        ImVec2 cMin(covX, covY);
        ImVec2 cMax(covX + covW, covY + covH);
        dl->AddRectFilled(cMin, cMax, IM_COL32(14, 18, 28, 220), 5.0f);
        dl->AddRect(cMin, cMax, IM_COL32(80, 180, 255, 230), 5.0f, 0, 1.5f);
        dl->AddText(ImVec2(covX + 15.0f, covY + (covH - covSz.y) * 0.5f), IM_COL32(230, 245, 255, 255), covText);
    }

    // 1B. Advanced Tactical Aim System, Reticles & Crosshair HUD
    bool uiWantsMouse = ImGui::GetIO().WantCaptureMouse;

    // Hit Marker Tick Animation Logic
    if (m_hitMarkerTimer > 0.0f) {
        m_hitMarkerTimer = std::max(0.0f, m_hitMarkerTimer - io.DeltaTime);
    }

    // 3D Tactical Gray Aiming Ray replaces 2D screen crosshair (Third-Person View)
    // Only render hitmarker feedback on screen when bullets impact
    if (!player.isAiming() && !uiWantsMouse) {
        ImVec2 mp = ImVec2(screenW * 0.5f, screenH * 0.5f);

        // Hip-fire Hit Marker
        if (m_hitMarkerTimer > 0.0f) {
            float hmAlpha = m_hitMarkerTimer / 0.16f;
            ImU32 hmCol = m_hitMarkerCritical ? IM_COL32(255, 45, 45, (int)(255 * hmAlpha)) : IM_COL32(255, 255, 255, (int)(245 * hmAlpha));
            float sz = 8.0f;
            dl->AddLine(ImVec2(mp.x - sz, mp.y - sz), ImVec2(mp.x - 3.0f, mp.y - 3.0f), hmCol, 2.0f);
            dl->AddLine(ImVec2(mp.x + sz, mp.y - sz), ImVec2(mp.x + 3.0f, mp.y - 3.0f), hmCol, 2.0f);
            dl->AddLine(ImVec2(mp.x - sz, mp.y + sz), ImVec2(mp.x - 3.0f, mp.y + 3.0f), hmCol, 2.0f);
            dl->AddLine(ImVec2(mp.x + sz, mp.y + sz), ImVec2(mp.x + 3.0f, mp.y + 3.0f), hmCol, 2.0f);
            if (m_hitMarkerCritical) {
                dl->AddCircle(mp, 4.0f, IM_COL32(255, 45, 45, (int)(220 * hmAlpha)), 12, 1.5f);
            }
        }
    }

    // ADS Mode: 3D Aiming Ray acts as the ballistic crosshair. Keep sniper scope vignette & hit markers.
    if (player.isAiming()) {
        int zoomLvl = player.getAdsZoomLevel();
        if (camera) {
            glm::vec2 screenAimPos = glm::vec2(screenW * 0.5f, screenH * 0.5f);

            if (zoomLvl >= 2) {
                // 4x/6x Precision Scope: Peripheral scope shadow vignette
                dl->AddCircle(ImVec2(screenAimPos.x, screenAimPos.y), screenH * 0.42f, IM_COL32(10, 12, 16, 250), 64, 40.0f);
            }

            // Tactical Hit Marker Feedback on screen
            if (m_hitMarkerTimer > 0.0f) {
                float hmAlpha = m_hitMarkerTimer / 0.16f;
                ImU32 hmCol = m_hitMarkerCritical ? IM_COL32(255, 45, 45, (int)(255 * hmAlpha)) : IM_COL32(255, 255, 255, (int)(245 * hmAlpha));
                float sz = 8.0f;
                dl->AddLine(ImVec2(screenAimPos.x - sz, screenAimPos.y - sz), ImVec2(screenAimPos.x - 3.0f, screenAimPos.y - 3.0f), hmCol, 2.0f);
                dl->AddLine(ImVec2(screenAimPos.x + sz, screenAimPos.y - sz), ImVec2(screenAimPos.x + 3.0f, screenAimPos.y - 3.0f), hmCol, 2.0f);
                dl->AddLine(ImVec2(screenAimPos.x - sz, screenAimPos.y + sz), ImVec2(screenAimPos.x - 3.0f, screenAimPos.y + 3.0f), hmCol, 2.0f);
                dl->AddLine(ImVec2(screenAimPos.x + sz, screenAimPos.y + sz), ImVec2(screenAimPos.x + 3.0f, screenAimPos.y + 3.0f), hmCol, 2.0f);
            }
        }
    }

    SupplyCrate* crate = world.getNearbyCrate(player.getPosition(), 2.8f);
    if (!crate || crate->looted) return;

    float dist = glm::distance(player.getPosition(), crate->position);
    bool isGround = crate->isGroundItem;
    std::string action = isGround ? "PICK UP" : "SEARCH";
    std::string displayName = (!crate->contents.empty()) ? crate->contents[0].name : crate->name;

    // 1. 3D In-World Projected Floating Badge Hovering Above Ground Item / Crate
    if (camera) {
        glm::vec3 worldBadgePos = crate->position + glm::vec3(0.0f, isGround ? 0.35f : 0.75f, 0.0f);
        glm::vec2 screenBadgePos;
        if (camera->worldToScreen(worldBadgePos, aspectRatio, screenW, screenH, screenBadgePos)) {
            char badgeText[128];
            std::snprintf(badgeText, sizeof(badgeText), "[F] %s", action.c_str());
            ImVec2 txtSz = ImGui::CalcTextSize(badgeText);
            float padX = 10.0f, padY = 4.0f;
            ImVec2 pillMin(screenBadgePos.x - txtSz.x * 0.5f - padX, screenBadgePos.y - txtSz.y * 0.5f - padY);
            ImVec2 pillMax(screenBadgePos.x + txtSz.x * 0.5f + padX, screenBadgePos.y + txtSz.y * 0.5f + padY);

            dl->AddRectFilled(pillMin, pillMax, IM_COL32(18, 20, 24, 220), 4.0f);
            dl->AddRect(pillMin, pillMax, IM_COL32(235, 185, 55, 230), 4.0f, 0, 1.5f);
            dl->AddText(ImVec2(pillMin.x + padX, pillMin.y + padY), IM_COL32(255, 255, 255, 255), badgeText);
        }
    }

    // 2. High-Fidelity Tactical HUD Action Banner at Bottom Center
    char bannerText[256];
    std::snprintf(bannerText, sizeof(bannerText), "[F] %s %s (%.1fm)", action.c_str(), displayName.c_str(), dist);
    ImVec2 txtSz = ImGui::CalcTextSize(bannerText);

    float bannerW = txtSz.x + 40.0f;
    float bannerH = 34.0f;
    float bannerX = (screenW - bannerW) * 0.5f;
    float bannerY = screenH - 120.0f;

    ImVec2 bMin(bannerX, bannerY);
    ImVec2 bMax(bannerX + bannerW, bannerY + bannerH);

    dl->AddRectFilled(bMin, bMax, IM_COL32(14, 16, 20, 230), 6.0f);
    dl->AddRect(bMin, bMax, IM_COL32(235, 185, 55, 220), 6.0f, 0, 1.5f);

    // Glowing [F] Key Box
    float keyBoxW = 24.0f;
    float keyBoxH = 22.0f;
    ImVec2 kMin(bMin.x + 8.0f, bMin.y + (bannerH - keyBoxH) * 0.5f);
    ImVec2 kMax(kMin.x + keyBoxW, kMin.y + keyBoxH);
    dl->AddRectFilled(kMin, kMax, IM_COL32(235, 185, 55, 240), 3.0f);
    ImVec2 fTxtSz = ImGui::CalcTextSize("F");
    dl->AddText(ImVec2(kMin.x + (keyBoxW - fTxtSz.x) * 0.5f, kMin.y + (keyBoxH - fTxtSz.y) * 0.5f), IM_COL32(15, 15, 15, 255), "F");

    char promptBody[256];
    std::snprintf(promptBody, sizeof(promptBody), "%s %s (%.1fm)", action.c_str(), displayName.c_str(), dist);
    dl->AddText(ImVec2(kMax.x + 10.0f, bMin.y + (bannerH - txtSz.y) * 0.5f), IM_COL32(245, 245, 245, 255), promptBody);


}

// =========================================================================
// 9B. TACTICAL AUDIT & LOCOMOTION TELEMETRY INSPECTOR (F1 / F3)
// =========================================================================
void UIManager::renderOverheadNameplates(const Camera& camera, float aspectRatio, Player& player, FrontlineWarfare* frontline) {
    if (m_gameState == GameState::MainMenu && (m_currentTab != MainMenuTab::Overview && m_currentTab != MainMenuTab::Customize)) {
        return; // Suppress nameplate over full-screen deployment, traders, and map docks
    }

    // Suppress local player overhead nameplate while GTA 5 weapon wheel is open
    if (m_weaponWheelOpen) {
        m_playerNameplate.alpha = 0.0f;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float dt = io.DeltaTime > 0.0f ? std::min(io.DeltaTime, 0.05f) : 0.016f;
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    ProfileManager& pm = ProfileManager::get();
    CharacterProfile* active = pm.getActiveProfile();
    Faction playerFaction = active ? active->faction : player.getFaction();

    // ---------------------------------------------------------
    // 1. LOCAL PLAYER OVERHEAD NAMEPLATE
    // ---------------------------------------------------------
    {
        // Obtain the real-time skeletal 3D world position of the head bone
        glm::vec3 headBonePos = player.getHeadPosition();
        glm::vec3 playerHeadWorld;
        if (glm::length(headBonePos) > 0.1f) {
            // Position directly 0.28m above the helmet in exact 3D world space
            playerHeadWorld = headBonePos + glm::vec3(0.0f, 0.28f, 0.0f);
        } else {
            // Fallback before first rig deform
            float headOffset = (player.getStats().stance == PlayerStance::Prone) ? 0.35f :
                               ((player.getStats().stance == PlayerStance::Crouching) ? 1.35f : 1.72f);
            playerHeadWorld = player.getPosition() + glm::vec3(0.0f, headOffset, 0.0f);
        }

        glm::vec2 targetScreen(0.0f);
        bool onScreen = camera.worldToScreen(playerHeadWorld, aspectRatio, screenW, screenH, targetScreen);

        if (onScreen && m_gameState != GameState::GameOver && !m_weaponWheelOpen) {
            if (!m_playerNameplate.initialized) {
                m_playerNameplate.pos = targetScreen;
                m_playerNameplate.alpha = 1.0f;
                m_playerNameplate.initialized = true;
            } else {
                // Snappy 0-lag tracking: stays solidly anchored directly above helmet even during sprint
                m_playerNameplate.pos = glm::mix(m_playerNameplate.pos, targetScreen, std::clamp(dt * 45.0f, 0.0f, 1.0f));
                m_playerNameplate.alpha = glm::mix(m_playerNameplate.alpha, 1.0f, std::clamp(dt * 12.0f, 0.0f, 1.0f));
            }
        } else {
            m_playerNameplate.alpha = glm::mix(m_playerNameplate.alpha, 0.0f, std::clamp(dt * 18.0f, 0.0f, 1.0f));
        }

        if (m_playerNameplate.alpha > 0.02f) {
            float a = m_playerNameplate.alpha;
            int lvl = active ? active->level : 1;
            std::string rankStr = getRankTitle(lvl, playerFaction);
            std::string nameStr = (active && !active->callsign.empty()) ? active->callsign : "NOMAD";

            ImVec2 rankSize = ImGui::CalcTextSize(rankStr.c_str());
            ImVec2 nameSize = ImGui::CalcTextSize(nameStr.c_str());

            float padX = 6.0f;
            float pillH = 19.0f;
            float rankW = rankSize.x + 8.0f;
            float totalW = rankW + nameSize.x + padX * 2.0f + 4.0f;

            // Center cleanly above head
            float px = m_playerNameplate.pos.x - totalW * 0.5f;
            float py = m_playerNameplate.pos.y - pillH - 8.0f;

            // Softly clamp within viewport bounds
            px = std::clamp(px, 10.0f, screenW - totalW - 10.0f);
            py = std::clamp(py, 10.0f, screenH - pillH - 10.0f);

            ImVec2 pillMin(px, py);
            ImVec2 pillMax(px + totalW, py + pillH);

            // Frosted glass background pill
            ImU32 bgCol = IM_COL32(10, 14, 18, (int)(180 * a));
            ImU32 borderCol = IM_COL32(255, 255, 255, (int)(38 * a));
            dl->AddRectFilled(pillMin, pillMax, bgCol, 4.0f);
            dl->AddRect(pillMin, pillMax, borderCol, 4.0f, 0, 1.0f);

            // Rank Badge Chip
            ImVec2 chipMin = pillMin;
            ImVec2 chipMax(pillMin.x + rankW, pillMax.y);
            ImU32 chipBg = (playerFaction == Faction::FactionB) ? IM_COL32(85, 30, 30, (int)(210 * a)) : IM_COL32(30, 55, 88, (int)(210 * a));
            ImU32 rankCol = (playerFaction == Faction::FactionB) ? IM_COL32(245, 95, 90, (int)(245 * a)) : IM_COL32(250, 205, 65, (int)(245 * a));

            dl->AddRectFilled(chipMin, chipMax, chipBg, 4.0f, ImDrawFlags_RoundCornersLeft);
            dl->AddText(ImVec2(chipMin.x + 4.0f, chipMin.y + (pillH - rankSize.y) * 0.5f), rankCol, rankStr.c_str());

            // Name / Callsign Text (with subtle drop shadow)
            float nameX = chipMax.x + 5.0f;
            float nameY = pillMin.y + (pillH - nameSize.y) * 0.5f;
            dl->AddText(ImVec2(nameX + 1.0f, nameY + 1.0f), IM_COL32(0, 0, 0, (int)(170 * a)), nameStr.c_str());
            dl->AddText(ImVec2(nameX, nameY), IM_COL32(238, 242, 248, (int)(240 * a)), nameStr.c_str());

            // Razor-thin Vitality Line along bottom (1.5px)
            float hpNorm = std::clamp(player.getStats().health / 100.0f, 0.0f, 1.0f);
            ImU32 hpColor = (hpNorm > 0.40f) ? IM_COL32(65, 215, 125, (int)(215 * a)) :
                            ((hpNorm > 0.20f) ? IM_COL32(235, 185, 50, (int)(225 * a)) : IM_COL32(235, 60, 50, (int)(235 * a)));
            dl->AddLine(ImVec2(pillMin.x + 2.0f, pillMax.y - 1.0f), ImVec2(pillMin.x + 2.0f + (totalW - 4.0f) * hpNorm, pillMax.y - 1.0f), hpColor, 1.5f);

                        // Local Player Overhead Voice Indicator
            if (VoiceChatManager::get().isTransmitting()) {
                float wavePulse = std::sin(ImGui::GetTime() * 14.0f) * 2.0f;
                dl->AddCircle(ImVec2(m_playerNameplate.pos.x, pillMin.y - 10.0f), 4.5f + wavePulse, IM_COL32(65, 225, 130, 240), 12, 1.5f);
                dl->AddCircleFilled(ImVec2(m_playerNameplate.pos.x, pillMin.y - 10.0f), 2.5f, IM_COL32(65, 225, 130, 255));
            }

            // Subtle locator chevron pointing down to helmet
            float cx = m_playerNameplate.pos.x;
            float cy = pillMax.y;
            dl->AddTriangleFilled(ImVec2(cx - 3.5f, cy), ImVec2(cx + 3.5f, cy), ImVec2(cx, cy + 3.0f), bgCol);
            dl->AddLine(ImVec2(cx - 3.5f, cy), ImVec2(cx, cy + 3.0f), borderCol, 1.0f);
            dl->AddLine(ImVec2(cx + 3.5f, cy), ImVec2(cx, cy + 3.0f), borderCol, 1.0f);
        }
    }

    // ---------------------------------------------------------
    // 2. FRONTLINE BOTS OVERHEAD NAMEPLATES (IN-GAME ONLY)
    // ---------------------------------------------------------
    if (frontline && m_gameState == GameState::InGame) {
        auto& bots = frontline->getBots();
        for (auto& bot : bots) {
            if (bot.state == BotState::Dead) {
                // Fade out dead bot plates
                if (m_botNameplates.find(bot.id) != m_botNameplates.end()) {
                    m_botNameplates[bot.id].alpha = glm::mix(m_botNameplates[bot.id].alpha, 0.0f, std::clamp(dt * 12.0f, 0.0f, 1.0f));
                }
                continue;
            }

            float distToCam = glm::distance(camera.getPosition(), bot.position);
            bool isFriendly = (bot.faction == playerFaction);

            // Proximity culling: Nameplates only show when close by (within 16m for allies, 10m for enemies)
            float maxVisDist = isFriendly ? 16.0f : 10.0f;
            if (distToCam > maxVisDist) {
                if (m_botNameplates.find(bot.id) != m_botNameplates.end()) {
                    m_botNameplates[bot.id].alpha = 0.0f;
                }
                continue;
            }

            // Enemy visibility check: only show if noticed player or engaged close-quarters
            if (!isFriendly) {
                if (!bot.isVisibleToPlayer) continue;
                if (!bot.isSpottedViaRadio && !bot.hasNoticedPlayer && distToCam > 8.0f) continue;
            }

            float headY = (bot.state == BotState::WoundedDown) ? 0.35f : (bot.isCrouched ? 1.50f : 1.90f);
            glm::vec3 botHeadWorld = bot.position + glm::vec3(0.0f, headY, 0.0f);

            glm::vec2 targetScreen(0.0f);
            bool onScreen = camera.worldToScreen(botHeadWorld, aspectRatio, screenW, screenH, targetScreen);

            SmoothedNameplate& plate = m_botNameplates[bot.id];

            if (onScreen) {
                float fadeStart = isFriendly ? 10.0f : 6.0f;
                float distFade = 1.0f - std::clamp((distToCam - fadeStart) / (maxVisDist - fadeStart), 0.0f, 1.0f);
                float targetAlpha = distFade;

                if (!plate.initialized) {
                    plate.pos = targetScreen;
                    plate.alpha = targetAlpha;
                    plate.initialized = true;
                } else {
                    plate.pos = glm::mix(plate.pos, targetScreen, std::clamp(dt * 15.0f, 0.0f, 1.0f));
                    plate.alpha = glm::mix(plate.alpha, targetAlpha, std::clamp(dt * 8.0f, 0.0f, 1.0f));
                }
            } else {
                plate.alpha = glm::mix(plate.alpha, 0.0f, std::clamp(dt * 12.0f, 0.0f, 1.0f));
            }

            if (plate.alpha > 0.02f) {
                float a = plate.alpha;
                std::string rankStr = bot.rank;
                std::string callStr = bot.callsign;

                if (m_isTrainingMode && (bot.isPatrolBot || bot.isStationarySentry)) {
                    rankStr = bot.assignment;
                } else if (m_isTrainingMode && (bot.isCqbAggressor || !isFriendly)) {
                    rankStr = "OPFOR";
                }

                // Tactical role prefix
                std::string rolePrefix = "";
                if (bot.role == CombatRole::SquadLeader) rolePrefix = "* ";
                else if (bot.role == CombatRole::Medic) rolePrefix = "+ ";

                std::string fullCall = rolePrefix + callStr;

                ImVec2 rankSize = ImGui::CalcTextSize(rankStr.c_str());
                ImVec2 nameSize = ImGui::CalcTextSize(fullCall.c_str());

                float pillH = 17.0f;
                float rankW = rankSize.x + 6.0f;
                float totalW = rankW + nameSize.x + 12.0f;

                // Speech dialogue bubble if player is close in training mode
                float distToPlayerBot = glm::distance(player.getPosition(), bot.position);
                if (m_isTrainingMode && distToPlayerBot < 4.5f && !bot.dialogue.empty()) {
                    ImVec2 diagSz = ImGui::CalcTextSize(bot.dialogue.c_str());
                    float bubbleW = diagSz.x + 16.0f;
                    float bubbleH = diagSz.y + 10.0f;
                    float bubbleX = plate.pos.x - bubbleW * 0.5f;
                    float bubbleY = plate.pos.y - 42.0f;
                    ImVec2 bMin(bubbleX, bubbleY);
                    ImVec2 bMax(bubbleX + bubbleW, bubbleY + bubbleH);
                    dl->AddRectFilled(bMin, bMax, IM_COL32(14, 18, 24, (int)(220 * a)), 4.0f);
                    dl->AddRect(bMin, bMax, IM_COL32(235, 185, 55, (int)(230 * a)), 4.0f, 0, 1.2f);
                    dl->AddText(ImVec2(bubbleX + 8.0f, bubbleY + 5.0f), IM_COL32(250, 245, 230, (int)(245 * a)), bot.dialogue.c_str());
                }

                float px = plate.pos.x - totalW * 0.5f;
                float py = plate.pos.y - 14.0f;
                px = std::clamp(px, 10.0f, screenW - totalW - 10.0f);
                py = std::clamp(py, 10.0f, screenH - pillH - 10.0f);

                ImVec2 pillMin(px, py);
                ImVec2 pillMax(px + totalW, py + pillH);

                ImU32 bgCol = IM_COL32(12, 16, 20, (int)(170 * a));
                bool isCrimson = (bot.faction == Faction::FactionB);
                ImU32 borderCol = isCrimson ? IM_COL32(235, 75, 70, (int)(80 * a)) : IM_COL32(75, 175, 245, (int)(80 * a));
                dl->AddRectFilled(pillMin, pillMax, bgCol, 3.5f);
                dl->AddRect(pillMin, pillMax, borderCol, 3.5f, 0, 1.0f);

                // Chip
                ImVec2 chipMin = pillMin;
                ImVec2 chipMax(pillMin.x + rankW, pillMax.y);
                ImU32 chipBg = isCrimson ? IM_COL32(75, 22, 22, (int)(200 * a)) : IM_COL32(22, 45, 75, (int)(200 * a));
                ImU32 rankCol = isCrimson ? IM_COL32(245, 105, 100, (int)(240 * a)) : IM_COL32(110, 195, 255, (int)(240 * a));

                dl->AddRectFilled(chipMin, chipMax, chipBg, 3.5f, ImDrawFlags_RoundCornersLeft);
                dl->AddText(ImVec2(chipMin.x + 3.0f, chipMin.y + (pillH - rankSize.y) * 0.5f), rankCol, rankStr.c_str());

                // Name
                float nameX = chipMax.x + 4.0f;
                float nameY = pillMin.y + (pillH - nameSize.y) * 0.5f;
                ImU32 nameCol = isCrimson ? IM_COL32(245, 215, 215, (int)(230 * a)) : IM_COL32(225, 235, 245, (int)(230 * a));
                dl->AddText(ImVec2(nameX + 1.0f, nameY + 1.0f), IM_COL32(0, 0, 0, (int)(160 * a)), fullCall.c_str());
                dl->AddText(ImVec2(nameX, nameY), nameCol, fullCall.c_str());

                // Bot Health micro-line
                float botHpNorm = std::clamp(bot.health / bot.maxHealth, 0.0f, 1.0f);
                ImU32 hpColor = isFriendly ? IM_COL32(65, 215, 125, (int)(200 * a)) : IM_COL32(235, 80, 70, (int)(200 * a));
                dl->AddLine(ImVec2(pillMin.x + 2.0f, pillMax.y - 1.0f), ImVec2(pillMin.x + 2.0f + (totalW - 4.0f) * botHpNorm, pillMax.y - 1.0f), hpColor, 1.0f);

                                // Remote Bot Overhead Voice Indicator
                const auto& spkList = VoiceChatManager::get().getActiveSpeakers();
                auto spkIt = spkList.find(bot.id);
                if (spkIt != spkList.end() && spkIt->second.isSpeaking) {
                    float pulse = std::sin(ImGui::GetTime() * 12.0f) * 2.5f;
                    dl->AddCircle(ImVec2(plate.pos.x, pillMin.y - 9.0f), 4.5f + pulse, IM_COL32(245, 195, 65, 240), 12, 1.5f);
                    dl->AddCircleFilled(ImVec2(plate.pos.x, pillMin.y - 9.0f), 2.5f, IM_COL32(245, 195, 65, 255));
                }

                // Small tactical status indicator if pinned or down
                if (bot.state == BotState::WoundedDown) {
                    ImVec2 statPos(pillMin.x, pillMin.y - 13.0f);
                    dl->AddText(statPos, IM_COL32(245, 75, 75, (int)(240 * a)), "[DOWN / WOUNDED]");
                } else if (bot.isPinned) {
                    ImVec2 statPos(pillMin.x, pillMin.y - 13.0f);
                    dl->AddText(statPos, IM_COL32(245, 185, 45, (int)(240 * a)), "[PINNED]");
                }
            }
        }

        // Remote MMO Players Overhead Nameplates
        NetworkEngine& net = NetworkEngine::get();
        for (const auto& [id, remote] : net.getRemotePlayers()) {
            if (!remote.active) continue;
            float rHeadY = (remote.currentState.stance == 2) ? 0.45f : ((remote.currentState.stance == 1) ? 1.45f : 1.95f);
            glm::vec3 rHeadWorld = remote.currentState.position + glm::vec3(0.0f, rHeadY, 0.0f);
            float distToCam = glm::distance(camera.getPosition(), remote.currentState.position);
            if (distToCam > 16.0f) continue;

            glm::vec2 targetScreen(0.0f);
            if (camera.worldToScreen(rHeadWorld, aspectRatio, screenW, screenH, targetScreen)) {
                float rDistFade = 1.0f - std::clamp((distToCam - 10.0f) / 6.0f, 0.0f, 1.0f);
                if (rDistFade <= 0.02f) continue;
                bool isFriendly = (remote.currentState.faction == (playerFaction == Faction::FactionB ? 1 : 0));
                std::string callStr = remote.currentState.callsign[0] ? remote.currentState.callsign : ("OPERATIVE_0" + std::to_string(id));
                std::string rankStr = isFriendly ? "SQUAD" : "ENEMY";

                ImVec2 rankSize = ImGui::CalcTextSize(rankStr.c_str());
                ImVec2 nameSize = ImGui::CalcTextSize(callStr.c_str());
                float rankW = rankSize.x + 8.0f;
                float nameW = nameSize.x + 10.0f;
                float totalW = rankW + nameW;
                float pillH = 18.0f;

                float px = targetScreen.x - totalW * 0.5f;
                float py = targetScreen.y - 14.0f;
                ImVec2 pillMin(px, py);
                ImVec2 pillMax(px + totalW, py + pillH);

                ImU32 bgCol = IM_COL32(12, 16, 20, 210);
                bool isCrimson = (remote.currentState.faction == 1);
                ImU32 borderCol = isCrimson ? IM_COL32(235, 75, 70, 200) : IM_COL32(75, 175, 245, 200);
                dl->AddRectFilled(pillMin, pillMax, bgCol, 3.5f);
                dl->AddRect(pillMin, pillMax, borderCol, 3.5f, 0, 1.0f);

                // Chip
                ImVec2 chipMin = pillMin;
                ImVec2 chipMax(pillMin.x + rankW, pillMax.y);
                ImU32 chipBg = isCrimson ? IM_COL32(115, 25, 25, 230) : IM_COL32(22, 65, 115, 230);
                ImU32 rankCol = isCrimson ? IM_COL32(255, 115, 110, 255) : IM_COL32(110, 205, 255, 255);
                dl->AddRectFilled(chipMin, chipMax, chipBg, 3.5f, ImDrawFlags_RoundCornersLeft);
                dl->AddText(ImVec2(chipMin.x + 4.0f, chipMin.y + (pillH - rankSize.y) * 0.5f), rankCol, rankStr.c_str());

                // Name
                float nameX = chipMax.x + 5.0f;
                float nameY = pillMin.y + (pillH - nameSize.y) * 0.5f;
                ImU32 nameCol = isCrimson ? IM_COL32(255, 225, 225, 255) : IM_COL32(235, 245, 255, 255);
                dl->AddText(ImVec2(nameX + 1.0f, nameY + 1.0f), IM_COL32(0, 0, 0, 180), callStr.c_str());
                dl->AddText(ImVec2(nameX, nameY), nameCol, callStr.c_str());
            }
        }
    }
}

void UIManager::renderTdmMatchHUD(const FrontlineWarfare* frontline, float screenW, float screenH) {
    (void)screenH;
    if (!frontline) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Top-Center Match Status Bar
    float barW = 420.0f;
    float barH = 50.0f;
    float startX = (screenW - barW) * 0.5f;
    float startY = 18.0f;

    // Dark military frosted background
    dl->AddRectFilled(ImVec2(startX, startY), ImVec2(startX + barW, startY + barH), IM_COL32(10, 14, 20, 225), 6.0f);
    dl->AddRect(ImVec2(startX, startY), ImVec2(startX + barW, startY + barH), IM_COL32(40, 50, 65, 200), 6.0f, 0, 1.2f);

    int teamAScore = frontline->getTeamAKills();
    int teamBScore = frontline->getTeamBKills();
    int scoreLimit = frontline->getScoreLimit();

    // Vanguard (Blue)
    dl->AddRectFilled(ImVec2(startX + 6, startY + 6), ImVec2(startX + 140, startY + barH - 6), IM_COL32(25, 75, 140, 220), 4.0f);
    dl->AddText(ImVec2(startX + 14, startY + 14), IM_COL32(190, 225, 255, 255), "VANGUARD");
    std::string scoreAStr = std::to_string(teamAScore);
    dl->AddText(ImVec2(startX + 110, startY + 14), IM_COL32(255, 255, 255, 255), scoreAStr.c_str());

    // Center: Match Clock & Target
    float matchTime = frontline->getMatchTime();
    int mins = static_cast<int>(matchTime) / 60;
    int secs = static_cast<int>(matchTime) % 60;
    char timeBuf[32];
    std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);

    ImVec2 timeSize = ImGui::CalcTextSize(timeBuf);
    dl->AddText(ImVec2(startX + (barW - timeSize.x) * 0.5f, startY + 8), IM_COL32(240, 245, 250, 255), timeBuf);

    std::string targetStr = "FIRST TO " + std::to_string(scoreLimit);
    ImVec2 tgtSize = ImGui::CalcTextSize(targetStr.c_str());
    dl->AddText(ImVec2(startX + (barW - tgtSize.x) * 0.5f, startY + 28), IM_COL32(145, 160, 180, 230), targetStr.c_str());

    // Right: Crimson (Red)
    dl->AddRectFilled(ImVec2(startX + barW - 140, startY + 6), ImVec2(startX + barW - 6, startY + barH - 6), IM_COL32(140, 35, 30, 220), 4.0f);
    std::string scoreBStr = std::to_string(teamBScore);
    dl->AddText(ImVec2(startX + barW - 130, startY + 14), IM_COL32(255, 255, 255, 255), scoreBStr.c_str());
    dl->AddText(ImVec2(startX + barW - 85, startY + 14), IM_COL32(255, 200, 200, 255), "CRIMSON");

    // Match Over Announcement Banner & End-of-Match Breakdown
    if (frontline->isTdmMatchOver()) {
        Faction win = frontline->getTdmWinner();
        std::string winMsg = (win == Faction::FactionA) ? "VANGUARD VICTORY" : ((win == Faction::FactionB) ? "CRIMSON VICTORY" : "MATCH DRAW");
        ImU32 winCol = (win == Faction::FactionA) ? IM_COL32(45, 150, 255, 255) : ((win == Faction::FactionB) ? IM_COL32(255, 60, 50, 255) : IM_COL32(230, 210, 60, 255));

        // Center Full Tactical Victory/Defeat Modal
        float modalW = 540.0f;
        float modalH = 155.0f;
        float mX = (screenW - modalW) * 0.5f;
        float mY = screenH * 0.32f;

        // Dark frosted backdrop with faction-themed illuminated border
        dl->AddRectFilled(ImVec2(mX, mY), ImVec2(mX + modalW, mY + modalH), IM_COL32(8, 12, 16, 248), 8.0f);
        dl->AddRect(ImVec2(mX, mY), ImVec2(mX + modalW, mY + modalH), winCol, 8.0f, 0, 2.2f);

        // Header Subtitle
        const char* subTitle = "OPERATION CONCLUDED // 5v5 TEAM DEATHMATCH";
        ImVec2 subSz = ImGui::CalcTextSize(subTitle);
        dl->AddText(ImVec2(mX + (modalW - subSz.x) * 0.5f, mY + 16.0f), IM_COL32(160, 175, 195, 230), subTitle);

        // Main Outcome
        ImVec2 sz = ImGui::CalcTextSize(winMsg.c_str());
        dl->AddText(ImVec2(mX + (modalW - sz.x) * 0.5f, mY + 42.0f), winCol, winMsg.c_str());

        // Final Scoreline
        std::string scoreSummary = "FINAL SCORE: VANGUARD " + std::to_string(teamAScore) + " - " + std::to_string(teamBScore) + " CRIMSON";
        ImVec2 scSz = ImGui::CalcTextSize(scoreSummary.c_str());
        dl->AddText(ImVec2(mX + (modalW - scSz.x) * 0.5f, mY + 76.0f), IM_COL32(230, 235, 245, 255), scoreSummary.c_str());

        // Return Countdown
        int secRemaining = std::max(0, static_cast<int>(std::ceil(m_tdmEndTimer)));
        std::string countStr = "RETURNING TO HIDEOUT IN " + std::to_string(secRemaining) + "s  //  [PRESS SPACE TO EXIT]";
        ImVec2 cSz = ImGui::CalcTextSize(countStr.c_str());
        dl->AddText(ImVec2(mX + (modalW - cSz.x) * 0.5f, mY + 112.0f), IM_COL32(140, 210, 160, 240), countStr.c_str());
    }
}

void UIManager::renderTdmScoreboard(const FrontlineWarfare* frontline, const Player& player) {
    if (!frontline) return;
    ImGuiIO& io = ImGui::GetIO();
    float boardW = 760.0f;
    float boardH = 380.0f;
    float startX = (io.DisplaySize.x - boardW) * 0.5f;
    float startY = (io.DisplaySize.y - boardH) * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(startX, startY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(boardW, boardH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.08f, 0.11f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.28f, 0.38f, 0.85f));

    if (ImGui::Begin("##Tdm5v5Scoreboard", nullptr, flags)) {
        // Header Title
        std::string modeSubtitle = frontline->isTdmAgainstAi() ? "MERCENARY // 5v5 TDM (AGAINST AI BOTS)" : "MERCENARY // 5v5 TDM (PLAYERS ONLY // PVP)";
        ImGui::TextColored(ImVec4(0.92f, 0.95f, 1.0f, 1.0f), "%s", modeSubtitle.c_str());
        ImGui::SameLine(boardW - 140.0f);
        ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "[HOLD TAB]");

        ImGui::Separator();
        ImGui::Spacing();

        float halfW = (boardW - 40.0f) * 0.5f;

        // LEFT COLUMN: TEAM ALPHA (Vanguard)
        ImGui::BeginChild("##TeamAlphaRoster", ImVec2(halfW, boardH - 65.0f), true);
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "TEAM ALPHA // VANGUARD (%d KILLS)", frontline->getTeamAKills());
        ImGui::Separator();

        ImGui::Columns(4, "##TeamACols", false);
        ImGui::SetColumnWidth(0, halfW * 0.44f);
        ImGui::SetColumnWidth(1, halfW * 0.22f);
        ImGui::SetColumnWidth(2, halfW * 0.18f);
        ImGui::SetColumnWidth(3, halfW * 0.16f);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "OPERATIVE"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "ROLE"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "PING"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "STATUS"); ImGui::NextColumn();
        ImGui::Separator();

        // 1. Human Player if Vanguard
        CharacterProfile* actProf = ProfileManager::get().getActiveProfile();
        std::string humanName = actProf ? actProf->callsign : "OPERATOR";
        if (player.getFaction() == Faction::FactionA) {
            ImGui::TextColored(ImVec4(0.98f, 0.85f, 0.25f, 1.0f), "> %s (YOU)", humanName.c_str()); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "Operator"); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "0ms"); ImGui::NextColumn();
            ImGui::TextColored(player.getStats().health > 0 ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                               player.getStats().health > 0 ? "ALIVE" : "DEAD"); ImGui::NextColumn();
        }

        // Remote Players on Team A
        for (const auto& [id, rem] : NetworkEngine::get().getRemotePlayers()) {
            if (!rem.active || rem.currentState.faction != 0) continue;
            const char* cs = rem.currentState.callsign[0] ? rem.currentState.callsign : "OPERATIVE";
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s", cs); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.80f, 1.0f), "Operator"); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "18ms"); ImGui::NextColumn();
            ImGui::TextColored(rem.currentState.health > 0 ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                               rem.currentState.health > 0 ? "ALIVE" : "DEAD"); ImGui::NextColumn();
        }

        // Bots on Team A
        for (const auto& b : frontline->getBots()) {
            if (b.faction != Faction::FactionA) continue;
            ImGui::TextColored(ImVec4(0.80f, 0.90f, 1.0f, 1.0f), "%s * %s", b.rank.c_str(), b.callsign.c_str()); ImGui::NextColumn();
            const char* roleName = (b.role == CombatRole::SquadLeader) ? "Lead" : ((b.role == CombatRole::AutomaticRifleman) ? "Gunner" : ((b.role == CombatRole::Medic) ? "Medic" : "Rifle"));
            ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.80f, 1.0f), "%s", roleName); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.50f, 0.80f, 0.50f, 1.0f), "BOT"); ImGui::NextColumn();
            ImGui::TextColored(b.state != BotState::Dead ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                               b.state != BotState::Dead ? "ALIVE" : "DEAD"); ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::EndChild();

        ImGui::SameLine(0, 16.0f);

        // RIGHT COLUMN: TEAM BRAVO (Crimson)
        ImGui::BeginChild("##TeamBravoRoster", ImVec2(halfW, boardH - 65.0f), true);
        ImGui::TextColored(ImVec4(1.0f, 0.42f, 0.38f, 1.0f), "TEAM BRAVO // CRIMSON (%d KILLS)", frontline->getTeamBKills());
        ImGui::Separator();

        ImGui::Columns(4, "##TeamBCols", false);
        ImGui::SetColumnWidth(0, halfW * 0.44f);
        ImGui::SetColumnWidth(1, halfW * 0.22f);
        ImGui::SetColumnWidth(2, halfW * 0.18f);
        ImGui::SetColumnWidth(3, halfW * 0.16f);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "OPERATIVE"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "ROLE"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "PING"); ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "STATUS"); ImGui::NextColumn();
        ImGui::Separator();

        // 1. Human Player if Crimson
        if (player.getFaction() == Faction::FactionB) {
            ImGui::TextColored(ImVec4(0.98f, 0.85f, 0.25f, 1.0f), "> %s (YOU)", humanName.c_str()); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "Operator"); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "0ms"); ImGui::NextColumn();
            ImGui::TextColored(player.getStats().health > 0 ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                               player.getStats().health > 0 ? "ALIVE" : "DEAD"); ImGui::NextColumn();
        }

        // Remote Players on Team B
        for (const auto& [id, rem] : NetworkEngine::get().getRemotePlayers()) {
            if (!rem.active || rem.currentState.faction != 1) continue;
            const char* cs = rem.currentState.callsign[0] ? rem.currentState.callsign : "OPERATIVE";
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.60f, 1.0f), "%s", cs); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.80f, 1.0f), "Operator"); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "18ms"); ImGui::NextColumn();
            ImGui::TextColored(rem.currentState.health > 0 ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                               rem.currentState.health > 0 ? "ALIVE" : "DEAD"); ImGui::NextColumn();
        }

        // Bots on Team B
        for (const auto& b : frontline->getBots()) {
            if (b.faction != Faction::FactionB) continue;
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.85f, 1.0f), "%s * %s", b.rank.c_str(), b.callsign.c_str()); ImGui::NextColumn();
            const char* roleName = (b.role == CombatRole::SquadLeader) ? "Lead" : ((b.role == CombatRole::AutomaticRifleman) ? "Gunner" : ((b.role == CombatRole::Medic) ? "Medic" : "Rifle"));
            ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.80f, 1.0f), "%s", roleName); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.50f, 0.80f, 0.50f, 1.0f), "BOT"); ImGui::NextColumn();
            ImGui::TextColored(b.state != BotState::Dead ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                               b.state != BotState::Dead ? "ALIVE" : "DEAD"); ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::EndChild();

        ImGui::End();
    }
    ImGui::PopStyleColor(2);
}

void UIManager::renderTdmRespawnOverlay(float screenW, float screenH) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    // Red vignette and casualty banner
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, screenH), IM_COL32(40, 10, 10, 130));

    float cardW = 380.0f;
    float cardH = 90.0f;
    float cX = (screenW - cardW) * 0.5f;
    float cY = (screenH - cardH) * 0.5f;

    dl->AddRectFilled(ImVec2(cX, cY), ImVec2(cX + cardW, cY + cardH), IM_COL32(14, 18, 24, 235), 6.0f);
    dl->AddRect(ImVec2(cX, cY), ImVec2(cX + cardW, cY + cardH), IM_COL32(235, 65, 55, 220), 6.0f, 0, 1.5f);

    dl->AddText(ImVec2(cX + 24.0f, cY + 16.0f), IM_COL32(255, 100, 90, 255), "OPERATIVE CASUALTY // TACTICAL RESPAWN");

    char timerBuf[64];
    std::snprintf(timerBuf, sizeof(timerBuf), "DEPLOYING TO BASE IN %.1fs...", m_tdmRespawnTimer);
    dl->AddText(ImVec2(cX + 24.0f, cY + 44.0f), IM_COL32(240, 245, 250, 255), timerBuf);

    // Respawn progress bar
    float progress = 1.0f - std::clamp(m_tdmRespawnTimer / 4.0f, 0.0f, 1.0f);
    dl->AddRectFilled(ImVec2(cX + 24.0f, cY + 70.0f), ImVec2(cX + cardW - 24.0f, cY + 76.0f), IM_COL32(35, 42, 52, 255), 2.0f);
    dl->AddRectFilled(ImVec2(cX + 24.0f, cY + 70.0f), ImVec2(cX + 24.0f + (cardW - 48.0f) * progress, cY + 76.0f), IM_COL32(50, 195, 115, 255), 2.0f);
}


void UIManager::renderAlertRadialMenu(Player& player, const glm::vec3& mouseWorldPos, const AimTargetInfo* aimInfo) {
    if (!m_alertWheelOpen) return;

    ImGuiIO& io = ImGui::GetIO();
    constexpr float PI_CONST = 3.14159265358979323846f;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (!ImGui::Begin("##TacticalAlertWheelOverlay", nullptr, flags)) {
        ImGui::End();
        return;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Subtle dark cinematic blur backdrop
    dl->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(6, 10, 14, 160));

    // Smooth cubic ease-out animation
    m_alertWheelAnimTime += io.DeltaTime * 7.5f;
    float animProgress = std::clamp(m_alertWheelAnimTime, 0.0f, 1.0f);
    float ease = 1.0f - std::pow(1.0f - animProgress, 3.0f);

    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    float innerR = 85.0f * ease;
    float outerR = 210.0f * ease;
    float hubR = 75.0f * ease;

    // Mouse pointer vector and angle detection
    ImVec2 mouseVec(io.MousePos.x - center.x, io.MousePos.y - center.y);
    float mouseDist = std::sqrt(mouseVec.x * mouseVec.x + mouseVec.y * mouseVec.y);

    struct AlertSliceInfo {
        PingType type;
        const char* title;
        const char* sub;
        ImU32 baseCol;
        ImU32 glowCol;
    };

    static const AlertSliceInfo s_alertSlices[5] = {
        { PingType::HostileContact, "ENEMY SPOTTED", "HOSTILE",      IM_COL32(235, 55, 55, 255),  IM_COL32(255, 70, 70, 255) },
        { PingType::TacticalMove,    "MOVE / RECON",  "WAYPOINT",     IM_COL32(245, 205, 45, 255), IM_COL32(255, 225, 75, 255) },
        { PingType::Danger,          "DANGER / WATCH","CAUTION",      IM_COL32(245, 125, 30, 255), IM_COL32(255, 150, 50, 255) },
        { PingType::UrgentAssist,    "NEED BACKUP",   "DEFEND / SOS", IM_COL32(45, 175, 245, 255), IM_COL32(75, 205, 255, 255) },
        { PingType::Supplies,        "SUPPLIES FOUND","LOOT / AMMO",  IM_COL32(50, 225, 95, 255),  IM_COL32(85, 245, 125, 255) }
    };

    int hoverSector = -1;
    if (mouseDist > innerR * 0.70f) {
        float mouseAngle = std::atan2(mouseVec.y, mouseVec.x);
        float shifted = mouseAngle + (PI_CONST * 0.5f) + (PI_CONST / 5.0f);
        while (shifted < 0.0f) shifted += 2.0f * PI_CONST;
        while (shifted >= 2.0f * PI_CONST) shifted -= 2.0f * PI_CONST;
        hoverSector = static_cast<int>(shifted / (2.0f * PI_CONST / 5.0f)) % 5;
    }
    m_selectedAlertIndex = hoverSector;

    // Draw 5 segmented slices
    const float sectorAngle = (2.0f * PI_CONST) / 5.0f;
    for (int i = 0; i < 5; ++i) {
        bool isHovered = (i == hoverSector);
        float aStart = -PI_CONST * 0.5f - sectorAngle * 0.5f + i * sectorAngle + 0.025f;
        float aEnd   = aStart + sectorAngle - 0.05f;

        float curOuterR = isHovered ? (outerR + 14.0f * ease) : outerR;
        float curInnerR = isHovered ? (innerR - 6.0f * ease) : innerR;

        ImU32 fillCol = isHovered ? IM_COL32(32, 42, 54, 235) : IM_COL32(18, 24, 32, 200);
        ImU32 borderCol = isHovered ? s_alertSlices[i].glowCol : IM_COL32(60, 75, 95, 170);

        const int arcSegments = 24;
        std::vector<ImVec2> arcPts;
        arcPts.reserve(arcSegments * 2 + 2);
        for (int s = 0; s <= arcSegments; ++s) {
            float a = aStart + (aEnd - aStart) * ((float)s / (float)arcSegments);
            arcPts.push_back(ImVec2(center.x + std::cos(a) * curOuterR, center.y + std::sin(a) * curOuterR));
        }
        for (int s = arcSegments; s >= 0; --s) {
            float a = aStart + (aEnd - aStart) * ((float)s / (float)arcSegments);
            arcPts.push_back(ImVec2(center.x + std::cos(a) * curInnerR, center.y + std::sin(a) * curInnerR));
        }

        dl->AddConvexPolyFilled(arcPts.data(), (int)arcPts.size(), fillCol);
        dl->AddPolyline(arcPts.data(), (int)arcPts.size(), borderCol, ImDrawFlags_Closed, isHovered ? 2.5f : 1.2f);

        float aMid = (aStart + aEnd) * 0.5f;
        float labelR = (curInnerR + curOuterR) * 0.5f;
        ImVec2 labelPos(center.x + std::cos(aMid) * labelR, center.y + std::sin(aMid) * labelR);

        dl->AddCircleFilled(ImVec2(labelPos.x, labelPos.y - 10.0f), isHovered ? 5.5f : 4.0f, s_alertSlices[i].baseCol);

        ImVec2 tSz = ImGui::CalcTextSize(s_alertSlices[i].title);
        dl->AddText(ImVec2(labelPos.x - tSz.x * 0.5f, labelPos.y - 2.0f),
                    isHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 215, 225, 220),
                    s_alertSlices[i].title);

        ImVec2 subSz = ImGui::CalcTextSize(s_alertSlices[i].sub);
        dl->AddText(ImVec2(labelPos.x - subSz.x * 0.5f, labelPos.y + 11.0f),
                    s_alertSlices[i].baseCol,
                    s_alertSlices[i].sub);
    }

    // Center Hub
    dl->AddCircleFilled(center, hubR, IM_COL32(14, 18, 24, 240));
    dl->AddCircle(center, hubR, IM_COL32(70, 90, 115, 220), 0, 2.0f);

    float targetDist = glm::distance(player.getPosition(), mouseWorldPos);

    if (hoverSector >= 0) {
        const auto& sel = s_alertSlices[hoverSector];
        dl->AddCircle(center, hubR - 4.0f, sel.baseCol, 0, 2.0f);

        char alertHeader[64];
        std::snprintf(alertHeader, sizeof(alertHeader), "[ %s ]", sel.sub);
        ImVec2 hSz = ImGui::CalcTextSize(alertHeader);
        dl->AddText(ImVec2(center.x - hSz.x * 0.5f, center.y - 22.0f), sel.glowCol, alertHeader);

        char distBuf[48];
        std::snprintf(distBuf, sizeof(distBuf), "RANGE: %.0fm", targetDist);
        ImVec2 dSz = ImGui::CalcTextSize(distBuf);
        dl->AddText(ImVec2(center.x - dSz.x * 0.5f, center.y - 4.0f), IM_COL32(220, 235, 245, 240), distBuf);

        const char* inst = "RELEASE TO PING";
        ImVec2 iSz = ImGui::CalcTextSize(inst);
        dl->AddText(ImVec2(center.x - iSz.x * 0.5f, center.y + 12.0f), IM_COL32(160, 180, 195, 200), inst);
    } else {
        const char* title = "TACTICAL ALERTS";
        ImVec2 tSz = ImGui::CalcTextSize(title);
        dl->AddText(ImVec2(center.x - tSz.x * 0.5f, center.y - 16.0f), IM_COL32(245, 210, 45, 240), title);

        char distBuf[48];
        std::snprintf(distBuf, sizeof(distBuf), "RANGE: %.0fm", targetDist);
        ImVec2 dSz = ImGui::CalcTextSize(distBuf);
        dl->AddText(ImVec2(center.x - dSz.x * 0.5f, center.y + 2.0f), IM_COL32(180, 195, 210, 200), distBuf);
    }

    ImGui::End();
}



// ----------------------------------------------------------------------------
// Tactical Datalink Pings (Enemy Spotted, Waypoint, Danger, Backup, Supplies)
// ----------------------------------------------------------------------------
void UIManager::addTacticalPing(PingType type, const glm::vec3& worldPos, const std::string& senderCallsign) {
    for (auto& p : m_tacticalPings) {
        if (p.senderCallsign == senderCallsign && glm::distance(p.worldPos, worldPos) < 2.5f) {
            p.type = type;
            p.worldPos = worldPos;
            p.life = p.maxLife;
            p.pulseTime = 0.0f;
            return;
        }
    }

    if (m_tacticalPings.size() >= 16) {
        m_tacticalPings.erase(m_tacticalPings.begin());
    }

    TacticalPing newPing;
    newPing.type = type;
    newPing.worldPos = worldPos;
    newPing.senderCallsign = senderCallsign.empty() ? "OPERATOR" : senderCallsign;
    newPing.maxLife = 10.0f;
    newPing.life = 10.0f;
    newPing.pulseTime = 0.0f;
    m_tacticalPings.push_back(newPing);
}

void UIManager::updateTacticalPings(float dt) {
    for (auto it = m_tacticalPings.begin(); it != m_tacticalPings.end(); ) {
        it->life -= dt;
        it->pulseTime += dt;
        if (it->life <= 0.0f) {
            it = m_tacticalPings.erase(it);
        } else {
            ++it;
        }
    }
}

void UIManager::renderTacticalPings(const Camera& camera, float aspectRatio, const Player& player) {
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime > 0.0f ? std::min(io.DeltaTime, 0.05f) : 0.016f;
    updateTacticalPings(dt);

    if (m_tacticalPings.empty()) return;
    if (m_gameState == GameState::MainMenu && (m_currentTab != MainMenuTab::Overview && m_currentTab != MainMenuTab::Customize)) {
        return;
    }

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;
    ImVec2 screenCenter(screenW * 0.5f, screenH * 0.5f);

    glm::vec3 camPos = camera.getPosition();
    glm::vec3 camFwd = camera.getForwardVector();
    glm::vec3 camRight = camera.getRightVector();
    glm::vec3 camUp = camera.getUpVector();

    for (const auto& ping : m_tacticalPings) {
        float dist = glm::distance(player.getPosition(), ping.worldPos);

        float lifeAlpha = std::clamp(ping.life / 1.5f, 0.0f, 1.0f);
        uint32_t aByte = static_cast<uint32_t>(lifeAlpha * 255.0f);
        if (aByte < 10) continue;

        const char* label = "PING";
        ImU32 primaryCol = IM_COL32(255, 255, 255, aByte);
        ImU32 glowCol = IM_COL32(255, 255, 255, aByte / 2);
        ImU32 bgCol = IM_COL32(15, 18, 24, static_cast<uint32_t>(lifeAlpha * 220.0f));

        switch (ping.type) {
            case PingType::HostileContact:
                label = "ENEMY SPOTTED";
                primaryCol = IM_COL32(255, 50, 50, aByte);
                glowCol = IM_COL32(255, 90, 90, aByte / 2);
                break;
            case PingType::TacticalMove:
                label = "WAYPOINT";
                primaryCol = IM_COL32(40, 215, 255, aByte);
                glowCol = IM_COL32(110, 235, 255, aByte / 2);
                break;
            case PingType::Danger:
                label = "DANGER";
                primaryCol = IM_COL32(255, 175, 25, aByte);
                glowCol = IM_COL32(255, 210, 80, aByte / 2);
                break;
            case PingType::UrgentAssist:
                label = "NEED BACKUP";
                primaryCol = IM_COL32(255, 65, 185, aByte);
                glowCol = IM_COL32(255, 140, 225, aByte / 2);
                break;
            case PingType::Supplies:
                label = "SUPPLIES FOUND";
                primaryCol = IM_COL32(45, 245, 135, aByte);
                glowCol = IM_COL32(130, 255, 185, aByte / 2);
                break;
        }

        glm::vec3 markerPos = ping.worldPos + glm::vec3(0.0f, 0.45f, 0.0f);
        glm::vec2 projectedScreen(0.0f);
        bool onScreen = camera.worldToScreen(markerPos, aspectRatio, screenW, screenH, projectedScreen);

        bool inViewBounds = onScreen &&
                           (projectedScreen.x >= 50.0f && projectedScreen.x <= screenW - 50.0f &&
                            projectedScreen.y >= 50.0f && projectedScreen.y <= screenH - 50.0f);

        if (inViewBounds) {
            ImVec2 sp(projectedScreen.x, projectedScreen.y);

            float waveFrac = fmodf(ping.pulseTime * 1.5f, 1.0f);
            float waveR = 8.0f + waveFrac * 22.0f;
            float waveA = (1.0f - waveFrac) * (float)aByte * 0.75f;
            dl->AddCircle(sp, waveR, (glowCol & 0x00FFFFFF) | ((uint32_t)waveA << 24), 0, 1.5f);

            float dSize = 9.0f;
            ImVec2 dPts[4] = {
                ImVec2(sp.x, sp.y - dSize - 2.0f),
                ImVec2(sp.x + dSize, sp.y),
                ImVec2(sp.x, sp.y + dSize + 2.0f),
                ImVec2(sp.x - dSize, sp.y)
            };
            dl->AddConvexPolyFilled(dPts, 4, bgCol);
            dl->AddPolyline(dPts, 4, primaryCol, ImDrawFlags_Closed, 2.2f);
            dl->AddCircleFilled(sp, 2.5f, primaryCol);

            glm::vec2 baseScreen(0.0f);
            if (camera.worldToScreen(ping.worldPos, aspectRatio, screenW, screenH, baseScreen)) {
                dl->AddLine(ImVec2(sp.x, sp.y + dSize + 2.0f), ImVec2(baseScreen.x, baseScreen.y), (primaryCol & 0x00FFFFFF) | (0x66 << 24), 1.5f);
                dl->AddCircleFilled(ImVec2(baseScreen.x, baseScreen.y), 3.0f, primaryCol);
            }

            char headerBuf[64];
            std::snprintf(headerBuf, sizeof(headerBuf), "%s", label);
            char infoBuf[64];
            std::snprintf(infoBuf, sizeof(infoBuf), "%s  [%.0fm]", ping.senderCallsign.c_str(), dist);

            ImVec2 hSz = ImGui::CalcTextSize(headerBuf);
            ImVec2 iSz = ImGui::CalcTextSize(infoBuf);
            float cardW = std::max(hSz.x, iSz.x) + 16.0f;
            float cardH = 30.0f;
            ImVec2 cardMin(sp.x - cardW * 0.5f, sp.y - dSize - 6.0f - cardH);
            ImVec2 cardMax(cardMin.x + cardW, cardMin.y + cardH);

            dl->AddRectFilled(cardMin, cardMax, bgCol, 4.0f);
            dl->AddRect(cardMin, cardMax, primaryCol, 4.0f, 0, 1.2f);

            dl->AddText(ImVec2(cardMin.x + 8.0f, cardMin.y + 2.0f), primaryCol, headerBuf);
            dl->AddText(ImVec2(cardMin.x + 8.0f, cardMin.y + 16.0f), IM_COL32(200, 215, 225, aByte), infoBuf);
        } else {
            glm::vec3 toTarget = markerPos - camPos;
            glm::vec3 dir = glm::length(toTarget) > 0.001f ? glm::normalize(toTarget) : glm::vec3(0,0,1);
            float fwd = glm::dot(dir, camFwd);
            float rgt = glm::dot(dir, camRight);
            float up = glm::dot(dir, camUp);

            glm::vec2 dir2D(rgt, -up);
            if (fwd < 0.0f) {
                dir2D = -dir2D;
                if (glm::length(dir2D) < 0.001f) dir2D = glm::vec2(0.0f, 1.0f);
            }
            if (glm::length(dir2D) > 0.001f) {
                dir2D = glm::normalize(dir2D);
            }

            float edgeMarginX = 52.0f;
            float edgeMarginY = 52.0f;
            float maxExtX = screenW * 0.5f - edgeMarginX;
            float maxExtY = screenH * 0.5f - edgeMarginY;

            float tX = (std::abs(dir2D.x) > 0.0001f) ? (maxExtX / std::abs(dir2D.x)) : 999999.0f;
            float tY = (std::abs(dir2D.y) > 0.0001f) ? (maxExtY / std::abs(dir2D.y)) : 999999.0f;
            float t = std::min(tX, tY);

            ImVec2 edgePos(screenCenter.x + dir2D.x * t, screenCenter.y + dir2D.y * t);

            ImVec2 norm = ImVec2(dir2D.x, dir2D.y);
            ImVec2 perp = ImVec2(-dir2D.y, dir2D.x);
            float arrLen = 14.0f;
            float arrWid = 9.0f;

            ImVec2 tip(edgePos.x + norm.x * 6.0f, edgePos.y + norm.y * 6.0f);
            ImVec2 b1(edgePos.x - norm.x * arrLen + perp.x * arrWid, edgePos.y - norm.y * arrLen + perp.y * arrWid);
            ImVec2 b2(edgePos.x - norm.x * arrLen - perp.x * arrWid, edgePos.y - norm.y * arrLen - perp.y * arrWid);

            ImVec2 arrowPts[3] = { tip, b1, b2 };
            dl->AddConvexPolyFilled(arrowPts, 3, primaryCol);
            dl->AddPolyline(arrowPts, 3, IM_COL32(0, 0, 0, aByte), ImDrawFlags_Closed, 1.5f);

            char offBuf[32];
            std::snprintf(offBuf, sizeof(offBuf), "%.0fm", dist);
            ImVec2 bSz = ImGui::CalcTextSize(offBuf);
            ImVec2 badgePos(edgePos.x - norm.x * (arrLen + 12.0f) - bSz.x * 0.5f,
                            edgePos.y - norm.y * (arrLen + 12.0f) - bSz.y * 0.5f);

            dl->AddRectFilled(ImVec2(badgePos.x - 4.0f, badgePos.y - 2.0f),
                              ImVec2(badgePos.x + bSz.x + 4.0f, badgePos.y + bSz.y + 2.0f),
                              bgCol, 3.0f);
            dl->AddText(badgePos, primaryCol, offBuf);
        }
    }
}


void UIManager::renderConstructionHUD(Player& player, World& world) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    if (ImGui::IsKeyPressed(ImGuiKey_1)) m_selectedStructureType = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_2)) m_selectedStructureType = 2;
    if (ImGui::IsKeyPressed(ImGuiKey_3)) m_selectedStructureType = 3;
    if (ImGui::IsKeyPressed(ImGuiKey_4)) m_selectedStructureType = 4;
    if (ImGui::IsKeyPressed(ImGuiKey_5)) m_selectedStructureType = 5;
    if (ImGui::IsKeyPressed(ImGuiKey_6)) m_selectedStructureType = 6;
    if (ImGui::IsKeyPressed(ImGuiKey_7)) m_selectedStructureType = 7;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_constructionMenuOpen = false;
        return;
    }

    struct StructInfo {
        int id;
        const char* name;
        const char* desc;
        int hp;
        ObstacleType obsType;
        glm::vec3 size;
    };

    static const StructInfo s_list[] = {
        { 1, "FOB COMMAND BUNKER",      "Respawn point & frontline HQ",             4500, ObstacleType::ConcreteBunker, {8.0f, 3.5f, 5.0f} },
        { 2, "MODULAR SANDBAG TRENCH",  "Ballistic cover & trench line",            800,  ObstacleType::Sandbag,        {5.0f, 1.1f, 0.9f} },
        { 3, "HEAVY MG PILLBOX (DShK)", "Reinforced fortified machine gun nest",    3000, ObstacleType::HescoBastion,   {3.0f, 3.0f, 3.0f} },
        { 4, "TACTICAL RADAR TOWER",    "400m radar intercept & squad scanning",    1500, ObstacleType::RadioTower,     {1.2f, 12.0f, 1.2f} },
        { 5, "DRAGON'S TEETH LINE",     "Anti-tank concrete armor barriers",        2500, ObstacleType::ConcreteBarrier,{6.0f, 1.4f, 1.0f} },
        { 6, "VEHICLE REPAIR PAD",      "Armor repair & fuel replenishment",        2000, ObstacleType::ConcreteBarrier,{8.0f, 0.4f, 8.0f} },
        { 7, "FIELD AMMO CACHE",        "Ammunition & supply distribution point",   1000, ObstacleType::WoodenCrate,    {2.5f, 2.0f, 2.5f} }
    };

    float panelW = 760.0f;
    float panelH = 175.0f;
    float panelX = (screenW - panelW) * 0.5f;
    float panelY = screenH - panelH - 45.0f;

    ImGui::SetNextWindowPos(ImVec2(panelX, panelY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.94f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.55f, 0.85f, 0.80f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);

    if (ImGui::Begin("##EngineerConstructionHUD", &m_constructionMenuOpen, flags)) {
        ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "MERCENARY // ENGINEER FIELD CONSTRUCTION & PERSISTENT FOB");
        ImGui::SameLine(panelW - 130.0f);
        if (ImGui::Button("[X] CLOSE (B)", ImVec2(110, 22))) {
            m_constructionMenuOpen = false;
        }

        ImGui::Separator();
        ImGui::Spacing();

        float btnW = (panelW - 40.0f) / 7.0f;
        for (int i = 0; i < 7; ++i) {
            if (i > 0) ImGui::SameLine();
            bool isSelected = (m_selectedStructureType == s_list[i].id);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.75f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.15f, 0.20f, 0.85f));
            }
            std::string label = "[" + std::to_string(i + 1) + "]\n" + s_list[i].name;
            if (ImGui::Button(label.c_str(), ImVec2(btnW, 55.0f))) {
                m_selectedStructureType = s_list[i].id;
            }
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        const StructInfo& cur = s_list[std::clamp(m_selectedStructureType - 1, 0, 6)];
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "Selected: %s (%d HP) | %s", cur.name, cur.hp, cur.desc);

        ImGui::Spacing();
        if (ImGui::Button("> PLACE STRUCTURE IN FRONT (LEFT CLICK)", ImVec2(-1, 34.0f))) {
            float yaw = player.getYaw();
            glm::vec3 forward(std::sin(yaw), 0.0f, -std::cos(yaw));
            glm::vec3 buildPos = player.getPosition() + forward * 4.0f;

            std::string regName = NetworkEngine::get().getCurrentMapName().empty() ? "kharkiv" : NetworkEngine::get().getCurrentMapName();
            NetworkEngine::get().buildStructure(static_cast<uint8_t>(cur.id), static_cast<uint8_t>(player.getFaction()), regName, buildPos, yaw);

            WorldObstacle obs;
            obs.position = buildPos;
            obs.size = cur.size;
            obs.type = cur.obsType;
            obs.color = glm::vec4(0.85f, 0.85f, 0.90f, 1.0f);
            obs.blocksLineOfSight = true;
            obs.destructible = true;
            obs.health = static_cast<float>(cur.hp);
            obs.yaw = yaw;
            world.getObstacles().push_back(obs);

            AudioEngine::get().playSound(SoundType::ItemEquip, 0.85f, 1.0f);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}


void UIManager::renderAcousticDebugVisualizer(const Player& player, const Camera& camera, float aspectRatio, float screenW, float screenH) {
    if (!m_audioDebugVisualizerEnabled) return;

    AudioEngine& audio = AudioEngine::get();
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
    }

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
    // On-screen notification when Cursor is manually freed with U
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
        ImGui::Separator();
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
        if (ImGui::Button("Visual (65 m/s)")) {
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

} // namespace Mercenary

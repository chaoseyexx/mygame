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
bool UIManager::renderCompetitorButton(const char* label, const ImVec2& size, bool isSelected, bool isHighlighted, ImDrawFlags roundFlags) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 actual_size = ImGui::CalcItemSize(size, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, ImVec2(pos.x + actual_size.x, pos.y + actual_size.y));
    ImGui::ItemSize(actual_size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* dl = window->DrawList;

    ImU32 btnBg;
    if (held || isSelected) {
        btnBg = IM_COL32(36, 25, 19, 240);
    } else if (hovered) {
        btnBg = IM_COL32(28, 32, 40, 235);
    } else if (isHighlighted) {
        btnBg = IM_COL32(26, 20, 16, 230);
    } else {
        btnBg = IM_COL32(18, 20, 24, 215);
    }

    ImU32 btnBorder;
    if (isSelected || isHighlighted) {
        btnBorder = hovered ? IM_COL32(235, 130, 50, 240) : IM_COL32(75, 52, 38, 255);
    } else if (hovered) {
        btnBorder = IM_COL32(65, 75, 92, 255);
    } else {
        btnBorder = IM_COL32(38, 44, 54, 220);
    }

    ImU32 textColor;
    if (isSelected || (isHighlighted && !hovered)) {
        textColor = IM_COL32(235, 130, 50, 255);
    } else if (hovered) {
        textColor = IM_COL32(245, 248, 255, 255);
    } else {
        textColor = IM_COL32(165, 172, 185, 255);
    }

    float rounding = 3.0f;
    dl->AddRectFilled(bb.Min, bb.Max, btnBg, rounding, roundFlags);
    dl->AddRect(bb.Min, bb.Max, btnBorder, rounding, roundFlags, 1.0f);

    if (isSelected || isHighlighted) {
        float glowH = std::min(12.0f, actual_size.y * 0.45f);
        ImU32 glowCol = (isSelected || hovered) ? IM_COL32(219, 98, 41, 70) : IM_COL32(219, 98, 41, 35);
        dl->AddRectFilledMultiColor(
            ImVec2(bb.Min.x + 1.0f, bb.Max.y - glowH),
            ImVec2(bb.Max.x - 1.0f, bb.Max.y - 2.5f),
            IM_COL32(219, 98, 41, 0), IM_COL32(219, 98, 41, 0),
            glowCol, glowCol
        );

        dl->AddRectFilled(
            ImVec2(bb.Min.x, bb.Max.y - 2.5f),
            ImVec2(bb.Max.x, bb.Max.y),
            IM_COL32(219, 98, 41, 255),
            roundFlags == ImDrawFlags_RoundCornersLeft ? 2.0f : (roundFlags == ImDrawFlags_RoundCornersRight ? 2.0f : 0.0f)
        );
    } else if (hovered) {
        dl->AddRectFilled(
            ImVec2(bb.Min.x, bb.Max.y - 1.5f),
            ImVec2(bb.Max.x, bb.Max.y),
            IM_COL32(180, 190, 205, 180),
            0.0f
        );
    }

    const char* text_display_end = ImGui::FindRenderedTextEnd(label);
    ImVec2 text_pos(bb.Min.x + (actual_size.x - label_size.x) * 0.5f,
                    bb.Min.y + (actual_size.y - label_size.y) * 0.5f - ((isSelected || isHighlighted) ? 1.0f : 0.0f));
    dl->AddText(text_pos, textColor, label, text_display_end);

    return pressed;
}

// =========================================================================
// =========================================================================
// 1. FIRST SCREEN: OPERATOR SELECTION & RECRUITMENT
// =========================================================================
void UIManager::renderRecruitOperativeModal(Player& player) {
    ImGuiIO& io = ImGui::GetIO();

    // Fullscreen backdrop dimming window
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.78f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##RecruitDimBackdrop", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | 
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    float modalW = 480.0f;
    float modalH = 360.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - modalW * 0.5f, io.DisplaySize.y * 0.5f - modalH * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.11f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.38f, 0.28f, 0.20f, 0.95f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##RecruitOperativeModalWindow", nullptr, flags)) {
        if (m_fontHeading) ImGui::PushFont(m_fontHeading);
        ImGui::TextColored(ImVec4(0.95f, 0.98f, 1.0f, 1.0f), "ENLIST COMBAT OPERATIVE");
        if (m_fontHeading) ImGui::PopFont();

        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        ImGui::TextDisabled("Create authenticated service dossier for Operative Slot 0%d", m_recruitSlotIndex + 1);
        if (m_fontSmall) ImGui::PopFont();

        ImGui::Separator();
        ImGui::Spacing();

        // 1. Callsign
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.96f, 1.0f), "CALLSIGN / IDENTITY");
        if (m_fontBold) ImGui::PopFont();

        std::string placeholder = "OPERATIVE_0" + std::to_string(m_recruitSlotIndex + 1);
        if (SteamIntegration::get().isInitialized() && m_recruitSlotIndex == 0) {
            placeholder = SteamIntegration::get().getPersonaName();
        }
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##NewOperativeCallsignInput", placeholder.c_str(), m_recruitNameBuf, sizeof(m_recruitNameBuf));

        ImGui::Spacing();
        ImGui::Spacing();

        // 2. Faction Allegiance
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.96f, 1.0f), "FACTION ALLEGIANCE");
        if (m_fontBold) ImGui::PopFont();

        float cardW = (modalW - 48.0f - 12.0f) * 0.5f;

        // Faction A: Mercenary
        bool isMerc = (m_recruitFaction == 1);
        if (renderCompetitorButton("MERCENARY COALITION\nAllied Coalition Forces", ImVec2(cardW, 52), isMerc, isMerc)) {
            m_recruitFaction = 1;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }

        ImGui::SameLine(0, 12);

        // Faction B: Crimson
        bool isCrim = (m_recruitFaction == 2);
        if (renderCompetitorButton("CRIMSON LEGION\nEastern Shock Brigade", ImVec2(cardW, 52), isCrim, isCrim)) {
            m_recruitFaction = 2;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 3. Action Buttons
        if (renderCompetitorButton("ENLIST OPERATIVE", ImVec2(modalW - 48.0f - 110.0f, 36), false, true)) {
            std::string finalName = m_recruitNameBuf;
            if (finalName.empty()) finalName = placeholder;
            ProfileManager::get().createCharacter(m_recruitSlotIndex, finalName, (m_recruitFaction == 1) ? Faction::FactionA : Faction::FactionB);
            ProfileManager::get().setActiveSlotIndex(m_recruitSlotIndex);
            CharacterProfile* newActive = ProfileManager::get().getActiveProfile();
            if (newActive) {
                player.getInventory() = newActive->savedInventory;
                player.updateArmorFromEquipped();
                player.setFaction(newActive->faction);
            }
            m_showRecruitModal = false;
            m_recruitNameBuf[0] = '\0';
            AudioEngine::get().playSound(SoundType::DeployHorn);
        }

        ImGui::SameLine(0, 10);
        if (renderCompetitorButton("CANCEL", ImVec2(100, 36))) {
            m_showRecruitModal = false;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void UIManager::renderInGameAuthScreen(Player& player) {
    ImGuiIO& io = ImGui::GetIO();
    float barH = 50.0f;
    ImGuiWindowFlags topFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | 
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    // 1. TOP HEADER BAR
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, barH), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
    if (ImGui::Begin("##AuthTopBar", nullptr, topFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(0, barH), ImVec2(io.DisplaySize.x, barH), IM_COL32(30, 38, 50, 255));

        // Monogram badge [M]
        dl->AddRectFilled(ImVec2(22, 10), ImVec2(52, 40), IM_COL32(0, 120, 242, 255), 5.0f);
        dl->AddText(ImVec2(31, 14), IM_COL32(255, 255, 255, 255), "M");

        if (m_fontBold) ImGui::PushFont(m_fontBold);
        dl->AddText(ImVec2(64, 15), IM_COL32(235, 242, 252, 255), "MERCENARY");
        if (m_fontBold) ImGui::PopFont();

        if (m_fontRegular) ImGui::PushFont(m_fontRegular);
        dl->AddText(ImVec2(185, 15), IM_COL32(110, 125, 145, 255), "CENTRAL AUTHORIZATION GATE");
        if (m_fontRegular) ImGui::PopFont();

        // Right Side: Server Cluster IP
        float rightX = io.DisplaySize.x - 340.0f;
        dl->AddCircleFilled(ImVec2(rightX, 25), 4.5f, IM_COL32(0, 180, 255, 255));
        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        char srvBuf[128];
        snprintf(srvBuf, sizeof(srvBuf), "Cluster: %s [Online]", NetworkEngine::get().getServerHost().c_str());
        dl->AddText(ImVec2(rightX + 12, 18), IM_COL32(170, 190, 215, 255), srvBuf);
        if (m_fontSmall) ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    // 2. CENTER AUTHENTICATION MODAL
    float modalW = 540.0f;
    float modalH = SteamIntegration::get().isInitialized() ? 320.0f : 460.0f;
    ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - modalW) * 0.5f, (io.DisplaySize.y - modalH) * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(modalW, modalH), ImGuiCond_Always);
    ImGuiWindowFlags modalFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | 
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.07f, 0.08f, 0.11f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.28f, 0.38f, 0.90f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    if (ImGui::Begin("##CentralAuthModal", nullptr, modalFlags)) {
        if (SteamIntegration::get().isInitialized()) {
            // ================= STEAM 1-CLICK SILENT AUTH =================
            if (!m_steamAuthInitiated) {
                m_steamAuthInitiated = true;
                NetworkEngine::get().sendAuthSteam(std::to_string(SteamIntegration::get().getSteamID()), SteamIntegration::get().getPersonaName());
            }

            if (m_fontHeading) ImGui::PushFont(m_fontHeading);
            ImGui::TextColored(ImVec4(0.00f, 0.75f, 1.00f, 1.0f), "STEAM CLOUD AUTHENTICATION");
            if (m_fontHeading) ImGui::PopFont();

            if (m_fontSmall) ImGui::PushFont(m_fontSmall);
            ImGui::TextDisabled("Secure hardware identity handshake with Mercenary Central Master Server.");
            if (m_fontSmall) ImGui::PopFont();

            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.95f, 1.0f), "Operative Persona: %s", SteamIntegration::get().getPersonaName().c_str());
            ImGui::TextDisabled("SteamID64: %llu", SteamIntegration::get().getSteamID());
            ImGui::Spacing();

            if (NetworkEngine::get().isAuthPending()) {
                ImGui::Spacing();
                std::string currentHost = NetworkEngine::get().getServerHost();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), ">>> CONTACTING SIVERSKY MASTER SERVER (%s)...", currentHost.c_str());
                ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "SYNCHRONIZING OPERATIVE BIOMETRICS, LOADOUT & SECURE STASH...");
            } else if (!NetworkEngine::get().getAuthError().empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[!] AUTHENTICATION ERROR: %s", NetworkEngine::get().getAuthError().c_str());
                ImGui::Spacing();
                if (renderCompetitorButton("RETRY STEAM HANDSHAKE", ImVec2(220, 36), false, true)) {
                    m_steamAuthInitiated = false;
                    NetworkEngine::get().clearAuthError();
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            }

            ImGui::SetCursorPosY(modalH - 56.0f);
            if (renderCompetitorButton("EXIT TO DESKTOP", ImVec2(160, 36))) {
                exit(0);
            }
        } else {
            // ================= STANDALONE USER LOGIN & REGISTRATION =================
            if (m_fontHeading) ImGui::PushFont(m_fontHeading);
            ImGui::TextColored(ImVec4(0.95f, 0.98f, 1.0f, 1.0f), "MERCENARY NETWORK CLEARANCE");
            if (m_fontHeading) ImGui::PopFont();

            if (m_fontSmall) ImGui::PushFont(m_fontSmall);
            ImGui::TextDisabled("Mandatory operative authentication required for production deployment.");
            if (m_fontSmall) ImGui::PopFont();

            ImGui::Separator();
            ImGui::Spacing();

            // TAB BAR: [ 1. OPERATIVE LOGIN ] [ 2. ENLIST (REGISTER) ]
            float tabBtnW = (modalW - 48.0f - 10.0f) * 0.5f;
            if (renderCompetitorButton("1. OPERATIVE LOGIN", ImVec2(tabBtnW, 34), m_authTab == 0, m_authTab == 0)) {
                m_authTab = 0;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::SameLine(0, 10);
            if (renderCompetitorButton("2. ENLIST OPERATIVE (REGISTER)", ImVec2(tabBtnW, 34), m_authTab == 1, m_authTab == 1)) {
                m_authTab = 1;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (m_authTab == 0) {
                // ---- TAB 0: LOGIN ----
                if (m_fontBold) ImGui::PushFont(m_fontBold);
                ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.96f, 1.0f), "OPERATIVE USERNAME");
                if (m_fontBold) ImGui::PopFont();
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##AuthUser", "Enter operative callsign or username", m_authUsernameBuf, sizeof(m_authUsernameBuf));

                ImGui::Spacing();

                if (m_fontBold) ImGui::PushFont(m_fontBold);
                ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.96f, 1.0f), "SECURITY ACCESS KEY");
                if (m_fontBold) ImGui::PopFont();
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##AuthPass", "Enter password key", m_authPasswordBuf, sizeof(m_authPasswordBuf), ImGuiInputTextFlags_Password);

                ImGui::Spacing();
                ImGui::Spacing();

                if (renderCompetitorButton(">  AUTHENTICATE & ENTER TERMINAL", ImVec2(-1, 40), false, true)) {
                    if (strlen(m_authUsernameBuf) > 0 && strlen(m_authPasswordBuf) > 0) {
                        NetworkEngine::get().sendAuthStandalone(m_authUsernameBuf, m_authPasswordBuf);
                        AudioEngine::get().playSound(SoundType::TacticalClick);
                    }
                }

                if (NetworkEngine::get().isAuthPending()) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), ">>> TRANSMITTING CREDENTIALS TO MASTER CLUSTER...");
                } else if (!NetworkEngine::get().getAuthError().empty()) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[!] ACCESS DENIED: %s", NetworkEngine::get().getAuthError().c_str());
                }
            } else {
                // ---- TAB 1: ENLIST / REGISTER ----
                if (m_fontBold) ImGui::PushFont(m_fontBold);
                ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.96f, 1.0f), "DESIRED USERNAME");
                if (m_fontBold) ImGui::PopFont();
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##RegUser", "Unique login account username", m_regUsernameBuf, sizeof(m_regUsernameBuf));

                ImGui::Spacing();

                if (m_fontBold) ImGui::PushFont(m_fontBold);
                ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.96f, 1.0f), "SECURITY PASSWORD");
                if (m_fontBold) ImGui::PopFont();
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##RegPass", "Account password key", m_regPasswordBuf, sizeof(m_regPasswordBuf), ImGuiInputTextFlags_Password);

                ImGui::Spacing();

                if (m_fontBold) ImGui::PushFont(m_fontBold);
                ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.96f, 1.0f), "COMBAT CALLSIGN");
                if (m_fontBold) ImGui::PopFont();
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##RegCallsign", "Public battlefield callsign (e.g. VIPER)", m_regCallsignBuf, sizeof(m_regCallsignBuf));

                ImGui::Spacing();

                // Faction Allegiance
                float fW = (modalW - 48.0f - 10.0f) * 0.5f;
                bool isCoalition = (m_regFaction == 0);
                if (renderCompetitorButton("MERCENARY COALITION\nAllied Frontline", ImVec2(fW, 36), isCoalition, isCoalition)) {
                    m_regFaction = 0;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
                ImGui::SameLine(0, 10);
                bool isCrimson = (m_regFaction == 1);
                if (renderCompetitorButton("CRIMSON LEGION\nShock Brigade", ImVec2(fW, 36), isCrimson, isCrimson)) {
                    m_regFaction = 1;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }

                ImGui::Spacing();

                if (renderCompetitorButton("+  ENLIST NEW OPERATIVE", ImVec2(-1, 38), false, true)) {
                    if (strlen(m_regUsernameBuf) > 0 && strlen(m_regPasswordBuf) > 0 && strlen(m_regCallsignBuf) > 0) {
                        NetworkEngine::get().sendRegister(m_regUsernameBuf, m_regPasswordBuf, m_regCallsignBuf, m_regFaction);
                        AudioEngine::get().playSound(SoundType::DeployHorn);
                    }
                }

                if (NetworkEngine::get().isRegisterPending()) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), ">>> ENLISTING OPERATIVE ON CENTRAL DATABASE...");
                } else if (!NetworkEngine::get().getRegisterStatus().empty()) {
                    ImGui::Spacing();
                    if (NetworkEngine::get().isRegisterSuccess()) {
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[+] %s", NetworkEngine::get().getRegisterStatus().c_str());
                        strncpy_s(m_authUsernameBuf, m_regUsernameBuf, sizeof(m_authUsernameBuf) - 1);
                        strncpy_s(m_authPasswordBuf, m_regPasswordBuf, sizeof(m_authPasswordBuf) - 1);
                        m_authTab = 0;
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[!] %s", NetworkEngine::get().getRegisterStatus().c_str());
                    }
                }
            }

            ImGui::SetCursorPosY(modalH - 48.0f);
            if (renderCompetitorButton("EXIT TO DESKTOP", ImVec2(160, 32))) {
                exit(0);
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

void UIManager::renderCharacterSelectScreen(Player& player) {
    if (!NetworkEngine::get().isAuthenticated()) {
        renderInGameAuthScreen(player);
        return;
    }
    ImGuiIO& io = ImGui::GetIO();
    float barH = 50.0f;
    ImGuiWindowFlags topFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | 
                                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    // =========================================================================
    // 1. TOP HEADER BAR: Sleek Obsidian Banner with Steam / Persona Telemetry
    // =========================================================================
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, barH), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
    if (ImGui::Begin("##CharSelectTopBar", nullptr, topFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(0, barH), ImVec2(io.DisplaySize.x, barH), IM_COL32(30, 38, 50, 255));

        // Monogram badge [M]
        dl->AddRectFilled(ImVec2(22, 10), ImVec2(52, 40), IM_COL32(0, 120, 242, 255), 5.0f);
        dl->AddText(ImVec2(31, 14), IM_COL32(255, 255, 255, 255), "M");

        if (m_fontBold) ImGui::PushFont(m_fontBold);
        dl->AddText(ImVec2(64, 15), IM_COL32(235, 242, 252, 255), "MERCENARY");
        if (m_fontBold) ImGui::PopFont();

        if (m_fontRegular) ImGui::PushFont(m_fontRegular);
        dl->AddText(ImVec2(185, 15), IM_COL32(110, 125, 145, 255), "OPERATIVE TERMINAL");
        if (m_fontRegular) ImGui::PopFont();

        // Right Side: Online Status & Steam Identity
        float rightX = io.DisplaySize.x - 300.0f;
        dl->AddCircleFilled(ImVec2(rightX, 25), 4.5f, IM_COL32(46, 204, 113, 255));
        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        if (SteamIntegration::get().isInitialized()) {
            char steamBuf[128];
            snprintf(steamBuf, sizeof(steamBuf), "Steam: %s [Online]", SteamIntegration::get().getPersonaName().c_str());
            dl->AddText(ImVec2(rightX + 12, 18), IM_COL32(170, 190, 215, 255), steamBuf);
        } else {
            dl->AddText(ImVec2(rightX + 12, 18), IM_COL32(140, 155, 175, 255), "Local Tactical Protocol [Online]");
        }
        if (m_fontSmall) ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    float sideW = 400.0f;
    float botH = 68.0f;
    float sideH = io.DisplaySize.y - barH - botH - 28.0f;
    ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | 
                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;

    ProfileManager& pm = ProfileManager::get();
    int activeIdx = pm.getActiveSlotIndex();

    // =========================================================================
    // 2. LEFT PANEL: OPERATIVE ROSTER (Docked on Left, 3D Bunker Operative Visible in Center!)
    // =========================================================================
    ImGui::SetNextWindowPos(ImVec2(28, barH + 14), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(sideW, sideH), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.08f, 0.11f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.80f));
    if (ImGui::Begin("##CharSelectRosterDock", nullptr, panelFlags)) {
        if (m_fontHeading) ImGui::PushFont(m_fontHeading);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "PERSONNEL ROSTER");
        if (m_fontHeading) ImGui::PopFont();

        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        ImGui::TextDisabled("Select or recruit combat operative for deployment:");
        if (m_fontSmall) ImGui::PopFont();

        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < 3; ++i) {
            CharacterProfile& slot = pm.getSlot(i);
            bool isSelected = (slot.created && activeIdx == i);
            ImGui::PushID(i);

            if (slot.created) {
                // Active Operator Card
                ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.10f, 0.16f, 0.24f, 0.95f) : ImVec4(0.09f, 0.11f, 0.14f, 0.80f));
                ImGui::PushStyleColor(ImGuiCol_Border, isSelected ? ImVec4(0.00f, 0.55f, 1.00f, 0.90f) : ImVec4(0.18f, 0.22f, 0.28f, 0.60f));

                if (ImGui::BeginChild("SlotCard", ImVec2(0, 116), true)) {
                    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
                    ImGui::TextColored(ImVec4(0.55f, 0.62f, 0.72f, 1.0f), "OPERATIVE 0%d", i + 1);
                    if (isSelected) {
                        ImGui::SameLine(sideW - 130);
                        ImGui::TextColored(ImVec4(0.00f, 0.75f, 1.00f, 1.0f), "[ ACTIVE ]");
                    }
                    if (m_fontSmall) ImGui::PopFont();

                    if (m_fontBold) ImGui::PushFont(m_fontBold);
                    ImGui::TextColored(ImVec4(0.95f, 0.98f, 1.0f, 1.0f), "%s", slot.name.c_str());
                    if (m_fontBold) ImGui::PopFont();

                    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
                    if (slot.faction == Faction::FactionA) {
                        ImGui::TextColored(ImVec4(0.38f, 0.72f, 1.00f, 1.0f), "MERCENARY COALITION");
                    } else {
                        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "CRIMSON LEGION");
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled(" |  LVL %02d", slot.level);

                    int totalRaids = std::max(1, slot.contractsCompleted + 2);
                    float kd = (float)slot.combatKills / std::max(1.0f, (float)(totalRaids - slot.contractsCompleted));
                    ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.82f, 1.0f), "K/D: %.2f  •  Kills: %d  •  Raids: %d", kd, slot.combatKills, slot.contractsCompleted);
                    if (m_fontSmall) ImGui::PopFont();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemClicked()) {
                    pm.setActiveSlotIndex(i);
                    CharacterProfile* newActive = pm.getActiveProfile();
                    if (newActive) {
                        player.getInventory() = newActive->savedInventory;
                        player.updateArmorFromEquipped();
                        player.setFaction(newActive->faction);
                    }
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            } else {
                // Sleek Minimalist Empty Slot Card (Production Standard)
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.09f, 0.12f, 0.65f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.50f));

                if (ImGui::BeginChild("EmptySlotCard", ImVec2(0, 100), true)) {
                    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
                    ImGui::TextColored(ImVec4(0.48f, 0.54f, 0.64f, 1.0f), "OPERATIVE 0%d", i + 1);
                    ImGui::SameLine(sideW - 130);
                    ImGui::TextDisabled("[ VACANT ]");
                    if (m_fontSmall) ImGui::PopFont();

                    ImGui::Spacing();
                    ImGui::SetCursorPosX(16);
                    if (m_fontBold) ImGui::PushFont(m_fontBold);
                    ImGui::TextColored(ImVec4(0.00f, 0.60f, 1.00f, 1.0f), "+ CREATE NEW OPERATIVE");
                    if (m_fontBold) ImGui::PopFont();

                    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
                    ImGui::TextDisabled("Select to enlist a new combat operative.");
                    if (m_fontSmall) ImGui::PopFont();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor(2);

                if (ImGui::IsItemClicked()) {
                    m_showRecruitModal = true;
                    m_recruitSlotIndex = i;
                    m_recruitFaction = 1;
                    m_recruitNameBuf[0] = '\0';
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            }
            ImGui::Spacing();
            ImGui::PopID();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    // =========================================================================
    // 3. RIGHT PANEL: OPERATIVE COMBAT DOSSIER (Docked on Right)
    // =========================================================================
    float dosW = 400.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - dosW - 28, barH + 14), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(dosW, sideH), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.08f, 0.11f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.80f));
    if (ImGui::Begin("##CharSelectDossierDock", nullptr, panelFlags)) {
        CharacterProfile* active = pm.getActiveProfile();

        if (m_fontHeading) ImGui::PushFont(m_fontHeading);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "OPERATIVE DOSSIER");
        if (m_fontHeading) ImGui::PopFont();

        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        ImGui::TextDisabled("Tactical profile & active battlefield deployment readiness:");
        if (m_fontSmall) ImGui::PopFont();

        ImGui::Separator();
        ImGui::Spacing();

        if (active) {
            // Dogtag header
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.13f, 0.18f, 0.90f));
            if (ImGui::BeginChild("DogtagHeader", ImVec2(0, 84), true)) {
                if (m_fontTitle) ImGui::PushFont(m_fontTitle);
                ImGui::TextColored(ImVec4(0.95f, 0.98f, 1.0f, 1.0f), "%s", active->name.c_str());
                if (m_fontTitle) ImGui::PopFont();

                if (m_fontSmall) ImGui::PushFont(m_fontSmall);
                if (active->faction == Faction::FactionA) {
                    ImGui::TextColored(ImVec4(0.38f, 0.72f, 1.00f, 1.0f), "MERCENARY COALITION");
                } else {
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "CRIMSON LEGION");
                }
                ImGui::SameLine();
                ImGui::TextDisabled("|  RANK %02d", active->level);
                if (m_fontSmall) ImGui::PopFont();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Telemetry Grid
            int totalRaids = std::max(1, active->contractsCompleted + 2);
            float sr = (float)active->contractsCompleted / (float)totalRaids * 100.0f;
            float kd = (float)active->combatKills / std::max(1.0f, (float)(totalRaids - active->contractsCompleted));

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.11f, 0.15f, 0.85f));
            if (ImGui::BeginChild("TeleGrid", ImVec2(0, 140), true)) {
                if (m_fontSmall) ImGui::PushFont(m_fontSmall);
                ImGui::TextDisabled("SURVIVAL RATE");
                ImGui::SameLine(180);
                ImGui::TextDisabled("KILL / DEATH");

                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "%.1f%%", sr);
                ImGui::SameLine(180);
                ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "%.2f", kd);

                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("CONFIRMED KILLS");
                ImGui::SameLine(180);
                ImGui::TextDisabled("SUCCESSFUL EXFILS");

                ImGui::TextColored(ImVec4(0.92f, 0.94f, 0.96f, 1.0f), "%d", active->combatKills);
                ImGui::SameLine(180);
                ImGui::TextColored(ImVec4(0.92f, 0.94f, 0.96f, 1.0f), "%d", active->contractsCompleted);

                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("DEPLOYED FUNDS:");
                ImGui::SameLine(130);
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "$ %d USD", active->cash);
                ImGui::SameLine(250);
                ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.15f, 1.0f), "%d GOLD", active->goldBars);
                if (m_fontSmall) ImGui::PopFont();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Loadout preview
            if (m_fontBold) ImGui::PushFont(m_fontBold);
            ImGui::TextColored(ImVec4(0.72f, 0.78f, 0.86f, 1.0f), "EQUIPPED COMBAT LOADOUT");
            if (m_fontBold) ImGui::PopFont();

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.10f, 0.14f, 0.80f));
            if (ImGui::BeginChild("EquipDossier", ImVec2(0, 100), true)) {
                if (m_fontSmall) ImGui::PushFont(m_fontSmall);
                ImGui::BulletText("Primary: Colt M4A1 SOPMOD 5.56x45mm");
                ImGui::BulletText("Sidearm: Beretta M9A3 9x19mm Parabellum");
                ImGui::BulletText("Body Armor: Crye Precision JPC 2.0 (Level IV)");
                ImGui::BulletText("Headgear: FAST MT High-Cut Ballistic Helmet");
                if (m_fontSmall) ImGui::PopFont();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.12f, 0.12f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.15f, 0.15f, 0.9f));
            if (ImGui::Button("Dismiss Operative (Delete Profile)", ImVec2(dosW - 32, 28))) {
                pm.deleteCharacter(pm.getActiveSlotIndex());
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::PopStyleColor(2);
        } else {
            ImGui::TextDisabled("No operative currently selected.");
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);

    // =========================================================================
    // 4. BOTTOM ACTION BAR: Exit, Telemetry & Radiant Blue DEPLOY Button
    // =========================================================================
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - botH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, botH), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));
    if (ImGui::Begin("##CharSelectBottomBar", nullptr, topFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(0, io.DisplaySize.y - botH), ImVec2(io.DisplaySize.x, io.DisplaySize.y - botH), IM_COL32(30, 38, 50, 255));

        // Exit to Desktop Button (Left)
        ImGui::SetCursorPos(ImVec2(28, 16));
        if (renderCompetitorButton("EXIT TO DESKTOP", ImVec2(160, 36))) {
            exit(0);
        }

        // Center Status Text
        float centerX = io.DisplaySize.x * 0.5f - 190.0f;
        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        dl->AddText(ImVec2(centerX, io.DisplaySize.y - botH + 26), IM_COL32(110, 125, 145, 255), 
                    "MERCENARY TACTICAL ENGINE v1.0.5  •  STANDALONE OPERATIVE TERMINAL");
        if (m_fontSmall) ImGui::PopFont();

        // Big Radiant Blue Action Button: [ ▶ DEPLOY OPERATIVE ] (Right)
        CharacterProfile* active = pm.getActiveProfile();
        float btnW = 280.0f;
        float btnH = 46.0f;
        ImGui::SetCursorPos(ImVec2(io.DisplaySize.x - btnW - 28, 11));

        if (active) {
            if (m_fontHeading) ImGui::PushFont(m_fontHeading);
            if (renderCompetitorButton(">  DEPLOY OPERATIVE", ImVec2(btnW, btnH), false, true)) {
                m_gameState = GameState::MainMenu;
                m_currentTab = MainMenuTab::Overview;
                if (active) {
                    player.getInventory() = active->savedInventory;
                    player.updateArmorFromEquipped();
                    player.setFaction(active->faction);
                }
                AudioEngine::get().playSound(SoundType::TacticalClick);
                AudioEngine::get().playSound(SoundType::ItemEquip, 0.8f, 1.0f);
                SteamIntegration::get().setRichPresence("status", "MERCENARY: PMC Hideout");
            }
            if (m_fontHeading) ImGui::PopFont();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.17f, 0.22f, 0.70f));
            ImGui::Button("NO OPERATIVE SELECTED", ImVec2(btnW, btnH));
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    if (m_showRecruitModal) {
        renderRecruitOperativeModal(player);
    }
}
void UIManager::renderTopNavigationBar(Player& player, CharacterProfile* active) {
    (void)player;
    ImGuiIO& io = ImGui::GetIO();
    float barH = 54.0f;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, barH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));

    if (ImGui::Begin("##TopGlobalNavbar", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // 1. BRAND LOGO & OPERATOR TAG (Left side)
        dl->AddRectFilled(ImVec2(18, 12), ImVec2(46, 42), IM_COL32(0, 120, 242, 255), 4.0f);
        dl->AddText(ImVec2(27, 16), IM_COL32(255, 255, 255, 255), "M");

        if (m_fontBold) ImGui::PushFont(m_fontBold);
        dl->AddText(ImVec2(54, 17), IM_COL32(245, 248, 252, 255), "MERCENARY");
        if (m_fontBold) ImGui::PopFont();

        if (active) {
            float dogtagX = 180.0f;
            char tagBuf[128];
            std::snprintf(tagBuf, sizeof(tagBuf), "[LVL %02d]  %s", active->level, active->name.c_str());
            dl->AddText(ImVec2(dogtagX, 18), IM_COL32(235, 180, 50, 255), tagBuf);
            if (active->faction == Faction::FactionA) {
                dl->AddText(ImVec2(dogtagX + ImGui::CalcTextSize(tagBuf).x + 10.0f, 18), IM_COL32(95, 175, 255, 255), "|  COALITION");
            } else {
                dl->AddText(ImVec2(dogtagX + ImGui::CalcTextSize(tagBuf).x + 10.0f, 18), IM_COL32(245, 85, 85, 255), "|  CRIMSON");
            }
        }

        // 2. CENTER NAVIGATION TABS (Flat clickable text, orange active underline indicator)
        const char* navTabLabels[] = { "OVERVIEW", "DEPLOYMENT", "EXCHANGE", "WARZONE MAP", "CUSTOMIZE" };
        const MainMenuTab navTabEnums[] = { MainMenuTab::Overview, MainMenuTab::Deployment, MainMenuTab::ExchangeMarket, MainMenuTab::TacticalMap, MainMenuTab::Customize };
        const int numNavTabs = 5;

        float totalTabsW = 0.0f;
        float tabWidths[5];
        for (int t = 0; t < numNavTabs; ++t) {
            tabWidths[t] = ImGui::CalcTextSize(navTabLabels[t]).x + 12.0f;
            totalTabsW += tabWidths[t] + 28.0f;
        }
        totalTabsW -= 28.0f;

        float navStart = std::max(360.0f, (io.DisplaySize.x * 0.5f) - (totalTabsW * 0.5f));
        float curNavX = navStart;
        float navY = 16.0f;

        for (int t = 0; t < numNavTabs; ++t) {
            bool isSel = (m_currentTab == navTabEnums[t]);
            ImVec2 textSize = ImGui::CalcTextSize(navTabLabels[t]);
            float hitW = textSize.x + 12.0f;
            float hitH = 32.0f;

            ImGui::SetCursorScreenPos(ImVec2(curNavX - 6.0f, navY - 4.0f));
            ImGui::InvisibleButton(navTabLabels[t], ImVec2(hitW, hitH));
            bool isHovered = ImGui::IsItemHovered();

            if (ImGui::IsItemClicked()) {
                m_currentTab = navTabEnums[t];
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }

            ImU32 txtCol = isSel ? IM_COL32(255, 255, 255, 255) :
                           (isHovered ? IM_COL32(225, 232, 242, 255) : IM_COL32(145, 155, 168, 255));
            
            dl->AddText(ImVec2(curNavX, navY), txtCol, navTabLabels[t]);

            if (isSel) {
                // Vibrant burnt orange indicator line (Exact competitor aesthetic)
                dl->AddLine(
                    ImVec2(curNavX - 2.0f, navY + 24.0f),
                    ImVec2(curNavX + textSize.x + 2.0f, navY + 24.0f),
                    IM_COL32(219, 98, 41, 255), 2.5f
                );
            }

            curNavX += hitW + 28.0f;
        }

        // 3. RIGHT SECTION: Currencies & Quick Utility Actions
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
        }
        if (active) {
            // Cash Chip ($ USD)
            char cashBuf[32];
            std::snprintf(cashBuf, sizeof(cashBuf), "$ %d", active->cash);
            dl->AddRectFilled(ImVec2(rightX, 13), ImVec2(rightX + 85, 41), IM_COL32(18, 24, 22, 220), 3.0f);
            dl->AddRect(ImVec2(rightX, 13), ImVec2(rightX + 85, 41), IM_COL32(40, 75, 55, 220), 3.0f, 0, 1.0f);
            dl->AddText(ImVec2(rightX + 10, 18), IM_COL32(75, 220, 115, 255), cashBuf);

            // Gold Bars Chip
            char goldBuf[32];
            std::snprintf(goldBuf, sizeof(goldBuf), "%d GOLD", active->goldBars);
            dl->AddRectFilled(ImVec2(rightX + 92, 13), ImVec2(rightX + 175, 41), IM_COL32(26, 22, 16, 220), 3.0f);
            dl->AddRect(ImVec2(rightX + 92, 13), ImVec2(rightX + 175, 41), IM_COL32(85, 70, 35, 220), 3.0f, 0, 1.0f);
            dl->AddText(ImVec2(rightX + 102, 18), IM_COL32(245, 195, 50, 255), goldBuf);
        }

        // Operatives Profile Selector Button
        ImGui::SetCursorScreenPos(ImVec2(rightX + 184, 12));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.12f, 0.15f, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.20f, 0.26f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.28f, 0.36f, 0.85f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
        if (ImGui::Button("OPERATIVES", ImVec2(98, 30))) {
            m_gameState = GameState::CharacterSelect;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }

        // Settings Modal Button
        ImGui::SetCursorScreenPos(ImVec2(rightX + 288, 12));
        if (m_optionsOpen) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.28f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.35f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.40f, 0.15f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.12f, 0.15f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.20f, 0.26f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.28f, 0.36f, 0.85f));
        }
        if (ImGui::Button("SETTINGS", ImVec2(80, 30))) {
            m_optionsOpen = !m_optionsOpen;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);

        // Subtle bottom border accent
        dl->AddLine(
            ImVec2(0, barH - 1.0f),
            ImVec2(io.DisplaySize.x, barH - 1.0f),
            IM_COL32(32, 38, 48, 255), 1.0f
        );
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
}

void UIManager::renderLobbyMainMenu(Player& player, const World& world, const MissionSystem& missions) {
    ImGuiIO& io = ImGui::GetIO();
    ProfileManager& pm = ProfileManager::get();
    CharacterProfile* active = pm.getActiveProfile();

    // 1. PINNED TOP ARENA BREAKOUT NAVBAR (Y=0, H=52)
    renderTopNavigationBar(player, active);

    float topY = 60.0f;
    float contentH = io.DisplaySize.y - topY - 20.0f;
    ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;

    if (m_currentTab == MainMenuTab::Overview) {
        // LEFT DOCKED WINDOW: Compact Operator Dossier (Width 390px)
        // Center/Right screen remains 100% open showing the 3D Bunker Hideout!
        ImGui::SetNextWindowPos(ImVec2(24, topY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(390, contentH), ImGuiCond_Always);

        if (ImGui::Begin("##OverviewDossierDock", nullptr, dockFlags)) {
            if (m_fontHeading) ImGui::PushFont(m_fontHeading);
            ImGui::TextColored(ImVec4(0.95f, 0.98f, 1.0f, 1.0f), "OPERATIVE DOSSIER");
            if (m_fontHeading) ImGui::PopFont();
            ImGui::Separator();
            ImGui::Spacing();

            if (active) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.14f, 0.18f, 0.95f));
                ImGui::BeginChild("DogtagEmboss", ImVec2(0, 72), true);
                ImGui::SetWindowFontScale(1.25f);
                ImGui::TextColored(ImVec4(0.95f, 0.92f, 0.88f, 1.0f), "%s", active->name.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::Spacing();
                if (active->faction == Faction::FactionA) {
                    ImGui::TextColored(ImVec4(0.38f, 0.72f, 1.00f, 1.0f), "MERCENARY COALITION");
                } else {
                    ImGui::TextColored(ImVec4(0.95f, 0.32f, 0.32f, 1.0f), "CRIMSON LEGION");
                }
                ImGui::SameLine();
                ImGui::TextDisabled(" |  RANK %02d", active->level);
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Spacing();

                int totalRaids = std::max(1, active->contractsCompleted + 2);
                float sr = (float)active->contractsCompleted / (float)totalRaids * 100.0f;
                float kd = (float)active->combatKills / std::max(1.0f, (float)(totalRaids - active->contractsCompleted));

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.13f, 0.16f, 0.90f));
                ImGui::BeginChild("StatsGrid", ImVec2(0, 140), true);
                ImGui::TextDisabled("SURVIVAL RATE");
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "%.1f%%", sr);
                ImGui::SameLine(180);
                ImGui::TextDisabled("KILL / DEATH");
                ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "%.2f", kd);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::TextDisabled("CONFIRMED KILLS");
                ImGui::TextColored(ImVec4(0.92f, 0.94f, 0.96f, 1.0f), "%d", active->combatKills);
                ImGui::SameLine(180);
                ImGui::TextDisabled("SUCCESSFUL RAIDS");
                ImGui::TextColored(ImVec4(0.92f, 0.94f, 0.96f, 1.0f), "%d", active->contractsCompleted);

                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Spacing();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // ACTIVE LOADOUT TELEMETRY
                ImGui::TextDisabled("ACTIVE LOADOUT SUMMARY:");
                auto& pri = player.getInventory().getEquipped(EquipSlot::PrimaryWeapon);
                auto& sec = player.getInventory().getEquipped(EquipSlot::SecondaryWeapon);
                auto& vest = player.getInventory().getEquipped(EquipSlot::Vest);
                auto& helm = player.getInventory().getEquipped(EquipSlot::Helmet);

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.11f, 0.14f, 0.90f));
                ImGui::BeginChild("EquipMiniPreview", ImVec2(0, 115), true);
                
                ImGui::TextDisabled("PRI:");
                ImGui::SameLine(50);
                if (pri.has_value()) {
                    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "%s", pri->name.c_str());
                } else {
                    ImGui::TextDisabled("[UNARMED PRIMARY]");
                }

                ImGui::TextDisabled("SEC:");
                ImGui::SameLine(50);
                if (sec.has_value()) {
                    ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.90f, 1.0f), "%s", sec->name.c_str());
                } else {
                    ImGui::TextDisabled("[NO SIDEARM]");
                }

                ImGui::TextDisabled("ARM:");
                ImGui::SameLine(50);
                if (vest.has_value()) {
                    ImGui::TextColored(ImVec4(0.35f, 0.92f, 0.65f, 1.0f), "%s", vest->name.c_str());
                } else {
                    ImGui::TextDisabled("[NO BODY ARMOR]");
                }

                ImGui::TextDisabled("HLM:");
                ImGui::SameLine(50);
                if (helm.has_value()) {
                    ImGui::TextColored(ImVec4(0.38f, 0.75f, 1.0f, 1.0f), "%s", helm->name.c_str());
                } else {
                    ImGui::TextDisabled("[NO HELMET]");
                }

                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Sleek Tactical Navigation Buttons
                if (renderCompetitorButton("PREPARE DEPLOYMENT", ImVec2(170, 38), false, true)) {
                    m_currentTab = MainMenuTab::Deployment;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
                ImGui::SameLine(0, 12.0f);
                if (renderCompetitorButton("DYNAMIC EXCHANGE", ImVec2(170, 38))) {
                    m_currentTab = MainMenuTab::ExchangeMarket;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            }

            ImGui::End();
        }

        // Dedicated Bottom-Right Matchmaking & Solo/Squad Dock
        renderBottomRightMatchmakingDock(player, active);
    }
    else if (m_currentTab == MainMenuTab::Deployment) {
        renderDeploymentStaging(player, active, world, missions);
    }
    else if (m_currentTab == MainMenuTab::ExchangeMarket) {
        renderExchangeMarket(player, active);
    }
    else if (m_currentTab == MainMenuTab::TacticalMap) {
        renderTacticalMap(player, world, missions);
    }
    else if (m_currentTab == MainMenuTab::Customize) {
        ImGui::SetNextWindowPos(ImVec2(24, topY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(480, contentH), ImGuiCond_Always);
        if (ImGui::Begin("##CustomizeControlsDock", nullptr, dockFlags)) {
            renderCustomizationMenu(active, &player);
            ImGui::End();
        }
    }

    if (m_showWeaponInspect) {
        renderWeaponInspectModal();
    }
}

// =========================================================================
// 2B-2. BOTTOM-RIGHT DEDICATED DEPLOYMENT & SOLO/SQUAD DOCK
// =========================================================================
void UIManager::renderBottomRightMatchmakingDock(Player& player, CharacterProfile* active) {
    (void)player;
    (void)active;
    ImGuiIO& io = ImGui::GetIO();
    float dockW = 420.0f;
    float dockH = 270.0f;
    float posX = io.DisplaySize.x - dockW - 24.0f;
    float posY = io.DisplaySize.y - dockH - 24.0f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(dockW, dockH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.08f, 0.11f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.25f, 0.32f, 0.85f));

    if (ImGui::Begin("##BottomRightMatchmakingDock", nullptr, flags)) {
        const auto& shard = NetworkEngine::get().getActiveShard();

        if (m_isMatchmaking) {
            m_matchTimer += io.DeltaTime;

            if (NetworkEngine::get().isClientConnected() && !NetworkEngine::get().isMatchmaking() && NetworkEngine::get().getCurrentSessionId() > 0) {
                m_isMatchmaking = false;
                m_isDeploying = true;
                m_isOnlineMultiplayer = true;
                m_deploymentTimer = 0.0f;
                m_tipRotateTimer = 0.0f;
                AudioEngine::get().playSound(SoundType::TacticalClick);
                AudioEngine::get().playSound(SoundType::DeployHorn);
            } else if (m_matchTimer > 12.0f) {
                m_matchmakingTimeout = true;
            }

            ImGui::Spacing();
            if (!m_matchmakingTimeout) {
                int spinIdx = (int)(m_matchTimer * 6.0f) % 4;
                const char* spinnerChars[] = { "[ / ]", "[ - ]", "[ \\ ]", "[ | ]" };

                ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s MATCHMAKING IN PROGRESS...", spinnerChars[spinIdx]);
                
                int elapsedSec = (int)m_matchTimer;
                int mm = elapsedSec / 60;
                int ss = elapsedSec % 60;
                ImGui::TextDisabled("ELAPSED: %02d:%02d  |  ESTIMATED WAIT: < 00:15", mm, ss);

                std::string tdmTitle = m_tdmAgainstAi ? "5v5 TDM // AGAINST AI (BOTS)" : "5v5 TDM // PLAYERS ONLY (PVP)";
                const char* playlistNames[] = {
                    tdmTitle.c_str(),
                    "TACTICAL INFILTRATION // FOB VANGUARD",
                    "PERSISTENT MMO WARZONE // CONTINENTAL"
                };
                ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.92f, 1.0f), "PLAYLIST: %s", playlistNames[std::clamp(m_matchmakingPlaylist, 0, 2)]);
                
                if (shard.isOnline) {
                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "REGION: OFFICIAL DEDICATED CLUSTER (%dms)", shard.pingMs);
                } else {
                    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "STATUS: ALLOCATING OPERATIVE TICKET...");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (renderCompetitorButton("[ CANCEL MATCHMAKING ]", ImVec2(-1, 38))) {
                    m_isMatchmaking = false;
                    m_matchTimer = 0.0f;
                    NetworkEngine::get().disconnectClient();
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            } else {
                ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.35f, 1.0f), "[!] MATCHMAKING NOTICE // CLUSTER STANDBY");
                ImGui::TextDisabled("Dedicated cluster did not respond within timeout.");
                ImGui::Spacing();

                if (renderCompetitorButton("> RETRY MATCHMAKING", ImVec2(-1, 34))) {
                    m_matchTimer = 0.0f;
                    m_matchmakingTimeout = false;
                    m_isMatchmaking = true;
                    m_isOnlineMultiplayer = true;
                    uint8_t desiredMode = (m_matchmakingPlaylist == 1) ? 2 : ((m_matchmakingPlaylist == 2) ? 0 : 1);
                    std::string mapName = (m_matchmakingPlaylist == 1) ? "fob_vanguard" : ((m_matchmakingPlaylist == 2) ? "active_warzone" : "tdm_yard");
                    if (!NetworkEngine::get().isClientConnected()) {
                        NetworkEngine::get().connectToServer(shard.host, shard.port);
                    }
                    uint8_t pFaction = (player.getFaction() == Faction::FactionB) ? 1 : 0;
                    NetworkEngine::get().requestMatchmaking(desiredMode, mapName, pFaction);
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }

                ImGui::Spacing();
                if (renderCompetitorButton("> DEPLOY SOLO / SQUAD (LOCAL AI COMBAT)", ImVec2(-1, 38), false, true)) {
                    m_isMatchmaking = false;
                    m_matchmakingTimeout = false;
                    m_isTrainingMode = false;
                    m_isOnlineMultiplayer = false;
                    m_isDeploying = true;
                    m_deploymentTimer = 0.0f;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                    AudioEngine::get().playSound(SoundType::DeployHorn);
                }

                ImGui::Spacing();
                if (renderCompetitorButton("CANCEL TO HIDEOUT", ImVec2(-1, 28))) {
                    m_isMatchmaking = false;
                    m_matchmakingTimeout = false;
                    NetworkEngine::get().disconnectClient();
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            }
        } else {
            // Header: Operations Protocol & Region Status
            ImGui::SetWindowFontScale(0.92f);
            ImGui::TextColored(ImVec4(0.55f, 0.60f, 0.68f, 1.0f), "TACTICAL OPERATIONS // AAA MATCHMAKING");
            ImGui::SameLine(dockW - 150.0f);
            if (shard.isOnline) {
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "[CLUSTER ONLINE]");
            } else {
                ImGui::TextColored(ImVec4(0.70f, 0.75f, 0.82f, 1.0f), "[CLUSTER READY]");
            }
            ImGui::SetWindowFontScale(1.0f);

            ImGui::Spacing();

            // 3-Mode Selector (TDM, Infiltration, Open World)
            float modeBtnW = (dockW - 32.0f - 12.0f) / 3.0f;
            if (renderCompetitorButton("TDM 5v5", ImVec2(modeBtnW, 28), m_matchmakingPlaylist == 0, m_matchmakingPlaylist == 0, ImDrawFlags_RoundCornersLeft)) {
                m_matchmakingPlaylist = 0;
                m_selectedGameMode = SelectedGameMode::TeamDeathmatch;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::SameLine(0, 6.0f);
            if (renderCompetitorButton("INFILTRATION", ImVec2(modeBtnW, 28), m_matchmakingPlaylist == 1, m_matchmakingPlaylist == 1, ImDrawFlags_RoundCornersNone)) {
                m_matchmakingPlaylist = 1;
                m_selectedGameMode = SelectedGameMode::Infiltration;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::SameLine(0, 6.0f);
            if (renderCompetitorButton("OPEN WORLD", ImVec2(modeBtnW, 28), m_matchmakingPlaylist == 2, m_matchmakingPlaylist == 2, ImDrawFlags_RoundCornersRight)) {
                m_matchmakingPlaylist = 2;
                m_selectedGameMode = SelectedGameMode::OpenWorld;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }

            ImGui::Spacing();

            if (m_matchmakingPlaylist == 0) {
                // TDM 5v5 Mode: Choose between Against AI (Bots) vs Players Only (PvP)
                float tdmOptW = (dockW - 32.0f - 8.0f) * 0.5f;
                if (renderCompetitorButton(m_tdmAgainstAi ? "[ AGAINST AI (BOTS) ]" : "AGAINST AI (BOTS)", ImVec2(tdmOptW, 28), m_tdmAgainstAi, m_tdmAgainstAi, ImDrawFlags_RoundCornersLeft)) {
                    m_tdmAgainstAi = true;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
                ImGui::SameLine(0, 8.0f);
                if (renderCompetitorButton(!m_tdmAgainstAi ? "[ PLAYERS ONLY (PVP) ]" : "PLAYERS ONLY (PVP)", ImVec2(tdmOptW, 28), !m_tdmAgainstAi, !m_tdmAgainstAi, ImDrawFlags_RoundCornersRight)) {
                    m_tdmAgainstAi = false;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            } else {
                // Segmented Operative Roster: SOLO vs SQUAD for Raid / Infiltration / Open World
                float pillW = (dockW - 32.0f - 8.0f) * 0.5f;
                if (renderCompetitorButton(!m_squadMode ? "[ SOLO OPERATIVE ]" : "SOLO OPERATIVE", ImVec2(pillW, 28), !m_squadMode, !m_squadMode, ImDrawFlags_RoundCornersLeft)) {
                    m_squadMode = false;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
                ImGui::SameLine(0, 8.0f);
                if (renderCompetitorButton(m_squadMode ? "[ SQUAD 4-MAN ]" : "SQUAD 4-MAN", ImVec2(pillW, 28), m_squadMode, m_squadMode, ImDrawFlags_RoundCornersRight)) {
                    m_squadMode = true;
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            }

            ImGui::Spacing();

            // Primary Glowing AAA Matchmaking Button
            if (m_fontHeading) ImGui::PushFont(m_fontHeading);
            if (renderCompetitorButton("> FIND MATCH // START MATCHMAKING", ImVec2(dockW - 32.0f, 42), false, true)) {
                m_isMatchmaking = true;
                m_matchTimer = 0.0f;
                m_matchmakingTimeout = false;
                m_isOnlineMultiplayer = true;

                uint8_t desiredMode = 1;
                std::string mapName = "tdm_yard";
                if (m_matchmakingPlaylist == 0) {
                    desiredMode = static_cast<uint8_t>(GameMode::TeamDeathmatch);
                    mapName = "tdm_yard";
                } else if (m_matchmakingPlaylist == 1) {
                    desiredMode = static_cast<uint8_t>(GameMode::Infiltration);
                    mapName = "fob_vanguard";
                } else if (m_matchmakingPlaylist == 2) {
                    desiredMode = static_cast<uint8_t>(GameMode::OpenWorld);
                    mapName = "active_warzone";
                }

                if (!NetworkEngine::get().isClientConnected()) {
                    NetworkEngine::get().connectToServer(shard.host, shard.port);
                }
                uint8_t pFaction = (player.getFaction() == Faction::FactionB) ? 1 : 0;
                NetworkEngine::get().requestMatchmaking(desiredMode, mapName, pFaction);

                AudioEngine::get().playSound(SoundType::TacticalClick);
                AudioEngine::get().playSound(SoundType::DeployHorn);
            }
            if (m_fontHeading) ImGui::PopFont();

            ImGui::Spacing();

            // Offline CQB Training Range
            if (renderCompetitorButton("[ CQB TRAINING RANGE // TARGET PRACTICE ]", ImVec2(dockW - 32.0f, 28))) {
                m_isTrainingMode = true;
                m_isDeploying = true;
                m_deploymentTimer = 0.0f;
                m_tipRotateTimer = 0.0f;
                AudioEngine::get().playSound(SoundType::TacticalClick);
                AudioEngine::get().playSound(SoundType::DeployHorn);
            }
        }

        ImGui::End();
    }
    ImGui::PopStyleColor(2);
}

// =========================================================================
// 2C. DEPLOYMENT STAGING: LOADOUT PAPERDOLL, STASH & FIELD QUARTERMASTER
// =========================================================================
void UIManager::renderServerOfflineModal(Player& player, CharacterProfile* active) {
    if (!m_serverOfflineModalOpen) return;
    (void)player;
    (void)active;

    ImGuiIO& io = ImGui::GetIO();
    float winW = 580.0f;
    float winH = 340.0f;
    float posX = io.DisplaySize.x * 0.5f - winW * 0.5f;
    float posY = io.DisplaySize.y * 0.5f - winH * 0.5f;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.09f, 0.12f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.85f, 0.25f, 0.20f, 0.90f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);

    if (ImGui::Begin("COMMUNICATION FAILURE // DEDICATED SHARD OFFLINE##OfflineModal", &m_serverOfflineModalOpen, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();

        // Red alert banner header
        dl->AddRectFilled(p0, ImVec2(p0.x + winW - 32.0f, p0.y + 36.0f), IM_COL32(180, 35, 30, 200), 3.0f);
        dl->AddRect(p0, ImVec2(p0.x + winW - 32.0f, p0.y + 36.0f), IM_COL32(235, 60, 50, 255), 3.0f, 0, 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(p0.x + 12.0f, p0.y + 8.0f));
        ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.95f, 1.0f), "[!] SERVER UNREACHABLE // DEPLOYMENT BLOCKED");

        ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + 48.0f));
        ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.94f, 1.0f),
            "Cannot deploy to live multiplayer warzone: The dedicated server shard is offline.");
        
        ImGui::Spacing();
        ImGui::TextDisabled("Status: %s", m_serverOfflineModalMsg.empty() ? "No response received from shard port 27015 (Host: 127.0.0.1)." : m_serverOfflineModalMsg.c_str());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "Select an option below to continue:");
        ImGui::Spacing();

        // Action 1: Deploy Solo Warzone (Full 600m Map Offline)
        if (renderCompetitorButton(">  DEPLOY SOLO WARZONE (OFFLINE - FULL 600m MAP)", ImVec2(-1, 44), false, true)) {
            m_serverOfflineModalOpen = false;
            m_isTrainingMode = false;
            m_isOnlineMultiplayer = false;
            m_isDeploying = true;
            m_deploymentTimer = 0.0f;
            m_tipRotateTimer = 0.0f;
            AudioEngine::get().playSound(SoundType::TacticalClick);
            AudioEngine::get().playSound(SoundType::DeployHorn);
        }

        ImGui::Spacing();

        // Action 2: CQB Training Range
        if (renderCompetitorButton("[ CQB TRAINING RANGE // TARGET PRACTICE ]", ImVec2(-1, 36))) {
            m_serverOfflineModalOpen = false;
            m_isTrainingMode = true;
            m_isDeploying = true;
            m_deploymentTimer = 0.0f;
            m_tipRotateTimer = 0.0f;
            AudioEngine::get().playSound(SoundType::TacticalClick);
            AudioEngine::get().playSound(SoundType::DeployHorn);
        }

        ImGui::Spacing();

        // Action 3: Return to Hideout
        if (renderCompetitorButton("RETURN TO PMC HIDEOUT", ImVec2(-1, 32))) {
            m_serverOfflineModalOpen = false;
            m_currentTab = MainMenuTab::Overview;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }

        ImGui::End();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

// =========================================================================
// 3. TARKOV WEAPON INSPECT / MODDING BLUEPRINT MODAL
// =========================================================================
void UIManager::renderDeathScreen(Player& player) {
    ImGuiIO& io = ImGui::GetIO();
    float winW = 500.0f;
    float winH = 290.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - winW * 0.5f, io.DisplaySize.y * 0.5f - winH * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

    ImGui::Begin("CASUALTY REPORT##DeathModal", nullptr, flags);

    ImGui::SetWindowFontScale(1.3f);
    ImGui::TextColored(ImVec4(0.95f, 0.25f, 0.25f, 1.0f), "KILLED IN ACTION");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("You sustained critical wounds on the Siversky Frontline.");
    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.45f, 1.0f), "All carried weapons, backpack, and gear were forfeited in the raid.");
    ImGui::TextDisabled("Your persistent bank account funds and FOB stash were preserved.");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("REDEPLOY AT BASE FOB", ImVec2(-1, 44))) {
        CharacterProfile* active = ProfileManager::get().getActiveProfile();
        player.spawnAtFactionBase(active ? active->faction : Faction::FactionA);
        player.getInventory().getItems().clear();
        player.getInventory().unequipItem(EquipSlot::PrimaryWeapon);
        player.getInventory().getEquipped(EquipSlot::SecondaryWeapon) = Inventory::createM9Pistol();
        player.getInventory().addItem(Inventory::create9mmAmmo(60));
        player.getInventory().addItem(Inventory::createBandage(2));
        player.getInventory().addItem(Inventory::createIFAK());
        player.resetFullHealthAndVitals();
        if (m_currentWorld) {
            m_currentWorld->initExtractionZones();
        }

        m_gameState = GameState::InGame;
        AudioEngine::get().playSound(SoundType::ItemEquip);
    }

    if (ImGui::Button("Return to PMC Hideout", ImVec2(-1, 32))) {
        CharacterProfile* active = ProfileManager::get().getActiveProfile();
        player.spawnAtFactionBase(active ? active->faction : Faction::FactionA);
        player.getInventory().getItems().clear();
        player.getInventory().unequipItem(EquipSlot::PrimaryWeapon);
        player.getInventory().getEquipped(EquipSlot::SecondaryWeapon) = Inventory::createM9Pistol();
        player.getInventory().addItem(Inventory::create9mmAmmo(60));
        player.getInventory().addItem(Inventory::createBandage(2));
        player.getInventory().addItem(Inventory::createIFAK());
        player.resetFullHealthAndVitals();
        if (active) {
            active->savedInventory = player.getInventory();
            ProfileManager::get().saveProfiles();
        }
        if (m_currentWorld) {
            m_currentWorld->initExtractionZones();
        }
        m_gameState = GameState::MainMenu;
        m_currentTab = MainMenuTab::Overview;
        AudioEngine::get().playSound(SoundType::TacticalClick);
        SteamIntegration::get().setRichPresence("status", "MERCENARY: PMC Hideout");
    }

    ImGui::End();
}

void UIManager::renderExtractionScreen(Player& player) {
    ImGuiIO& io = ImGui::GetIO();
    float winW = 620.0f;
    float winH = 430.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - winW * 0.5f, io.DisplaySize.y * 0.5f - winH * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove;

    ImGui::Begin("EXTRACTION DEBRIEFING##ExfilModal", nullptr, flags);

    ImGui::SetWindowFontScale(1.35f);
    ImGui::TextColored(ImVec4(0.20f, 0.95f, 0.45f, 1.0f), "EXTRACTION SUCCESSFUL");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    CharacterProfile* active = ProfileManager::get().getActiveProfile();
    std::string opName = active ? active->callsign : "OPERATIVE";
    std::string rankTitle = active ? getRankTitle(active->level, active->faction) : "PVT";

    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "Operator: [%s] %s  |  Faction: %s",
                       rankTitle.c_str(), opName.c_str(),
                       (active && active->faction == Faction::FactionA) ? "Mercenary Coalition" : "Crimson Legion");

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.95f, 0.82f, 0.25f, 1.0f), "EXTRACTED REWARDS & COMBAT SUMMARY:");

    ImGui::BulletText("Extraction Bonus: +$1,500 USD credited to bank wallet");
    if (active) {
        ImGui::BulletText("Total Raid Casualties Inflicted: %d confirmed combat kills", active->combatKills);
        ImGui::BulletText("Raid Career Record: %d Survived / %d Total Deployments", active->raidsSurvived, active->raidsTotal);
        ImGui::BulletText("Total Persistent Liquid Funds: $%d USD", active->cash);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "SECURED GEAR IN ESCAPE:");
    auto& pW = player.getInventory().getEquipped(EquipSlot::PrimaryWeapon);
    auto& sW = player.getInventory().getEquipped(EquipSlot::SecondaryWeapon);
    auto& vW = player.getInventory().getEquipped(EquipSlot::Vest);
    auto& hW = player.getInventory().getEquipped(EquipSlot::Helmet);

    ImGui::Text("  - Primary Weapon: %s", pW.has_value() ? pW->name.c_str() : "None");
    ImGui::Text("  - Sidearm: %s", sW.has_value() ? sW->name.c_str() : "None");
    ImGui::Text("  - Body Armor: %s  |  Helmet: %s", vW.has_value() ? vW->name.c_str() : "None", hW.has_value() ? hW->name.c_str() : "None");
    ImGui::Text("  - Backpack Items Saved: %d items transferred to PMC Hideout Stash", (int)player.getInventory().getItems().size());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("RETURN TO MAIN MENU", ImVec2(-1, 46))) {
        if (m_currentWorld) {
            m_currentWorld->initExtractionZones();
        }
        m_gameState = GameState::MainMenu;
        m_currentTab = MainMenuTab::Overview;
        AudioEngine::get().playSound(SoundType::TacticalClick);
        SteamIntegration::get().setRichPresence("status", "MERCENARY: PMC Hideout");
    }

    ImGui::End();
}


} // namespace Mercenary

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
#include "engine/Renderer.hpp"
#include "FrontlineAI.hpp"
#include "ExchangeMarket.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <iomanip>

namespace Mercenary {
void UIManager::inspectWeapon(const Item& item) {
    m_inspectedItem = item;
    m_showWeaponInspect = true;
    AudioEngine::get().playSound(SoundType::TacticalClick, 0.8f, 1.2f);
}

void UIManager::drawSlotWatermarkIcon(ImDrawList* dl, const char* slotName, ImVec2 center, float size) {
    ImU32 col = IM_COL32(65, 78, 95, 70); // Faint high-tech tactical watermark

    if (strstr(slotName, "HEAD") || strstr(slotName, "HELMET")) {
        // High-cut combat helmet watermark outline
        dl->AddCircle(ImVec2(center.x, center.y - size * 0.05f), size * 0.34f, col, 12, 1.5f);
        dl->AddLine(ImVec2(center.x - size * 0.32f, center.y + size * 0.12f), ImVec2(center.x + size * 0.32f, center.y + size * 0.12f), col, 1.5f);
        dl->AddRect(ImVec2(center.x - size * 0.08f, center.y - size * 0.02f), ImVec2(center.x + size * 0.08f, center.y + size * 0.14f), col, 1.0f);
    }
    else if (strstr(slotName, "BODY") || strstr(slotName, "ARMOR") || strstr(slotName, "VEST")) {
        // Plate carrier outline
        ImVec2 p0(center.x - size * 0.28f, center.y - size * 0.32f);
        ImVec2 p1(center.x + size * 0.28f, center.y + size * 0.32f);
        dl->AddRect(p0, p1, col, 4.0f, 0, 1.5f);
        dl->AddLine(ImVec2(center.x - size * 0.22f, center.y - size * 0.10f), ImVec2(center.x + size * 0.22f, center.y - size * 0.10f), col, 1.0f);
        dl->AddLine(ImVec2(center.x - size * 0.22f, center.y + size * 0.08f), ImVec2(center.x + size * 0.22f, center.y + size * 0.08f), col, 1.0f);
    }
    else if (strstr(slotName, "CHEST") || strstr(slotName, "RIG")) {
        // Chest rig 4-pouch watermark
        for (int p = -2; p <= 1; ++p) {
            float ox = center.x + (p + 0.5f) * size * 0.18f;
            dl->AddRect(ImVec2(ox - size * 0.07f, center.y - size * 0.22f), ImVec2(ox + size * 0.07f, center.y + size * 0.22f), col, 2.0f, 0, 1.2f);
        }
    }
    else if (strstr(slotName, "PRIMARY") || strstr(slotName, "WEAPON")) {
        // Rifle watermark
        float y = center.y;
        dl->AddLine(ImVec2(center.x - size * 0.42f, y), ImVec2(center.x + size * 0.45f, y), col, 2.0f);
        dl->AddRect(ImVec2(center.x - size * 0.05f, y - size * 0.14f), ImVec2(center.x + size * 0.12f, y + size * 0.08f), col, 1.0f, 0, 1.2f);
        dl->AddLine(ImVec2(center.x + size * 0.02f, y + size * 0.08f), ImVec2(center.x - size * 0.04f, y + size * 0.32f), col, 1.5f); // Mag
        dl->AddLine(ImVec2(center.x - size * 0.20f, y), ImVec2(center.x - size * 0.42f, y + size * 0.16f), col, 1.5f); // Stock
    }
    else if (strstr(slotName, "SIDEARM")) {
        // Pistol watermark
        dl->AddRect(ImVec2(center.x - size * 0.24f, center.y - size * 0.18f), ImVec2(center.x + size * 0.26f, center.y - size * 0.02f), col, 2.0f, 0, 1.5f);
        dl->AddLine(ImVec2(center.x - size * 0.12f, center.y - size * 0.02f), ImVec2(center.x - size * 0.22f, center.y + size * 0.30f), col, 2.0f);
        dl->AddCircle(ImVec2(center.x - size * 0.02f, center.y + size * 0.08f), size * 0.08f, col, 8, 1.2f);
    }
    else if (strstr(slotName, "BACKPACK") || strstr(slotName, "PACK")) {
        // Backpack watermark
        dl->AddRect(ImVec2(center.x - size * 0.26f, center.y - size * 0.32f), ImVec2(center.x + size * 0.26f, center.y + size * 0.32f), col, 5.0f, 0, 1.5f);
        dl->AddLine(ImVec2(center.x, center.y - size * 0.22f), ImVec2(center.x, center.y + size * 0.15f), col, 1.2f); // Y-zip
        dl->AddLine(ImVec2(center.x, center.y - size * 0.05f), ImVec2(center.x - size * 0.20f, center.y - size * 0.22f), col, 1.2f);
        dl->AddLine(ImVec2(center.x, center.y - size * 0.05f), ImVec2(center.x + size * 0.20f, center.y - size * 0.22f), col, 1.2f);
    }
    else if (strstr(slotName, "POCKET")) {
        dl->AddRect(ImVec2(center.x - size * 0.22f, center.y - size * 0.22f), ImVec2(center.x + size * 0.22f, center.y + size * 0.22f), col, 3.0f, 0, 1.2f);
        dl->AddLine(ImVec2(center.x - size * 0.18f, center.y - size * 0.06f), ImVec2(center.x + size * 0.18f, center.y - size * 0.06f), col, 1.0f);
    }
    else {
        // Sleek dashed tactical target reticle (+) with corner tick brackets
        float r = size * 0.14f;
        dl->AddLine(ImVec2(center.x - r, center.y), ImVec2(center.x + r, center.y), col, 1.5f);
        dl->AddLine(ImVec2(center.x, center.y - r), ImVec2(center.x, center.y + r), col, 1.5f);

        float b = size * 0.35f;
        float l = size * 0.08f;
        // 4 corner ticks
        dl->AddLine(ImVec2(center.x - b, center.y - b), ImVec2(center.x - b + l, center.y - b), col, 1.0f);
        dl->AddLine(ImVec2(center.x - b, center.y - b), ImVec2(center.x - b, center.y - b + l), col, 1.0f);

        dl->AddLine(ImVec2(center.x + b, center.y - b), ImVec2(center.x + b - l, center.y - b), col, 1.0f);
        dl->AddLine(ImVec2(center.x + b, center.y - b), ImVec2(center.x + b, center.y - b + l), col, 1.0f);

        dl->AddLine(ImVec2(center.x - b, center.y + b), ImVec2(center.x - b + l, center.y + b), col, 1.0f);
        dl->AddLine(ImVec2(center.x - b, center.y + b), ImVec2(center.x - b, center.y + b - l), col, 1.0f);

        dl->AddLine(ImVec2(center.x + b, center.y + b), ImVec2(center.x + b - l, center.y + b), col, 1.0f);
        dl->AddLine(ImVec2(center.x + b, center.y + b), ImVec2(center.x + b, center.y + b - l), col, 1.0f);
    }
}

void UIManager::drawEmptyVisualSlot(const char* slotName, const char* iconText, float width, float height) {
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + width, p0.y + height);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Dark inset tactical cell
    dl->AddRectFilled(p0, p1, IM_COL32(16, 20, 25, 220), 4.0f);
    dl->AddRect(p0, p1, IM_COL32(40, 48, 58, 160), 4.0f, 0, 1.0f);

    // Slot category label at top-left
    dl->AddText(ImVec2(p0.x + 6, p0.y + 4), IM_COL32(95, 110, 128, 200), slotName);

    // Sleek vector watermark silhouette in center
    ImVec2 center(p0.x + width * 0.5f, p0.y + height * 0.52f);
    float wmSize = std::min(width, height) * 0.85f;
    drawSlotWatermarkIcon(dl, slotName, center, wmSize);

    // Centered subtle placeholder text
    if (iconText && iconText[0] != '\0' && width > 90.0f) {
        ImVec2 txtSz = ImGui::CalcTextSize(iconText);
        dl->AddText(ImVec2(p0.x + (width - txtSz.x) * 0.5f, p1.y - txtSz.y - 4),
                    IM_COL32(65, 78, 92, 130), iconText);
    }

    ImGui::Dummy(ImVec2(width, height));
}

void UIManager::drawProceduralItemIcon(ImDrawList* dl, const Item& item, ImVec2 center, float width, float height) {
    const std::string& name = item.name;

    // --- 1. WEAPONS ---
    if (item.category == ItemCategory::Weapon) {
        float scale = std::min(width / 95.0f, height / 45.0f);

        if (name.find("M4A1") != std::string::npos || (item.weaponType == WeaponType::Rifle && name.find("AK") == std::string::npos)) {
            // COLT M4A1 SOPMOD 5.56x45mm NATO Assault Rifle
            float cy = center.y + 2.0f;
            float lx = center.x - 38.0f * scale;
            float rx = center.x + 40.0f * scale;

            // Receiver & Quad-rail handguard
            dl->AddRectFilled(ImVec2(center.x - 12.0f * scale, cy - 6.0f * scale), ImVec2(center.x + 8.0f * scale, cy + 5.0f * scale), IM_COL32(48, 52, 58, 255), 1.5f);
            dl->AddRectFilled(ImVec2(center.x + 8.0f * scale, cy - 4.5f * scale), ImVec2(center.x + 28.0f * scale, cy + 3.5f * scale), IM_COL32(68, 74, 82, 255), 1.0f);
            // Rail grooves
            for (float r = center.x + 10.0f * scale; r < center.x + 27.0f * scale; r += 4.0f * scale) {
                dl->AddLine(ImVec2(r, cy - 6.0f * scale), ImVec2(r, cy - 4.5f * scale), IM_COL32(95, 102, 112, 255), 1.0f);
            }
            // Barrel & Birdcage flash hider
            dl->AddLine(ImVec2(center.x + 28.0f * scale, cy - 1.0f * scale), ImVec2(rx, cy - 1.0f * scale), IM_COL32(38, 42, 46, 255), 2.5f * scale);
            dl->AddRectFilled(ImVec2(rx - 4.0f * scale, cy - 3.0f * scale), ImVec2(rx, cy + 1.0f * scale), IM_COL32(25, 28, 32, 255));

            // EOTech Holographic Optic on top rail
            dl->AddRectFilled(ImVec2(center.x - 6.0f * scale, cy - 13.0f * scale), ImVec2(center.x + 6.0f * scale, cy - 6.0f * scale), IM_COL32(35, 38, 44, 255), 1.5f);
            dl->AddCircleFilled(ImVec2(center.x, cy - 9.5f * scale), 1.5f * scale, IM_COL32(255, 185, 45, 255)); // Amber Holographic Reticle Dot!

            // Curved 30-round STANAG/PMAG magazine
            ImVec2 mP0(center.x + 2.0f * scale, cy + 5.0f * scale);
            ImVec2 mP1(center.x + 7.0f * scale, cy + 5.0f * scale);
            ImVec2 mP2(center.x + 3.0f * scale, cy + 18.0f * scale);
            ImVec2 mP3(center.x - 3.0f * scale, cy + 17.5f * scale);
            dl->AddQuadFilled(mP0, mP1, mP2, mP3, IM_COL32(30, 32, 36, 255));
            dl->AddLine(mP0, mP3, IM_COL32(85, 92, 102, 220), 1.0f);

            // Ergonomic Pistol Grip
            dl->AddLine(ImVec2(center.x - 8.0f * scale, cy + 5.0f * scale), ImVec2(center.x - 14.0f * scale, cy + 15.0f * scale), IM_COL32(40, 44, 48, 255), 3.5f * scale);

            // Buffer Tube & SOPMOD Crane Stock
            dl->AddLine(ImVec2(center.x - 12.0f * scale, cy - 1.5f * scale), ImVec2(lx + 8.0f * scale, cy - 1.5f * scale), IM_COL32(38, 42, 46, 255), 3.0f * scale);
            dl->AddRectFilled(ImVec2(lx, cy - 6.0f * scale), ImVec2(lx + 10.0f * scale, cy + 7.0f * scale), IM_COL32(52, 56, 62, 255), 2.0f);
            dl->AddLine(ImVec2(lx, cy - 6.0f * scale), ImVec2(lx, cy + 7.0f * scale), IM_COL32(25, 28, 30, 255), 2.5f * scale); // Buttplate
        }
        else if (name.find("AK") != std::string::npos || name.find("Kalashnikov") != std::string::npos) {
            // KALASHNIKOV AK-74M 5.45x39mm Assault Rifle
            float cy = center.y + 2.0f;
            float lx = center.x - 38.0f * scale;
            float rx = center.x + 40.0f * scale;

            // Stamped steel receiver & curved dust cover
            dl->AddRectFilled(ImVec2(center.x - 14.0f * scale, cy - 6.5f * scale), ImVec2(center.x + 6.0f * scale, cy + 4.5f * scale), IM_COL32(40, 42, 46, 255), 1.5f);
            // Wooden / plum polymer foregrip
            dl->AddRectFilled(ImVec2(center.x + 6.0f * scale, cy - 5.0f * scale), ImVec2(center.x + 24.0f * scale, cy + 4.0f * scale), IM_COL32(145, 85, 45, 245), 2.0f);
            // Barrel, gas tube & front sight block
            dl->AddLine(ImVec2(center.x + 24.0f * scale, cy - 1.0f * scale), ImVec2(rx, cy - 1.0f * scale), IM_COL32(32, 34, 38, 255), 2.2f * scale);
            dl->AddLine(ImVec2(center.x + 6.0f * scale, cy - 5.5f * scale), ImVec2(center.x + 22.0f * scale, cy - 5.5f * scale), IM_COL32(38, 40, 44, 255), 1.5f * scale);
            dl->AddLine(ImVec2(rx - 8.0f * scale, cy - 1.0f * scale), ImVec2(rx - 8.0f * scale, cy - 7.0f * scale), IM_COL32(32, 34, 38, 255), 1.5f * scale);
            // Slant muzzle brake
            dl->AddLine(ImVec2(rx - 4.0f * scale, cy - 3.0f * scale), ImVec2(rx, cy + 1.0f * scale), IM_COL32(220, 160, 45, 255), 2.0f * scale);

            // Iconic curved orange Bakelite banana magazine!
            ImVec2 mP0(center.x + 0.0f * scale, cy + 4.5f * scale);
            ImVec2 mP1(center.x + 6.0f * scale, cy + 4.5f * scale);
            ImVec2 mP2(center.x + 14.0f * scale, cy + 18.0f * scale);
            ImVec2 mP3(center.x + 8.0f * scale, cy + 18.0f * scale);
            dl->AddQuadFilled(mP0, mP1, mP2, mP3, IM_COL32(195, 90, 40, 255));
            dl->AddLine(mP1, mP2, IM_COL32(235, 125, 65, 220), 1.0f);

            // Pistol Grip & Polymer Buttstock
            dl->AddLine(ImVec2(center.x - 8.0f * scale, cy + 4.5f * scale), ImVec2(center.x - 14.0f * scale, cy + 14.0f * scale), IM_COL32(145, 85, 45, 245), 3.0f * scale);
            dl->AddQuadFilled(ImVec2(center.x - 14.0f * scale, cy - 4.0f * scale), ImVec2(center.x - 14.0f * scale, cy + 2.0f * scale),
                              ImVec2(lx, cy + 5.0f * scale), ImVec2(lx, cy - 6.0f * scale), IM_COL32(45, 48, 54, 255));
        }
        else if (name.find("Mosin") != std::string::npos || item.weaponType == WeaponType::Sniper) {
            // MOSIN-NAGANT M91/30 PU SNIPER RIFLE
            float cy = center.y + 2.0f;
            float lx = center.x - 42.0f * scale;
            float rx = center.x + 44.0f * scale;

            // Full-length rich walnut wood stock
            dl->AddLine(ImVec2(lx, cy + 1.0f * scale), ImVec2(center.x + 30.0f * scale, cy + 1.0f * scale), IM_COL32(150, 85, 42, 255), 4.5f * scale);
            // Long slender barrel
            dl->AddLine(ImVec2(center.x + 10.0f * scale, cy - 1.0f * scale), ImVec2(rx, cy - 1.0f * scale), IM_COL32(35, 36, 40, 255), 2.0f * scale);
            // Front hooded post sight
            dl->AddLine(ImVec2(rx - 5.0f * scale, cy - 1.0f * scale), ImVec2(rx - 5.0f * scale, cy - 6.0f * scale), IM_COL32(35, 36, 40, 255), 1.5f * scale);

            // PU 3.5x Telescopic Sniper Scope on high mount
            dl->AddRectFilled(ImVec2(center.x - 14.0f * scale, cy - 11.0f * scale), ImVec2(center.x + 10.0f * scale, cy - 7.0f * scale), IM_COL32(28, 30, 34, 255), 1.0f);
            dl->AddLine(ImVec2(center.x - 8.0f * scale, cy - 7.0f * scale), ImVec2(center.x - 8.0f * scale, cy - 1.0f * scale), IM_COL32(40, 42, 46, 255), 2.0f);
            dl->AddLine(ImVec2(center.x + 4.0f * scale, cy - 7.0f * scale), ImVec2(center.x + 4.0f * scale, cy - 1.0f * scale), IM_COL32(40, 42, 46, 255), 2.0f);
            dl->AddCircleFilled(ImVec2(center.x + 9.0f * scale, cy - 9.0f * scale), 1.5f * scale, IM_COL32(95, 205, 245, 240)); // Blue Coated Scope Lens

            // Turned-down bolt handle
            dl->AddLine(ImVec2(center.x - 2.0f * scale, cy - 1.5f * scale), ImVec2(center.x + 2.0f * scale, cy + 4.0f * scale), IM_COL32(190, 195, 205, 255), 1.5f * scale);
            // Straight rifle buttstock
            dl->AddQuadFilled(ImVec2(center.x - 18.0f * scale, cy - 1.0f * scale), ImVec2(center.x - 18.0f * scale, cy + 3.0f * scale),
                              ImVec2(lx, cy + 7.0f * scale), ImVec2(lx, cy - 3.0f * scale), IM_COL32(140, 78, 38, 255));
        }
        else if (name.find("M9") != std::string::npos || item.weaponType == WeaponType::Pistol) {
            // BERETTA M9A3 9x19mm TACTICAL SIDEARM
            float cy = center.y + 1.0f;
            float lx = center.x - 18.0f * scale;
            float rx = center.x + 22.0f * scale;

            // Slide with open-top barrel cutout
            dl->AddRectFilled(ImVec2(lx, cy - 6.5f * scale), ImVec2(rx, cy - 0.5f * scale), IM_COL32(65, 70, 78, 255), 1.5f);
            dl->AddLine(ImVec2(center.x - 6.0f * scale, cy - 4.5f * scale), ImVec2(center.x + 8.0f * scale, cy - 4.5f * scale), IM_COL32(28, 30, 34, 255), 1.8f * scale); // Exposed black barrel
            // Threaded muzzle
            dl->AddRectFilled(ImVec2(rx, cy - 5.5f * scale), ImVec2(rx + 3.5f * scale, cy - 1.5f * scale), IM_COL32(35, 38, 42, 255));

            // Frame, trigger guard & curved trigger
            dl->AddRectFilled(ImVec2(lx + 4.0f * scale, cy - 0.5f * scale), ImVec2(center.x + 12.0f * scale, cy + 3.5f * scale), IM_COL32(185, 155, 110, 255), 1.0f); // Coyote tan frame
            dl->AddCircle(ImVec2(center.x + 1.0f * scale, cy + 4.0f * scale), 4.5f * scale, IM_COL32(40, 42, 46, 255), 8, 1.5f);
            dl->AddLine(ImVec2(center.x + 1.0f * scale, cy + 1.0f * scale), ImVec2(center.x - 1.5f * scale, cy + 4.5f * scale), IM_COL32(30, 32, 36, 255), 1.5f); // Trigger

            // Checked grip handle & mag floorplate
            ImVec2 g0(lx + 2.0f * scale, cy + 2.0f * scale);
            ImVec2 g1(lx + 10.0f * scale, cy + 2.0f * scale);
            ImVec2 g2(lx + 4.0f * scale, cy + 17.0f * scale);
            ImVec2 g3(lx - 4.0f * scale, cy + 16.5f * scale);
            dl->AddQuadFilled(g0, g1, g2, g3, IM_COL32(165, 135, 95, 255));
            dl->AddRectFilled(ImVec2(lx - 5.0f * scale, cy + 16.5f * scale), ImVec2(lx + 5.0f * scale, cy + 19.0f * scale), IM_COL32(35, 38, 42, 255), 1.0f);
        }
    }
    // --- 2. AMMUNITION ---
    else if (item.category == ItemCategory::Ammo) {
        float scale = std::min(width / 80.0f, height / 55.0f);

        // Heavy-duty military ammo box
        ImVec2 b0(center.x - 22.0f * scale, center.y - 12.0f * scale);
        ImVec2 b1(center.x + 14.0f * scale, center.y + 14.0f * scale);
        dl->AddRectFilled(b0, b1, IM_COL32(52, 62, 50, 255), 3.0f); // Olive Drab Ammo Can
        dl->AddRect(b0, b1, IM_COL32(85, 102, 82, 255), 3.0f, 0, 1.2f);
        // Beveled lid
        dl->AddLine(ImVec2(b0.x, b0.y + 6.0f * scale), ImVec2(b1.x, b0.y + 6.0f * scale), IM_COL32(35, 42, 34, 255), 1.5f);

        // Caliber Badge stamped across box
        std::string badge = "AMMO";
        if (name.find("5.56") != std::string::npos) badge = "5.56";
        else if (name.find("5.45") != std::string::npos) badge = "5.45";
        else if (name.find("7.62") != std::string::npos) badge = "7.62";
        else if (name.find("9x19") != std::string::npos || name.find("9mm") != std::string::npos) badge = "9MM";

        ImVec2 bdSz = ImGui::CalcTextSize(badge.c_str());
        dl->AddText(ImVec2(b0.x + ( (b1.x - b0.x) - bdSz.x) * 0.5f, b0.y + 7.5f * scale), IM_COL32(235, 215, 85, 240), badge.c_str());

        // In front of the box: Two Gleaming Brass Cartridges!
        for (int c = 0; c < 2; ++c) {
            float cx = center.x + (15.0f + c * 7.0f) * scale;
            float cy = center.y + 2.0f * scale;

            // Copper Spitzer Bullet Tip
            ImVec2 t0(cx - 2.5f * scale, cy - 8.0f * scale);
            ImVec2 t1(cx + 2.5f * scale, cy - 8.0f * scale);
            ImVec2 t2(cx, cy - 14.0f * scale);
            dl->AddTriangleFilled(t0, t1, t2, IM_COL32(225, 135, 65, 255));

            // Brass Shell Casing
            dl->AddRectFilled(ImVec2(cx - 2.5f * scale, cy - 8.0f * scale), ImVec2(cx + 2.5f * scale, cy + 9.0f * scale), IM_COL32(245, 215, 75, 255), 0.5f);
            // Specular metallic sheen streak
            dl->AddLine(ImVec2(cx - 0.5f * scale, cy - 7.0f * scale), ImVec2(cx - 0.5f * scale, cy + 8.0f * scale), IM_COL32(255, 245, 160, 220), 1.0f);
            // Extractor groove & rim
            dl->AddLine(ImVec2(cx - 2.5f * scale, cy + 7.0f * scale), ImVec2(cx + 2.5f * scale, cy + 7.0f * scale), IM_COL32(170, 140, 45, 255), 1.0f);
            dl->AddRectFilled(ImVec2(cx - 3.0f * scale, cy + 9.0f * scale), ImVec2(cx + 3.0f * scale, cy + 10.5f * scale), IM_COL32(235, 205, 70, 255));
        }
    }
    // --- 3. MEDICAL ---
    else if (item.category == ItemCategory::Medical) {
        float scale = std::min(width / 75.0f, height / 50.0f);

        if (name.find("IFAK") != std::string::npos) {
            // TACTICAL IFAK (INDIVIDUAL FIRST AID KIT) POUCH
            ImVec2 p0(center.x - 18.0f * scale, center.y - 14.0f * scale);
            ImVec2 p1(center.x + 18.0f * scale, center.y + 14.0f * scale);
            dl->AddRectFilled(p0, p1, IM_COL32(45, 75, 52, 255), 4.0f); // Olive Drab Cordura
            dl->AddRect(p0, p1, IM_COL32(70, 115, 80, 255), 4.0f, 0, 1.2f);

            // Centered White Badge with Red Geneva Medical Cross
            ImVec2 b0(center.x - 8.0f * scale, center.y - 8.0f * scale);
            ImVec2 b1(center.x + 8.0f * scale, center.y + 8.0f * scale);
            dl->AddRectFilled(b0, b1, IM_COL32(245, 245, 245, 255), 2.0f);

            // Red Cross
            dl->AddRectFilled(ImVec2(center.x - 2.0f * scale, center.y - 6.0f * scale), ImVec2(center.x + 2.0f * scale, center.y + 6.0f * scale), IM_COL32(230, 40, 40, 255));
            dl->AddRectFilled(ImVec2(center.x - 6.0f * scale, center.y - 2.0f * scale), ImVec2(center.x + 6.0f * scale, center.y + 2.0f * scale), IM_COL32(230, 40, 40, 255));

            // Zipper track
            dl->AddLine(ImVec2(p0.x, p0.y + 4.0f * scale), ImVec2(p1.x, p0.y + 4.0f * scale), IM_COL32(25, 30, 25, 220), 1.2f);
        }
        else if (name.find("Tourniquet") != std::string::npos || name.find("CAT") != std::string::npos) {
            // CAT COMBAT TOURNIQUET
            dl->AddCircle(center, 12.0f * scale, IM_COL32(35, 38, 42, 255), 16, 4.0f * scale); // Black nylon strap
            // High-visibility bright Orange windlass buckle!
            dl->AddRectFilled(ImVec2(center.x - 6.0f * scale, center.y - 12.0f * scale), ImVec2(center.x + 6.0f * scale, center.y - 3.0f * scale), IM_COL32(255, 120, 20, 255), 1.5f);
            // Black polymer windlass rod
            dl->AddLine(ImVec2(center.x - 14.0f * scale, center.y - 4.0f * scale), ImVec2(center.x + 14.0f * scale, center.y + 8.0f * scale), IM_COL32(20, 22, 25, 255), 3.0f * scale);
            // White "TIME:" label
            dl->AddRectFilled(ImVec2(center.x - 5.0f * scale, center.y + 5.0f * scale), ImVec2(center.x + 5.0f * scale, center.y + 11.0f * scale), IM_COL32(240, 240, 240, 240));
        }
        else if (name.find("Splint") != std::string::npos) {
            // ALUMINUM SAM SPLINT
            dl->AddCircle(center, 13.0f * scale, IM_COL32(255, 115, 20, 255), 16, 4.5f * scale); // Safety orange outer face
            dl->AddCircle(center, 8.5f * scale, IM_COL32(35, 65, 110, 255), 16, 2.5f * scale); // Blue foam inner core
        }
        else {
            // MILITARY FIELD BANDAGE ROLL
            ImVec2 b0(center.x - 14.0f * scale, center.y - 10.0f * scale);
            ImVec2 b1(center.x + 14.0f * scale, center.y + 10.0f * scale);
            dl->AddRectFilled(b0, b1, IM_COL32(215, 205, 180, 245), 3.0f);
            // Sterile wrap spirals
            for (float x = b0.x + 4.0f * scale; x < b1.x - 2.0f * scale; x += 5.0f * scale) {
                dl->AddLine(ImVec2(x, b0.y), ImVec2(x, b1.y), IM_COL32(185, 175, 150, 240), 1.2f);
            }
            // Green medical cross stamp
            dl->AddRectFilled(ImVec2(center.x - 1.5f * scale, center.y - 4.0f * scale), ImVec2(center.x + 1.5f * scale, center.y + 4.0f * scale), IM_COL32(45, 185, 90, 255));
            dl->AddRectFilled(ImVec2(center.x - 4.0f * scale, center.y - 1.5f * scale), ImVec2(center.x + 4.0f * scale, center.y + 1.5f * scale), IM_COL32(45, 185, 90, 255));
        }
    }
    // --- 4. SURVIVAL ---
    else if (item.category == ItemCategory::Survival) {
        float scale = std::min(width / 75.0f, height / 50.0f);

        if (name.find("MRE") != std::string::npos || name.find("Ration") != std::string::npos) {
            // COMBAT RATION MRE RETORT POUCH
            ImVec2 p0(center.x - 16.0f * scale, center.y - 14.0f * scale);
            ImVec2 p1(center.x + 16.0f * scale, center.y + 14.0f * scale);
            dl->AddRectFilled(p0, p1, IM_COL32(180, 155, 110, 245), 2.0f); // Tan waterproof foil
            // Hermetic seal crimps
            dl->AddLine(ImVec2(p0.x, p0.y + 3.0f * scale), ImVec2(p1.x, p0.y + 3.0f * scale), IM_COL32(140, 115, 75, 240), 1.5f);
            dl->AddLine(ImVec2(p0.x, p1.y - 3.0f * scale), ImVec2(p1.x, p1.y - 3.0f * scale), IM_COL32(140, 115, 75, 240), 1.5f);
            // "MRE" Stencil
            ImVec2 mSz = ImGui::CalcTextSize("MRE");
            dl->AddText(ImVec2(center.x - mSz.x * 0.5f, center.y - mSz.y * 0.5f), IM_COL32(45, 40, 32, 220), "MRE");
            // Plastic field spoon outline
            dl->AddLine(ImVec2(center.x + 11.0f * scale, center.y - 10.0f * scale), ImVec2(center.x + 11.0f * scale, center.y + 10.0f * scale), IM_COL32(120, 95, 65, 200), 1.5f);
        }
        else {
            // MILITARY FIELD CANTEEN
            dl->AddRectFilled(ImVec2(center.x - 12.0f * scale, center.y - 8.0f * scale), ImVec2(center.x + 12.0f * scale, center.y + 14.0f * scale), IM_COL32(55, 75, 55, 245), 5.0f);
            dl->AddRectFilled(ImVec2(center.x - 4.0f * scale, center.y - 14.0f * scale), ImVec2(center.x + 4.0f * scale, center.y - 8.0f * scale), IM_COL32(35, 45, 35, 255), 1.0f); // Cap
            // Water droplet icon
            dl->AddCircleFilled(ImVec2(center.x, center.y + 4.0f * scale), 4.5f * scale, IM_COL32(80, 205, 255, 240));
            dl->AddTriangleFilled(ImVec2(center.x - 3.5f * scale, center.y + 3.0f * scale), ImVec2(center.x + 3.5f * scale, center.y + 3.0f * scale), ImVec2(center.x, center.y - 3.0f * scale), IM_COL32(80, 205, 255, 240));
        }
    }
    // --- 5. GEAR (HELMETS, ARMOR, RIGS, BACKPACKS) ---
    else if (item.category == ItemCategory::Gear) {
        float scale = std::min(width / 75.0f, height / 50.0f);

        if (name.find("Helmet") != std::string::npos || name.find("FAST") != std::string::npos) {
            // FAST MT BALLISTIC HELMET
            dl->AddCircleFilled(ImVec2(center.x, center.y - 2.0f * scale), 14.0f * scale, IM_COL32(175, 160, 130, 245));
            // High-cut rim lip & ARC rails
            dl->AddLine(ImVec2(center.x - 14.0f * scale, center.y + 3.0f * scale), ImVec2(center.x + 14.0f * scale, center.y + 3.0f * scale), IM_COL32(135, 120, 95, 255), 2.5f * scale);
            // Wilcox NVG Shroud front bracket
            dl->AddRectFilled(ImVec2(center.x - 3.5f * scale, center.y - 4.0f * scale), ImVec2(center.x + 3.5f * scale, center.y + 3.0f * scale), IM_COL32(35, 38, 42, 255), 1.0f);
        }
        else if (name.find("JPC") != std::string::npos || name.find("Carrier") != std::string::npos || name.find("Vest") != std::string::npos) {
            // CRYE PRECISION JPC 2.0 PLATE CARRIER
            ImVec2 p0(center.x - 14.0f * scale, center.y - 14.0f * scale);
            ImVec2 p1(center.x + 14.0f * scale, center.y + 14.0f * scale);
            dl->AddRectFilled(p0, p1, IM_COL32(165, 150, 120, 245), 3.0f); // Coyote plate bag
            // 3 Rows of MOLLE Webbing
            for (float my = center.y - 4.0f * scale; my <= center.y + 8.0f * scale; my += 5.5f * scale) {
                dl->AddLine(ImVec2(p0.x + 2.0f * scale, my), ImVec2(p1.x - 2.0f * scale, my), IM_COL32(85, 75, 58, 200), 1.5f);
            }
            // Shoulder straps
            dl->AddLine(ImVec2(p0.x + 2.0f * scale, p0.y), ImVec2(p0.x + 2.0f * scale, p0.y - 4.0f * scale), IM_COL32(120, 105, 80, 255), 2.5f * scale);
            dl->AddLine(ImVec2(p1.x - 2.0f * scale, p0.y), ImVec2(p1.x - 2.0f * scale, p0.y - 4.0f * scale), IM_COL32(120, 105, 80, 255), 2.5f * scale);
        }
        else if (name.find("Rig") != std::string::npos || name.find("D3CRX") != std::string::npos) {
            // HALEY STRATEGIC D3CRX CHEST RIG (4 Rifle Mag Pouches)
            for (int m = -2; m <= 1; ++m) {
                float mx = center.x + (m + 0.5f) * 7.5f * scale;
                dl->AddRectFilled(ImVec2(mx - 3.2f * scale, center.y - 9.0f * scale), ImVec2(mx + 3.2f * scale, center.y + 11.0f * scale), IM_COL32(145, 130, 100, 245), 1.5f);
                dl->AddLine(ImVec2(mx, center.y - 9.0f * scale), ImVec2(mx, center.y - 12.0f * scale), IM_COL32(35, 38, 42, 255), 1.5f); // Elastic retention bungee
            }
        }
        else {
            // MYSTERY RANCH 3-DAY ASSAULT PACK
            ImVec2 p0(center.x - 14.0f * scale, center.y - 15.0f * scale);
            ImVec2 p1(center.x + 14.0f * scale, center.y + 15.0f * scale);
            dl->AddRectFilled(p0, p1, IM_COL32(160, 140, 105, 245), 4.0f);
            // Signature 3-Zip Y-Shaped Zipper Configuration!
            dl->AddLine(ImVec2(center.x, center.y - 3.0f * scale), ImVec2(center.x, p1.y - 3.0f * scale), IM_COL32(30, 32, 36, 255), 2.0f);
            dl->AddLine(ImVec2(center.x, center.y - 3.0f * scale), ImVec2(p0.x + 2.0f * scale, p0.y + 3.0f * scale), IM_COL32(30, 32, 36, 255), 2.0f);
            dl->AddLine(ImVec2(center.x, center.y - 3.0f * scale), ImVec2(p1.x - 2.0f * scale, p0.y + 3.0f * scale), IM_COL32(30, 32, 36, 255), 2.0f);
            // Side compression straps
            dl->AddLine(ImVec2(p0.x, center.y + 4.0f * scale), ImVec2(p0.x + 4.0f * scale, center.y + 4.0f * scale), IM_COL32(40, 42, 45, 255), 2.0f);
            dl->AddLine(ImVec2(p1.x - 4.0f * scale, center.y + 4.0f * scale), ImVec2(p1.x, center.y + 4.0f * scale), IM_COL32(40, 42, 45, 255), 2.0f);
        }
    }
}

void UIManager::drawVisualItemSlot(Item& item, size_t index, bool isEquipped, EquipSlot slot, float width, float height, Player* player) {
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1 = ImVec2(p0.x + width, p0.y + height);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::PushID((int)index + (isEquipped ? 10000 : 20000) + (int)slot * 50);

    // Invisible button for hover & click
    ImGui::InvisibleButton("ItemSlotBtn", ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    bool rightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

    // Category Border Colors (Arena Breakout authentic palette)
    ImU32 borderCol = IM_COL32(55, 68, 85, 220);
    ImU32 accentCol = IM_COL32(70, 85, 105, 255);
    const char* catTag = "ITEM";

    switch (item.category) {
        case ItemCategory::Weapon:
            borderCol = IM_COL32(235, 160, 45, 255);   // Gold / Amber
            accentCol = IM_COL32(255, 190, 70, 255);
            catTag = "WEAPON";
            break;
        case ItemCategory::Ammo:
            borderCol = IM_COL32(230, 210, 65, 255);   // Brass Yellow
            accentCol = IM_COL32(245, 225, 95, 255);
            catTag = "AMMO";
            break;
        case ItemCategory::Medical:
            borderCol = IM_COL32(40, 210, 115, 255);   // Emerald Green
            accentCol = IM_COL32(65, 235, 140, 255);
            catTag = "MEDIC";
            break;
        case ItemCategory::Gear:
            borderCol = IM_COL32(50, 160, 240, 255);   // Tactical Blue
            accentCol = IM_COL32(90, 195, 255, 255);
            catTag = "GEAR";
            break;
        case ItemCategory::Survival:
            borderCol = IM_COL32(175, 110, 235, 255);  // Rare Purple
            accentCol = IM_COL32(205, 145, 255, 255);
            catTag = "SURVIVAL";
            break;
    }

    if (hovered) {
        dl->AddRectFilled(p0, p1, IM_COL32(35, 45, 58, 245), 3.0f);
        dl->AddRect(p0, p1, accentCol, 3.0f, 0, 2.0f);
    } else {
        dl->AddRectFilled(p0, p1, IM_COL32(20, 24, 30, 220), 3.0f);
        dl->AddRect(p0, p1, borderCol, 3.0f, 0, 1.2f);
    }

    // Top-left category tag pill
    ImVec2 catSz = ImGui::CalcTextSize(catTag);
    dl->AddRectFilled(ImVec2(p0.x + 3, p0.y + 3), ImVec2(p0.x + catSz.x + 9, p0.y + catSz.y + 4), IM_COL32(10, 12, 16, 220), 2.0f);
    dl->AddText(ImVec2(p0.x + 6, p0.y + 3), accentCol, catTag);

    // ================= RENDER HIGH-RES PROCEDURAL 2D GRAPHICAL ICON IN CENTER =================
    ImVec2 iconCenter(p0.x + width * 0.5f, p0.y + height * 0.44f);
    float availIconW = width * 0.84f;
    float availIconH = height * 0.56f;
    drawProceduralItemIcon(dl, item, iconCenter, availIconW, availIconH);

    // ================= SLEEK HIGH-CONTRAST BOTTOM LABEL BANNER =================
    std::string shortName = item.name;
    if (shortName.find("Colt M4A1") != std::string::npos) shortName = (width < 140.0f) ? "M4A1" : "M4A1 SOPMOD";
    else if (shortName.find("Kalashnikov") != std::string::npos) shortName = (width < 140.0f) ? "AK-74M" : "AK-74M 5.45x39";
    else if (shortName.find("Mosin") != std::string::npos) shortName = (width < 140.0f) ? "MOSIN" : "MOSIN M91/30";
    else if (shortName.find("Beretta") != std::string::npos) shortName = (width < 140.0f) ? "M9A3" : "BERETTA M9A3";
    else if (shortName.find("5.56") != std::string::npos) shortName = "5.56 NATO";
    else if (shortName.find("5.45") != std::string::npos) shortName = "5.45 AMMO";
    else if (shortName.find("7.62") != std::string::npos) shortName = "7.62x54R";
    else if (shortName.find("9x19") != std::string::npos) shortName = "9x19 AMMO";
    else if (shortName.find("IFAK") != std::string::npos) shortName = (width < 140.0f) ? "IFAK" : "TACTICAL IFAK";
    else if (shortName.find("Bandage") != std::string::npos) shortName = (width < 140.0f) ? "BANDAGE" : "FIELD BANDAGE";
    else if (shortName.find("Tourniquet") != std::string::npos) shortName = (width < 140.0f) ? "C-A-T" : "CAT TOURNIQUET";
    else if (shortName.find("Splint") != std::string::npos) shortName = (width < 140.0f) ? "SPLINT" : "SAM SPLINT";
    else if (shortName.find("MRE") != std::string::npos) shortName = "COMBAT MRE";
    else if (shortName.find("Canteen") != std::string::npos) shortName = (width < 140.0f) ? "CANTEEN" : "FIELD CANTEEN";
    else if (shortName.find("Helmet") != std::string::npos) shortName = (width < 140.0f) ? "FAST HELM" : "FAST HELMET";
    else if (shortName.find("JPC") != std::string::npos) shortName = "CRYE JPC 2.0";
    else if (shortName.find("D3CRX") != std::string::npos) shortName = "HALEY D3CRX";
    else if (shortName.find("Pack") != std::string::npos) shortName = "3-DAY PACK";

    if (shortName.length() > 11 && width < 115.0f) {
        shortName = shortName.substr(0, 9) + "..";
    }

    // Subtle dark banner backdrop at bottom
    dl->AddRectFilled(ImVec2(p0.x + 2, p1.y - 18.0f), ImVec2(p1.x - 2, p1.y - 2), IM_COL32(10, 12, 16, 215), 2.0f);
    dl->AddText(ImVec2(p0.x + 5, p1.y - 16.5f), IM_COL32(235, 240, 248, 255), shortName.c_str());

    // Badges:
    // If ammo or quantity > 1: display pill badge at bottom right
    if (item.category == ItemCategory::Ammo || item.quantity > 1) {
        char qtyBuf[24];
        snprintf(qtyBuf, sizeof(qtyBuf), "x%d", item.quantity);
        ImVec2 qSz = ImGui::CalcTextSize(qtyBuf);
        ImVec2 qP0 = ImVec2(p1.x - qSz.x - 7, p1.y - qSz.y - 4);
        dl->AddRectFilled(qP0, ImVec2(p1.x - 2, p1.y - 2), IM_COL32(15, 18, 22, 240), 2.0f);
        dl->AddText(ImVec2(qP0.x + 2, qP0.y + 1), IM_COL32(250, 230, 80, 255), qtyBuf);
    } else if (item.category == ItemCategory::Weapon) {
        char magBuf[24];
        snprintf(magBuf, sizeof(magBuf), "%d/%d", item.currentMag, item.maxMag);
        ImVec2 mSz = ImGui::CalcTextSize(magBuf);
        ImVec2 mP0 = ImVec2(p1.x - mSz.x - 7, p1.y - mSz.y - 4);
        dl->AddRectFilled(mP0, ImVec2(p1.x - 2, p1.y - 2), IM_COL32(15, 18, 22, 240), 2.0f);
        dl->AddText(ImVec2(mP0.x + 2, mP0.y + 1), IM_COL32(255, 195, 75, 255), magBuf);
    }

    // Bottom subtle durability bar for weapons and armor
    if (item.category == ItemCategory::Weapon || item.category == ItemCategory::Gear) {
        float barW = width - 8.0f;
        float barY = p1.y - 2.5f;
        dl->AddLine(ImVec2(p0.x + 4, barY), ImVec2(p0.x + 4 + barW, barY), IM_COL32(35, 40, 48, 200), 2.0f);
        dl->AddLine(ImVec2(p0.x + 4, barY), ImVec2(p0.x + 4 + barW * 0.95f, barY), IM_COL32(50, 220, 110, 240), 2.0f);
    }

    // Rich Inspection Tooltip on Hover
    if (hovered) {
        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "%s", item.name.c_str());
        ImGui::Separator();
        ImGui::TextDisabled("Category: %s  |  Weight: %.2f kg", catTag, item.weightKg);

        if (item.category == ItemCategory::Weapon) {
            ImGui::Text("Caliber: %s  |  Fire Rate: %.0f RPM", item.caliber.c_str(), item.fireRateRpm);
            ImGui::Text("Ammo: %d / %d  |  Effective Range: %.0fm", item.currentMag, item.maxMag, item.effectiveRange);
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.95f, 1.0f), "Damage: %.0f  |  Ergo: %.0f  |  Vert Recoil: %.0f", item.damage, item.ergonomics, item.recoilVertical);

            // Tarkov Comparative Tooltips against currently equipped weapon
            if (!isEquipped && player) {
                EquipSlot targetSlot = (item.weaponType == WeaponType::Pistol) ? EquipSlot::SecondaryWeapon : EquipSlot::PrimaryWeapon;
                const auto& equippedOpt = player->getInventory().getEquipped(targetSlot);
                if (equippedOpt.has_value()) {
                    const Item& eq = *equippedOpt;
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f), "VS EQUIPPED (%s):", eq.name.c_str());

                    auto showComparison = [](const char* statName, float diff, bool higherIsBetter, const char* suffix) {
                        if (std::abs(diff) < 0.1f) {
                            ImGui::TextDisabled("  %s: Matching", statName);
                        } else {
                            bool isGood = higherIsBetter ? (diff > 0.0f) : (diff < 0.0f);
                            ImVec4 color = isGood ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
                            ImGui::Text("  %s: ", statName);
                            ImGui::SameLine();
                            ImGui::TextColored(color, "%s%.1f %s", (diff > 0.0f ? "+" : ""), diff, suffix);
                        }
                    };

                    showComparison("Damage", item.damage - eq.damage, true, "HP");
                    showComparison("Fire Rate", item.fireRateRpm - eq.fireRateRpm, true, "RPM");
                    showComparison("Recoil Kick", item.recoilVertical - eq.recoilVertical, false, "");
                    showComparison("Handling Ergo", item.ergonomics - eq.ergonomics, true, "");
                    showComparison("Weight", item.weightKg - eq.weightKg, false, "kg");
                }
            }
        } else if (item.category == ItemCategory::Medical) {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "Trauma Bandage / Field Medkit (Restores HP & stops bleed)");
        } else if (item.category == ItemCategory::Gear) {
            ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Ballistic Protection / Modular Combat Gear");
            if (!isEquipped && player) {
                EquipSlot targetSlot = EquipSlot::Vest;
                if (item.name.find("Helmet") != std::string::npos || item.name.find("FAST") != std::string::npos || item.name.find("6B47") != std::string::npos) {
                    targetSlot = EquipSlot::Helmet;
                }
                const auto& equippedOpt = player->getInventory().getEquipped(targetSlot);
                if (equippedOpt.has_value()) {
                    const Item& eq = *equippedOpt;
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f), "VS EQUIPPED (%s):", eq.name.c_str());
                    float weightDiff = item.weightKg - eq.weightKg;
                    bool isWeightGood = weightDiff <= 0.0f;
                    ImGui::Text("  Weight: ");
                    ImGui::SameLine();
                    ImGui::TextColored(isWeightGood ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                        "%s%.2f kg", (weightDiff > 0.0f ? "+" : ""), weightDiff);
                }
            }
        }

        if (!item.description.empty()) {
            ImGui::TextDisabled("\"%s\"", item.description.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (isEquipped) {
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "[L-Click] Unequip to Stash   [R-Click] Inspect Weapon   [G Key] Drop in World");
        } else {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "[L-Click] Equip / Use Item   [R-Click] Drop / Discard");
        }
        ImGui::EndTooltip();
    }

    // Interactions
    if (clicked && player) {
        if (isEquipped) {
            player->getInventory().unequipItem(slot);
            player->updateArmorFromEquipped();
            CharacterProfile* act = ProfileManager::get().getActiveProfile();
            if (act) {
                act->savedInventory = player->getInventory();
                ProfileManager::get().saveProfiles();
            }
            AudioEngine::get().playSound(SoundType::ItemEquip, 0.85f, 0.95f);
        } else {
            if (item.category == ItemCategory::Weapon) {
                if (item.weaponType == WeaponType::Pistol) {
                    player->getInventory().equipItem(index, EquipSlot::SecondaryWeapon);
                } else {
                    player->getInventory().equipItem(index, EquipSlot::PrimaryWeapon);
                }
                AudioEngine::get().playSound(SoundType::ItemEquip);
            } else if (item.category == ItemCategory::Medical) {
                player->useMedicalItem(index);
            } else if (item.category == ItemCategory::Survival) {
                player->useSurvivalItem(index);
            } else if (item.category == ItemCategory::Gear) {
                if (item.name.find("Helmet") != std::string::npos || item.name.find("FAST") != std::string::npos || item.name.find("6B47") != std::string::npos) {
                    player->getInventory().equipItem(index, EquipSlot::Helmet);
                } else if (item.name.find("Rig") != std::string::npos || item.name.find("D3CRX") != std::string::npos) {
                    player->getInventory().equipItem(index, EquipSlot::TacticalRig);
                } else if (item.name.find("Backpack") != std::string::npos || item.name.find("Pack") != std::string::npos) {
                    player->getInventory().equipItem(index, EquipSlot::Backpack);
                } else {
                    player->getInventory().equipItem(index, EquipSlot::Vest);
                }
                player->updateArmorFromEquipped();
                AudioEngine::get().playSound(SoundType::ItemEquip);
            }
            CharacterProfile* act = ProfileManager::get().getActiveProfile();
            if (act) {
                act->savedInventory = player->getInventory();
                ProfileManager::get().saveProfiles();
            }
        }
    }

    if (rightClicked) {
        if (m_gameState == GameState::InGame && m_currentWorld) {
            if (isEquipped && player) {
                player->dropEquippedItem(*m_currentWorld, slot);
            } else if (!isEquipped && player) {
                player->dropItem(*m_currentWorld, index);
            }
        } else {
            if (item.category == ItemCategory::Weapon) {
                inspectWeapon(item);
            } else if (!isEquipped && player) {
                player->getInventory().removeItem(index);
                AudioEngine::get().playSound(SoundType::StashMove);
            }
        }
    }

    ImGui::PopID();
}

void UIManager::drawArenaBreakoutPaperdoll(Player& player, CharacterProfile* active, bool isInGame) {
    (void)active;
    Inventory& inv = player.getInventory();

    ImGui::SetWindowFontScale(1.12f);
    ImGui::TextColored(ImVec4(0.92f, 0.58f, 0.18f, 1.0f), "LOADOUT & PAPERDOLL");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    // Top Row: Headwear & Body Armor
    auto& helmet = inv.getEquipped(EquipSlot::Helmet);
    if (helmet.has_value()) {
        drawVisualItemSlot(*helmet, 0, true, EquipSlot::Helmet, 175, 74, &player);
    } else {
        drawEmptyVisualSlot("HEADWEAR", "[HELMET]", 175, 74);
    }

    ImGui::SameLine();

    auto& armor = inv.getEquipped(EquipSlot::Vest);
    if (armor.has_value()) {
        drawVisualItemSlot(*armor, 0, true, EquipSlot::Vest, 175, 74, &player);
    } else {
        drawEmptyVisualSlot("BODY ARMOR", "[PLATE CARRIER]", 175, 74);
    }

    ImGui::Spacing();

    // Row 2: Tactical Chest Rig
    auto& rig = inv.getEquipped(EquipSlot::TacticalRig);
    if (rig.has_value()) {
        drawVisualItemSlot(*rig, 0, true, EquipSlot::TacticalRig, 358, 64, &player);
    } else {
        drawEmptyVisualSlot("TACTICAL CHEST RIG", "[MOLLE RIG 12-SLOT]", 358, 64);
    }

    ImGui::Spacing();

    // Row 3: Primary Weapon (Holster 1)
    auto& pri = inv.getEquipped(EquipSlot::PrimaryWeapon);
    if (pri.has_value()) {
        drawVisualItemSlot(*pri, 0, true, EquipSlot::PrimaryWeapon, 358, 80, &player);
    } else {
        drawEmptyVisualSlot("PRIMARY WEAPON", "[SLING HOLSTER 1]", 358, 80);
    }

    ImGui::Spacing();

    // Row 4: Secondary Sidearm (Holster 2) & Backpack
    auto& sec = inv.getEquipped(EquipSlot::SecondaryWeapon);
    if (sec.has_value()) {
        drawVisualItemSlot(*sec, 0, true, EquipSlot::SecondaryWeapon, 175, 74, &player);
    } else {
        drawEmptyVisualSlot("SIDEARM", "[HOLSTER 2]", 175, 74);
    }

    ImGui::SameLine();

    auto& backpack = inv.getEquipped(EquipSlot::Backpack);
    if (backpack.has_value()) {
        drawVisualItemSlot(*backpack, 0, true, EquipSlot::Backpack, 175, 74, &player);
    } else {
        drawEmptyVisualSlot("BACKPACK", "[ASSAULT PACK 30L]", 175, 74);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Quick Pockets: 4 Slots
    ImGui::TextDisabled("QUICK ACCESS POCKETS:");
    for (int p = 0; p < 4; ++p) {
        ImGui::PushID(p + 500);
        char pName[16];
        snprintf(pName, sizeof(pName), "POCKET %d", p + 1);
        char pIcon[16];
        snprintf(pIcon, sizeof(pIcon), "[%d]", p + 4);
        drawEmptyVisualSlot(pName, pIcon, 83.5f, 52.0f);
        if (p < 3) ImGui::SameLine();
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Bottom Arena Breakout Live Vitals Strip
    const auto& stats = player.getStats();
    float curHp = stats.health;
    float maxHp = 100.0f;
    float curStam = stats.stamina;
    float maxStam = 100.0f;
    float wt = inv.getTotalWeightKg();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.12f, 0.15f, 0.95f));
    ImGui::BeginChild("ArenaBreakoutVitalsBar", ImVec2(358, 64), true);

    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "+ HP %.0f/%.0f", curHp, maxHp);
    ImGui::SameLine(180);
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.95f, 1.0f), "NRG %.0f%%", (curStam / std::max(1.0f, maxStam)) * 100.0f);

    ImGui::TextColored((wt > 35.0f) ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f) : ImVec4(0.85f, 0.88f, 0.92f, 1.0f),
                       "WT: %.1f kg / 48 kg", wt);
    ImGui::SameLine(180);
    int estValue = (int)(wt * 850.0f) + (pri.has_value() ? 12000 : 0) + (sec.has_value() ? 3200 : 0);
    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "$ GEAR: %d", estValue);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void UIManager::drawArenaBreakoutGridStash(Player& player, CharacterProfile* active, bool isInGame) {
    (void)active;
    Inventory& inv = player.getInventory();
    auto& items = inv.getItems();

    // Top Header & Category Filter Pills
    ImGui::SetWindowFontScale(1.12f);
    ImGui::TextColored(ImVec4(0.92f, 0.58f, 0.18f, 1.0f), isInGame ? "FIELD GEAR & BACKPACK" : "ARMORY & STASH");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine(isInGame ? 280 : 360);
    ImGui::TextDisabled(isInGame ? "%d Field Items" : "%d Items in Storage", (int)items.size());

    ImGui::Separator();
    ImGui::Spacing();

    // Category Filter Pills
    auto drawFilterPill = [&](const char* label, int filterId) {
        bool sel = (m_inventoryFilter == filterId);
        if (sel) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.48f, 0.12f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 0.85f));
        }
        if (ImGui::Button(label, ImVec2(isInGame ? 68 : 82, 26))) {
            m_inventoryFilter = filterId;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    };

    drawFilterPill("ALL", 0);
    drawFilterPill("WEAPONS", 1);
    drawFilterPill("AMMO", 2);
    drawFilterPill("MEDS", 3);
    drawFilterPill("GEAR", 4);

    if (ImGui::Button("AUTO-SORT", ImVec2(isInGame ? 75 : 88, 26))) {
        inv.sortItems();
        AudioEngine::get().playSound(SoundType::StashMove);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Grid Container Scroll Area
    ImGui::BeginChild("ArenaBreakoutGridScroll", ImVec2(0, isInGame ? 420 : 490), true);

    float cardW = isInGame ? 92.0f : 106.0f;
    float cardH = 82.0f;
    int cols = isInGame ? 4 : 5;

    int slotCol = 0;
    int renderedItems = 0;

    for (size_t i = 0; i < items.size(); ++i) {
        Item& item = items[i];

        if (m_inventoryFilter == 1 && item.category != ItemCategory::Weapon) continue;
        if (m_inventoryFilter == 2 && item.category != ItemCategory::Ammo) continue;
        if (m_inventoryFilter == 3 && item.category != ItemCategory::Medical) continue;
        if (m_inventoryFilter == 4 && item.category != ItemCategory::Gear) continue;

        drawVisualItemSlot(item, i, false, EquipSlot::PrimaryWeapon, cardW, cardH, &player);
        renderedItems++;
        slotCol++;

        if (slotCol < cols) {
            ImGui::SameLine(0.0f, 8.0f);
        } else {
            slotCol = 0;
            ImGui::Spacing();
        }
    }

    // Fill remaining grid spaces with empty tactical cells
    int minCells = std::max(15, ((renderedItems / cols) + 1) * cols);
    for (int e = renderedItems; e < minCells; ++e) {
        drawEmptyVisualSlot("EMPTY", "[+]", cardW, cardH);
        slotCol++;
        if (slotCol < cols) {
            ImGui::SameLine(0.0f, 8.0f);
        } else {
            slotCol = 0;
            ImGui::Spacing();
        }
    }

    ImGui::EndChild();
}

void UIManager::renderDeploymentStaging(Player& player, CharacterProfile* active, const World& world, const MissionSystem& missions) {
    (void)world;
    (void)missions;
    ImGuiIO& io = ImGui::GetIO();
    float topY = 56.0f;
    float contentH = io.DisplaySize.y - topY - 16.0f;
    float contentW = io.DisplaySize.x - 32.0f;

    ImGui::SetNextWindowPos(ImVec2(16, topY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(contentW, contentH), ImGuiCond_Always);
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.08f, 0.11f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.70f));

    if (ImGui::Begin("##DeploymentStagingDock", nullptr, winFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // 1. TOP HEADER & TELEMETRY
        ImGui::SetCursorPos(ImVec2(16, 12));
        
        // Return / Back Button
        if (ImGui::Button("<##DeployBack", ImVec2(32, 28))) {
            m_currentTab = MainMenuTab::Overview;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        ImGui::SameLine(0, 10.0f);

        ImGui::SetWindowFontScale(1.15f);
        ImGui::TextColored(ImVec4(0.95f, 0.98f, 1.0f, 1.0f), "DEPLOYMENT STAGING");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(0, 12.0f);
        ImGui::TextDisabled("");

        // Right Funds & Infiltration Status
        float rightFundsX = contentW - 460.0f;
        ImGui::SetCursorPos(ImVec2(rightFundsX, 10));
        if (active) {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "$ %d USD", active->cash);
            ImGui::SameLine(0, 10.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.15f, 1.0f), "%d GOLD", active->goldBars);
            ImGui::SameLine(0, 10.0f);
            if (ImGui::Button("[EXCHANGE]##DeployExchange", ImVec2(85, 26))) {
                m_currentTab = MainMenuTab::ExchangeMarket;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::SameLine(0, 10.0f);
            if (m_squadMode) {
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "[SQUAD]");
            } else {
                ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "[SOLO]");
            }
        }

        ImGui::SetCursorPosY(46);
        ImGui::Separator();

        // 2. ACTIVE LOADOUT SLOTS BAR (AAA Horizontal Card Row)
        ImGui::SetCursorPosY(52);
        ImGui::SetWindowFontScale(0.92f);
        ImGui::TextColored(ImVec4(0.65f, 0.70f, 0.78f, 1.0f), "ACTIVE LOADOUT");
        ImGui::SetWindowFontScale(1.0f);

        auto& pri = player.getInventory().getEquipped(EquipSlot::PrimaryWeapon);
        auto& sec = player.getInventory().getEquipped(EquipSlot::SecondaryWeapon);
        auto& vest = player.getInventory().getEquipped(EquipSlot::Vest);
        auto& helm = player.getInventory().getEquipped(EquipSlot::Helmet);
        auto& rig = player.getInventory().getEquipped(EquipSlot::TacticalRig);
        auto& pack = player.getInventory().getEquipped(EquipSlot::Backpack);

        auto drawEquipSlotCard = [&](const char* slotTitle, const std::optional<Item>& itemOpt, int slotIdx, EquipSlot eqSlot, float cardW) {
            bool isSelected = (m_selectedLoadoutSlot == slotIdx);
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 p1(p0.x + cardW, p0.y + 76.0f);

            ImU32 bgCol = isSelected ? IM_COL32(16, 26, 42, 240) : IM_COL32(14, 17, 22, 220);
            ImU32 borderCol = isSelected ? IM_COL32(0, 132, 255, 255) : IM_COL32(50, 56, 68, 180);
            dl->AddRectFilled(p0, p1, bgCol, 3.0f);
            dl->AddRect(p0, p1, borderCol, 3.0f, 0, isSelected ? 2.0f : 1.0f);

            dl->AddText(ImVec2(p0.x + 8.0f, p0.y + 6.0f), isSelected ? IM_COL32(0, 155, 255, 255) : IM_COL32(140, 150, 165, 200), slotTitle);

            if (itemOpt.has_value()) {
                dl->AddText(ImVec2(p0.x + 8.0f, p0.y + 24.0f), IM_COL32(235, 240, 248, 255), itemOpt->name.c_str());
                ImVec2 iconCenter(p1.x - 55.0f, p0.y + 42.0f);
                drawProceduralItemIcon(dl, *itemOpt, iconCenter, 85.0f, 38.0f);

                char subDetail[48];
                if (itemOpt->category == ItemCategory::Weapon) {
                    snprintf(subDetail, sizeof(subDetail), "CAL: %s", itemOpt->caliber.empty() ? "STANDARD" : itemOpt->caliber.c_str());
                } else {
                    snprintf(subDetail, sizeof(subDetail), "WEIGHT: %.1f KG", itemOpt->weightKg);
                }
                dl->AddText(ImVec2(p0.x + 8.0f, p0.y + 56.0f), IM_COL32(110, 195, 130, 230), subDetail);
            } else {
                dl->AddText(ImVec2(p0.x + 8.0f, p0.y + 36.0f), IM_COL32(120, 128, 140, 180), "[EMPTY SLOT]");
            }

            char slotBtnId[32];
            snprintf(slotBtnId, sizeof(slotBtnId), "##EqSlot_%d", slotIdx);
            ImGui::SetCursorScreenPos(p0);
            if (ImGui::InvisibleButton(slotBtnId, ImVec2(cardW, 76.0f))) {
                m_selectedLoadoutSlot = slotIdx;
                if (slotIdx == 0) m_traderCategoryFilter = 1;
                else if (slotIdx == 1) m_traderCategoryFilter = 2;
                else if (slotIdx == 2 || slotIdx == 3) m_traderCategoryFilter = 4;
                else if (slotIdx == 4) m_traderCategoryFilter = 5;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::SameLine(0, 8.0f);
        };

        float slotCardW = (contentW - 32.0f - 32.0f) * 0.20f;
        drawEquipSlotCard("PRIMARY WEAPON", pri, 0, EquipSlot::PrimaryWeapon, slotCardW);
        drawEquipSlotCard("SIDEARM / CQB", sec, 1, EquipSlot::SecondaryWeapon, slotCardW);
        drawEquipSlotCard("BODY ARMOR VEST", vest, 2, EquipSlot::Vest, slotCardW);
        drawEquipSlotCard("BALLISTIC HELMET", helm, 3, EquipSlot::Helmet, slotCardW);
        drawEquipSlotCard("TACTICAL RIG & PACK", rig.has_value() ? rig : pack, 4, EquipSlot::TacticalRig, slotCardW);

        ImGui::NewLine();
        ImGui::SetCursorPosY(150);
        ImGui::Separator();

        // 3. SUB-CATEGORY FILTER PILLS
        ImGui::SetCursorPos(ImVec2(16, 158));

        auto drawFilterPill = [&](const char* label, int filterIdx, float pillW = 120.0f) {
            bool isSel = (m_traderCategoryFilter == filterIdx);
            if (renderCompetitorButton(label, ImVec2(pillW, 28), isSel, isSel)) {
                m_traderCategoryFilter = filterIdx;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::SameLine(0, 8.0f);
        };

        drawFilterPill("ALL ITEMS", 0, 105.0f);
        drawFilterPill("ASSAULT RIFLES", 1, 130.0f);
        drawFilterPill("SMG & SIDEARMS", 2, 135.0f);
        drawFilterPill("SNIPER & DMR", 3, 125.0f);
        drawFilterPill("ARMOR & GEAR", 4, 125.0f);
        drawFilterPill("AMMO & GRENADES", 5, 140.0f);

        // 4. MAIN SPLIT: STORE CATALOG GRID (LEFT) + DETAIL FLYOUT (RIGHT)
        float splitY = 196.0f;
        float catalogH = contentH - splitY - 10.0f;
        float flyoutW = 400.0f;
        float gridW = contentW - flyoutW - 24.0f;

        // LEFT: Store Catalog Grid
        ImGui::SetCursorPos(ImVec2(16, splitY));
        ImGui::BeginChild("DeploymentCatalogGrid", ImVec2(gridW, catalogH), true);

        const auto& catalog = m_armory.getArmoryCatalog();
        int cols = std::max(1, (int)(gridW / 260.0f));
        int colIdx = 0;

        for (size_t i = 0; i < catalog.size(); ++i) {
            const auto& shopEntry = catalog[i];
            const auto& item = shopEntry.item;

            bool matchFilter = true;
            if (m_traderCategoryFilter == 1) {
                matchFilter = (item.category == ItemCategory::Weapon && (item.name.find("AK") != std::string::npos || item.name.find("M4") != std::string::npos || item.weaponType == WeaponType::Rifle));
            } else if (m_traderCategoryFilter == 2) {
                matchFilter = (item.category == ItemCategory::Weapon && (item.weaponType == WeaponType::Pistol || item.name.find("M9") != std::string::npos || item.name.find("MP5") != std::string::npos));
            } else if (m_traderCategoryFilter == 3) {
                matchFilter = (item.category == ItemCategory::Weapon && (item.weaponType == WeaponType::Sniper || item.name.find("Mosin") != std::string::npos || item.name.find("DMR") != std::string::npos));
            } else if (m_traderCategoryFilter == 4) {
                matchFilter = (item.category == ItemCategory::Gear);
            } else if (m_traderCategoryFilter == 5) {
                matchFilter = (item.category == ItemCategory::Ammo || item.category == ItemCategory::Medical || item.name.find("Grenade") != std::string::npos);
            }
            if (!matchFilter) continue;

            ImGui::PushID((int)i + 9000);
            bool isSelectedCard = (m_selectedShopItemIndex == (int)i);

            bool isEquipped = false;
            if (pri.has_value() && pri->name == item.name) isEquipped = true;
            else if (sec.has_value() && sec->name == item.name) isEquipped = true;
            else if (vest.has_value() && vest->name == item.name) isEquipped = true;
            else if (helm.has_value() && helm->name == item.name) isEquipped = true;
            else if (rig.has_value() && rig->name == item.name) isEquipped = true;

            float cardW = (gridW - (cols * 12.0f) - 16.0f) / cols;
            float cardH = 150.0f;

            ImVec2 cP0 = ImGui::GetCursorScreenPos();
            ImVec2 cP1(cP0.x + cardW, cP0.y + cardH);

            ImU32 cBg = isSelectedCard ? IM_COL32(26, 32, 42, 245) : IM_COL32(14, 18, 24, 230);
            ImU32 cBorder = isSelectedCard ? IM_COL32(0, 132, 255, 255) : (isEquipped ? IM_COL32(40, 180, 100, 200) : IM_COL32(42, 48, 60, 200));

            dl->AddRectFilled(cP0, cP1, cBg, 3.0f);
            dl->AddRect(cP0, cP1, cBorder, 3.0f, 0, (isSelectedCard || isEquipped) ? 2.0f : 1.0f);

            dl->AddText(ImVec2(cP0.x + 8.0f, cP0.y + 6.0f), IM_COL32(90, 100, 115, 180), ":::");

            const char* catTag = (item.category == ItemCategory::Weapon) ? "FIREARM" : (item.category == ItemCategory::Gear ? "GEAR" : (item.category == ItemCategory::Ammo ? "AMMO" : "TACTICAL"));
            dl->AddText(ImVec2(cP0.x + 30.0f, cP0.y + 6.0f), IM_COL32(140, 150, 165, 200), catTag);

            char wBuf[24];
            snprintf(wBuf, sizeof(wBuf), "%.1f KG", item.weightKg);
            dl->AddText(ImVec2(cP1.x - 58.0f, cP0.y + 6.0f), IM_COL32(120, 130, 145, 180), wBuf);

            ImVec2 iconCenter(cP0.x + cardW * 0.5f, cP0.y + 58.0f);
            drawProceduralItemIcon(dl, item, iconCenter, cardW * 0.70f, 50.0f);

            dl->AddLine(ImVec2(cP0.x + 6.0f, cP1.y - 40.0f), ImVec2(cP1.x - 6.0f, cP1.y - 40.0f), IM_COL32(50, 56, 68, 140));

            if (isEquipped) {
                dl->AddText(ImVec2(cP1.x - 90.0f, cP1.y - 34.0f), IM_COL32(40, 220, 120, 255), "[EQUIPPED]");
            } else {
                char pBuf[32];
                snprintf(pBuf, sizeof(pBuf), "$%d", shopEntry.priceUsd);
                float priceW = ImGui::CalcTextSize(pBuf).x;
                dl->AddText(ImVec2(cP1.x - priceW - 8.0f, cP1.y - 34.0f), IM_COL32(95, 215, 130, 255), pBuf);
            }

            std::string displayName = item.name;
            float maxNameW = cardW - 95.0f;
            if (ImGui::CalcTextSize(displayName.c_str()).x > maxNameW) {
                while (!displayName.empty() && ImGui::CalcTextSize((displayName + "...").c_str()).x > maxNameW) {
                    displayName.pop_back();
                }
                displayName += "...";
            }
            dl->AddText(ImVec2(cP0.x + 8.0f, cP1.y - 34.0f), IM_COL32(235, 240, 248, 255), displayName.c_str());


            char cardBtnId[32];
            snprintf(cardBtnId, sizeof(cardBtnId), "##ShopCard_%d", (int)i);
            ImGui::SetCursorScreenPos(cP0);
            if (ImGui::InvisibleButton(cardBtnId, ImVec2(cardW, cardH))) {
                m_selectedShopItemIndex = (int)i;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }

            ImGui::PopID();

            colIdx++;
            if (colIdx < cols) {
                ImGui::SameLine(0, 10.0f);
            } else {
                colIdx = 0;
                ImGui::Spacing();
            }
        }

        ImGui::EndChild();

        // 5. RIGHT: Hardware Inspection & Infiltration Launch Panel
        float rightColX = contentW - flyoutW + 16.0f;
        float launchDockH = 230.0f;
        float flyoutChildH = catalogH - launchDockH - 8.0f;

        ImGui::SetCursorPos(ImVec2(rightColX, splitY));
        ImGui::BeginChild("DeploymentFlyoutPane", ImVec2(flyoutW, flyoutChildH), true);

        if (m_selectedShopItemIndex >= 0 && m_selectedShopItemIndex < (int)catalog.size()) {
            const auto& selectedEntry = catalog[m_selectedShopItemIndex];
            const auto& item = selectedEntry.item;

            ImGui::SetWindowFontScale(1.22f);
            ImGui::TextColored(ImVec4(0.96f, 0.94f, 0.90f, 1.0f), "%s", item.name.c_str());
            ImGui::SetWindowFontScale(1.0f);

            const char* subType = (item.category == ItemCategory::Weapon) ? "FIREARM" : (item.category == ItemCategory::Gear ? "TACTICAL GEAR" : "COMBAT MUNITIONS");
            ImGui::TextColored(ImVec4(0.85f, 0.55f, 0.18f, 1.0f), "%s", subType);
            ImGui::SameLine(flyoutW - 120.0f);
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "$%d USD", selectedEntry.priceUsd);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Large Visual Weapon Showcase
            ImVec2 previewP0 = ImGui::GetCursorScreenPos();
            ImVec2 previewP1(previewP0.x + flyoutW - 32.0f, previewP0.y + 54.0f);
            dl->AddRectFilled(previewP0, previewP1, IM_COL32(12, 15, 20, 255), 3.0f);
            dl->AddRect(previewP0, previewP1, IM_COL32(40, 48, 60, 180), 3.0f);
            ImVec2 previewCenter(previewP0.x + (flyoutW - 32.0f) * 0.5f, previewP0.y + 27.0f);
            drawProceduralItemIcon(dl, item, previewCenter, 180.0f, 48.0f);

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 58.0f);
            ImGui::Spacing();

            ImGui::TextDisabled("SPECIFICATIONS:");
            if (item.category == ItemCategory::Weapon) {
                ImGui::BulletText("Fire Modes: Semi / Full-Auto");
                ImGui::BulletText("Caliber: %s", item.caliber.empty() ? "7.62x39mm" : item.caliber.c_str());
                ImGui::BulletText("Effective Range: %.0fm  |  Velocity: %.0f m/s", item.effectiveRange > 0.0f ? item.effectiveRange : 450.0f, item.muzzleVelocityMs > 0.0f ? item.muzzleVelocityMs : 880.0f);
            } else if (item.category == ItemCategory::Gear) {
                ImGui::BulletText("Protection: NIJ Level IV Multi-Hit Ceramic");
                ImGui::BulletText("Weight: %.2f kg  |  Material: High-Tenacity Cordura", item.weightKg);
            } else {
                ImGui::BulletText("Type: Military Issue Tactical Munition");
                ImGui::BulletText("Quantity: %d  |  Weight: %.2f kg", item.quantity, item.weightKg);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            auto drawStatBar = [&](const char* statName, float val, ImVec4 col) {
                ImGui::TextDisabled("%-12s", statName);
                ImGui::SameLine(100.0f);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
                ImGui::ProgressBar(val, ImVec2(-1, 10), "");
                ImGui::PopStyleColor();
            };

            float dmgVal = (item.name.find("Mosin") != std::string::npos) ? 0.95f : ((item.name.find("M4") != std::string::npos) ? 0.72f : 0.65f);
            float rpmVal = (item.name.find("M4") != std::string::npos) ? 0.88f : ((item.name.find("AK") != std::string::npos) ? 0.70f : 0.25f);
            float accVal = (item.name.find("Mosin") != std::string::npos) ? 0.92f : 0.78f;
            float rngVal = (item.name.find("Mosin") != std::string::npos) ? 0.90f : 0.68f;

            drawStatBar("FIREPOWER", dmgVal, ImVec4(0.95f, 0.45f, 0.15f, 1.0f));
            drawStatBar("FIRE RATE", rpmVal, ImVec4(0.35f, 0.75f, 0.95f, 1.0f));
            drawStatBar("ACCURACY", accVal, ImVec4(0.35f, 0.95f, 0.45f, 1.0f));
            drawStatBar("RANGE", rngVal, ImVec4(0.95f, 0.85f, 0.35f, 1.0f));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool itemEquipped = false;
            int ownedInvIdx = -1;
            auto& items = player.getInventory().getItems();
            for (size_t k = 0; k < items.size(); ++k) {
                if (items[k].name == item.name) {
                    ownedInvIdx = (int)k;
                    break;
                }
            }
            if (pri.has_value() && pri->name == item.name) itemEquipped = true;
            else if (sec.has_value() && sec->name == item.name) itemEquipped = true;
            else if (vest.has_value() && vest->name == item.name) itemEquipped = true;
            else if (helm.has_value() && helm->name == item.name) itemEquipped = true;
            else if (rig.has_value() && rig->name == item.name) itemEquipped = true;

            bool canAfford = active ? (active->cash >= selectedEntry.priceUsd) : false;

            if (itemEquipped) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.20f, 0.8f));
                ImGui::Button("[ CURRENTLY EQUIPPED IN LOADOUT ]", ImVec2(-1, 36));
                ImGui::PopStyleColor();
            }
            else if (ownedInvIdx >= 0) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.65f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.75f, 0.42f, 1.0f));
                if (ImGui::Button("EQUIP TO LOADOUT", ImVec2(-1, 36))) {
                    EquipSlot targetSlot = EquipSlot::PrimaryWeapon;
                    if (item.category == ItemCategory::Weapon) {
                        if (item.weaponType == WeaponType::Pistol) targetSlot = EquipSlot::SecondaryWeapon;
                    } else if (item.category == ItemCategory::Gear) {
                        if (item.name.find("Vest") != std::string::npos || item.name.find("Carrier") != std::string::npos || item.name.find("JPC") != std::string::npos) targetSlot = EquipSlot::Vest;
                        else if (item.name.find("Helmet") != std::string::npos || item.name.find("FAST") != std::string::npos) targetSlot = EquipSlot::Helmet;
                        else if (item.name.find("Rig") != std::string::npos) targetSlot = EquipSlot::TacticalRig;
                        else if (item.name.find("Pack") != std::string::npos) targetSlot = EquipSlot::Backpack;
                    }
                    player.getInventory().equipItem(ownedInvIdx, targetSlot);
                    player.updateArmorFromEquipped();
                    if (active) {
                        active->savedInventory = player.getInventory();
                        ProfileManager::get().saveProfiles();
                    }
                    AudioEngine::get().playSound(SoundType::ItemEquip, 0.8f, 1.0f);
                }
                ImGui::PopStyleColor(2);
            }
            else {
                if (!canAfford) ImGui::BeginDisabled();

                char buyBtnText[64];
                snprintf(buyBtnText, sizeof(buyBtnText), "BUY & EQUIP ($%d)", selectedEntry.priceUsd);
                if (renderCompetitorButton(buyBtnText, ImVec2(-1, 38), false, true)) {
                    if (active) {
                        m_armory.buyAndEquip(m_selectedShopItemIndex, player.getInventory(), *active, false);
                        player.updateArmorFromEquipped();
                    }
                }

                if (!canAfford) ImGui::EndDisabled();
            }

            ImGui::Spacing();
            if (item.category == ItemCategory::Weapon) {
                if (renderCompetitorButton("INSPECT WEAPON (BLUEPRINT)", ImVec2(-1, 28))) {
                    inspectWeapon(item);
                }
            }
        }
        else {
            ImGui::SetWindowFontScale(1.15f);
            ImGui::TextColored(ImVec4(0.92f, 0.58f, 0.18f, 1.0f), "LOADOUT OVERVIEW");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("TOTAL COMBAT WEIGHT:");
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "%.1f KG  [ LIGHTEST / COMBAT READY ]", player.getInventory().getTotalWeightKg());

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextDisabled("SELECT ANY WEAPON, AMMO OR GEAR TO EQUIP OR PURCHASE DIRECTLY.");
        }

        ImGui::EndChild();

        // 6. BOTTOM FIXED INFILTRATION LAUNCH DOCK (AAA MATCHMAKING DECK)
        ImGui::SetCursorPos(ImVec2(rightColX, splitY + flyoutChildH + 8.0f));
        ImGui::BeginChild("DeploymentLaunchDock", ImVec2(flyoutW, launchDockH), true);

        const auto& shard = NetworkEngine::get().getActiveShard();
        const auto& shards = NetworkEngine::get().getShards();
        int curShard = NetworkEngine::get().getSelectedShardIndex();

        if (m_isMatchmaking) {
            m_matchTimer += io.DeltaTime;

            // Check if connection and session assigned with server
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
                // Animated radar sweep spinner
                int spinIdx = (int)(m_matchTimer * 6.0f) % 4;
                const char* spinnerChars[] = { "[ / ]", "[ - ]", "[ \\ ]", "[ | ]" };
                
                ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s SEARCHING FOR OPTIMAL MATCH...", spinnerChars[spinIdx]);
                
                int elapsedSec = (int)m_matchTimer;
                int mm = elapsedSec / 60;
                int ss = elapsedSec % 60;
                ImGui::TextDisabled("ELAPSED: %02d:%02d  |  ESTIMATED WAIT: < 00:15", mm, ss);

                const char* playlistNames[] = {
                    "5v5 TEAM DEATHMATCH // CQB YARD",
                    "TACTICAL INFILTRATION // FOB VANGUARD",
                    "PERSISTENT MMO WARZONE // CONTINENTAL"
                };
                ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.92f, 1.0f), "PLAYLIST: %s", playlistNames[std::clamp(m_matchmakingPlaylist, 0, 2)]);
                
                if (shard.isOnline) {
                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "REGION: GLOBAL OFFICIAL CLUSTER (%dms)", shard.pingMs);
                } else {
                    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "STATUS: ALLOCATING OPERATIVE TICKET...");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // CANCEL MATCHMAKING BUTTON
                if (renderCompetitorButton("[ CANCEL MATCHMAKING ]", ImVec2(-1, 38))) {
                    m_isMatchmaking = false;
                    m_matchTimer = 0.0f;
                    NetworkEngine::get().disconnectClient();
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            } else {
                // Matchmaking Notice (Cluster standby / fallback)
                ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.35f, 1.0f), "[!] MATCHMAKING NOTICE // CLUSTER STANDBY");
                ImGui::TextDisabled("Dedicated cluster did not respond within timeout.");
                ImGui::Spacing();

                if (renderCompetitorButton("> RETRY MATCHMAKING", ImVec2(-1, 34))) {
                    m_matchTimer = 0.0f;
                    m_matchmakingTimeout = false;
                    NetworkEngine::get().connectToServer(shard.host, shard.port);
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }

                ImGui::Spacing();
                if (renderCompetitorButton("> DEPLOY SOLO / SQUAD (LOCAL AI COMBAT)", ImVec2(-1, 40), false, true)) {
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
            // AAA Matchmaking Operational Deck
            ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "TACTICAL OPERATIONS PLAYLIST:");
            const char* playlists[] = {
                "5v5 TEAM DEATHMATCH // CQB YARD",
                "TACTICAL INFILTRATION // FOB VANGUARD",
                "PERSISTENT MMO WARZONE // CONTINENTAL"
            };
            if (ImGui::BeginCombo("##PlaylistCombo", playlists[m_matchmakingPlaylist])) {
                for (int p = 0; p < 3; ++p) {
                    bool isSelected = (m_matchmakingPlaylist == p);
                    if (ImGui::Selectable(playlists[p], isSelected)) {
                        m_matchmakingPlaylist = p;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "MATCHMAKING REGION / RDP SHARD:");
            std::string previewLabel = shards[curShard].name;
            if (shards[curShard].isOnline) {
                previewLabel += " [" + std::to_string(shards[curShard].pingMs) + "ms]";
            }
            if (ImGui::BeginCombo("##RegionSelector", previewLabel.c_str())) {
                for (int s = 0; s < (int)shards.size(); ++s) {
                    bool isSelected = (curShard == s);
                    std::string optLabel = shards[s].name;
                    if (shards[s].isOnline) {
                        optLabel += " (" + std::to_string(shards[s].pingMs) + "ms)";
                    } else {
                        optLabel += " [STANDBY / PROBING]";
                    }
                    if (ImGui::Selectable(optLabel.c_str(), isSelected)) {
                        NetworkEngine::get().setSelectedShardIndex(s);
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Region Health Indicator
            if (shards[curShard].isOnline) {
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "CLUSTER STATUS: ONLINE  |  Ping: %dms  |  Active: %d",
                                   shards[curShard].pingMs, shards[curShard].onlineCount);
            } else {
                ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.85f, 1.0f), "CLUSTER STATUS: OFFICIAL STANDBY (AUTO-CONNECT)");
            }

            ImGui::Spacing();

            // SQUAD VS SOLO TOGGLE PILL
            ImGui::TextColored(ImVec4(0.70f, 0.78f, 0.88f, 1.0f), "OPERATIVE ROSTER:");
            if (ImGui::RadioButton("SOLO", !m_squadMode)) { m_squadMode = false; }
            ImGui::SameLine();
            if (ImGui::RadioButton("SQUAD 4-MAN", m_squadMode)) { m_squadMode = true; }

            ImGui::Spacing();

            // 1. PRIMARY AAA MATCHMAKING CTA BUTTON
            if (renderCompetitorButton("> FIND MATCH // START MATCHMAKING", ImVec2(-1, 46), false, true)) {
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
                    NetworkEngine::get().connectToServer(shards[curShard].host, shards[curShard].port);
                }
                uint8_t pFaction = (player.getFaction() == Faction::FactionB) ? 1 : 0;
                NetworkEngine::get().requestMatchmaking(desiredMode, mapName, pFaction);

                AudioEngine::get().playSound(SoundType::TacticalClick);
                AudioEngine::get().playSound(SoundType::DeployHorn);
            }

            ImGui::Spacing();

            // 2. OFFLINE TACTICAL SIMULATION (FULL MAP, AI BACKFILL)
            if (renderCompetitorButton("[ OFFLINE SOLO WARZONE // BOT SIMULATION ]", ImVec2(-1, 36))) {
                m_isTrainingMode = false;
                m_isOnlineMultiplayer = false;
                m_isDeploying = true;
                m_deploymentTimer = 0.0f;
                m_tipRotateTimer = 0.0f;
                AudioEngine::get().playSound(SoundType::TacticalClick);
                AudioEngine::get().playSound(SoundType::DeployHorn);
            }

            ImGui::Spacing();

            // 3. CQB TRAINING RANGE
            if (renderCompetitorButton("[ CQB TRAINING RANGE // FIRING DRILLS ]", ImVec2(-1, 32))) {
                m_isTrainingMode = true;
                m_isDeploying = true;
                m_deploymentTimer = 0.0f;
                m_tipRotateTimer = 0.0f;
                AudioEngine::get().playSound(SoundType::TacticalClick);
                AudioEngine::get().playSound(SoundType::DeployHorn);
            }
        }

        ImGui::EndChild();
        ImGui::End();
    }
    ImGui::PopStyleColor(2);
}

// =========================================================================
// 2D. TACTICAL DEPLOYMENT BRIEFING & LOADING OVERLAY (PHASE 3)
// =========================================================================
void UIManager::renderTacticalDeploymentScreen(Player& player, CharacterProfile* active, World* world, FrontlineWarfare* frontline, Renderer* renderer) {
    ImGuiIO& io = ImGui::GetIO();
    float screenW = io.DisplaySize.x;
    float screenH = io.DisplaySize.y;

    m_deploymentTimer += io.DeltaTime;
    m_tipRotateTimer += io.DeltaTime;
    if (m_tipRotateTimer >= 3.6f) {
        m_tipRotateTimer = 0.0f;
        m_currentTacticalTip = (m_currentTacticalTip + 1) % 5;
    }

    // Server Unreachable Guard: Only enforce when attempting online dedicated shard raid
    if (m_isOnlineMultiplayer && !m_isTrainingMode) {
        if (m_deploymentTimer > 4.0f && !NetworkEngine::get().isClientConnected()) {
            const auto& shard = NetworkEngine::get().getActiveShard();
            m_isDeploying = false;
            m_deploymentTimer = 0.0f;
            m_serverOfflineModalOpen = true;
            m_serverOfflineModalMsg = "Dedicated server shard (" + shard.host + ":" + std::to_string(shard.port) + ") is unreachable. Deploy offline or start local server.";
            NetworkEngine::get().disconnectClient();
            AudioEngine::get().playSound(SoundType::TacticalClick);
            return;
        }
    }

    // Loading duration: 2.8 seconds
    if (m_deploymentTimer >= 2.8f) {
        m_isDeploying = false;
        m_deploymentTimer = 0.0f;
        m_gameState = GameState::InGame;
        player.resetFullHealthAndVitals();

        if (active) {
            if (m_isTrainingMode) {
                if (world) {
                    world->generateMilitaryBase();
                }
                if (renderer) {
                    renderer->rebuildTerrainMesh(true);
                }
                if (frontline && world) {
                    player.setFaction(active->faction);
                    frontline->initTrainingGarrison(*world, active->faction);
                }
                float trY = World::sampleTerrainHeight(0.0f, 2.0f);
                player.setPosition(glm::vec3(0.0f, trY, 2.0f));
                player.setYaw(0.0f);
                SteamIntegration::get().setRichPresence("status", "MERCENARY: Offline Training Simulation");
            } else if (m_matchmakingPlaylist == 2) {
                // OPEN WORLD MMO: 6-Hex Ukrainian Frontline Theater
                std::string targetMap = NetworkEngine::get().getCurrentMapName();
                if (targetMap.empty()) {
                    targetMap = (active->faction == Faction::FactionB) ? "kherson" : "kharkiv";
                }
                std::string mapPath = "assets/maps/" + targetMap + ".vmap";
                if (world) {
                    if (world->loadMap(mapPath) || world->loadMap("ASSETS/maps/" + targetMap + ".vmap")) {
                        std::cout << "[World Deployment] Loaded Continental Hex: " << mapPath << " (" 
                                  << world->getObstacles().size() << " obstacles, "
                                  << world->getCrates().size() << " crates, "
                                  << world->getSectors().size() << " sectors)\n" << std::flush;
                    } else {
                        world->generateWarzone();
                    }
                }
                if (renderer) {
                    renderer->rebuildTerrainMesh(false);
                }
                if (frontline && world) {
                    player.setFaction(active->faction);
                    frontline->init(15);
                }
                glm::vec3 spawnPos = NetworkEngine::get().getSpawnPosition();
                if (glm::length(spawnPos) > 1.0f) {
                    float startY = World::sampleTerrainHeight(spawnPos.x, spawnPos.z);
                    player.setPosition(glm::vec3(spawnPos.x, startY, spawnPos.z));
                } else if (active->faction == Faction::FactionB) {
                    float startY = World::sampleTerrainHeight(0.0f, 40.0f);
                    player.setPosition(glm::vec3(0.0f, startY, 40.0f));
                    player.setYaw(3.14159f);
                } else {
                    float startY = World::sampleTerrainHeight(0.0f, -40.0f);
                    player.setPosition(glm::vec3(0.0f, startY, -40.0f));
                    player.setYaw(0.0f);
                }
                SteamIntegration::get().setRichPresence("status", ("MERCENARY: " + targetMap + " Frontline").c_str());
            } else if (m_matchmakingPlaylist == 1) {
                // TACTICAL INFILTRATION: FOB Vanguard (Extraction Raid)
                if (world) {
                    world->generateMilitaryBase();
                }
                if (renderer) {
                    renderer->rebuildTerrainMesh(true);
                }
                if (frontline && world) {
                    player.setFaction(active->faction);
                    bool spawnBots = m_tdmAgainstAi;
                    frontline->initTeamDeathmatch(*world, active->faction, spawnBots);
                }
                float startY = World::sampleTerrainHeight(-120.0f, -120.0f);
                player.setPosition(glm::vec3(-120.0f, startY, -120.0f));
                player.setYaw(0.785f);
                SteamIntegration::get().setRichPresence("status", "MERCENARY: FOB Vanguard Infiltration");
            } else {
                // 5v5 TEAM DEATHMATCH: Killhouse / Yard
                if (world) {
                    world->generateTeamDeathmatch();
                }
                if (renderer) {
                    renderer->rebuildTerrainMesh(false);
                }
                if (frontline && world) {
                    player.setFaction(active->faction);
                    bool spawnBots = m_tdmAgainstAi;
                    frontline->initTeamDeathmatch(*world, active->faction, spawnBots);
                }
                if (active->faction == Faction::FactionA) {
                    player.setPosition(glm::vec3(0.0f, 0.0f, -26.0f));
                    player.setYaw(0.0f);
                } else {
                    player.setPosition(glm::vec3(0.0f, 0.0f, 26.0f));
                    player.setYaw(3.14159f);
                }
                SteamIntegration::get().setRichPresence("status", "MERCENARY: 5v5 Team Deathmatch");
            }

            // Ensure combat loadout
            if (!player.getInventory().getEquipped(EquipSlot::PrimaryWeapon).has_value()) {
                player.getInventory().getEquipped(EquipSlot::PrimaryWeapon) = Inventory::createM4A1();
                player.getInventory().addItem(Inventory::create556Ammo(180));
            }
            if (!player.getInventory().getEquipped(EquipSlot::SecondaryWeapon).has_value()) {
                player.getInventory().getEquipped(EquipSlot::SecondaryWeapon) = Inventory::createM9Pistol();
                player.getInventory().addItem(Inventory::create9mmAmmo(60));
            }
            if (!player.getInventory().getEquipped(EquipSlot::Vest).has_value()) {
                player.getInventory().getEquipped(EquipSlot::Vest) = Inventory::createPlateCarrier();
            }
            if (!player.getInventory().getEquipped(EquipSlot::Helmet).has_value()) {
                player.getInventory().getEquipped(EquipSlot::Helmet) = Inventory::createFastHelmet();
            }
            player.getInventory().addItem(Inventory::createBandage(3));
        }
        AudioEngine::get().playSound(SoundType::DeployHorn);
        return;
    }

    // Full-screen tactical overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(screenW, screenH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.07f, 0.99f));
    if (ImGui::Begin("##TacticalDeploymentBriefingScreen", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // 1. Fullscreen Subtle Military Grid
        for (float x = 0; x < screenW; x += 90.0f) {
            dl->AddLine(ImVec2(x, 0), ImVec2(x, screenH), IM_COL32(24, 30, 40, 60), 1.0f);
        }
        for (float y = 0; y < screenH; y += 90.0f) {
            dl->AddLine(ImVec2(0, y), ImVec2(screenW, y), IM_COL32(24, 30, 40, 60), 1.0f);
        }

        // Radar Scanning Line sweeping vertically
        float scanY = std::fmod(m_deploymentTimer * 180.0f, screenH);
        dl->AddLine(ImVec2(0, scanY), ImVec2(screenW, scanY), IM_COL32(219, 98, 41, 70), 1.5f);
        dl->AddRectFilledMultiColor(ImVec2(0, scanY - 24.0f), ImVec2(screenW, scanY),
                                    IM_COL32(219, 98, 41, 0), IM_COL32(219, 98, 41, 0),
                                    IM_COL32(219, 98, 41, 25), IM_COL32(219, 98, 41, 25));

        // 2. Top Header Bar
        float headerH = 64.0f;
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(screenW, headerH), IM_COL32(14, 17, 23, 240));
        dl->AddLine(ImVec2(0, headerH), ImVec2(screenW, headerH), IM_COL32(36, 44, 56, 255), 1.0f);

        // Header Title
        if (m_fontHeading) ImGui::PushFont(m_fontHeading);
        dl->AddText(ImVec2(36.0f, 18.0f), IM_COL32(240, 245, 252, 255), m_isTrainingMode ? "MERCENARY: MILITARY PROVING GROUNDS // OFFLINE TRAINING" : "MERCENARY: WARZONE DEPLOYMENT BRIEFING");
        if (m_fontHeading) ImGui::PopFont();

        // Mode Pill (Right header)
        if (m_isTrainingMode) {
            float pillW = 310.0f;
            float pillX = screenW - pillW - 36.0f;
            dl->AddRectFilled(ImVec2(pillX, 16.0f), ImVec2(pillX + pillW, 48.0f), IM_COL32(36, 25, 19, 240), 3.0f);
            dl->AddRect(ImVec2(pillX, 16.0f), ImVec2(pillX + pillW, 48.0f), IM_COL32(219, 98, 41, 220), 3.0f, 0, 1.0f);
            dl->AddText(ImVec2(pillX + 14.0f, 23.0f), IM_COL32(235, 130, 50, 255), "[ FOB VANGUARD DRILL FACILITY ]");
        } else {
            float pillW = 260.0f;
            float pillX = screenW - pillW - 36.0f;
            dl->AddRectFilled(ImVec2(pillX, 16.0f), ImVec2(pillX + pillW, 48.0f), IM_COL32(18, 30, 36, 240), 3.0f);
            dl->AddRect(ImVec2(pillX, 16.0f), ImVec2(pillX + pillW, 48.0f), IM_COL32(45, 195, 225, 220), 3.0f, 0, 1.0f);
            dl->AddText(ImVec2(pillX + 16.0f, 23.0f), IM_COL32(75, 220, 245, 255), "[ LIVE DEDICATED WARZONE ]");
        }

        // 3. Two Columns Layout
        float contentY = headerH + 28.0f;
        float bottomBarH = 92.0f;
        float contentH = screenH - contentY - bottomBarH;
        float colMargin = 36.0f;
        float colSpacing = 28.0f;
        float colW = (screenW - (colMargin * 2.0f) - colSpacing) * 0.5f;

        // ---------- LEFT COLUMN: SATELLITE RECON & TOPOGRAPHY ----------
        float col1X = colMargin;
        dl->AddRectFilled(ImVec2(col1X, contentY), ImVec2(col1X + colW, contentY + contentH), IM_COL32(12, 15, 20, 240), 4.0f);
        dl->AddRect(ImVec2(col1X, contentY), ImVec2(col1X + colW, contentY + contentH), IM_COL32(36, 44, 56, 255), 4.0f, 0, 1.0f);

        // Section Title
        dl->AddRectFilled(ImVec2(col1X + 16.0f, contentY + 16.0f), ImVec2(col1X + 19.0f, contentY + 34.0f), IM_COL32(219, 98, 41, 255), 1.5f);
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        dl->AddText(ImVec2(col1X + 28.0f, contentY + 16.0f), IM_COL32(245, 248, 252, 255), m_isTrainingMode ? "FOB VANGUARD PROVING GROUNDS GIS" : "TACTICAL GIS SATELLITE RECONNAISSANCE");
        if (m_fontBold) ImGui::PopFont();
        dl->AddText(ImVec2(col1X + 28.0f, contentY + 38.0f), IM_COL32(140, 150, 165, 255), m_isTrainingMode ? "BASE BLUEPRINT & TACTICAL FACILITY SCHEMATIC" : "KH-11 KENNAN ORBITAL IMAGERY // RESOLUTION: 0.15m/px");

        // Inner Map Canvas Box
        float mapBoxX = col1X + 20.0f;
        float mapBoxY = contentY + 68.0f;
        float mapBoxW = colW - 40.0f;
        float mapBoxH = contentH - 88.0f;
        dl->AddRectFilled(ImVec2(mapBoxX, mapBoxY), ImVec2(mapBoxX + mapBoxW, mapBoxY + mapBoxH), IM_COL32(8, 10, 14, 255), 3.0f);
        dl->AddRect(ImVec2(mapBoxX, mapBoxY), ImVec2(mapBoxX + mapBoxW, mapBoxY + mapBoxH), IM_COL32(28, 34, 44, 255), 3.0f, 0, 1.0f);

        // Map Topo Contours & Features
        float mapMidX = mapBoxX + mapBoxW * 0.5f;
        float mapMidY = mapBoxY + mapBoxH * 0.5f;

        if (m_isTrainingMode) {
            // 1. Center Base HQ
            dl->AddRectFilled(ImVec2(mapMidX - 35.0f, mapMidY - 30.0f), ImVec2(mapMidX + 35.0f, mapMidY + 30.0f), IM_COL32(35, 42, 52, 230), 2.0f);
            dl->AddRect(ImVec2(mapMidX - 35.0f, mapMidY - 30.0f), ImVec2(mapMidX + 35.0f, mapMidY + 30.0f), IM_COL32(65, 80, 100, 255), 2.0f);
            dl->AddText(ImVec2(mapMidX - 24.0f, mapMidY - 7.0f), IM_COL32(210, 225, 245, 255), "BASE HQ");

            // 2. East Shooting Range
            float rLeft = mapMidX + 50.0f;
            float rRight = mapBoxX + mapBoxW - 14.0f;
            dl->AddRectFilled(ImVec2(rLeft, mapMidY - 45.0f), ImVec2(rRight, mapMidY + 45.0f), IM_COL32(38, 30, 22, 220), 2.0f);
            dl->AddRect(ImVec2(rLeft, mapMidY - 45.0f), ImVec2(rRight, mapMidY + 45.0f), IM_COL32(219, 130, 45, 240), 2.0f);
            dl->AddText(ImVec2(rLeft + 8.0f, mapMidY - 38.0f), IM_COL32(235, 160, 60, 255), "SHOOTING RANGE");
            dl->AddText(ImVec2(rLeft + 8.0f, mapMidY - 18.0f), IM_COL32(190, 195, 205, 255), "Lane 1: 15-35m [Pistol]");
            dl->AddText(ImVec2(rLeft + 8.0f, mapMidY + 2.0f), IM_COL32(190, 195, 205, 255), "Lane 2: 50-100m [Rifle]");
            dl->AddText(ImVec2(rLeft + 8.0f, mapMidY + 22.0f), IM_COL32(190, 195, 205, 255), "Lane 3: 125-200m [Sniper]");

            // 3. West Armory & Bunker
            float aRight = mapMidX - 50.0f;
            float aLeft = mapBoxX + 14.0f;
            dl->AddRectFilled(ImVec2(aLeft, mapMidY - 40.0f), ImVec2(aRight, mapMidY + 40.0f), IM_COL32(24, 38, 32, 220), 2.0f);
            dl->AddRect(ImVec2(aLeft, mapMidY - 40.0f), ImVec2(aRight, mapMidY + 40.0f), IM_COL32(50, 185, 120, 240), 2.0f);
            dl->AddText(ImVec2(aLeft + 8.0f, mapMidY - 30.0f), IM_COL32(75, 225, 145, 255), "ARSENAL & ARMORY");
            dl->AddText(ImVec2(aLeft + 8.0f, mapMidY - 10.0f), IM_COL32(180, 205, 190, 255), "Weapon Racks & Stash");
            dl->AddText(ImVec2(aLeft + 8.0f, mapMidY + 12.0f), IM_COL32(180, 205, 190, 255), "Class 4/5 Ballistic Gear");

            // 4. South Motor Pool
            float mTop = mapMidY + 50.0f;
            float mBtm = mapBoxY + mapBoxH - 52.0f;
            dl->AddRectFilled(ImVec2(mapMidX - 70.0f, mTop), ImVec2(mapMidX + 70.0f, mBtm), IM_COL32(30, 32, 36, 220), 2.0f);
            dl->AddRect(ImVec2(mapMidX - 70.0f, mTop), ImVec2(mapMidX + 70.0f, mBtm), IM_COL32(70, 75, 85, 255), 2.0f);
            dl->AddText(ImVec2(mapMidX - 55.0f, mTop + 8.0f), IM_COL32(195, 205, 220, 255), "LOGISTICS MOTOR POOL (4 TRUCKS)");

            // 5. North Gate Checkpoint & CQB Shoot House
            float nBtm = mapMidY - 50.0f;
            float nTop = mapBoxY + 14.0f;
            dl->AddRectFilled(ImVec2(mapMidX - 80.0f, nTop), ImVec2(mapMidX + 80.0f, nBtm), IM_COL32(38, 24, 26, 220), 2.0f);
            dl->AddRect(ImVec2(mapMidX - 80.0f, nTop), ImVec2(mapMidX + 80.0f, nBtm), IM_COL32(215, 65, 65, 240), 2.0f);
            dl->AddText(ImVec2(mapMidX - 72.0f, nTop + 6.0f), IM_COL32(245, 95, 95, 255), "NORTH GATE // CQB KILLHOUSE");

            // Insertion Staging Point
            float lzPulse = 14.0f + 8.0f * std::sin(m_deploymentTimer * 5.0f);
            int lzAlpha = static_cast<int>(180 + 75 * std::sin(m_deploymentTimer * 5.0f));
            dl->AddCircleFilled(ImVec2(mapMidX, mapMidY), 5.0f, IM_COL32(219, 98, 41, 255));
            dl->AddCircle(ImVec2(mapMidX, mapMidY), lzPulse, IM_COL32(219, 98, 41, lzAlpha), 0, 1.8f);

            // Telemetry footer
            dl->AddRectFilled(ImVec2(mapBoxX + 12.0f, mapBoxY + mapBoxH - 42.0f), ImVec2(mapBoxX + mapBoxW - 12.0f, mapBoxY + mapBoxH - 10.0f), IM_COL32(14, 18, 24, 220), 2.0f);
            dl->AddText(ImVec2(mapBoxX + 20.0f, mapBoxY + mapBoxH - 36.0f), IM_COL32(165, 180, 200, 255), "FOB VANGUARD  |  GRID: 49°14'N 37°48'E  |  STATUS: PERIMETER SECURED");
        } else {
            // River Corridor
            dl->AddRectFilled(ImVec2(mapBoxX + 10.0f, mapMidY - 14.0f), ImVec2(mapBoxX + mapBoxW - 10.0f, mapMidY + 14.0f), IM_COL32(26, 48, 68, 200), 2.0f);
            dl->AddText(ImVec2(mapMidX - 90.0f, mapMidY - 7.0f), IM_COL32(75, 145, 195, 220), "SIVERSKY DONETS RIVERBED");

            // FOB Alpha (Allied base north)
            float fobAY = mapBoxY + mapBoxH * 0.22f;
            dl->AddCircleFilled(ImVec2(mapMidX, fobAY), 7.0f, IM_COL32(45, 150, 240, 255));
            dl->AddCircle(ImVec2(mapMidX, fobAY), 14.0f, IM_COL32(45, 150, 240, 160), 0, 1.5f);
            dl->AddText(ImVec2(mapMidX + 18.0f, fobAY - 7.0f), IM_COL32(190, 220, 250, 255), "FOB ALPHA [COALITION]");

            // Crimson Redoubt (Hostile base south)
            float fobBY = mapBoxY + mapBoxH * 0.78f;
            dl->AddCircleFilled(ImVec2(mapMidX, fobBY), 7.0f, IM_COL32(235, 55, 50, 255));
            dl->AddCircle(ImVec2(mapMidX, fobBY), 14.0f, IM_COL32(235, 55, 50, 160), 0, 1.5f);
            dl->AddText(ImVec2(mapMidX + 18.0f, fobBY - 7.0f), IM_COL32(250, 180, 180, 255), "CRIMSON REDOUBT [SHOCK BRIGADE]");

            // Insertion LZ Animated Crosshair & Pulsing Ring
            float lzPulse = 16.0f + 10.0f * std::sin(m_deploymentTimer * 5.0f);
            int lzAlpha = static_cast<int>(180 + 75 * std::sin(m_deploymentTimer * 5.0f));
            dl->AddCircleFilled(ImVec2(mapMidX, mapMidY), 5.0f, IM_COL32(219, 98, 41, 255));
            dl->AddCircle(ImVec2(mapMidX, mapMidY), lzPulse, IM_COL32(219, 98, 41, lzAlpha), 0, 1.8f);
            dl->AddLine(ImVec2(mapMidX - lzPulse - 8.0f, mapMidY), ImVec2(mapMidX + lzPulse + 8.0f, mapMidY), IM_COL32(219, 98, 41, 150), 1.0f);
            dl->AddLine(ImVec2(mapMidX, mapMidY - lzPulse - 8.0f), ImVec2(mapMidX, mapMidY + lzPulse + 8.0f), IM_COL32(219, 98, 41, 150), 1.0f);
            dl->AddText(ImVec2(mapMidX - 65.0f, mapMidY + 22.0f), IM_COL32(235, 145, 60, 255), "TARGET LZ: FALCON");

            // GPS Telemetry Sub-overlay
            dl->AddRectFilled(ImVec2(mapBoxX + 12.0f, mapBoxY + mapBoxH - 42.0f), ImVec2(mapBoxX + mapBoxW - 12.0f, mapBoxY + mapBoxH - 10.0f), IM_COL32(14, 18, 24, 220), 2.0f);
            dl->AddText(ImVec2(mapBoxX + 20.0f, mapBoxY + mapBoxH - 36.0f), IM_COL32(165, 180, 200, 255), "MGRS: 36U VB 4821 9144  |  ELEV: 114m AMSL  |  AZIMUTH: 342° NW");
        }

        // ---------- RIGHT COLUMN: MISSION DOSSIER & DIRECTIVES ----------
        float col2X = colMargin + colW + colSpacing;
        dl->AddRectFilled(ImVec2(col2X, contentY), ImVec2(col2X + colW, contentY + contentH), IM_COL32(12, 15, 20, 240), 4.0f);
        dl->AddRect(ImVec2(col2X, contentY), ImVec2(col2X + colW, contentY + contentH), IM_COL32(36, 44, 56, 255), 4.0f, 0, 1.0f);

        // Section Title
        dl->AddRectFilled(ImVec2(col2X + 16.0f, contentY + 16.0f), ImVec2(col2X + 19.0f, contentY + 34.0f), IM_COL32(219, 98, 41, 255), 1.5f);
        if (m_fontBold) ImGui::PushFont(m_fontBold);
        dl->AddText(ImVec2(col2X + 28.0f, contentY + 16.0f), IM_COL32(245, 248, 252, 255), m_isTrainingMode ? "OPERATION DOSSIER: FOB VANGUARD PROVING GROUNDS" : "OPERATION DOSSIER: SIVERSKY RIVERFRONT");
        if (m_fontBold) ImGui::PopFont();
        dl->AddText(ImVec2(col2X + 28.0f, contentY + 38.0f), IM_COL32(140, 150, 165, 255), m_isTrainingMode ? "STANDALONE PROVING GROUNDS // WEAPONS CALIBRATION" : "SECTOR 04 COMBAT DEPLOYMENT ORDERS // WEAPONS FREE");

        // Operational Parameters Grid
        float curP = contentY + 70.0f;
        auto drawParam = [&](const char* tag, const char* val, ImU32 col = IM_COL32(225, 232, 242, 255)) {
            dl->AddText(ImVec2(col2X + 24.0f, curP), IM_COL32(130, 140, 155, 255), tag);
            dl->AddText(ImVec2(col2X + 180.0f, curP), col, val);
            curP += 24.0f;
        };

        drawParam("OPERATION CODENAME:", "SIVERSKY RIVERFRONT // DAWN RAID", IM_COL32(245, 195, 50, 255));
        drawParam("DESIGNATED SECTOR:", "Sector 04 (Donbas Salient Corridor)");
        drawParam("ATMOSPHERE & TEMP:", "Overcast / Rain Drizzle (14 deg C, Wind 4.2m/s WNW)");
        drawParam("OPTICAL VISIBILITY:", "600m (Intermittent Ground Fog / Haze)");
        drawParam("COMBAT THREAT LEVEL:", "TIER 4 (ELEVATED) // Heavy Small Arms Fire", IM_COL32(235, 75, 65, 255));
        drawParam("RULES OF ENGAGEMENT:", "WEAPONS FREE // Neutralize All Hostile Patrols");
        drawParam("PRIMARY DIRECTIVE:", "Secure FOB Alpha Perimeter & Reclaim Outposts");
        drawParam("EXFILTRATION ROUTE:", "South Rail Trestle Bridge [EXFIL-01] Opens T+5m");

        curP += 12.0f;
        dl->AddLine(ImVec2(col2X + 24.0f, curP), ImVec2(col2X + colW - 24.0f, curP), IM_COL32(32, 38, 48, 255), 1.0f);
        curP += 16.0f;

        // Ready or Not / Tarkov Tactical Field Survival Tips Box
        float tipBoxW = colW - 48.0f;
        float tipBoxH = 110.0f;
        dl->AddRectFilled(ImVec2(col2X + 24.0f, curP), ImVec2(col2X + 24.0f + tipBoxW, curP + tipBoxH), IM_COL32(18, 22, 28, 230), 3.0f);
        dl->AddRect(ImVec2(col2X + 24.0f, curP), ImVec2(col2X + 24.0f + tipBoxW, curP + tipBoxH), IM_COL32(36, 44, 56, 255), 3.0f, 0, 1.0f);
        // Left burnt orange accent bar
        dl->AddRectFilled(ImVec2(col2X + 24.0f, curP), ImVec2(col2X + 27.0f, curP + tipBoxH), IM_COL32(219, 98, 41, 255), 2.0f);

        dl->AddText(ImVec2(col2X + 38.0f, curP + 12.0f), IM_COL32(219, 98, 41, 255), "OPERATIONAL FIELD SURVIVAL PROTOCOL //");

        const char* tipTitles[5] = {
            "ARTERIAL HEMORRHAGE & TOURNIQUETS",
            "BALLISTIC ARMOR PENETRATION",
            "RECOIL RECOVERY & STANCE CONTROL",
            "ACOUSTIC CONCEALMENT & FLANKING",
            "DYNAMIC COMMODITIES & GOLD MARKET"
        };
        const char* tipBodies[5] = {
            "Arterial bleed cuts drain up to 5000ml of blood in 45s, causing fatal blackouts.\nApply a CAT tourniquet immediately to stabilize before field bandaging.",
            "NIJ Level IV ceramic body armor absorbs 5.45x39mm and 5.56x45mm ball rounds.\nWhen engaging armored mercenaries, target the legs, pelvic girdle, or face.",
            "Full-auto initial burst kick is severe. Crouch or drop prone to reduce horizontal\ndispersion by 45% and regain crisp center-mass reticle stability.",
            "Footsteps on gravel, mud, and steel grates alert hostile bots from up to 75m away.\nCrouch-walk silently when clearing bunker doors or river outposts.",
            "Uncut gold bullion bars looted from frontline cash boxes maintain high value.\nTrade bullion at the hideout exchange terminal to fund optics and suppressor gear."
        };

        dl->AddText(ImVec2(col2X + 38.0f, curP + 34.0f), IM_COL32(245, 248, 252, 255), tipTitles[m_currentTacticalTip]);
        dl->AddText(ImVec2(col2X + 38.0f, curP + 56.0f), IM_COL32(175, 185, 200, 255), tipBodies[m_currentTacticalTip]);

        // 4. Bottom Loading Telemetry Bar
        float btmY = screenH - bottomBarH;
        dl->AddRectFilled(ImVec2(0, btmY), ImVec2(screenW, screenH), IM_COL32(10, 13, 17, 255));
        dl->AddLine(ImVec2(0, btmY), ImVec2(screenW, btmY), IM_COL32(36, 44, 56, 255), 1.0f);

        float progressNorm = std::clamp(m_deploymentTimer / 2.8f, 0.0f, 1.0f);

        // Stage Description Text
        const char* stageText = "";
        if (progressNorm < 0.25f) {
            stageText = m_isTrainingMode ?
                "[01/04] STAGING MILITARY PROVING GROUNDS & SHOOTING LANES..." :
                "[01/04] WARMING PIPELINE CACHES & TERRAIN COLLIDERS...";
        } else if (progressNorm < 0.55f) {
            stageText = m_isTrainingMode ?
                "[02/04] PROVISIONING ARSENAL WEAPON RACKS & CALIBRATING TARGETS..." :
                "[02/04] NEGOTIATING CRYPTOGRAPHIC HANDSHAKE WITH SHARD...";
        } else if (progressNorm < 0.85f) {
            stageText = m_isTrainingMode ?
                "[03/04] MOBILIZING BASE GARRISON SENTRIES & PATROL FIRETEAMS..." :
                "[03/04] CALIBRATING WEAPONS BALLISTICS & DISPERSION...";
        } else {
            stageText = m_isTrainingMode ?
                "[04/04] CLEARANCE GRANTED // ENTERING FOB VANGUARD BASE..." :
                "[04/04] INFILTRATION CLEARANCE GRANTED // COMMENCING DROP...";
        }

        dl->AddText(ImVec2(colMargin, btmY + 16.0f), IM_COL32(219, 98, 41, 255), stageText);

        char pctBuf[32];
        std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", static_cast<int>(progressNorm * 100.0f));
        float pBarW = screenW - (colMargin * 2.0f) - 180.0f;
        dl->AddText(ImVec2(colMargin + pBarW - 40.0f, btmY + 16.0f), IM_COL32(200, 210, 225, 255), pctBuf);

        // Progress Bar Track
        float pBarY = btmY + 44.0f;
        float pBarH = 12.0f;
        dl->AddRectFilled(ImVec2(colMargin, pBarY), ImVec2(colMargin + pBarW, pBarY + pBarH), IM_COL32(20, 25, 32, 255), 3.0f);
        dl->AddRect(ImVec2(colMargin, pBarY), ImVec2(colMargin + pBarW, pBarY + pBarH), IM_COL32(36, 44, 56, 255), 3.0f, 0, 1.0f);

        // Progress Bar Fill with burnt-orange glow
        if (progressNorm > 0.01f) {
            float fillW = pBarW * progressNorm;
            dl->AddRectFilled(ImVec2(colMargin, pBarY), ImVec2(colMargin + fillW, pBarY + pBarH), IM_COL32(219, 98, 41, 255), 3.0f);
            // Glowing leading edge
            dl->AddRectFilled(ImVec2(colMargin + fillW - 4.0f, pBarY), ImVec2(colMargin + fillW, pBarY + pBarH), IM_COL32(255, 185, 90, 255), 3.0f);
        }

        // Abort Button
        ImGui::SetCursorScreenPos(ImVec2(screenW - colMargin - 150.0f, btmY + 24.0f));
        if (renderCompetitorButton("ABORT INSERTION", ImVec2(150.0f, 38.0f))) {
            m_isDeploying = false;
            m_deploymentTimer = 0.0f;
            if (!m_isTrainingMode) NetworkEngine::get().disconnectClient();
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }

        ImGui::End();
    }
    ImGui::PopStyleColor();
}

// =========================================================================
// 2E. DEDICATED SERVER OFFLINE MODAL (STRICT SERVER-DOWN ENFORCEMENT)
// =========================================================================
void UIManager::renderWeaponInspectModal() {
    ImGuiIO& io = ImGui::GetIO();
    float winW = 560.0f;
    float winH = 430.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - winW * 0.5f, io.DisplaySize.y * 0.5f - winH * 0.5f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("WEAPON SPECIFICATIONS & MODDING DOSSIER##WeaponInspectModal", &m_showWeaponInspect, flags)) {
        ImGui::SetWindowFontScale(1.2f);
        ImGui::TextColored(ImVec4(0.92f, 0.58f, 0.18f, 1.0f), "%s", m_inspectedItem.name.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("%s", m_inspectedItem.description.c_str());
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.95f, 1.0f), "TECHNICAL BALLISTICS & HANDLING SPECIFICATIONS:");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Chamber Caliber: ");
        ImGui::SameLine(180);
        ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.35f, 1.0f), "%s", m_inspectedItem.caliber.c_str());

        ImGui::Text("Firing Modes: ");
        ImGui::SameLine(180);
        ImGui::Text("%s (%.0f RPM)", m_inspectedItem.isAutomatic ? "FULL-AUTO / SEMI" : "SEMI-AUTO", m_inspectedItem.fireRateRpm);

        ImGui::Text("Magazine Capacity: ");
        ImGui::SameLine(180);
        ImGui::Text("%d / %d Rounds", m_inspectedItem.currentMag, m_inspectedItem.maxMag);

        ImGui::Text("Muzzle Velocity: ");
        ImGui::SameLine(180);
        ImGui::Text("%.0f m/s", m_inspectedItem.muzzleVelocityMs);

        ImGui::Text("Effective Range: ");
        ImGui::SameLine(180);
        ImGui::Text("%.0f meters", m_inspectedItem.effectiveRange);

        ImGui::Text("Weight: ");
        ImGui::SameLine(180);
        ImGui::Text("%.2f kg", m_inspectedItem.weightKg);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Ergonomics & Recoil Bars
        ImGui::Text("Ergonomics: %.0f / 100", m_inspectedItem.ergonomics);
        ImGui::ProgressBar(m_inspectedItem.ergonomics / 100.0f, ImVec2(-1, 12), "");

        ImGui::Text("Vertical Recoil: %.0f", m_inspectedItem.recoilVertical);
        ImGui::ProgressBar(std::min(1.0f, m_inspectedItem.recoilVertical / 140.0f), ImVec2(-1, 12), "");

        ImGui::Text("Horizontal Recoil: %.0f", m_inspectedItem.recoilHorizontal);
        ImGui::ProgressBar(std::min(1.0f, m_inspectedItem.recoilHorizontal / 200.0f), ImVec2(-1, 12), "");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("CLOSE INSPECT", ImVec2(-1, 38))) {
            m_showWeaponInspect = false;
        }

        ImGui::End();
    }
}

// =========================================================================
// 3B. PMC OPERATIVE CUSTOMIZATION (MMO REAL-TIME DYNAMIC IDENTITY & TURNTABLE)
// =========================================================================
void UIManager::renderCustomizationMenu(CharacterProfile* active, Player* player) {
    if (!active) return;

    CharacterCustomization& c = active->customization;
    bool modified = false;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 1. Header
    if (m_fontHeading) ImGui::PushFont(m_fontHeading);
    ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "OPERATIVE CUSTOMIZATION");
    if (m_fontHeading) ImGui::PopFont();
    if (m_fontSmall) ImGui::PushFont(m_fontSmall);
    ImGui::TextDisabled("Modify operative facial morphology, uniform camo, and combat appearance.");
    if (m_fontSmall) ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. Modern Flat Text Sub-Navigation Tabs with Burnt Orange Underline
    const char* subTabNames[] = { "FACE & HAIR", "UNIFORM", "PRESETS", "CAMERA & GEAR" };
    const int numSubTabs = 4;
    float curSubX = ImGui::GetCursorScreenPos().x;
    float subY = ImGui::GetCursorScreenPos().y;

    for (int t = 0; t < numSubTabs; ++t) {
        bool isSel = (m_customizeTab == t);
        ImVec2 textSize = ImGui::CalcTextSize(subTabNames[t]);
        float hitW = textSize.x + 8.0f;
        float hitH = 26.0f;

        ImGui::SetCursorScreenPos(ImVec2(curSubX - 4.0f, subY - 2.0f));
        char subBtnId[32];
        std::snprintf(subBtnId, sizeof(subBtnId), "##subtab_%d", t);
        ImGui::InvisibleButton(subBtnId, ImVec2(hitW, hitH));
        bool isHovered = ImGui::IsItemHovered();

        if (ImGui::IsItemClicked()) {
            m_customizeTab = t;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }

        ImU32 txtCol = isSel ? IM_COL32(255, 255, 255, 255) :
                       (isHovered ? IM_COL32(225, 232, 242, 255) : IM_COL32(135, 145, 160, 255));
        
        dl->AddText(ImVec2(curSubX, subY + 2.0f), txtCol, subTabNames[t]);

        if (isSel) {
            dl->AddLine(
                ImVec2(curSubX - 2.0f, subY + 22.0f),
                ImVec2(curSubX + textSize.x + 2.0f, subY + 22.0f),
                IM_COL32(219, 98, 41, 255), 2.5f
            );
        }

        curSubX += hitW + 20.0f;
    }

    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + 16, subY + 30.0f));
    ImGui::Separator();
    ImGui::Spacing();

    // Helper lambda to render competitor-grade segmented pill selector
    auto drawSegmentedSelector = [&](const char* label, const char* options[], int count, int& curIdx) {
        ImGui::TextDisabled("%s", label);
        ImGui::Spacing();
        float availW = ImGui::GetContentRegionAvail().x;
        float pillW = availW / count;
        float rowY = ImGui::GetCursorScreenPos().y;
        float startX = ImGui::GetCursorScreenPos().x;
        float bH = 28.0f;

        for (int i = 0; i < count; ++i) {
            float bX = startX + (i * pillW);
            bool isSelected = (curIdx == i);

            ImGui::SetCursorScreenPos(ImVec2(bX, rowY));
            char btnId[64];
            std::snprintf(btnId, sizeof(btnId), "##cseg_%s_%d", label, i);
            ImGui::InvisibleButton(btnId, ImVec2(pillW, bH));
            bool isHovered = ImGui::IsItemHovered();

            if (ImGui::IsItemClicked()) {
                curIdx = i;
                modified = true;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }

            ImDrawFlags roundFlags = ImDrawFlags_RoundCornersNone;
            if (i == 0) roundFlags = ImDrawFlags_RoundCornersLeft;
            if (i == count - 1) roundFlags = (roundFlags == ImDrawFlags_RoundCornersLeft) ? ImDrawFlags_RoundCornersAll : ImDrawFlags_RoundCornersRight;

            ImU32 btnBg = isSelected ? IM_COL32(36, 25, 19, 235) :
                          (isHovered ? IM_COL32(28, 32, 40, 230) : IM_COL32(18, 20, 24, 210));
            ImU32 btnBorder = isSelected ? IM_COL32(52, 36, 26, 255) : IM_COL32(40, 46, 56, 255);
            ImU32 textColor = isSelected ? IM_COL32(235, 130, 50, 255) :
                              (isHovered ? IM_COL32(220, 226, 235, 255) : IM_COL32(155, 162, 172, 255));

            dl->AddRectFilled(ImVec2(bX, rowY), ImVec2(bX + pillW, rowY + bH), btnBg, 3.0f, roundFlags);
            dl->AddRect(ImVec2(bX, rowY), ImVec2(bX + pillW, rowY + bH), btnBorder, 3.0f, roundFlags, 1.0f);

            if (isSelected) {
                dl->AddRectFilledMultiColor(
                    ImVec2(bX + 1.0f, rowY + bH - 10.0f),
                    ImVec2(bX + pillW - 1.0f, rowY + bH - 2.5f),
                    IM_COL32(219, 98, 41, 0), IM_COL32(219, 98, 41, 0),
                    IM_COL32(219, 98, 41, 60), IM_COL32(219, 98, 41, 60)
                );
                dl->AddRectFilled(
                    ImVec2(bX, rowY + bH - 2.5f),
                    ImVec2(bX + pillW, rowY + bH),
                    IM_COL32(219, 98, 41, 255),
                    roundFlags == ImDrawFlags_RoundCornersLeft ? 2.0f : (roundFlags == ImDrawFlags_RoundCornersRight ? 2.0f : 0.0f)
                );
            }

            ImVec2 tSize = ImGui::CalcTextSize(options[i]);
            dl->AddText(ImVec2(bX + (pillW - tSize.x) * 0.5f, rowY + (bH - tSize.y) * 0.5f - (isSelected ? 1.0f : 0.0f)), textColor, options[i]);
        }
        ImGui::SetCursorScreenPos(ImVec2(startX, rowY + bH));
        ImGui::Dummy(ImVec2(availW, 14.0f));
    };

    ImGui::BeginChild("CustomScrollArea", ImVec2(0, 0), false);

    // ================= SUB-TAB 0: FACE, SKIN & HAIR =================
    if (m_customizeTab == 0) {
        // 1. Face Model Morphology
        const char* faces[] = { "NOMAD", "RAZOR", "VIPER", "TITAN" };
        drawSegmentedSelector("FACE MORPHOLOGY", faces, 4, c.faceId);

        // 2. Controlled Parametric Face Sliders
        ImGui::TextDisabled("FACIAL STRUCTURE & AGE");
        ImGui::Spacing();
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##OperativeAge", &c.age, 20.0f, 65.0f, "Age: %.0f YRS")) modified = true;
        if (ImGui::SliderFloat("##MelaninTone", &c.melanin, 0.0f, 1.0f, "Melanin: %.2f")) modified = true;
        if (ImGui::SliderFloat("##JawWidth", &c.jawWidth, 0.92f, 1.10f, "Jaw Width: %.2fx")) modified = true;
        if (ImGui::SliderFloat("##ChinLength", &c.chinLength, -0.005f, 0.006f, "Chin: %.3f m")) modified = true;
        if (ImGui::SliderFloat("##NoseScale", &c.noseScale, 0.90f, 1.12f, "Nose: %.2fx")) modified = true;
        ImGui::PopItemWidth();

        ImGui::Spacing();
        ImGui::Spacing();

        // 3. Interactive Skin Tone Color Swatches
        ImGui::TextDisabled("SKIN COMPLEXION PRESETS");
        ImGui::Spacing();
        ImU32 skinColors[] = {
            IM_COL32(240, 208, 185, 255), // Fair Light
            IM_COL32(215, 178, 142, 255), // Slavic Tan
            IM_COL32(182, 142, 108, 255), // Olive Med
            IM_COL32(125, 88, 62, 255)    // Warm Deep
        };
        const char* skinLabels[] = { "Fair", "Slavic", "Olive", "Deep" };
        float swatchW = (ImGui::GetContentRegionAvail().x - 18.0f) * 0.25f;

        for (int i = 0; i < 4; ++i) {
            ImVec2 sp0 = ImGui::GetCursorScreenPos();
            ImVec2 sp1(sp0.x + swatchW, sp0.y + 32.0f);
            bool isSel = (c.skinToneId == i);

            dl->AddRectFilled(sp0, sp1, skinColors[i], 3.0f);
            if (isSel) {
                dl->AddRect(sp0, sp1, IM_COL32(219, 98, 41, 255), 3.0f, 0, 2.5f);
            } else {
                dl->AddRect(sp0, sp1, IM_COL32(45, 52, 62, 220), 3.0f, 0, 1.0f);
            }

            ImU32 txtCol = (i >= 2) ? IM_COL32(245, 245, 245, 255) : IM_COL32(25, 25, 25, 255);
            ImVec2 lSize = ImGui::CalcTextSize(skinLabels[i]);
            dl->AddText(ImVec2(sp0.x + (swatchW - lSize.x) * 0.5f, sp0.y + (32.0f - lSize.y) * 0.5f), txtCol, skinLabels[i]);

            ImGui::PushID(100 + i);
            if (ImGui::InvisibleButton("##SkinBtn", ImVec2(swatchW, 32.0f))) {
                c.skinToneId = i;
                modified = true;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::PopID();
            if (i < 3) ImGui::SameLine(0, 6.0f);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // 4. Hairstyles
        const char* hairs[] = { "BUZZCUT", "UNDERCUT", "CREWCUT", "SHAVED" };
        drawSegmentedSelector("HAIRSTYLE", hairs, 4, c.hairId);

        // 5. Hair Color Swatches
        ImGui::TextDisabled("HAIR COLOR");
        ImGui::Spacing();
        ImU32 hairColors[] = {
            IM_COL32(52, 36, 26, 255),   // Dark Brown
            IM_COL32(22, 22, 24, 255),   // Jet Black
            IM_COL32(185, 155, 95, 255), // Dirty Blonde
            IM_COL32(165, 172, 180, 255) // Silver
        };
        const char* hairLabels[] = { "Brown", "Black", "Blonde", "Silver" };

        for (int i = 0; i < 4; ++i) {
            ImVec2 hp0 = ImGui::GetCursorScreenPos();
            ImVec2 hp1(hp0.x + swatchW, hp0.y + 32.0f);
            bool isSel = (c.hairColorId == i);

            dl->AddRectFilled(hp0, hp1, hairColors[i], 3.0f);
            if (isSel) {
                dl->AddRect(hp0, hp1, IM_COL32(219, 98, 41, 255), 3.0f, 0, 2.5f);
            } else {
                dl->AddRect(hp0, hp1, IM_COL32(55, 62, 72, 220), 3.0f, 0, 1.0f);
            }

            ImU32 txtCol = (i < 2) ? IM_COL32(240, 240, 240, 255) : IM_COL32(20, 20, 20, 255);
            ImVec2 lSize = ImGui::CalcTextSize(hairLabels[i]);
            dl->AddText(ImVec2(hp0.x + (swatchW - lSize.x) * 0.5f, hp0.y + (32.0f - lSize.y) * 0.5f), txtCol, hairLabels[i]);

            ImGui::PushID(200 + i);
            if (ImGui::InvisibleButton("##HairColBtn", ImVec2(swatchW, 32.0f))) {
                c.hairColorId = i;
                modified = true;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::PopID();
            if (i < 3) ImGui::SameLine(0, 6.0f);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // 6. Facial Hair
        const char* beards[] = { "FULL BEARD", "STUBBLE", "GOATEE", "CLEAN" };
        drawSegmentedSelector("FACIAL HAIR", beards, 4, c.beardId);
    }
    // ================= SUB-TAB 1: UNIFORM, CAMO & GLOVES =================
    else if (m_customizeTab == 1) {
        ImGui::TextDisabled("CAMOUFLAGE PATTERN");
        ImGui::Spacing();

        struct CamoOption {
            int id;
            const char* name;
            const char* sub;
            ImU32 col0;
            ImU32 col1;
        };

        Faction activeFaction = active ? active->faction : (player ? player->getFaction() : Faction::FactionA);

        std::vector<CamoOption> camos;
        if (activeFaction == Faction::FactionB) {
            camos.push_back({2, "COVERT CHARCOAL", "Black Ops Urban Warfare", IM_COL32(40, 42, 46, 255), IM_COL32(22, 24, 26, 255)});
            camos.push_back({3, "M81 WOODLAND", "Classic Tactical Woodland", IM_COL32(65, 82, 50, 255), IM_COL32(85, 60, 40, 255)});
            if (c.camoId != 2 && c.camoId != 3) {
                c.camoId = 2;
                modified = true;
            }
        } else {
            camos.push_back({0, "MULTICAM OCP", "All-Terrain NATO Standard", IM_COL32(145, 132, 102, 255), IM_COL32(92, 105, 75, 255)});
            camos.push_back({1, "MM-14 DIGITAL", "UA Armed Forces Pixel", IM_COL32(115, 130, 95, 255), IM_COL32(165, 155, 125, 255)});
            if (c.camoId != 0 && c.camoId != 1) {
                c.camoId = 0;
                modified = true;
            }
        }

        float cardW = ImGui::GetContentRegionAvail().x;
        for (size_t i = 0; i < camos.size(); ++i) {
            ImVec2 cp0 = ImGui::GetCursorScreenPos();
            ImVec2 cp1(cp0.x + cardW, cp0.y + 44.0f);
            bool isSel = (c.camoId == camos[i].id);

            dl->AddRectFilled(cp0, cp1, isSel ? IM_COL32(36, 25, 19, 235) : IM_COL32(20, 23, 28, 220), 4.0f);

            // Preview Camo Dual Swatch
            dl->AddRectFilled(ImVec2(cp0.x + 10, cp0.y + 8), ImVec2(cp0.x + 36, cp0.y + 36), camos[i].col0, 3.0f);
            dl->AddRectFilled(ImVec2(cp0.x + 24, cp0.y + 8), ImVec2(cp0.x + 48, cp0.y + 36), camos[i].col1, 3.0f);

            dl->AddText(ImVec2(cp0.x + 58, cp0.y + 8), isSel ? IM_COL32(235, 130, 50, 255) : IM_COL32(225, 230, 238, 255), camos[i].name);
            dl->AddText(ImVec2(cp0.x + 58, cp0.y + 24), IM_COL32(130, 142, 158, 210), camos[i].sub);

            if (isSel) {
                dl->AddRect(cp0, cp1, IM_COL32(219, 98, 41, 255), 4.0f, 0, 1.5f);
                dl->AddText(ImVec2(cp1.x - 72, cp0.y + 14), IM_COL32(219, 98, 41, 255), "EQUIPPED");
            } else {
                dl->AddRect(cp0, cp1, IM_COL32(44, 50, 62, 200), 4.0f, 0, 1.0f);
            }

            ImGui::PushID(300 + (int)i);
            if (ImGui::InvisibleButton("##CamoBtn", ImVec2(cardW, 44))) {
                c.camoId = camos[i].id;
                modified = true;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }
            ImGui::PopID();
            ImGui::Spacing();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Tactical Gloves
        const char* gloves[] = { "COYOTE TAN", "CARBON BLACK", "OLIVE DRAB" };
        drawSegmentedSelector("TACTICAL COMBAT GLOVES", gloves, 3, c.gloveId);

        // IFF Identification Armband
        const char* crimsonArms[] = { "RED (CRIMSON)", "WHITE", "NONE" };
        const char* coalitionArms[] = { "BLUE (COALITION)", "YELLOW", "NONE" };
        int armIdx = (c.armbandId == 4) ? 2 : ((c.armbandId == 1 || c.armbandId == 0) ? 0 : 1);

        if (activeFaction == Faction::FactionB) {
            drawSegmentedSelector("IFF ARMBAND", crimsonArms, 3, armIdx);
            c.armbandId = (armIdx == 0) ? 1 : ((armIdx == 1) ? 3 : 4);
        } else {
            drawSegmentedSelector("IFF ARMBAND", coalitionArms, 3, armIdx);
            c.armbandId = (armIdx == 0) ? 0 : ((armIdx == 1) ? 2 : 4);
        }
    }
    // ================= SUB-TAB 2: PRESETS =================
    else if (m_customizeTab == 2) {
        ImGui::TextDisabled("TACTICAL ROLE LOADOUT PRESETS");
        ImGui::Spacing();

        auto drawPresetCard = [&](const char* title, const char* desc, int face, int hair, int beard, int camo) {
            float cardW = ImGui::GetContentRegionAvail().x;
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 p1(p0.x + cardW, p0.y + 54.0f);

            dl->AddRectFilled(p0, p1, IM_COL32(20, 23, 28, 220), 4.0f);
            dl->AddRect(p0, p1, IM_COL32(44, 50, 62, 200), 4.0f, 0, 1.0f);

            dl->AddText(ImVec2(p0.x + 12, p0.y + 8), IM_COL32(245, 248, 252, 255), title);
            dl->AddText(ImVec2(p0.x + 12, p0.y + 28), IM_COL32(130, 140, 155, 255), desc);

            dl->AddText(ImVec2(p1.x - 70, p0.y + 18), IM_COL32(219, 98, 41, 255), "APPLY >");

            ImGui::PushID(title);
            if (ImGui::InvisibleButton("##PresetBtn", ImVec2(cardW, 54.0f))) {
                c.faceId = face;
                c.hairId = hair;
                c.beardId = beard;
                c.camoId = camo;
                modified = true;
                AudioEngine::get().playSound(SoundType::ItemEquip);
            }
            ImGui::PopID();
            ImGui::Spacing();
        };

        drawPresetCard("RECON SCOUT", "Lightweight silhouette, buzzcut, low-drag profile.", 0, 0, 3, 0);
        drawPresetCard("ASSAULT INFANTRY", "Modern military undercut, tactical stubble, all-terrain uniform.", 1, 1, 1, 0);
        drawPresetCard("URBAN BREACHER", "Titan morphology, full tactical beard, covert camo.", 3, 2, 0, 2);
        drawPresetCard("VETERAN OPERATOR", "Silver crewcut, goatee, veteran service uniform.", 2, 2, 2, 1);
    }
    // ================= SUB-TAB 3: CAMERA & GEAR =================
    else if (m_customizeTab == 3) {
        ImGui::TextDisabled("VIEWPORT STANCE & CAMERA");
        ImGui::Spacing();

        const char* camViews[] = { "PORTRAIT (FACE)", "FULL BODY" };
        int camIdx = m_customizeFaceZoom ? 0 : 1;
        drawSegmentedSelector("CAMERA FOCUS", camViews, 2, camIdx);
        m_customizeFaceZoom = (camIdx == 0);

        const char* gearViews[] = { "CIVILIAN ATTIRE", "COMBAT GEAR" };
        int gearIdx = m_previewCombatGearInCustomize ? 1 : 0;
        drawSegmentedSelector("EQUIPMENT VISIBILITY", gearViews, 2, gearIdx);
        m_previewCombatGearInCustomize = (gearIdx == 1);

        ImGui::Spacing();
        ImGui::TextDisabled("TURNTABLE ROTATION");
        ImGui::Spacing();
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##OrbitAngle", &m_operatorTurntableYaw, -3.14159f, 3.14159f, "Angle: %.2f rad")) {
            setOperatorTurntable(m_operatorTurntableYaw);
        }
        ImGui::PopItemWidth();
    }

    ImGui::EndChild();

    if (modified) {
        ProfileManager::get().saveProfiles();
        // Profile customization persists in CharacterProfile and active player renders directly
    }
}



void UIManager::renderInventoryWindow(Player& player) {
    if (!m_inventoryOpen) return;

    ImGuiIO& io = ImGui::GetIO();
    ProfileManager& pm = ProfileManager::get();
    CharacterProfile* active = pm.getActiveProfile();

    // Dark semi-transparent atmospheric raid backdrop
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGuiWindowFlags bgFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.05f, 0.07f, 0.76f));
    if (ImGui::Begin("##InGameInventoryBackdrop", nullptr, bgFlags)) {
        ImGui::SetCursorPos(ImVec2(io.DisplaySize.x * 0.5f - 160.0f, 16.0f));
        ImGui::SetWindowFontScale(1.1f);
        ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.20f, 1.0f), "TACTICAL INVENTORY [PRESS TAB TO CLOSE]");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
    }
    ImGui::PopStyleColor();

    float topY = 56.0f;
    float contentH = io.DisplaySize.y - topY - 24.0f;
    ImGuiWindowFlags dockFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;

    // LEFT DOCKED WINDOW: Arena Breakout Paperdoll Equipment & Vitals
    ImGui::SetNextWindowPos(ImVec2(32.0f, topY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(390.0f, contentH), ImGuiCond_Always);
    if (ImGui::Begin("##InGamePaperdollDock", nullptr, dockFlags)) {
        drawArenaBreakoutPaperdoll(player, active, true);
        ImGui::End();
    }

    // RIGHT DOCKED WINDOW: Arena Breakout Tactical Grid (Rig + Backpack Stash)
    float rightW = std::min(580.0f, io.DisplaySize.x - 460.0f);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - rightW - 32.0f, topY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rightW, contentH), ImGuiCond_Always);
    if (ImGui::Begin("##InGameGridDock", nullptr, dockFlags)) {
        drawArenaBreakoutGridStash(player, active, true);
        ImGui::End();
    }
}


} // namespace Mercenary

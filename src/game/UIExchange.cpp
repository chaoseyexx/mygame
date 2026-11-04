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
void UIManager::renderExchangeMarket(Player& player, CharacterProfile* active) {
    ImGuiIO& io = ImGui::GetIO();
    float topY = 56.0f;
    float contentH = io.DisplaySize.y - topY - 16.0f;
    float contentW = io.DisplaySize.x - 32.0f;

    ImGui::SetNextWindowPos(ImVec2(16, topY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(contentW, contentH), ImGuiCond_Always);
    ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.08f, 0.11f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.75f, 0.18f, 0.45f));

    if (ImGui::Begin("##ExchangeMarketWindow", nullptr, winFlags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ExchangeMarket& market = ExchangeMarket::get();
        float curPrice = market.getCurrentPrice();
        float chgPct = market.get24hChangePct();
        float chgUsd = market.get24hChangeUsd();
        bool isUp = (chgUsd >= 0.0f);

        // =========================================================================
        // 1. TOP HEADER & COMMODITY TICKER BAR
        // =========================================================================
        ImGui::SetCursorPos(ImVec2(16, 12));
        if (ImGui::Button("<##ExBack", ImVec2(32, 28))) {
            m_currentTab = MainMenuTab::Overview;
            AudioEngine::get().playSound(SoundType::TacticalClick);
        }
        ImGui::SameLine(0, 10.0f);

        ImGui::SetWindowFontScale(1.15f);
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.20f, 1.0f), "GOLD COMMODITY EXCHANGE");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(0, 10.0f);
        ImGui::TextDisabled("| XAU / USD (1 KILOBAR 999.9 FINE BULLION)");

        // Center Live Ticker Price
        float tickerX = std::max(460.0f, contentW * 0.38f);
        ImGui::SameLine(tickerX);
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextColored(ImVec4(0.96f, 0.95f, 0.90f, 1.0f), "$%.2f", curPrice);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(0, 8.0f);

        ImVec4 chgCol = isUp ? ImVec4(0.25f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.25f, 0.25f, 1.0f);
        ImGui::TextColored(chgCol, "%s$%.2f (%s%.2f%%) %s", isUp ? "+" : "", chgUsd, isUp ? "+" : "", chgPct, isUp ? "^" : "v");

        // Right 24h Stats
        float statsX = contentW - 420.0f;
        ImGui::SameLine(statsX);
        ImGui::TextDisabled("24H HIGH: "); ImGui::SameLine(0, 2); ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "$%.1f", market.get24hHigh());
        ImGui::SameLine(0, 10);
        ImGui::TextDisabled("LOW: "); ImGui::SameLine(0, 2); ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "$%.1f", market.get24hLow());
        ImGui::SameLine(0, 10);
        ImGui::TextDisabled("VOL: "); ImGui::SameLine(0, 2); ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.20f, 1.0f), "%d BARS", market.get24hVolumeBars());

        ImGui::SetCursorPosY(46);
        ImGui::Separator();

        // =========================================================================
        // 2. MAIN LAYOUT: LEFT (CHART & ORDER BOOK) + RIGHT (TRADING TERMINAL)
        // =========================================================================
        float splitY = 54.0f;
        float rightTerminalW = 410.0f;
        float leftPaneW = contentW - rightTerminalW - 24.0f;
        float paneH = contentH - splitY - 12.0f;

        // --- LEFT PANE ---
        ImGui::SetCursorPos(ImVec2(16, splitY));
        ImGui::BeginChild("ExchangeLeftPane", ImVec2(leftPaneW, paneH), false);
        {
            // Timeframe / Interval Bar
            ImGui::TextDisabled("INTERVAL:");
            ImGui::SameLine(0, 8.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.48f, 0.12f, 1.0f));
            ImGui::Button("1M LIVE", ImVec2(70, 24));
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 6.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.15f, 0.19f, 0.85f));
            ImGui::Button("5M", ImVec2(50, 24)); ImGui::SameLine(0, 6.0f);
            ImGui::Button("15M", ImVec2(50, 24)); ImGui::SameLine(0, 6.0f);
            ImGui::Button("1H", ImVec2(50, 24)); ImGui::SameLine(0, 6.0f);
            ImGui::Button("1D", ImVec2(50, 24));
            ImGui::PopStyleColor();

            ImGui::SameLine(leftPaneW - 190.0f);
            ImGui::TextColored(ImVec4(0.25f, 0.95f, 0.45f, 1.0f), "* LIVE STREAMING (0.6s)");

            // Real-Time Vector Chart Canvas
            float chartH = 270.0f;
            ImVec2 chartP0 = ImGui::GetCursorScreenPos();
            ImVec2 chartP1(chartP0.x + leftPaneW, chartP0.y + chartH);

            // Chart Background
            dl->AddRectFilled(chartP0, chartP1, IM_COL32(11, 14, 19, 255), 4.0f);
            dl->AddRect(chartP0, chartP1, IM_COL32(40, 48, 62, 200), 4.0f);

            // Watermark
            dl->AddText(ImVec2(chartP0.x + 14.0f, chartP0.y + 12.0f), IM_COL32(80, 90, 105, 140), "XAU / USD · BULLION LIQUIDITY POOL · 1-MINUTE CANDLE TICKS");

            // Compute dynamic min/max for chart scaling
            const auto& history = market.getPriceHistory();
            float minP = 999999.0f;
            float maxP = -999999.0f;
            for (float val : history) {
                if (val < minP) minP = val;
                if (val > maxP) maxP = val;
            }
            float rangeP = std::max(20.0f, maxP - minP);
            minP -= rangeP * 0.08f;
            maxP += rangeP * 0.08f;
            rangeP = maxP - minP;

            // Draw horizontal price gridlines
            int gridLines = 5;
            for (int g = 0; g <= gridLines; ++g) {
                float gy = chartP1.y - 20.0f - (g / (float)gridLines) * (chartH - 45.0f);
                float gPrice = minP + (g / (float)gridLines) * rangeP;
                dl->AddLine(ImVec2(chartP0.x + 8.0f, gy), ImVec2(chartP1.x - 70.0f, gy), IM_COL32(32, 38, 48, 180), 1.0f);
                char pStr[32];
                snprintf(pStr, sizeof(pStr), "$%.1f", gPrice);
                dl->AddText(ImVec2(chartP1.x - 62.0f, gy - 7.0f), IM_COL32(110, 120, 135, 200), pStr);
            }

            // Draw vector polyline & gradient area
            if (history.size() >= 2) {
                std::vector<ImVec2> linePoints;
                float chartW = (chartP1.x - chartP0.x - 75.0f);
                float stepX = chartW / (float)(history.size() - 1);

                for (size_t i = 0; i < history.size(); ++i) {
                    float px = chartP0.x + 10.0f + i * stepX;
                    float py = chartP1.y - 20.0f - ((history[i] - minP) / rangeP) * (chartH - 45.0f);
                    linePoints.push_back(ImVec2(px, py));
                }

                // Shaded filled polygon underneath
                for (size_t i = 0; i + 1 < linePoints.size(); ++i) {
                    ImVec2 pA = linePoints[i];
                    ImVec2 pB = linePoints[i + 1];
                    ImVec2 pA_bot(pA.x, chartP1.y - 20.0f);
                    ImVec2 pB_bot(pB.x, chartP1.y - 20.0f);

                    ImU32 fillCol = isUp ? IM_COL32(235, 175, 25, 38) : IM_COL32(215, 65, 45, 35);
                    dl->AddQuadFilled(pA, pB, pB_bot, pA_bot, fillCol);
                    dl->AddLine(pA, pB, isUp ? IM_COL32(245, 185, 35, 255) : IM_COL32(235, 75, 55, 255), 2.2f);
                }

                // Pulsing head point on latest price
                ImVec2 headPt = linePoints.back();
                dl->AddCircleFilled(headPt, 4.5f, IM_COL32(255, 255, 255, 255));
                dl->AddCircle(headPt, 7.5f, isUp ? IM_COL32(245, 185, 35, 200) : IM_COL32(235, 75, 55, 200), 0, 2.0f);

                // Dotted crosshair to right axis
                dl->AddLine(ImVec2(headPt.x, headPt.y), ImVec2(chartP1.x - 70.0f, headPt.y), IM_COL32(245, 185, 35, 180), 1.0f);

                // Live price badge on axis
                char curPStr[32];
                snprintf(curPStr, sizeof(curPStr), "$%.2f", curPrice);
                dl->AddRectFilled(ImVec2(chartP1.x - 68.0f, headPt.y - 10.0f), ImVec2(chartP1.x - 4.0f, headPt.y + 10.0f), isUp ? IM_COL32(24, 140, 60, 240) : IM_COL32(160, 30, 30, 240), 3.0f);
                dl->AddText(ImVec2(chartP1.x - 64.0f, headPt.y - 7.0f), IM_COL32(255, 255, 255, 255), curPStr);
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + chartH + 10.0f);

            // Bottom Split: Order Book (Left half) & Geopolitical News Feed (Right half)
            float subPaneW = (leftPaneW - 12.0f) * 0.5f;
            float subPaneH = paneH - chartH - 48.0f;

            // --- Order Book ---
            ImGui::BeginChild("OrderBookDock", ImVec2(subPaneW, subPaneH), true);
            {
                ImGui::SetWindowFontScale(1.05f);
                ImGui::TextColored(ImVec4(0.92f, 0.75f, 0.25f, 1.0f), "ORDER BOOK DEPTH");
                ImGui::SetWindowFontScale(1.0f);
                ImGui::SameLine(subPaneW - 130.0f);
                ImGui::TextDisabled("SPREAD: $2.40");
                ImGui::Separator();

                ImGui::TextDisabled("%-12s %-10s %-12s", "PRICE (USD)", "AMOUNT", "TOTAL (USD)");

                // Asks (Sellers - Red)
                const auto& asks = market.getAsks();
                for (int i = (int)asks.size() - 1; i >= 0; --i) {
                    const auto& row = asks[i];
                    ImVec2 p0 = ImGui::GetCursorScreenPos();
                    float depthW = std::clamp((row.amount / 30.0f) * subPaneW, 10.0f, subPaneW);
                    dl->AddRectFilled(p0, ImVec2(p0.x + depthW, p0.y + 16.0f), IM_COL32(180, 40, 40, 40));

                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "$%.2f", row.price);
                    ImGui::SameLine(100);
                    ImGui::Text("%d BARS", row.amount);
                    ImGui::SameLine(200);
                    ImGui::TextDisabled("$%.0f", row.total);
                }

                // Mid Spread Line
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.20f, 1.0f), "--- MARKET MID SPREAD: $%.2f ---", curPrice);
                ImGui::Spacing();

                // Bids (Buyers - Green)
                const auto& bids = market.getBids();
                for (size_t i = 0; i < bids.size(); ++i) {
                    const auto& row = bids[i];
                    ImVec2 p0 = ImGui::GetCursorScreenPos();
                    float depthW = std::clamp((row.amount / 30.0f) * subPaneW, 10.0f, subPaneW);
                    dl->AddRectFilled(p0, ImVec2(p0.x + depthW, p0.y + 16.0f), IM_COL32(40, 180, 70, 40));

                    ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "$%.2f", row.price);
                    ImGui::SameLine(100);
                    ImGui::Text("%d BARS", row.amount);
                    ImGui::SameLine(200);
                    ImGui::TextDisabled("$%.0f", row.total);
                }
            }
            ImGui::EndChild();

            ImGui::SameLine(0, 10.0f);

            // --- Geopolitical News & Macro Intel ---
            ImGui::BeginChild("GeopoliticalNewsDock", ImVec2(subPaneW, subPaneH), true);
            {
                ImGui::SetWindowFontScale(1.05f);
                ImGui::TextColored(ImVec4(0.92f, 0.75f, 0.25f, 1.0f), "WARZONE GEOPOLITICAL DISPATCH");
                ImGui::SetWindowFontScale(1.0f);
                ImGui::SameLine(subPaneW - 130.0f);
                ImGui::TextDisabled("LIVE WIRE");
                ImGui::Separator();

                const auto& newsFeed = market.getNewsFeed();
                for (size_t n = 0; n < newsFeed.size(); ++n) {
                    const auto& item = newsFeed[n];
                    ImGui::PushID((int)n + 4000);

                    ImVec4 badgeCol = item.isBullish ? ImVec4(0.25f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
                    ImGui::TextColored(badgeCol, "[%s %s%.1f%%]", item.isBullish ? "BULLISH" : "BEARISH", item.isBullish ? "+" : "", item.impactPct);
                    ImGui::SameLine(0, 6.0f);
                    ImGui::TextDisabled("[%s]", item.timestamp.c_str());

                    ImGui::TextWrapped("%s", item.headline.c_str());
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::PopID();
                }

                // Interactive Volatility Testing Buttons
                ImGui::Spacing();
                ImGui::TextDisabled("MACRO VOLATILITY SIMULATION:");
                if (renderCompetitorButton("+2.5% CAPITAL SURGE", ImVec2(145, 26))) {
                    market.triggerVolatilityEvent(1.025f, "Major defense conglomerate shifts liquidity to physical bullion");
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
                ImGui::SameLine(0, 8.0f);
                if (renderCompetitorButton("-2.5% SUPPLY DUMP", ImVec2(145, 26))) {
                    market.triggerVolatilityEvent(0.975f, "Black market logistics syndicate liquidates bullion reserve");
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        // --- RIGHT PANE: TRADING TERMINAL & VAULT ---
        float rightColX = contentW - rightTerminalW + 16.0f;
        ImGui::SetCursorPos(ImVec2(rightColX, splitY));

        ImGui::BeginChild("ExchangeTradingTerminal", ImVec2(rightTerminalW, paneH), true);
        {
            ImGui::SetWindowFontScale(1.15f);
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.20f, 1.0f), "TRADING TERMINAL & VAULT");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Separator();
            ImGui::Spacing();

            // Operator Balances Card
            if (active) {
                ImVec2 bP0 = ImGui::GetCursorScreenPos();
                ImVec2 bP1(bP0.x + rightTerminalW - 32.0f, bP0.y + 72.0f);
                dl->AddRectFilled(bP0, bP1, IM_COL32(14, 18, 24, 255), 4.0f);
                dl->AddRect(bP0, bP1, IM_COL32(45, 52, 65, 200), 4.0f);

                dl->AddText(ImVec2(bP0.x + 10.0f, bP0.y + 8.0f), IM_COL32(130, 140, 155, 200), "OPERATIVE VAULT BALANCES");
                char cStr[48], gStr[48], vStr[48];
                snprintf(cStr, sizeof(cStr), "$ %d USD", active->cash);
                snprintf(gStr, sizeof(gStr), "%d GOLD BARS", active->goldBars);
                float estVal = active->goldBars * curPrice;
                snprintf(vStr, sizeof(vStr), "VAULT VALUE: $%.0f USD", estVal);

                dl->AddText(ImVec2(bP0.x + 10.0f, bP0.y + 26.0f), IM_COL32(95, 225, 130, 255), cStr);
                dl->AddText(ImVec2(bP0.x + 150.0f, bP0.y + 26.0f), IM_COL32(255, 215, 30, 255), gStr);
                dl->AddText(ImVec2(bP0.x + 10.0f, bP0.y + 48.0f), IM_COL32(180, 190, 205, 220), vStr);

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 78.0f);
            }

            ImGui::Spacing();

            // Buy / Sell Mode Tabs
            float tabBtnW = (rightTerminalW - 38.0f) * 0.5f;
            if (renderCompetitorButton("BUY GOLD (USD -> GOLD)", ImVec2(tabBtnW, 36), m_exchangeIsBuyTab, m_exchangeIsBuyTab, ImDrawFlags_RoundCornersLeft)) {
                m_exchangeIsBuyTab = true;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }

            ImGui::SameLine(0, 6.0f);

            if (renderCompetitorButton("SELL GOLD (GOLD -> USD)", ImVec2(tabBtnW, 36), !m_exchangeIsBuyTab, !m_exchangeIsBuyTab, ImDrawFlags_RoundCornersRight)) {
                m_exchangeIsBuyTab = false;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Quick Amount Buttons
            ImGui::TextDisabled("QUICK QUANTITY (BARS):");
            if (m_exchangeIsBuyTab) {
                if (ImGui::Button("+1 BAR", ImVec2(70, 26))) m_exchangeQuantity = 1;
                ImGui::SameLine(0, 4);
                if (ImGui::Button("+5 BARS", ImVec2(70, 26))) m_exchangeQuantity = 5;
                ImGui::SameLine(0, 4);
                if (ImGui::Button("+10 BARS", ImVec2(75, 26))) m_exchangeQuantity = 10;
                ImGui::SameLine(0, 4);
                if (ImGui::Button("MAX BUY", ImVec2(75, 26))) {
                    if (active) m_exchangeQuantity = std::max(1, (int)(active->cash / curPrice));
                }
            } else {
                if (ImGui::Button("1 BAR", ImVec2(70, 26))) m_exchangeQuantity = 1;
                ImGui::SameLine(0, 4);
                if (ImGui::Button("5 BARS", ImVec2(70, 26))) m_exchangeQuantity = 5;
                ImGui::SameLine(0, 4);
                if (ImGui::Button("10 BARS", ImVec2(75, 26))) m_exchangeQuantity = 10;
                ImGui::SameLine(0, 4);
                if (ImGui::Button("SELL ALL", ImVec2(75, 26))) {
                    if (active) m_exchangeQuantity = std::max(1, active->goldBars);
                }
            }

            ImGui::Spacing();
            ImGui::SliderInt("##ExQtySlider", &m_exchangeQuantity, 1, m_exchangeIsBuyTab ? 100 : std::max(1, active ? active->goldBars : 1));
            m_exchangeQuantity = std::max(1, m_exchangeQuantity);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Execution Breakdown Calculations
            int totalCostUsd = static_cast<int>(std::ceil(m_exchangeQuantity * curPrice));
            int totalPayoutUsd = static_cast<int>(std::floor(m_exchangeQuantity * curPrice * 0.9975f));

            ImGui::TextDisabled("EXECUTION ESTIMATE:");
            ImGui::BulletText("Order Type: %s", m_exchangeIsBuyTab ? "MARKET BUY (USD -> GOLD)" : "MARKET SELL (GOLD -> USD)");
            ImGui::BulletText("Quantity: %d BARS (%.1f kg 999.9 Fine Au)", m_exchangeQuantity, m_exchangeQuantity * 1.0f);
            ImGui::BulletText("Spot Price: $%.2f USD / Bar", curPrice);
            ImGui::BulletText("Broker Commission: $0.00 USD (Zero-Fee)");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (m_exchangeIsBuyTab) {
                ImGui::SetWindowFontScale(1.1f);
                ImGui::TextColored(ImVec4(0.95f, 0.92f, 0.88f, 1.0f), "TOTAL CASH REQUIRED:");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "$ %d USD", totalCostUsd);
                ImGui::SetWindowFontScale(1.0f);

                bool canAfford = active ? (active->cash >= totalCostUsd) : false;
                if (!canAfford) ImGui::BeginDisabled();

                char buyBtnStr[64];
                snprintf(buyBtnStr, sizeof(buyBtnStr), "CONFIRM BUY (%d BARS)", m_exchangeQuantity);
                if (renderCompetitorButton(buyBtnStr, ImVec2(-1, 44), false, true)) {
                    if (active) {
                        market.buyGoldBars(*active, m_exchangeQuantity);
                    }
                }
                if (!canAfford) {
                    ImGui::EndDisabled();
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "INSUFFICIENT FUNDS (Need $%d more)", totalCostUsd - (active ? active->cash : 0));
                }
            } else {
                ImGui::SetWindowFontScale(1.1f);
                ImGui::TextColored(ImVec4(0.95f, 0.92f, 0.88f, 1.0f), "TOTAL CASH RECEIVED:");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "$ %d USD", totalPayoutUsd);
                ImGui::SetWindowFontScale(1.0f);

                bool hasBars = active ? (active->goldBars >= m_exchangeQuantity) : false;
                if (!hasBars) ImGui::BeginDisabled();

                char sellBtnStr[64];
                snprintf(sellBtnStr, sizeof(sellBtnStr), "CONFIRM SELL (%d BARS)", m_exchangeQuantity);
                if (renderCompetitorButton(sellBtnStr, ImVec2(-1, 44), false, true)) {
                    if (active) {
                        market.sellGoldBars(*active, m_exchangeQuantity);
                    }
                }
                if (!hasBars) {
                    ImGui::EndDisabled();
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "INSUFFICIENT BARS IN VAULT (Have %d, need %d)", active ? active->goldBars : 0, m_exchangeQuantity);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Recent Executed Orders Feed
            ImGui::TextDisabled("LIVE EXECUTED TRADES:");
            ImGui::BeginChild("RecentTradesScroll", ImVec2(0, 0), false);
            {
                const auto& trades = market.getRecentTrades();
                for (size_t t = 0; t < trades.size(); ++t) {
                    const auto& tr = trades[t];
                    ImVec4 col = tr.isBuy ? ImVec4(0.35f, 0.95f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f);

                    if (tr.isPlayer) {
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.20f, 1.0f), "[YOU]");
                        ImGui::SameLine();
                    }
                    ImGui::TextColored(col, "%s %d BARS @ $%.2f", tr.isBuy ? "BOUGHT" : "SOLD", tr.goldBars, tr.pricePerBar);
                    ImGui::SameLine(rightTerminalW - 90.0f);
                    ImGui::TextDisabled("%s", tr.timestamp.c_str());
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::End();
    }
    ImGui::PopStyleColor(2);
}

// =========================================================================
// 2B. MAIN MENU GAME LOBBY (TOP NAVBAR & UNOBSTRUCTED 3D HIDEOUT)
// =========================================================================
void UIManager::renderTacticalMap(const Player& player, const World& world, const MissionSystem& missions) {
    if (!m_mapOpen && m_currentTab != MainMenuTab::TacticalMap) return;

    ImGuiIO& io = ImGui::GetIO();
    bool inMainMenu = (m_gameState == GameState::MainMenu);

    float winX = inMainMenu ? 24.0f : (io.DisplaySize.x * 0.5f - 490.0f);
    float winY = inMainMenu ? 64.0f : (io.DisplaySize.y * 0.5f - 370.0f);
    float winW = inMainMenu ? (io.DisplaySize.x - 48.0f) : 980.0f;
    float winH = inMainMenu ? (io.DisplaySize.y - 78.0f) : 740.0f;

    ImGui::SetNextWindowPos(ImVec2(winX, winY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 0.98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.22f, 0.28f, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));

    struct HexInfo {
        uint32_t sessionId;
        const char* regionId;
        const char* name;
        const char* theaterRole;
        const char* mapFile;
        const char* tacticalOverview;
        int faction; // 0: Coalition (Blue), 1: Crimson (Red), 2: Contested
        float controlProgress;
        int activeFobs;
        int hotness; // 0..100
        int col; // 0=West, 1=East
        int row; // 0=North, 1=Central, 2=South
    };

    static const HexInfo s_hexTheaters[6] = {
        { 101, "sumy", "1. SUMY REGION", "COALITION AIRBASE // FORESTS", "sumy.vmap", "Dense pine forests and Coalition Northern Airbase logistics hub. High infantry concealment and supply staging.", 0, 0.85f, 4, 30, 0, 0 },
        { 102, "kharkiv", "2. KHARKIV REGION", "HIGHWAYS // MECHANIZED STAGING", "kharkiv.vmap", "M-03 Highway corridor, industrial tractor depots, and sunflower windbreaks. Combined-arms transport artery.", 0, 0.65f, 7, 65, 1, 0 },
        { 103, "donetsk", "3. DONETSK URBAN", "INDUSTRIAL RUINS // CQB FORTRESS", "donetsk.vmap", "Dense urban meat-grinder, coking plants, blast furnaces, and slag heaps. Brutal room-to-room CQB engagements.", 2, 0.50f, 12, 98, 0, 1 },
        { 104, "luhansk", "4. LUHANSK RIDGES", "ARTILLERY RIDGES // HILLS", "luhansk.vmap", "Rolling chalk ridges, open valleys, and prepared trench belts. Heavy counter-battery artillery duels.", 1, 0.60f, 6, 75, 1, 1 },
        { 105, "zaporizhzhia", "5. ZAPORIZHZHIA STEPPE", "STEPPE ARMOR // TANK DITCHES", "zaporizhzhia.vmap", "Expansive agricultural steppe, anti-tank ditch systems, and minefield belts. Armored clashes and ATGM strikes.", 2, 0.50f, 5, 80, 0, 2 },
        { 106, "kherson", "6. KHERSON DELTA", "CRIMSON AIRBASE // DNIPRO DELTA", "kherson.vmap", "Strategic Dnipro River Delta, pontoon assault crossings, and Crimson Southern Airbase fortress safezone.", 1, 0.90f, 3, 40, 1, 2 }
    };

    static int s_selectedHexIndex = 1; // Default to Kharkiv (Session 102)

    bool open = true;
    if (ImGui::Begin("##TacticalFrontlineGISMap", &open, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        float leftPaneW = inMainMenu ? 370.0f : 300.0f;
        float mapCanvasW = winW - leftPaneW - 50.0f;
        float mapCanvasH = winH - 36.0f;

        // ================= LEFT COLUMN: THEATER INTEL & SELECTED HEX BRIEFING =================
        ImGui::BeginChild("MapIntelPane", ImVec2(leftPaneW, mapCanvasH), false);

        if (m_fontHeading) ImGui::PushFont(m_fontHeading);
        ImGui::TextColored(ImVec4(0.96f, 0.98f, 1.0f, 1.0f), "THEATER INTEL");
        if (m_fontHeading) ImGui::PopFont();

        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        ImGui::TextDisabled("CONTINENTAL WARZONE // 6-HEX COMBINED-ARMS (96 km²)");
        if (m_fontSmall) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const HexInfo& curHex = s_hexTheaters[s_selectedHexIndex];

        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "%s", curHex.name);
        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        ImGui::TextColored(ImVec4(0.65f, 0.75f, 0.85f, 1.0f), "ROLE: %s", curHex.theaterRole);
        ImGui::TextDisabled("MAP FILE: %s | SESSION #%d", curHex.mapFile, curHex.sessionId);
        if (m_fontSmall) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("TACTICAL ENVIRONMENT");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + leftPaneW - 15.0f);
        ImGui::TextColored(ImVec4(0.85f, 0.88f, 0.92f, 1.0f), "%s", curHex.tacticalOverview);
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Hex Sector Balance
        ImGui::TextDisabled("TERRITORIAL CONTROL");
        ImGui::Spacing();
        ImVec2 bP0 = ImGui::GetCursorScreenPos();
        float barW = leftPaneW - 15.0f;
        float barH = 10.0f;
        float fracA = (curHex.faction == 0) ? curHex.controlProgress : ((curHex.faction == 1) ? (1.0f - curHex.controlProgress) : 0.50f);
        dl->AddRectFilled(bP0, ImVec2(bP0.x + barW, bP0.y + barH), IM_COL32(220, 55, 55, 255), 2.0f);
        dl->AddRectFilled(bP0, ImVec2(bP0.x + barW * fracA, bP0.y + barH), IM_COL32(35, 130, 240, 255), 2.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 16.0f);

        if (m_fontSmall) ImGui::PushFont(m_fontSmall);
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "COALITION %d%%", (int)(fracA * 100.0f));
        ImGui::SameLine(barW - 90.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "CRIMSON %d%%", (int)((1.0f - fracA) * 100.0f));
        if (m_fontSmall) ImGui::PopFont();

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.85f, 1.0f), "Persistent FOBs: %d | Threat Index: %d/100", curHex.activeFobs, curHex.hotness);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (missions.hasActiveContract()) {
            const auto& act = missions.getActiveContract();
            ImGui::TextDisabled("ACTIVE CONTRACT");
            ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "%s", act.title.c_str());
            if (m_fontSmall) ImGui::PushFont(m_fontSmall);
            ImGui::TextDisabled("Objective active in frontline battleground.");
            if (m_fontSmall) ImGui::PopFont();
        }

        if (inMainMenu) {
            ImGui::SetCursorPosY(mapCanvasH - 52.0f);
            std::string btnText = std::string(">  DEPLOY TO ") + curHex.name;
            if (renderCompetitorButton(btnText.c_str(), ImVec2(leftPaneW - 10.0f, 42.0f), false, true)) {
                const auto& shard = NetworkEngine::get().getActiveShard();
                if (shard.isOnline) {
                    uint8_t pFaction = (player.getFaction() == Faction::FactionB) ? 1 : 0;
                    NetworkEngine::get().setDesiredSessionId(curHex.sessionId);
                    NetworkEngine::get().setCurrentMapName(curHex.regionId);
                    NetworkEngine::get().joinSession(curHex.sessionId, pFaction);
                    m_matchmakingPlaylist = 2; // Open World MMO
                    m_isDeploying = true;
                    m_deploymentTimer = 0.0f;
                    AudioEngine::get().playSound(SoundType::DeployHorn);
                } else {
                    m_serverOfflineModalOpen = true;
                    m_serverOfflineModalMsg = "Dedicated server shard is offline. Start MercenaryServer.exe or enter Training Mode.";
                    AudioEngine::get().playSound(SoundType::TacticalClick);
                }
            }
        } else {
            ImGui::SetCursorPosY(mapCanvasH - 42.0f);
            if (ImGui::Button("CLOSE MAP [M]", ImVec2(leftPaneW - 10.0f, 36.0f))) {
                m_mapOpen = false;
            }
        }

        ImGui::EndChild();

        // ================= RIGHT COLUMN: INTERACTIVE 6-HEX THEATER GRID =================
        ImGui::SameLine(0, 16.0f);
        ImVec2 mapP0 = ImGui::GetCursorScreenPos();
        ImVec2 mapP1 = ImVec2(mapP0.x + mapCanvasW, mapP0.y + mapCanvasH);

        // Dark Military GIS Canvas Base
        dl->AddRectFilled(mapP0, mapP1, IM_COL32(10, 12, 16, 255), 4.0f);
        dl->AddRect(mapP0, mapP1, IM_COL32(28, 34, 44, 255), 4.0f, 0, 1.0f);

        // Grid lines
        for (int i = 1; i < 6; ++i) {
            float gx = mapP0.x + (mapCanvasW * i / 6.0f);
            dl->AddLine(ImVec2(gx, mapP0.y), ImVec2(gx, mapP1.y), IM_COL32(18, 24, 32, 120), 1.0f);
        }
        for (int j = 1; j < 6; ++j) {
            float gy = mapP0.y + (mapCanvasH * j / 6.0f);
            dl->AddLine(ImVec2(mapP0.x, gy), ImVec2(mapP1.x, gy), IM_COL32(18, 24, 32, 120), 1.0f);
        }

        // Active Contested Frontline Line across center
        float frontLineY = mapP0.y + mapCanvasH * 0.50f;
        dl->AddLine(ImVec2(mapP0.x, frontLineY), ImVec2(mapP1.x, frontLineY), IM_COL32(235, 120, 30, 200), 2.5f);
        dl->AddText(ImVec2(mapP0.x + 12.0f, frontLineY - 18.0f), IM_COL32(245, 140, 45, 230), "[ ★ ACTIVE CONTESTED FRONTLINE // SIVERSKY ARTERY ]");

        // Calculate card dimensions: 2 Columns x 3 Rows
        float cardPadX = 14.0f;
        float cardPadY = 12.0f;
        float cardW = (mapCanvasW - cardPadX * 3.0f) * 0.5f;
        float cardH = (mapCanvasH - cardPadY * 4.0f) / 3.0f;

        ImVec2 mousePos = io.MousePos;

        for (int i = 0; i < 6; ++i) {
            const auto& hex = s_hexTheaters[i];
            float cX = mapP0.x + cardPadX + hex.col * (cardW + cardPadX);
            float cY = mapP0.y + cardPadY + hex.row * (cardH + cardPadY);
            ImVec2 cP0(cX, cY);
            ImVec2 cP1(cX + cardW, cY + cardH);

            bool isHovered = (mousePos.x >= cP0.x && mousePos.x <= cP1.x && mousePos.y >= cP0.y && mousePos.y <= cP1.y);
            bool isSelected = (s_selectedHexIndex == i);

            if (isHovered && io.MouseClicked[0]) {
                s_selectedHexIndex = i;
                AudioEngine::get().playSound(SoundType::TacticalClick);
            }

            // Background & Border
            ImU32 bgCol = isSelected ? IM_COL32(24, 32, 46, 240) : (isHovered ? IM_COL32(18, 24, 34, 220) : IM_COL32(14, 18, 24, 200));
            ImU32 borderCol = isSelected ? IM_COL32(235, 150, 40, 255) : (isHovered ? IM_COL32(70, 140, 220, 200) : IM_COL32(32, 40, 52, 180));
            float borderThick = isSelected ? 2.5f : 1.0f;

            dl->AddRectFilled(cP0, cP1, bgCol, 4.0f);
            dl->AddRect(cP0, cP1, borderCol, 4.0f, 0, borderThick);

            // Faction Indicator Pill
            ImU32 facPillCol = (hex.faction == 0) ? IM_COL32(35, 130, 240, 255) : ((hex.faction == 1) ? IM_COL32(220, 45, 45, 255) : IM_COL32(235, 140, 30, 255));
            dl->AddRectFilled(ImVec2(cP0.x + 8, cP0.y + 8), ImVec2(cP0.x + 14, cP1.y - 8), facPillCol, 2.0f);

            // Hex Name & Session Badge
            dl->AddText(ImVec2(cP0.x + 22, cP0.y + 10), isSelected ? IM_COL32(255, 220, 110, 255) : IM_COL32(230, 235, 245, 255), hex.name);
            char sessBuf[32];
            std::snprintf(sessBuf, sizeof(sessBuf), "SESSION #%d", hex.sessionId);
            ImVec2 sSize = ImGui::CalcTextSize(sessBuf);
            dl->AddText(ImVec2(cP1.x - sSize.x - 12, cP0.y + 10), IM_COL32(120, 140, 165, 220), sessBuf);

            // Role Subtitle
            dl->AddText(ImVec2(cP0.x + 22, cP0.y + 28), IM_COL32(150, 165, 185, 240), hex.theaterRole);

            // Hotness & FOB count
            char statBuf[64];
            std::snprintf(statBuf, sizeof(statBuf), "Threat: %d/100 | FOBs: %d", hex.hotness, hex.activeFobs);
            dl->AddText(ImVec2(cP0.x + 22, cP1.y - 24), (hex.hotness > 80) ? IM_COL32(245, 90, 50, 255) : IM_COL32(100, 180, 120, 255), statBuf);

            // Selected Checkmark
            if (isSelected) {
                dl->AddText(ImVec2(cP1.x - 110, cP1.y - 24), IM_COL32(235, 180, 50, 255), "[ SELECTED ]");
            }
        }

        // Live Player Blip if in-game
        if (!inMainMenu) {
            float px = mapP0.x + mapCanvasW * 0.75f;
            float py = mapP0.y + mapCanvasH * 0.25f;
            dl->AddCircleFilled(ImVec2(px, py), 7.0f, IM_COL32(255, 255, 255, 255));
            dl->AddCircle(ImVec2(px, py), 14.0f, IM_COL32(235, 140, 40, 220), 0, 2.0f);
            dl->AddText(ImVec2(px + 12, py - 6), IM_COL32(255, 255, 255, 255), "OPERATOR");
        }

        ImGui::End();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

} // namespace Mercenary

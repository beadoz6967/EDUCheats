#include "menu_ui.hpp"
#include "../theme.hpp"
#include <imgui.h>
#include <cstdio>

namespace menu_ui {

namespace {

// Renders the EDUCheats brand block — wordmark on the left, tri-color
// stripe stack on the right, subtitle underneath. Matches the institutional
// educanet visual identity (white wordmark on dark, stacked stripes).
void BrandHeader() {
    constexpr float kBlockH         = 60.f;
    constexpr float kTitleFontScale = 1.9f;
    constexpr float kSubFontScale   = 0.85f;
    constexpr float kStripeW        = 34.f;
    constexpr float kStripeH        = 10.f;
    constexpr float kStripeGap      = 4.f;
    constexpr float kRightPad       = 4.f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0     = ImGui::GetCursorScreenPos();
    float  availW = ImGui::GetContentRegionAvail().x;

    // Background bar — pure black behind the brand, matching the educanet
    // wordmark plate
    dl->AddRectFilled(p0, { p0.x + availW, p0.y + kBlockH },
                      IM_COL32(0, 0, 0, 230), 4.f);

    // Tri-color stripe stack on the right side
    float stripesX = p0.x + availW - kStripeW - kRightPad;
    float stripesY = p0.y + (kBlockH - (kStripeH * 3 + kStripeGap * 2)) * 0.5f;
    ImU32 stripeColors[3] = { theme::kAccentRed, theme::kAccentYellow, theme::kAccentBlue };
    for (int i = 0; i < 3; ++i) {
        float y = stripesY + i * (kStripeH + kStripeGap);
        dl->AddRectFilled({ stripesX, y },
                          { stripesX + kStripeW, y + kStripeH },
                          stripeColors[i], 1.5f);
    }

    // Wordmark — "EDUCheats", large bold-style white
    ImFont* font = ImGui::GetFont();
    float titleSize = ImGui::GetFontSize() * kTitleFontScale;
    float subSize   = ImGui::GetFontSize() * kSubFontScale;

    const char* title = "EDUCheats";
    const char* sub   = "External Overlay  //  v1.2";

    ImVec2 titlePos{ p0.x + 14.f, p0.y + 6.f };
    ImVec2 subPos  { p0.x + 14.f, p0.y + 6.f + titleSize + 2.f };

    dl->AddText(font, titleSize, titlePos, theme::kWhite, title);
    dl->AddText(font, subSize,   subPos,   theme::kMuted, sub);

    ImGui::Dummy({ availW, kBlockH + 6.f });
}

void StatusRow(const char* label, const char* value, ImU32 valueColor) {
    ImGui::TextColored(theme::ToVec4(theme::kMuted), "%s", label);
    ImGui::SameLine(160.f);
    ImGui::TextColored(theme::ToVec4(valueColor), "%s", value);
}

bool ToggleRow(const char* label, std::atomic<bool>& val) {
    bool b = val.load();
    bool changed = ImGui::Checkbox(label, &b);
    if (changed) val.store(b);
    return changed;
}

} // namespace

void Draw(ESPConfig& cfg, GameState& state, Config& persist,
          bool& visibleInOut) {
    if (!visibleInOut) return;

    ImGui::SetNextWindowSize(ImVec2(440, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar;

    bool open = true;
    if (!ImGui::Begin("##educheats_main", &open, flags)) {
        if (!open) visibleInOut = false;
        ImGui::End();
        return;
    }

    BrandHeader();

    // Status block ------------------------------------------------------------
    ImGui::TextColored(theme::ToVec4(theme::kAccentYellow), "STATUS");
    ImGui::Separator();

    {
        float d = state.nearestEnemyDist.load();
        char buf[32];
        if (d >= 0.f) std::snprintf(buf, sizeof(buf), "%.1f m", d);
        else          std::snprintf(buf, sizeof(buf), "no visible enemy");
        StatusRow("Nearest enemy", buf, d >= 0.f ? theme::kWhite : theme::kMuted);
    }
    {
        int n = state.entityCount.load();
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", n);
        StatusRow("Players tracked", buf, theme::kWhite);
    }
    {
        bool mok = state.matrixOk.load();
        StatusRow("View matrix",
                  mok ? "OK" : "stale / zero",
                  mok ? theme::kHealthHigh : theme::kAccentRed);
    }

    ImGui::Dummy({ 0, 8.f });

    // Toggles -----------------------------------------------------------------
    ImGui::TextColored(theme::ToVec4(theme::kAccentYellow), "ESP");
    ImGui::Separator();

    bool dirty = false;
    dirty |= ToggleRow("Master enable",  cfg.enabled);
    dirty |= ToggleRow("Name",           cfg.nameESP);
    dirty |= ToggleRow("Health bar",     cfg.healthBar);
    dirty |= ToggleRow("HP numbers",     cfg.hpNumbers);
    dirty |= ToggleRow("Distance",       cfg.distanceESP);

    int colorMode = cfg.colorMode.load();
    if (ImGui::RadioButton("Team color", colorMode == 0)) { cfg.colorMode.store(0); dirty = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Enemy red",  colorMode == 1)) { cfg.colorMode.store(1); dirty = true; }

    if (dirty) persist.Save(cfg);

    ImGui::Dummy({ 0, 8.f });

    // Footer ------------------------------------------------------------------
    ImGui::TextColored(theme::ToVec4(theme::kMuted),
                       "[INSERT] toggle menu    [END] exit");

    ImGui::End();
    if (!open) visibleInOut = false;
}

} // namespace menu_ui

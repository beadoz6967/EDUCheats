#include "menu_ui.hpp"
#include "../theme.hpp"
#include <imgui.h>
#include <cstdio>

namespace menu_ui {
namespace {

void BrandHeader() {
    constexpr float kBlockH         = 64.f;
    constexpr float kTitleFontScale = 1.85f;
    constexpr float kSubFontScale   = 0.82f;
    constexpr float kStripeW        = 32.f;
    constexpr float kStripeH        = 9.f;
    constexpr float kStripeGap      = 4.f;
    constexpr float kRightPad       = 6.f;

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2      p0  = ImGui::GetCursorScreenPos();
    float       aw  = ImGui::GetContentRegionAvail().x;

    // Background plate
    dl->AddRectFilled(p0, { p0.x + aw, p0.y + kBlockH },
                      IM_COL32(0x00, 0x00, 0x00, 0xE8), 4.f);

    // Thin top accent line
    dl->AddRectFilled(p0, { p0.x + aw, p0.y + 2.f }, theme::kAccentYellow);

    // Tri-color stripe stack — right side
    float sx = p0.x + aw - kStripeW - kRightPad;
    float sy = p0.y + (kBlockH - (kStripeH * 3 + kStripeGap * 2)) * 0.5f;
    const ImU32 stripes[3] = { theme::kAccentRed, theme::kAccentYellow, theme::kAccentBlue };
    for (int i = 0; i < 3; ++i) {
        float fy = sy + i * (kStripeH + kStripeGap);
        dl->AddRectFilled({ sx, fy }, { sx + kStripeW, fy + kStripeH }, stripes[i], 2.f);
    }

    // Wordmark
    ImFont* font      = ImGui::GetFont();
    float   titleSz   = ImGui::GetFontSize() * kTitleFontScale;
    float   subSz     = ImGui::GetFontSize() * kSubFontScale;

    dl->AddText(font, titleSz, { p0.x + 14.f, p0.y + 6.f },
                theme::kWhite, "EDUCheats");
    dl->AddText(font, subSz,   { p0.x + 14.f, p0.y + 6.f + titleSz + 2.f },
                theme::kMuted, "External Overlay  //  v1.3");

    ImGui::Dummy({ aw, kBlockH + 4.f });
}

// Section header: left accent bar + uppercase yellow label
void SectionHeader(const char* label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      p  = ImGui::GetCursorScreenPos();
    float       lh = ImGui::GetTextLineHeight();

    dl->AddRectFilled(p, { p.x + 3.f, p.y + lh }, theme::kAccentYellow);
    ImGui::SetCursorScreenPos({ p.x + 10.f, p.y });
    ImGui::TextColored(theme::ToVec4(theme::kAccentYellow), "%s", label);
    ImGui::Dummy({ 0.f, 3.f });
}

// Status row: colored dot indicator + label + right-aligned value
void StatusRow(const char* label, const char* value, ImU32 dotColor) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      p  = ImGui::GetCursorScreenPos();
    float       lh = ImGui::GetTextLineHeight();

    dl->AddCircleFilled({ p.x + 5.f, p.y + lh * 0.5f }, 4.f, IM_COL32(0, 0, 0, 160));
    dl->AddCircleFilled({ p.x + 5.f, p.y + lh * 0.5f }, 3.f, dotColor);

    ImGui::SetCursorScreenPos({ p.x + 14.f, p.y });
    ImGui::TextColored(theme::ToVec4(theme::kMuted), "%s", label);
    ImGui::SameLine(160.f);
    ImGui::TextColored(theme::ToVec4(theme::kWhite), "%s", value);
}

// Checkbox toggle — returns true if value changed
bool ToggleRow(const char* label, std::atomic<bool>& val) {
    bool b       = val.load();
    bool changed = ImGui::Checkbox(label, &b);
    if (changed) val.store(b);
    return changed;
}

// Swatch button: highlighted when active, dim otherwise
bool SwatchButton(const char* label, bool active, ImU32 activeColor) {
    ImVec4 bg = active
        ? theme::ToVec4(activeColor)
        : ImVec4(0.08f, 0.13f, 0.26f, 1.f);
    ImVec4 hv = active
        ? theme::ToVec4(activeColor)
        : ImVec4(0.14f, 0.22f, 0.36f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Button,        bg);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hv);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  theme::ToVec4(activeColor));
    bool clicked = ImGui::SmallButton(label);
    ImGui::PopStyleColor(3);
    return clicked && !active;
}

} // namespace

void Draw(ESPConfig& cfg, AimbotConfig& ab, GameState& state, Config& persist, bool& visibleInOut) {
    if (!visibleInOut) return;

    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar    |
        ImGuiWindowFlags_NoCollapse    |
        ImGuiWindowFlags_NoScrollbar   |
        ImGuiWindowFlags_AlwaysAutoResize;

    bool open = true;
    if (!ImGui::Begin("##educheats_main", &open, flags)) {
        if (!open) visibleInOut = false;
        ImGui::End();
        return;
    }

    BrandHeader();

    // STATUS -----------------------------------------------------------------------
    ImGui::Dummy({ 0.f, 2.f });
    SectionHeader("STATUS");

    {
        float d = state.nearestEnemyDist.load();
        char  buf[32];
        ImU32 dot;
        if (d >= 0.f) {
            std::snprintf(buf, sizeof(buf), "%.1f m", d);
            dot = d < 12.f ? theme::kAccentRed
                : d < 30.f ? theme::kAccentYellow
                           : theme::kHealthHigh;
        } else {
            std::snprintf(buf, sizeof(buf), "none");
            dot = theme::kMuted;
        }
        StatusRow("Nearest enemy", buf, dot);
    }
    {
        int  n   = state.entityCount.load();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d / 64", n);
        StatusRow("Players visible", buf, n > 0 ? theme::kAccentYellow : theme::kMuted);
    }
    {
        bool mok = state.matrixOk.load();
        StatusRow("View matrix", mok ? "active" : "stale",
                  mok ? theme::kHealthHigh : theme::kAccentRed);
    }
    {
        float fps = ImGui::GetIO().Framerate;
        char  buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f fps", fps);
        ImU32 dot = fps >= 60.f ? theme::kHealthHigh
                  : fps >= 30.f ? theme::kAccentYellow
                                : theme::kAccentRed;
        StatusRow("Overlay fps", buf, dot);
    }

    ImGui::Dummy({ 0.f, 7.f });

    // ESP --------------------------------------------------------------------------
    SectionHeader("ESP");

    bool dirty = false;
    dirty |= ToggleRow("Master enable",  cfg.enabled);
    dirty |= ToggleRow("Name",           cfg.nameESP);
    dirty |= ToggleRow("Health bar",     cfg.healthBar);
    dirty |= ToggleRow("HP numbers",     cfg.hpNumbers);
    dirty |= ToggleRow("Distance",       cfg.distanceESP);
    dirty |= ToggleRow("Skeleton",       cfg.skeleton);

    ImGui::Dummy({ 0.f, 4.f });

    // Box color mode — swatch buttons
    {
        int cm = cfg.colorMode.load();
        ImGui::TextColored(theme::ToVec4(theme::kMuted), "Box color");
        ImGui::SameLine(160.f);
        if (SwatchButton(" Team  ", cm == 0, theme::kTeamBox))  { cfg.colorMode.store(0); dirty = true; }
        ImGui::SameLine(0.f, 4.f);
        if (SwatchButton(" Enemy ", cm == 1, theme::kEnemyBox)) { cfg.colorMode.store(1); dirty = true; }
    }

    // Custom color / thickness controls
    {
        ImGui::Dummy({ 0.f, 6.f });
        ImGui::TextColored(theme::ToVec4(theme::kMuted), "Custom colors");

        // Box color
        ImVec4 defBox = theme::ToVec4(theme::kEnemyBox);
        uint32_t packedBox = cfg.boxColor.load();
        float boxCol[4] = { defBox.x, defBox.y, defBox.z, defBox.w };
        if (packedBox != 0u) {
            boxCol[0] = ((packedBox >> IM_COL32_R_SHIFT) & 0xFF) / 255.f;
            boxCol[1] = ((packedBox >> IM_COL32_G_SHIFT) & 0xFF) / 255.f;
            boxCol[2] = ((packedBox >> IM_COL32_B_SHIFT) & 0xFF) / 255.f;
            boxCol[3] = ((packedBox >> IM_COL32_A_SHIFT) & 0xFF) / 255.f;
        }
        if (ImGui::ColorEdit4("Box color", boxCol)) {
            uint32_t nc = IM_COL32((int)(boxCol[0]*255.f),(int)(boxCol[1]*255.f),(int)(boxCol[2]*255.f),(int)(boxCol[3]*255.f));
            cfg.boxColor.store(nc);
            dirty = true;
        }

        // Skeleton color
        ImVec4 defSk = theme::ToVec4(theme::kEnemyBox);
        uint32_t packedSk = cfg.skeletonColor.load();
        float skCol[4] = { defSk.x, defSk.y, defSk.z, defSk.w };
        if (packedSk != 0u) {
            skCol[0] = ((packedSk >> IM_COL32_R_SHIFT) & 0xFF) / 255.f;
            skCol[1] = ((packedSk >> IM_COL32_G_SHIFT) & 0xFF) / 255.f;
            skCol[2] = ((packedSk >> IM_COL32_B_SHIFT) & 0xFF) / 255.f;
            skCol[3] = ((packedSk >> IM_COL32_A_SHIFT) & 0xFF) / 255.f;
        }
        if (ImGui::ColorEdit4("Skeleton color", skCol)) {
            uint32_t nc = IM_COL32((int)(skCol[0]*255.f),(int)(skCol[1]*255.f),(int)(skCol[2]*255.f),(int)(skCol[3]*255.f));
            cfg.skeletonColor.store(nc);
            dirty = true;
        }

        // Thickness and joint radius
        float thick = cfg.skeletonThick.load();
        if (ImGui::SliderFloat("Skeleton thickness", &thick, 0.5f, 6.0f)) { cfg.skeletonThick.store(thick); dirty = true; }
        float jr = cfg.jointRadius.load();
        if (ImGui::SliderFloat("Joint radius", &jr, 1.0f, 10.0f)) { cfg.jointRadius.store(jr); dirty = true; }
    }

    if (dirty) persist.Save(cfg, ab);

    ImGui::Dummy({ 0.f, 7.f });

    // AIMBOT -----------------------------------------------------------------------
    SectionHeader("AIMBOT");

    bool abDirty = false;
    abDirty |= ToggleRow("Enable aimbot", ab.enabled);
    abDirty |= ToggleRow("Rage mode",     ab.rageMode);

    {
        bool rage = ab.rageMode.load();

        if (!rage) {
            float fov = ab.fov.load();
            if (ImGui::SliderFloat("FOV (deg)", &fov, 1.f, 30.f))
                { ab.fov.store(fov); abDirty = true; }

            float smooth = ab.smooth.load();
            if (ImGui::SliderFloat("Smooth", &smooth, 1.f, 20.f))
                { ab.smooth.store(smooth); abDirty = true; }
        } else {
            ImGui::TextColored(theme::ToVec4(theme::kMuted), "FOV");
            ImGui::SameLine(160.f);
            ImGui::TextColored(theme::ToVec4(theme::kAccentRed), "360 (rage)");
            ImGui::TextColored(theme::ToVec4(theme::kMuted), "Smooth");
            ImGui::SameLine(160.f);
            ImGui::TextColored(theme::ToVec4(theme::kAccentRed), "instant (rage)");
        }

        // Bone target: Head or Chest
        int bone = ab.boneTarget.load();
        const char* boneNames[] = { "Head (7)", "Chest (4)" };
        int boneIdx = (bone == 4) ? 1 : 0;
        if (ImGui::Combo("Target bone", &boneIdx, boneNames, 2)) {
            ab.boneTarget.store(boneIdx == 1 ? 4 : 7);
            abDirty = true;
        }

        ImGui::TextColored(theme::ToVec4(theme::kMuted), "Hotkey");
        ImGui::SameLine(160.f);
        ImGui::TextColored(theme::ToVec4(theme::kWhite), "MOUSE1 (hold)");
    }

    if (abDirty) persist.Save(cfg, ab);

    ImGui::Dummy({ 0.f, 8.f });

    // FOOTER -----------------------------------------------------------------------
    ImGui::Separator();
    ImGui::Dummy({ 0.f, 2.f });
    ImGui::TextColored(theme::ToVec4(theme::kMuted),
                       "[INSERT] toggle menu    [END] exit");

    ImGui::End();
    if (!open) visibleInOut = false;
}

} // namespace menu_ui

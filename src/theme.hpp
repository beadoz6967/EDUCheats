#pragma once
#include <imgui.h>

// Educanet brand palette. Institutional blue base, white text,
// red/yellow/blue accent stripe used for status, dividers, and emphasis.
namespace theme {

constexpr ImU32 kBgNavy       = IM_COL32(0x0B, 0x1C, 0x5C, 0xFF);
constexpr ImU32 kBgNavyTrans  = IM_COL32(0x0B, 0x1C, 0x5C, 0xE0); // 88% opacity
constexpr ImU32 kBgNavyDeeper = IM_COL32(0x07, 0x14, 0x40, 0xFF);
constexpr ImU32 kWhite        = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
constexpr ImU32 kWhiteSoft    = IM_COL32(0xE6, 0xEC, 0xF5, 0xFF);
constexpr ImU32 kMuted        = IM_COL32(0x8E, 0x9A, 0xB8, 0xFF);

constexpr ImU32 kAccentRed    = IM_COL32(0xD4, 0x2B, 0x2B, 0xFF);
constexpr ImU32 kAccentYellow = IM_COL32(0xF5, 0xC4, 0x00, 0xFF);
constexpr ImU32 kAccentBlue   = IM_COL32(0x1E, 0x6F, 0xBF, 0xFF);

constexpr ImU32 kEnemyBox     = IM_COL32(0xFF, 0x4D, 0x4D, 0xFF);
constexpr ImU32 kTeamBox      = IM_COL32(0x4D, 0xB3, 0xFF, 0xFF);

constexpr ImU32 kHealthHigh   = IM_COL32(0x6F, 0xD1, 0x60, 0xFF);
constexpr ImU32 kHealthMid    = IM_COL32(0xF5, 0xC4, 0x00, 0xFF);
constexpr ImU32 kHealthLow    = IM_COL32(0xD4, 0x2B, 0x2B, 0xFF);
constexpr ImU32 kHealthBg     = IM_COL32(0x14, 0x1A, 0x2A, 0xC8);

inline ImVec4 ToVec4(ImU32 c) {
    return ImVec4(
        ((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.f,
        ((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.f,
        ((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.f,
        ((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.f);
}

inline void ApplyEducanetStyle() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowPadding     = ImVec2(14, 12);
    s.FramePadding      = ImVec2(8, 5);
    s.ItemSpacing       = ImVec2(8, 6);
    s.ItemInnerSpacing  = ImVec2(6, 4);
    s.IndentSpacing     = 18.f;
    s.ScrollbarSize     = 12.f;
    s.GrabMinSize       = 10.f;

    s.WindowRounding    = 6.f;
    s.FrameRounding     = 4.f;
    s.PopupRounding     = 4.f;
    s.ScrollbarRounding = 6.f;
    s.GrabRounding      = 3.f;
    s.TabRounding       = 4.f;

    s.WindowBorderSize  = 0.f;
    s.FrameBorderSize   = 0.f;
    s.PopupBorderSize   = 0.f;
    s.ChildBorderSize   = 0.f;
    s.TabBorderSize     = 0.f;

    s.WindowTitleAlign  = ImVec2(0.5f, 0.5f);
    s.ButtonTextAlign   = ImVec2(0.5f, 0.5f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]            = ToVec4(kBgNavyTrans);
    c[ImGuiCol_PopupBg]             = ToVec4(kBgNavyDeeper);
    c[ImGuiCol_ChildBg]             = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_Text]                = ToVec4(kWhite);
    c[ImGuiCol_TextDisabled]        = ToVec4(kMuted);

    c[ImGuiCol_TitleBg]             = ToVec4(kBgNavyDeeper);
    c[ImGuiCol_TitleBgActive]       = ToVec4(kBgNavyDeeper);
    c[ImGuiCol_TitleBgCollapsed]    = ToVec4(kBgNavyDeeper);

    c[ImGuiCol_FrameBg]             = ImVec4(0.06f, 0.10f, 0.22f, 0.85f);
    c[ImGuiCol_FrameBgHovered]      = ImVec4(0.10f, 0.16f, 0.32f, 0.95f);
    c[ImGuiCol_FrameBgActive]       = ImVec4(0.14f, 0.22f, 0.40f, 1.00f);

    c[ImGuiCol_Button]              = ImVec4(0.10f, 0.16f, 0.34f, 1.00f);
    c[ImGuiCol_ButtonHovered]       = ImVec4(0.16f, 0.24f, 0.44f, 1.00f);
    c[ImGuiCol_ButtonActive]        = ToVec4(kAccentBlue);

    c[ImGuiCol_Header]              = ImVec4(0.12f, 0.20f, 0.36f, 0.85f);
    c[ImGuiCol_HeaderHovered]       = ImVec4(0.18f, 0.28f, 0.48f, 0.95f);
    c[ImGuiCol_HeaderActive]        = ToVec4(kAccentBlue);

    c[ImGuiCol_CheckMark]           = ToVec4(kAccentYellow);
    c[ImGuiCol_SliderGrab]          = ToVec4(kAccentBlue);
    c[ImGuiCol_SliderGrabActive]    = ToVec4(kAccentYellow);

    c[ImGuiCol_Separator]           = ImVec4(0.20f, 0.30f, 0.50f, 1.00f);
    c[ImGuiCol_SeparatorHovered]    = ToVec4(kAccentBlue);
    c[ImGuiCol_SeparatorActive]     = ToVec4(kAccentYellow);

    c[ImGuiCol_ScrollbarBg]         = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]       = ImVec4(0.20f, 0.30f, 0.50f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.30f, 0.40f, 0.60f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ToVec4(kAccentBlue);

    c[ImGuiCol_ResizeGrip]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]   = ToVec4(kAccentBlue);
    c[ImGuiCol_ResizeGripActive]    = ToVec4(kAccentYellow);
}

} // namespace theme

#include "esp_render.hpp"
#include "../theme.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace esp_render {

namespace {

bool WorldToScreen(const ViewMatrix& view, const Vector3& pos,
                   int winW, int winH, ImVec2& out) {
    const float* m = &view.m[0][0];
    float w = m[12]*pos.x + m[13]*pos.y + m[14]*pos.z + m[15];
    if (w < 0.001f) return false;

    float x = m[0]*pos.x + m[1]*pos.y + m[2]*pos.z + m[3];
    float y = m[4]*pos.x + m[5]*pos.y + m[6]*pos.z + m[7];

    out.x = (winW / 2.f) + (x / w) * (winW / 2.f);
    out.y = (winH / 2.f) - (y / w) * (winH / 2.f);
    return true;
}

ImU32 HealthColor(int hp) {
    float t = std::clamp(hp / 100.f, 0.f, 1.f);
    if (t > 0.5f) {
        float k = (t - 0.5f) * 2.f;
        return IM_COL32(
            static_cast<int>(0xF5 * (1.f - k) + 0x6F * k),
            static_cast<int>(0xC4 * (1.f - k) + 0xD1 * k),
            static_cast<int>(0x00 * (1.f - k) + 0x60 * k),
            0xFF);
    }
    float k = t * 2.f;
    return IM_COL32(
        static_cast<int>(0xD4 * (1.f - k) + 0xF5 * k),
        static_cast<int>(0x2B * (1.f - k) + 0xC4 * k),
        static_cast<int>(0x2B * (1.f - k) + 0x00 * k),
        0xFF);
}

void DrawCornerBox(ImDrawList* dl, float x, float y, float w, float h,
                    ImU32 color, float thickness) {
    float clen = h / 4.f;
    if (clen < 4.f) clen = 4.f;

    float x2 = x + w;
    float y2 = y + h;

    // Top-left
    dl->AddLine({ x, y }, { x + clen, y }, color, thickness);
    dl->AddLine({ x, y }, { x, y + clen }, color, thickness);
    // Top-right
    dl->AddLine({ x2, y }, { x2 - clen, y }, color, thickness);
    dl->AddLine({ x2, y }, { x2, y + clen }, color, thickness);
    // Bottom-left
    dl->AddLine({ x, y2 }, { x + clen, y2 }, color, thickness);
    dl->AddLine({ x, y2 }, { x, y2 - clen }, color, thickness);
    // Bottom-right
    dl->AddLine({ x2, y2 }, { x2 - clen, y2 }, color, thickness);
    dl->AddLine({ x2, y2 }, { x2, y2 - clen }, color, thickness);
}

void DrawTextCentered(ImDrawList* dl, ImVec2 pos, ImU32 color, const char* text) {
    ImVec2 sz = ImGui::CalcTextSize(text);
    ImVec2 origin{ pos.x - sz.x * 0.5f, pos.y };

    // Cheap text outline — render 4 dark offsets then the bright body
    ImU32 outline = IM_COL32(0, 0, 0, 200);
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy)
                dl->AddText({ origin.x + dx, origin.y + dy }, outline, text);
    dl->AddText(origin, color, text);
}

} // namespace

void DrawAll(const PlayerESPData players[64], int count, int localTeam,
             const ViewMatrix& view, int winW, int winH,
             const ESPConfig& cfg) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    bool teamColor = (cfg.colorMode.load() == 0);

    for (int i = 0; i < count; ++i) {
        const PlayerESPData& p = players[i];
        if (!p.alive) continue;

        ImVec2 feet, head;
        if (!WorldToScreen(view, p.origin,  winW, winH, feet)) continue;
        if (!WorldToScreen(view, p.headPos, winW, winH, head)) continue;

        float boxH = feet.y - head.y;
        if (boxH < 6.f) continue;

        float boxW = boxH * 0.5f;
        float x    = feet.x - boxW * 0.5f;
        float y    = head.y;

        ImU32 boxColor = (teamColor && !p.isEnemy) ? theme::kTeamBox : theme::kEnemyBox;

        DrawCornerBox(dl, x, y, boxW, boxH, boxColor, 1.6f);

        // Health bar — left side, 3px wide
        if (cfg.healthBar.load()) {
            float barW = 3.f;
            float barX = x - barW - 3.f;
            float fillH = boxH * std::clamp(p.health / 100.f, 0.f, 1.f);

            dl->AddRectFilled({ barX, y }, { barX + barW, y + boxH },
                              theme::kHealthBg, 1.f);
            dl->AddRectFilled({ barX, y + boxH - fillH }, { barX + barW, y + boxH },
                              HealthColor(p.health), 1.f);
        }

        // HP number — right edge of bar fill, white outlined text
        if (cfg.hpNumbers.load()) {
            float barX = x - 3.f - 3.f;
            float fillH = boxH * std::clamp(p.health / 100.f, 0.f, 1.f);
            float topY  = y + boxH - fillH - 7.f;

            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", std::clamp(p.health, 0, 100));
            ImVec2 sz = ImGui::CalcTextSize(buf);
            ImVec2 origin{ barX - sz.x - 2.f, topY };
            ImU32 outline = IM_COL32(0, 0, 0, 200);
            for (int dx = -1; dx <= 1; ++dx)
                for (int dy = -1; dy <= 1; ++dy)
                    if (dx || dy)
                        dl->AddText({ origin.x + dx, origin.y + dy }, outline, buf);
            dl->AddText(origin, theme::kWhite, buf);
        }

        // Name — above the box
        if (cfg.nameESP.load() && !p.name.empty()) {
            ImVec2 textSize = ImGui::CalcTextSize(p.name.c_str());
            DrawTextCentered(dl, { x + boxW * 0.5f, y - textSize.y - 4.f },
                             theme::kWhite, p.name.c_str());
        }

        // Distance — below the box
        if (cfg.distanceESP.load()) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "[%.1fm]", p.distance);
            DrawTextCentered(dl, { x + boxW * 0.5f, y + boxH + 2.f },
                             theme::kAccentYellow, buf);
        }
    }
}

} // namespace esp_render

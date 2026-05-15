#include "esp_render.hpp"
#include "../theme.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

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

// Full 8-direction outline then colored body — legible on any background.
void DrawTextOutlined(ImDrawList* dl, ImVec2 pos, ImU32 color, const char* text) {
    constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 220);
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy)
                dl->AddText({ pos.x + dx, pos.y + dy }, kShadow, text);
    dl->AddText(pos, color, text);
}

void DrawTextCentered(ImDrawList* dl, ImVec2 pos, ImU32 color, const char* text) {
    ImVec2 sz = ImGui::CalcTextSize(text);
    DrawTextOutlined(dl, { pos.x - sz.x * 0.5f, pos.y }, color, text);
}

// Health → color: Red (#D42B2B) → Yellow (#F5C400) → Green (#6FD160)
ImU32 HealthColor(int hp) {
    float t = std::clamp(hp / 100.f, 0.f, 1.f);
    if (t > 0.5f) {
        float k = (t - 0.5f) * 2.f;
        return IM_COL32(
            (int)(0xF5 + (0x6F - 0xF5) * k),
            (int)(0xC4 + (0xD1 - 0xC4) * k),
            (int)(0x00 + (0x60 - 0x00) * k),
            0xFF);
    }
    float k = t * 2.f;
    return IM_COL32(
        (int)(0xD4 + (0xF5 - 0xD4) * k),
        (int)(0x2B + (0xC4 - 0x2B) * k),
        (int)(0x2B + (0x00 - 0x2B) * k),
        0xFF);
}

// Distance → color: close = red, mid = yellow, far = soft white
ImU32 DistanceColor(float meters) {
    if (meters < 12.f) return theme::kEnemyBox;
    if (meters < 30.f) return theme::kAccentYellow;
    return theme::kWhiteSoft;
}

// Two-pass corner box: shadow pass gives depth on any background, color pass sits on top.
void DrawCornerBox(ImDrawList* dl, float x, float y, float w, float h, ImU32 color) {
    constexpr float kThick  = 1.8f;
    constexpr float kShadowT = kThick + 1.4f;
    constexpr ImU32 kShadow  = IM_COL32(0, 0, 0, 190);

    float clen = std::max(h / 3.5f, 5.f);
    float x2 = x + w, y2 = y + h;

    auto seg = [&](ImVec2 a, ImVec2 b) {
        dl->AddLine(a, b, kShadow, kShadowT);
        dl->AddLine(a, b, color,   kThick);
    };

    seg({x,  y }, {x + clen, y      });
    seg({x,  y }, {x,        y + clen});
    seg({x2, y }, {x2 - clen,y      });
    seg({x2, y }, {x2,       y + clen});
    seg({x,  y2}, {x + clen, y2     });
    seg({x,  y2}, {x,        y2 - clen});
    seg({x2, y2}, {x2 - clen,y2     });
    seg({x2, y2}, {x2,       y2 - clen});
}

} // namespace

void DrawAll(const PlayerESPData players[64], int count, int localTeam,
             const ViewMatrix& view, int winW, int winH,
             const ESPConfig& cfg) {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const bool teamColor = (cfg.colorMode.load() == 0);
    const float lineH    = ImGui::GetTextLineHeight();

    for (int i = 0; i < count; ++i) {
        const PlayerESPData& p = players[i];
        if (!p.alive) continue;

        ImVec2 feet, head;
        if (!WorldToScreen(view, p.origin,  winW, winH, feet)) continue;
        if (!WorldToScreen(view, p.headPos, winW, winH, head)) continue;

        float boxH = feet.y - head.y;
        if (boxH < 8.f) continue;

        float boxW = boxH * 0.45f;
        float x    = feet.x - boxW * 0.5f;
        float y    = head.y;
        ImU32 col  = (!p.isEnemy && teamColor) ? theme::kTeamBox : theme::kEnemyBox;

        // Corner box
        DrawCornerBox(dl, x, y, boxW, boxH, col);

        // Head circle — mirrors the two-pass pattern of the box
        {
            float r = std::clamp(boxW * 0.11f, 2.f, 5.5f);
            dl->AddCircle(head, r + 0.7f, IM_COL32(0, 0, 0, 190), 0, 1.6f);
            dl->AddCircle(head, r,         col, 0, 1.5f);
        }

        // Health bar: dark bordered backing + gradient fill
        constexpr float kBarW = 4.f;
        const float barX = x - kBarW - 4.f;

        if (cfg.healthBar.load()) {
            float fillH = boxH * std::clamp(p.health / 100.f, 0.f, 1.f);
            dl->AddRectFilled({ barX - 1.f, y - 1.f },
                              { barX + kBarW + 1.f, y + boxH + 1.f },
                              IM_COL32(0, 0, 0, 180), 2.f);
            dl->AddRectFilled({ barX, y + boxH - fillH },
                              { barX + kBarW, y + boxH },
                              HealthColor(p.health), 1.5f);
        }

        // HP number — colored to match the bar gradient
        if (cfg.hpNumbers.load()) {
            float fillH = boxH * std::clamp(p.health / 100.f, 0.f, 1.f);
            char  buf[8];
            std::snprintf(buf, sizeof(buf), "%d", std::clamp(p.health, 0, 100));
            ImVec2 sz = ImGui::CalcTextSize(buf);
            DrawTextOutlined(dl,
                { barX - sz.x - 3.f, y + boxH - fillH - lineH - 1.f },
                HealthColor(p.health), buf);
        }

        // Name — pill background for legibility on busy backgrounds
        if (cfg.nameESP.load() && !p.name.empty()) {
            const char* nm = p.name.c_str();
            ImVec2 sz = ImGui::CalcTextSize(nm);
            float  tx = feet.x - sz.x * 0.5f;
            float  ty = y - sz.y - 6.f;
            constexpr float kPad = 4.f;
            dl->AddRectFilled({ tx - kPad,        ty - 1.f },
                              { tx + sz.x + kPad, ty + sz.y + 1.f },
                              IM_COL32(0, 0, 0, 155), 3.f);
            DrawTextOutlined(dl, { tx, ty }, theme::kWhite, nm);
        }

        // Distance — proximity-tinted, no brackets
        if (cfg.distanceESP.load()) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0fm", p.distance);
            DrawTextCentered(dl, { feet.x, y + boxH + 3.f }, DistanceColor(p.distance), buf);
        }

        // Skeleton overlay — prefer resolved bone positions when available
        if (cfg.skeleton.load()) {
            // Prefer attachment-resolved bones when available. The scan loop
            // typically fills attachments in this order: 0=head, 1=chest,
            // 2=leftFoot, 3=rightFoot, last=pelvis/origin. We'll draw a small
            // anatomical skeleton (spine, hips, legs) and render joints as
            // filled circles for clarity similar to the reference overlay.
            if (p.boneCount >= 2) {
                auto project = [&](int idx, ImVec2& out) -> bool {
                    if (idx < 0 || idx >= p.boneCount) return false;
                    return WorldToScreen(view, p.bones[idx], winW, winH, out);
                };

                ImVec2 headS{ -1.f, -1.f }, chestS{ -1.f, -1.f }, pelvisS{ -1.f, -1.f };
                ImVec2 leftLegS{ -1.f, -1.f }, rightLegS{ -1.f, -1.f };

                project(0, headS);
                project(1, chestS);
                project(p.boneCount - 1, pelvisS);
                project(2, leftLegS);
                project(3, rightLegS);

                ImU32 scol = cfg.skeletonColor.load() != 0u ? static_cast<ImU32>(cfg.skeletonColor.load()) : col;
                float lineThick = cfg.skeletonThick.load();
                float jointR = cfg.jointRadius.load();

                auto drawSeg = [&](const ImVec2& a, const ImVec2& b) {
                    if (a.x < 0 || b.x < 0) return;
                    dl->AddLine(a, b, IM_COL32(0,0,0,190), lineThick + 1.6f);
                    dl->AddLine(a, b, scol, lineThick);
                };

                auto drawJoint = [&](const ImVec2& pnt) {
                    if (pnt.x < 0) return;
                    dl->AddCircleFilled(pnt, jointR + 1.0f, IM_COL32(0,0,0,200));
                    dl->AddCircleFilled(pnt, jointR, scol);
                    dl->AddCircle(pnt, jointR * 0.45f, IM_COL32(0,0,0,120), 0, 1.0f);
                };

                // Spine: head -> chest -> pelvis
                drawSeg(headS, chestS);
                drawSeg(chestS, pelvisS);

                // Hips/legs: chest -> left/right leg (attachments often approximate hip->foot)
                drawSeg(chestS, leftLegS);
                drawSeg(chestS, rightLegS);

                // Draw joints where available (prefer head, chest, pelvis, feet)
                drawJoint(headS);
                drawJoint(chestS);
                drawJoint(pelvisS);
                drawJoint(leftLegS);
                drawJoint(rightLegS);

                // If there are additional bones (e.g., hands/shoulders) try to draw
                // short connections between consecutive resolved bones to give a
                // fuller skeleton when the dumper returns more attachments.
                ImVec2 prev{ -1.f, -1.f };
                for (int b = 0; b < p.boneCount; ++b) {
                    ImVec2 s;
                    if (!WorldToScreen(view, p.bones[b], winW, winH, s)) continue;
                    if (prev.x >= 0) drawSeg(prev, s);
                    prev = s;
                }
            }
        }

        // If skeleton is enabled but not enough bones were resolved, draw the original approximation
        // When skeleton is disabled, do not draw any skeleton lines.
        if (cfg.skeleton.load() && p.boneCount < 2) {
            auto lerp = [](const ImVec2& a, const ImVec2& b, float t) {
                return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
            };

            ImVec2 headS = head;
            ImVec2 feetS = feet;
            ImVec2 neckS  = lerp(headS, feetS, 0.18f);
            ImVec2 chestS = lerp(headS, feetS, 0.34f);
            ImVec2 pelvisS = lerp(headS, feetS, 0.56f);

            ImVec2 leftShoulder  = ImVec2(chestS.x - boxW * 0.55f, chestS.y);
            ImVec2 rightShoulder = ImVec2(chestS.x + boxW * 0.55f, chestS.y);

            ImVec2 leftHip  = ImVec2(pelvisS.x - boxW * 0.25f, pelvisS.y);
            ImVec2 rightHip = ImVec2(pelvisS.x + boxW * 0.25f, pelvisS.y);

            ImVec2 leftKnee  = lerp(leftHip, feetS, 0.55f);
            ImVec2 rightKnee = lerp(rightHip, feetS, 0.55f);

            ImVec2 leftFoot  = ImVec2(feetS.x - boxW * 0.20f, feetS.y);
            ImVec2 rightFoot = ImVec2(feetS.x + boxW * 0.20f, feetS.y);

            ImU32 scol = col;
            float thickness = 1.6f;

            // Spine
            dl->AddLine(headS, neckS, IM_COL32(0,0,0,180), thickness + 1.2f);
            dl->AddLine(headS, neckS, scol, thickness);
            dl->AddLine(neckS, chestS, IM_COL32(0,0,0,180), thickness + 1.2f);
            dl->AddLine(neckS, chestS, scol, thickness);
            dl->AddLine(chestS, pelvisS, IM_COL32(0,0,0,180), thickness + 1.2f);
            dl->AddLine(chestS, pelvisS, scol, thickness);

            // Arms
            dl->AddLine(chestS, leftShoulder, IM_COL32(0,0,0,160), thickness + 1.2f);
            dl->AddLine(chestS, leftShoulder, scol, thickness);
            dl->AddLine(chestS, rightShoulder, IM_COL32(0,0,0,160), thickness + 1.2f);
            dl->AddLine(chestS, rightShoulder, scol, thickness);

            dl->AddLine(leftShoulder, lerp(leftShoulder, pelvisS, 0.25f), IM_COL32(0,0,0,140), thickness + 1.2f);
            dl->AddLine(leftShoulder, lerp(leftShoulder, pelvisS, 0.25f), scol, thickness);
            dl->AddLine(rightShoulder, lerp(rightShoulder, pelvisS, 0.25f), IM_COL32(0,0,0,140), thickness + 1.2f);
            dl->AddLine(rightShoulder, lerp(rightShoulder, pelvisS, 0.25f), scol, thickness);

            // Hips -> legs
            dl->AddLine(pelvisS, leftHip, IM_COL32(0,0,0,160), thickness + 1.2f);
            dl->AddLine(pelvisS, leftHip, scol, thickness);
            dl->AddLine(pelvisS, rightHip, IM_COL32(0,0,0,160), thickness + 1.2f);
            dl->AddLine(pelvisS, rightHip, scol, thickness);

            dl->AddLine(leftHip, leftKnee, IM_COL32(0,0,0,140), thickness + 1.2f);
            dl->AddLine(leftHip, leftKnee, scol, thickness);
            dl->AddLine(rightHip, rightKnee, IM_COL32(0,0,0,140), thickness + 1.2f);
            dl->AddLine(rightHip, rightKnee, scol, thickness);

            dl->AddLine(leftKnee, leftFoot, IM_COL32(0,0,0,140), thickness + 1.2f);
            dl->AddLine(leftKnee, leftFoot, scol, thickness);
            dl->AddLine(rightKnee, rightFoot, IM_COL32(0,0,0,140), thickness + 1.2f);
            dl->AddLine(rightKnee, rightFoot, scol, thickness);
        }
    }
}

} // namespace esp_render

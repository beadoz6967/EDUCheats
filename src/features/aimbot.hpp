#pragma once
#include "../sdk.hpp"
#include "esp.hpp"
#include <cmath>
#include <algorithm>

namespace aimbot {

inline void NormalizeAngles(Vector3& a) {
    // Pitch: clamp to [-89, 89] (can't look past straight up/down)
    a.x = std::clamp(a.x, -89.f, 89.f);
    // Yaw: wrap to [-180, 180]
    while (a.y > 180.f)  a.y -= 360.f;
    while (a.y < -180.f) a.y += 360.f;
    a.z = 0.f;
}

// World delta → {pitch, yaw, 0} in degrees
inline Vector3 CalcAngle(const Vector3& from, const Vector3& to) {
    Vector3 delta = { to.x - from.x, to.y - from.y, to.z - from.z };
    float hyp = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    Vector3 angle;
    angle.x = -std::atan2f(delta.z, hyp) * (180.f / 3.14159265358979f);
    angle.y =  std::atan2f(delta.y, delta.x) * (180.f / 3.14159265358979f);
    angle.z = 0.f;
    NormalizeAngles(angle);
    return angle;
}

// Angular distance between two angles in degrees (2D: pitch + yaw)
inline float AngleDist(const Vector3& a, const Vector3& b) {
    float dp = a.x - b.x;
    float dy = a.y - b.y;
    while (dy > 180.f)  dy -= 360.f;
    while (dy < -180.f) dy += 360.f;
    return std::sqrt(dp * dp + dy * dy);
}

// Move 1/factor of remaining delta toward target per call
inline Vector3 SmoothAngle(const Vector3& current, const Vector3& target, float factor) {
    if (factor <= 1.f) return target;
    float dp = target.x - current.x;
    float dy = target.y - current.y;
    while (dy > 180.f)  dy -= 360.f;
    while (dy < -180.f) dy += 360.f;
    Vector3 r = { current.x + dp / factor, current.y + dy / factor, 0.f };
    NormalizeAngles(r);
    return r;
}

// Returns index into players[] of best target, or -1 if none in FOV
inline int SelectTarget(const PlayerESPData players[], int count,
                        const Vector3& eyePos,
                        const Vector3& viewAngles,
                        float fov, int boneIdx) {
    int   best     = -1;
    float bestDist = fov;  // reject anything outside this cone

    for (int i = 0; i < count; ++i) {
        if (!players[i].alive || !players[i].isEnemy) continue;
        if (players[i].boneCount <= boneIdx)          continue;

        Vector3 aimAngle = CalcAngle(eyePos, players[i].bones[boneIdx]);
        float   dist     = AngleDist(viewAngles, aimAngle);

        if (dist < bestDist) { bestDist = dist; best = i; }
    }
    return best;
}

} // namespace aimbot

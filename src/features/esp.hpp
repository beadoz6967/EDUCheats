#pragma once
#include "../sdk.hpp"
#include <Windows.h>
#include <atomic>
#include <string>

// Pure data types shared between the entity scan loop, the renderer,
// the menu, and the persistent config layer. Lives here because every
// other module needs it; rendering itself happens in src/gui/.

struct ESPConfig {
    std::atomic<bool> enabled     { true };
    std::atomic<bool> nameESP     { true };
    std::atomic<bool> healthBar   { true };
    // 0 = team color (blue/red), 1 = enemy always red
    std::atomic<int>  colorMode   { 0    };
    std::atomic<bool> distanceESP { true };
    std::atomic<bool> hpNumbers   { true };
    // Toggle skeleton lines (independent of box settings)
    std::atomic<bool> skeleton    { true };

    // Customization: packed ImU32 colors (0 = use default theme), thickness and joint size
    std::atomic<uint32_t> boxColor        { 0u };
    std::atomic<uint32_t> skeletonColor   { 0u };
    std::atomic<float>    skeletonThick   { 1.6f };
    std::atomic<float>    jointRadius     { 2.5f };
};

struct AimbotConfig {
    std::atomic<bool>  enabled    { false };
    std::atomic<int>   key        { VK_LBUTTON };
    std::atomic<float> fov        { 5.0f };    // degrees; legit 3-8, rage 360
    std::atomic<float> smooth     { 5.0f };    // divisor; 1=instant, higher=slower
    std::atomic<bool>  rageMode   { false };   // rage: fov=360, smooth=1
    std::atomic<int>   boneTarget { 7     };   // 7=head, 4=chest
};

// Live runtime state shared with the menu UI (not persisted).
// nearestEnemyDist is in meters. -1.f means no visible enemy this frame.
struct GameState {
    std::atomic<float> nearestEnemyDist { -1.f };
    std::atomic<int>   entityCount      { 0    };
    std::atomic<bool>  matrixOk         { false };
};

struct PlayerESPData {
    bool        alive    = false;
    bool        isEnemy  = false;
    int         health   = 0;
    float       distance = 0.f;    // meters from local player
    std::string name;
    Vector3     origin;
    Vector3     headPos;           // origin + (0, 0, 72)
    // Resolved bone/attachment positions (world space). Only a small
    // fixed-size buffer is stored to avoid dynamic allocation in the
    // real-time scan loop.
    Vector3     bones[64];
    int         boneCount = 0;
};

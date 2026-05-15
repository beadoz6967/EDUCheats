#pragma once
#include "../sdk.hpp"
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
};

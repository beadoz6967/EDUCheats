#pragma once
#include "../memory.hpp"
#include "../sdk.hpp"
#include <atomic>
#include <string>

struct ESPConfig {
    std::atomic<bool> enabled    { true  };
    std::atomic<bool> nameESP    { true  };
    std::atomic<bool> healthBar  { true  };
    // 0 = team color (blue/red), 1 = enemy always red
    std::atomic<int>  colorMode  { 0     };
};

struct PlayerESPData {
    bool        alive       = false;
    bool        isEnemy     = false;
    int         health      = 0;
    std::string name;
    Vector3     origin;
    Vector3     headPos;    // origin + (0, 0, 72)
};

class ESPOverlay {
public:
    ESPOverlay(const Memory& mem, uintptr_t clientBase, ESPConfig& cfg)
        : m_mem(mem), m_clientBase(clientBase), m_cfg(cfg) {}

    // Creates and runs the overlay window. Blocking — run on dedicated thread.
    void Run(int localTeam);

    // Called from the main loop to push fresh entity data.
    void UpdatePlayers(const PlayerESPData players[64], int count, int localTeam);

    // Called by WndProc (free function in same TU needs public access)
    void Paint(HDC hdc);

    // Post WM_DESTROY to cleanly exit the message loop
    void Stop();

private:
    void CreateOverlayWindow();
    void RenderFrame(HDC hdc, int localTeam);
    void DrawBoxESP(HDC hdc, const PlayerESPData& p, int winW, int winH);
    void DrawHealthBar(HDC hdc, const PlayerESPData& p, int x, int y, int h);
    void DrawName(HDC hdc, const PlayerESPData& p, int x, int y);

    bool WorldToScreen(const Vector3& pos, Vector2& out) const;
    void ReadViewMatrix();

    const Memory&  m_mem;
    uintptr_t      m_clientBase;
    ESPConfig&     m_cfg;

    HWND           m_hwnd     = nullptr;
    int            m_winW     = 0;
    int            m_winH     = 0;
    ViewMatrix     m_vMatrix  {};

    PlayerESPData  m_players[64]{};
    int            m_playerCount = 0;
    int            m_localTeam   = 0;
    CRITICAL_SECTION m_dataLock{};
};

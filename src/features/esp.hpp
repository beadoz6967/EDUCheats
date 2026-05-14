#pragma once
#include "../memory.hpp"
#include "../sdk.hpp"
#include <atomic>
#include <string>

struct ESPConfig {
    std::atomic<bool> enabled     { true };
    std::atomic<bool> nameESP     { true };
    std::atomic<bool> healthBar   { true };
    // 0 = team color (blue/red), 1 = enemy always red
    std::atomic<int>  colorMode   { 0    };
    std::atomic<bool> distanceESP { true };
    std::atomic<bool> hpNumbers   { true };
};

// Live runtime state shared with the menu thread (not persisted).
// nearestEnemyDist is in meters. -1.f means no visible enemy this frame.
struct GameState {
    std::atomic<float> nearestEnemyDist { -1.f };
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

class ESPOverlay {
public:
    ESPOverlay(const Memory& mem, uintptr_t clientBase, ESPConfig& cfg)
        : m_mem(mem), m_clientBase(clientBase), m_cfg(cfg) {
        InitializeCriticalSection(&m_dataLock);
    }
    ~ESPOverlay();

    ESPOverlay(const ESPOverlay&)            = delete;
    ESPOverlay& operator=(const ESPOverlay&) = delete;

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
    void DrawCornerBox(HDC hdc, const PlayerESPData& p, int x, int y, int w, int h);
    void DrawHealthBar(HDC hdc, const PlayerESPData& p, int x, int y, int h);
    void DrawHpNumber (HDC hdc, const PlayerESPData& p, int x, int y, int h);
    void DrawName     (HDC hdc, const PlayerESPData& p, int centerX, int topY);
    void DrawDistance (HDC hdc, const PlayerESPData& p, int centerX, int topY);

    bool WorldToScreen(const Vector3& pos, Vector2& out) const;
    void ReadViewMatrix();
    void RefreshWindowBounds();

    // Double-buffer: render full scene into m_memDC then BitBlt to window DC.
    // Recreated whenever window dimensions change.
    void EnsureBackBuffer(HDC windowDC);
    void DestroyBackBuffer();

    const Memory&   m_mem;
    uintptr_t       m_clientBase;
    ESPConfig&      m_cfg;

    HWND            m_hwnd     = nullptr;
    int             m_winW     = 0;
    int             m_winH     = 0;
    ViewMatrix      m_vMatrix  {};

    PlayerESPData   m_players[64]{};
    int             m_playerCount = 0;
    int             m_localTeam   = 0;
    CRITICAL_SECTION m_dataLock{};

    HPEN            m_penEnemy      = nullptr;
    HPEN            m_penTeam       = nullptr;
    HBRUSH          m_brushBlack    = nullptr;
    HBRUSH          m_brushHealthBg = nullptr;

    HDC             m_memDC        = nullptr;
    HBITMAP         m_memBmp       = nullptr;
    HBITMAP         m_oldBmp       = nullptr;
    int             m_bufW         = 0;
    int             m_bufH         = 0;
};

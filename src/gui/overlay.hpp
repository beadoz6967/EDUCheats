#pragma once
#include "../features/esp.hpp"
#include "../memory.hpp"
#include "../sdk.hpp"
#include "../config.hpp"
#include <Windows.h>
#include <atomic>
#include <mutex>

// Owns the transparent topmost overlay window, DX11 swapchain, and ImGui
// frame loop. Renders ESP via ImGui DrawList and the Educanet-themed menu
// via ImGui::Begin. Click-through toggles when menu visibility changes
// so the user can interact with checkboxes / sliders.
class Overlay {
public:
    Overlay(const Memory& mem, uintptr_t clientBase,
            ESPConfig& cfg, GameState& state, Config& persist);
    ~Overlay();

    Overlay(const Overlay&)            = delete;
    Overlay& operator=(const Overlay&) = delete;

    // Blocking. Spawns DX11+ImGui, runs message + render loop on caller thread.
    void Run(std::atomic<bool>& running);

    // Called from the entity-scan thread to publish a fresh snapshot.
    void PushPlayers(const PlayerESPData players[64], int count,
                     int localTeam, ViewMatrix view);

    // ImGui WndProc forwarder must see private state — friend.
    friend LRESULT CALLBACK OverlayWndProc(HWND, UINT, WPARAM, LPARAM);

private:
    bool CreateOverlayWindow();
    bool CreateDeviceAndSwapchain();
    void CreateRenderTarget();
    void DestroyRenderTarget();
    void DestroyDevice();

    void RefreshGameWindowBounds();
    void RenderFrame();
    void ToggleMenu();
    void ApplyClickThrough(bool clickThrough);

    const Memory& m_mem;
    uintptr_t     m_clientBase;
    ESPConfig&    m_cfg;
    GameState&    m_state;
    Config&       m_persist;

    HWND          m_hwnd     = nullptr;
    int           m_winW     = 0;
    int           m_winH     = 0;

    // DX11
    void*         m_device          = nullptr; // ID3D11Device*
    void*         m_deviceCtx       = nullptr; // ID3D11DeviceContext*
    void*         m_swapchain       = nullptr; // IDXGISwapChain*
    void*         m_renderTargetView= nullptr; // ID3D11RenderTargetView*

    // Shared snapshot
    std::mutex      m_lock;
    PlayerESPData   m_players[64]{};
    int             m_playerCount = 0;
    int             m_localTeam   = 0;
    ViewMatrix      m_view{};

    // UI state
    bool m_menuVisible  = false;
    bool m_prevInsert   = false;
    bool m_clickThrough = true;
};

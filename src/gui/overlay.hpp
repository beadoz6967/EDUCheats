#pragma once
#include "../features/esp.hpp"
#include "../config.hpp"
#include <Windows.h>
#include <mutex>

// Internal DLL overlay: hooks IDXGISwapChain::Present (vtable[8]) via MinHook.
// ImGui renders inside CS2's own render target. No separate transparent window.
class Overlay {
public:
    // Hook Present and store config refs. Call once from the main thread.
    void Install(ESPConfig&, AimbotConfig&, GameState&, Config&);

    // Unhook, release D3D11 refs, shut down ImGui. Safe to call once.
    void Uninstall();

    // Thread-safe snapshot update from the entity-scan thread.
    void PushPlayers(const PlayerESPData* players, int count,
                     int localTeam, const ViewMatrix& view);

    bool IsRunning() const;
};

extern Overlay g_overlay;

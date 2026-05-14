#include <Windows.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstdio>

#include "memory.hpp"
#include "offsets.hpp"
#include "sdk.hpp"
#include "features/esp.hpp"
#include "menu/menu.hpp"

static std::atomic<bool> g_running{ true };

int main() {
    Memory mem;

    // Wait for CS2
    while (!mem.Attach("cs2.exe")) {
        if (GetAsyncKeyState(VK_END) & 0x8000) return 0;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    uintptr_t clientBase = mem.GetModuleBase("client.dll");
    if (!clientBase) {
        return 1;
    }

    ESPConfig espCfg;
    ESPOverlay overlay(mem, clientBase, espCfg);
    Menu menu(espCfg);

    // Overlay thread — runs its own Win32 message loop
    std::thread overlayThread([&]() {
        overlay.Run(0);
    });

    // Menu thread — console input loop
    std::thread menuThread([&]() {
        menu.Run(g_running);
    });

    // Resolve entity list base once
    uintptr_t entityListBase = mem.Read<uintptr_t>(clientBase + offsets::dwEntityList);

    CEntityList entityList(entityListBase, mem);

    // Main entity read loop — ~60 Hz
    while (g_running) {
        uintptr_t localControllerPtr = mem.Read<uintptr_t>(clientBase + offsets::dwLocalPlayerController);
        int localTeam = 0;
        if (localControllerPtr) {
            CCSPlayerController localCtrl(localControllerPtr, mem);
            localTeam = localCtrl.GetTeamNum();
        }

        PlayerESPData players[64]{};
        int count = 0;

        for (int i = 1; i <= 64; ++i) {
            uintptr_t ctrlPtr = entityList.GetController(i);
            if (!ctrlPtr || ctrlPtr == localControllerPtr) continue;

            CCSPlayerController ctrl(ctrlPtr, mem);
            int team = ctrl.GetTeamNum();
            if (team != 2 && team != 3) continue; // not a player slot

            uint32_t pawnHandle = ctrl.GetPawnHandle();
            uintptr_t pawnPtr   = entityList.HandleToPtr(pawnHandle);
            if (!pawnPtr) continue;

            C_CSPlayerPawn pawn(pawnPtr, mem);
            if (!pawn.IsAlive()) continue;

            PlayerESPData& d = players[count];
            d.alive    = true;
            d.isEnemy  = (team != localTeam);
            d.health   = std::clamp(pawn.GetHealth(), 0, 100);
            d.name     = ctrl.GetName();
            d.origin   = pawn.GetOrigin();
            d.headPos  = { d.origin.x, d.origin.y, d.origin.z + 72.f };
            ++count;
        }

        overlay.UpdatePlayers(players, count, localTeam);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    overlay.Stop();

    overlayThread.join();
    menuThread.join();

    return 0;
}

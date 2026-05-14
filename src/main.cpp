#include <Windows.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "memory.hpp"
#include "offsets.hpp"
#include "sdk.hpp"
#include "config.hpp"
#include "features/esp.hpp"
#include "menu/menu.hpp"

static std::atomic<bool> g_running{ true };

static constexpr float kUnitsToMeters = 0.01905f;

static float Distance3D(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

int main() {
    Memory mem;
    uintptr_t clientBase = 0;

    printf("[EDUCheats] Waiting for cs2.exe...\n");

    // Wait for CS2 process AND client.dll — client.dll loads a few seconds after the process appears
    while (true) {
        if (GetAsyncKeyState(VK_END) & 0x8000) return 0;

        if (!mem.IsAttached()) {
            if (!mem.Attach("cs2.exe")) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            printf("[EDUCheats] cs2.exe found. Waiting for client.dll...\n");
        }

        clientBase = mem.GetModuleBase("client.dll");
        if (clientBase) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    printf("[EDUCheats] Attached. client.dll base: 0x%llX\n",
           static_cast<unsigned long long>(clientBase));

    ESPConfig espCfg;
    GameState gameState;
    Config    persist;

    // Load saved settings (or write defaults if missing). Silent on error.
    persist.Load(espCfg);

    ESPOverlay overlay(mem, clientBase, espCfg);
    Menu       menu(espCfg, gameState, persist);

    // Overlay thread — runs its own Win32 message loop
    std::thread overlayThread([&]() {
        overlay.Run(0);
    });

    // Menu thread — console input loop
    std::thread menuThread([&]() {
        menu.Run(g_running);
    });

    // Main entity read loop — ~128 Hz
    while (g_running) {
        // Re-resolve entity list each tick in case it shifted during loading
        uintptr_t entityListBase = mem.Read<uintptr_t>(clientBase + offsets::dwEntityList);
        CEntityList entityList(entityListBase, mem);

        uintptr_t localControllerPtr = mem.Read<uintptr_t>(clientBase + offsets::dwLocalPlayerController);
        int localTeam = 0;
        if (localControllerPtr) {
            CCSPlayerController localCtrl(localControllerPtr, mem);
            localTeam = localCtrl.GetTeamNum();
        }

        // Local pawn via global pointer — single deref vs three-step handle resolution
        Vector3 localOrigin{};
        uintptr_t localPawnPtr = mem.Read<uintptr_t>(clientBase + offsets::dwLocalPlayerPawn);
        if (localPawnPtr) {
            C_CSPlayerPawn localPawn(localPawnPtr, mem);
            localOrigin = localPawn.GetOrigin();
        }

        PlayerESPData players[64]{};
        int count = 0;
        float nearestEnemyMeters = -1.f;

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
            d.distance = Distance3D(localOrigin, d.origin) * kUnitsToMeters;
            ++count;

            if (d.isEnemy && (nearestEnemyMeters < 0.f || d.distance < nearestEnemyMeters)) {
                nearestEnemyMeters = d.distance;
            }
        }

        gameState.nearestEnemyDist.store(nearestEnemyMeters);
        overlay.UpdatePlayers(players, count, localTeam);

        std::this_thread::sleep_for(std::chrono::milliseconds(7));
    }

    overlay.Stop();

    overlayThread.join();
    menuThread.join();

    // Final persist — belt-and-suspenders; menu already saves on every toggle.
    persist.Save(espCfg);

    return 0;
}

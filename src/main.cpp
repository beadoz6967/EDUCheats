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
#include "gui/overlay.hpp"

static std::atomic<bool> g_running{ true };

static constexpr float kUnitsToMeters = 0.01905f;

static float Distance3D(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

static void EnableDpiAwareness() {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (!u) return;
    // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = (HANDLE)-4
    typedef BOOL(WINAPI* PFN)(HANDLE);
    if (auto fn = (PFN)GetProcAddress(u, "SetProcessDpiAwarenessContext"))
        fn((HANDLE)-4);
    else if (auto fn2 = (BOOL(WINAPI*)())GetProcAddress(u, "SetProcessDPIAware"))
        fn2();
}

static bool AllocateConsoleWindow() {
    if (GetConsoleWindow()) return true;
    if (!AllocConsole()) return false;
    FILE* dummy = nullptr;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
    freopen_s(&dummy, "CONIN$",  "r", stdin);
    SetConsoleTitleA("EDUCheats // log");
    return true;
}

int main() {
    EnableDpiAwareness();
    AllocateConsoleWindow();

    printf("==============================================\n");
    printf("  EDUCheats — External Overlay\n");
    printf("  [INSERT] toggle menu in-overlay\n");
    printf("  [END]    exit\n");
    printf("==============================================\n\n");

    Memory mem;
    uintptr_t clientBase = 0;

    printf("[boot] Waiting for cs2.exe...\n");
    while (true) {
        if (GetAsyncKeyState(VK_END) & 0x8000) return 0;

        if (!mem.IsAttached()) {
            if (!mem.Attach("cs2.exe")) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            printf("[boot] cs2.exe attached (pid %lu). Waiting for client.dll...\n", mem.GetPID());
        }

        clientBase = mem.GetModuleBase("client.dll");
        if (clientBase) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    printf("[boot] client.dll base: 0x%llX\n", static_cast<unsigned long long>(clientBase));

    ESPConfig espCfg;
    GameState gameState;
    Config    persist;
    persist.Load(espCfg);
    printf("[boot] Config loaded from %s\n", persist.Path().c_str());

    Overlay overlay(mem, clientBase, espCfg, gameState, persist);

    // Overlay runs on its own thread so the entity scan loop never blocks
    // on Present() or message processing.
    std::thread overlayThread([&]() {
        overlay.Run(g_running);
    });

    int lastEntityLog = -1;
    int loggedFrames  = 0;

    while (g_running) {
        uintptr_t entityListBase = mem.Read<uintptr_t>(clientBase + offsets::dwEntityList);
        CEntityList entityList(entityListBase, mem);

        uintptr_t localControllerPtr = mem.Read<uintptr_t>(clientBase + offsets::dwLocalPlayerController);
        int localTeam = 0;
        if (localControllerPtr) {
            CCSPlayerController localCtrl(localControllerPtr, mem);
            localTeam = localCtrl.GetTeamNum();
        }


        Vector3 localOrigin{};
        uintptr_t localPawnPtr = mem.Read<uintptr_t>(clientBase + offsets::dwLocalPlayerPawn);
        if (localPawnPtr) {
            C_CSPlayerPawn localPawn(localPawnPtr, mem);
            localOrigin = localPawn.GetOrigin();
        }

        // Read the view matrix here so the overlay always has a fresh copy
        ViewMatrix view{};
        mem.ReadBuffer(clientBase + offsets::dwViewMatrix, &view, sizeof(view));

        // Heuristic: a fresh, populated matrix has non-zero w-row entries.
        // If everything is zero the offset is stale, the user is in the
        // main menu, or the dll just unloaded.
        bool matrixOk = (view.m[3][0] != 0.f || view.m[3][1] != 0.f ||
                          view.m[3][2] != 0.f || view.m[3][3] != 0.f);
        gameState.matrixOk.store(matrixOk);

        PlayerESPData players[64]{};
        int count = 0;
        float nearestEnemyMeters = -1.f;

        for (int i = 1; i <= 128; ++i) {
            uintptr_t ctrlPtr = entityList.GetController(i);
            if (!ctrlPtr || ctrlPtr == localControllerPtr) continue;

            CCSPlayerController ctrl(ctrlPtr, mem);
            int team = ctrl.GetTeamNum();
            if (team != 2 && team != 3) continue;

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
        gameState.entityCount.store(count);
        overlay.PushPlayers(players, count, localTeam, view);

        // Periodic status log every ~2 seconds when state changes meaningfully
        ++loggedFrames;
        if (loggedFrames >= 256) {
            loggedFrames = 0;
            if (count != lastEntityLog) {
                printf("[scan] players=%d matrixOk=%d nearest=%.1fm\n",
                       count, matrixOk ? 1 : 0,
                       nearestEnemyMeters < 0.f ? 0.f : nearestEnemyMeters);
                lastEntityLog = count;
            }
        }

        if (GetAsyncKeyState(VK_END) & 0x8000) g_running = false;

        std::this_thread::sleep_for(std::chrono::milliseconds(7));
    }

    overlayThread.join();
    persist.Save(espCfg);
    printf("[boot] Shutdown clean.\n");
    return 0;
}

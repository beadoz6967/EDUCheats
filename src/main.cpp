#include <Windows.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

#include "memory.hpp"
#include "offsets.hpp"
#include "sdk.hpp"
#include "config.hpp"
#include "features/esp.hpp"
#include "gui/overlay.hpp"

static std::atomic<bool> g_running{ true };

static constexpr float kUnitsToMeters = 0.01905f;

static std::string ResolveDebugPath() {
    char buf[MAX_PATH]{};
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return "bone_debug.txt";

    std::string exe(buf, len);
    const auto slash = exe.find_last_of("\\/");
    if (slash == std::string::npos) return "bone_debug.txt";
    return exe.substr(0, slash + 1) + "bone_debug.txt";
}

static void DeleteDebugFileIfPresent(const std::string& path) {
    DeleteFileA(path.c_str());
}

static std::string HexDump(const uint8_t* data, size_t size, uintptr_t baseAddress) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; i += 16) {
        out << "0x" << std::setw(8) << static_cast<unsigned long long>(baseAddress + i) << ": ";
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                out << std::setw(2) << static_cast<int>(data[i + j]) << ' ';
            } else {
                out << "   ";
            }
        }
        out << "| ";
        for (size_t j = 0; j < 16 && i + j < size; ++j) {
            uint8_t c = data[i + j];
            out << ((c >= 32 && c <= 126) ? static_cast<char>(c) : '.');
        }
        out << '\n';
    }
    return out.str();
}

struct BoneData {
    Vector3 pos;
    uint8_t pad[0x14]{};
};

static void WriteBoneDebugFile(const std::string& path,
                               const PlayerESPData players[64],
                               int count,
                               int localTeam,
                               bool matrixOk,
                               float nearestEnemyMeters,
                               const Vector3& localOrigin) {
    if (count <= 0) {
        DeleteDebugFileIfPresent(path);
        return;
    }

    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        printf("[debug] failed to open %s (err=%lu)\n", path.c_str(), GetLastError());
        return;
    }

    static bool loggedPath = false;
    if (!loggedPath) {
        printf("[debug] writing bone log to %s\n", path.c_str());
        loggedPath = true;
    }

    SYSTEMTIME st{};
    GetLocalTime(&st);

    out << "EDUCheats bone debug\n";
    out << "timestamp=" << std::setfill('0')
        << std::setw(2) << st.wHour << ":"
        << std::setw(2) << st.wMinute << ":"
        << std::setw(2) << st.wSecond << "."
        << std::setw(3) << st.wMilliseconds << "\n";
    out << "players_visible=" << count << "\n";
    out << "local_team=" << localTeam << "\n";
    out << "matrix_ok=" << (matrixOk ? 1 : 0) << "\n";
    out << "nearest_enemy_meters=" << (nearestEnemyMeters < 0.f ? 0.f : nearestEnemyMeters) << "\n";
    out << std::fixed << std::setprecision(2);
    out << "local_origin=" << localOrigin.x << "," << localOrigin.y << "," << localOrigin.z << "\n\n";

    for (int i = 0; i < count; ++i) {
        const PlayerESPData& p = players[i];
        out << "[player " << i << "]\n";
        out << "name=" << p.name << "\n";
        out << "alive=" << (p.alive ? 1 : 0) << " enemy=" << (p.isEnemy ? 1 : 0) << " health=" << p.health << " distance_m=" << p.distance << "\n";
        out << "origin=" << p.origin.x << "," << p.origin.y << "," << p.origin.z << "\n";
        out << "head=" << p.headPos.x << "," << p.headPos.y << "," << p.headPos.z << "\n";
        out << "boneCount=" << p.boneCount << "\n";
        for (int b = 0; b < p.boneCount && b < 64; ++b) {
            out << "bone[" << b << "]=" << p.bones[b].x << "," << p.bones[b].y << "," << p.bones[b].z << "\n";
        }
        out << "\n";
    }

    out.flush();
}

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
    const std::string boneDebugPath = ResolveDebugPath();

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
            d.boneCount = 1;
            d.bones[0]  = d.origin;

            uintptr_t gameScene = mem.Read<uintptr_t>(pawnPtr + client::C_CSPlayerPawn::m_pGameSceneNode);
            if (gameScene) {
                uintptr_t boneArray = mem.Read<uintptr_t>(gameScene + client::CGameSceneNode::m_modelState + 0x80);
                if (boneArray) {
                    BoneData rawBones[30]{};
                    if (mem.ReadBuffer(boneArray, rawBones, sizeof(rawBones))) {
                        d.boneCount = 30;
                        for (int b = 0; b < 30; ++b) {
                            d.bones[b] = rawBones[b].pos;
                        }

                        constexpr int kHead = 7;
                        if (kHead < d.boneCount) {
                            d.headPos = d.bones[kHead];
                        }
                    }
                }
            }

            d.distance = Distance3D(localOrigin, d.origin) * kUnitsToMeters;
            ++count;

            if (d.isEnemy && (nearestEnemyMeters < 0.f || d.distance < nearestEnemyMeters)) {
                nearestEnemyMeters = d.distance;
            }
        }

        gameState.nearestEnemyDist.store(nearestEnemyMeters);
        gameState.entityCount.store(count);
        overlay.PushPlayers(players, count, localTeam, view);
        WriteBoneDebugFile(boneDebugPath, players, count, localTeam, matrixOk, nearestEnemyMeters, localOrigin);

        // Periodic status log every ~2 seconds when state changes meaningfully
        ++loggedFrames;
        if (loggedFrames >= 256) {
            loggedFrames = 0;
            if (count != lastEntityLog) {
                printf("[scan] players=%d matrixOk=%d nearest=%.1fm\n",
                       count, matrixOk ? 1 : 0,
                       nearestEnemyMeters < 0.f ? 0.f : nearestEnemyMeters);
                if (count > 0) {
                    PlayerESPData& pd = players[0];
                    printf("[scan] first player boneCount=%d\n", pd.boneCount);
                    if (pd.boneCount > 0) {
                        Vector3 b0 = pd.bones[0];
                        printf("[scan] first bone[0]=%.2f,%.2f,%.2f\n", b0.x, b0.y, b0.z);
                    }
                }
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

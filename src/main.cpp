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

struct BoneDebugEntry {
    std::string name;
    uint16_t eyeAttach = 0;
    uint16_t chestAttach = 0;
    uint16_t leftFootAttach = 0;
    uint16_t rightFootAttach = 0;
    bool eyeResolved = false;
    bool chestResolved = false;
    bool leftFootResolved = false;
    bool rightFootResolved = false;
    Vector3 eyePos{};
    Vector3 chestPos{};
    Vector3 leftFootPos{};
    Vector3 rightFootPos{};
    std::string eyeTrace;
    std::string chestTrace;
    std::string leftFootTrace;
    std::string rightFootTrace;
};

static void WriteBoneDebugFile(const std::string& path,
                               const PlayerESPData players[64],
                               const BoneDebugEntry debugEntries[64],
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
    if (!out.is_open()) return;

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
        const BoneDebugEntry& d = debugEntries[i];
        out << "eyeAttach=" << d.eyeAttach << " resolved=" << (d.eyeResolved ? 1 : 0)
            << " pos=" << d.eyePos.x << "," << d.eyePos.y << "," << d.eyePos.z << "\n";
        out << "chestAttach=" << d.chestAttach << " resolved=" << (d.chestResolved ? 1 : 0)
            << " pos=" << d.chestPos.x << "," << d.chestPos.y << "," << d.chestPos.z << "\n";
        out << "leftFootAttach=" << d.leftFootAttach << " resolved=" << (d.leftFootResolved ? 1 : 0)
            << " pos=" << d.leftFootPos.x << "," << d.leftFootPos.y << "," << d.leftFootPos.z << "\n";
        out << "rightFootAttach=" << d.rightFootAttach << " resolved=" << (d.rightFootResolved ? 1 : 0)
            << " pos=" << d.rightFootPos.x << "," << d.rightFootPos.y << "," << d.rightFootPos.z << "\n";
        out << "[eye trace]\n" << d.eyeTrace << "\n";
        out << "[chest trace]\n" << d.chestTrace << "\n";
        out << "[leftFoot trace]\n" << d.leftFootTrace << "\n";
        out << "[rightFoot trace]\n" << d.rightFootTrace << "\n";
        for (int b = 0; b < p.boneCount && b < 64; ++b) {
            out << "bone[" << b << "]=" << p.bones[b].x << "," << p.bones[b].y << "," << p.bones[b].z << "\n";
        }
        out << "\n";
    }
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
        BoneDebugEntry boneDebug[64]{};
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
            BoneDebugEntry& bd = boneDebug[count];
            d.alive    = true;
            d.isEnemy  = (team != localTeam);
            d.health   = std::clamp(pawn.GetHealth(), 0, 100);
            d.name     = ctrl.GetName();
            bd.name    = d.name;
            d.origin   = pawn.GetOrigin();
            d.headPos  = { d.origin.x, d.origin.y, d.origin.z + 72.f };
            // Attempt to resolve attachment-based bone positions. Read
            // attachment handles (uint16) from the pawn structure and
            // query the scene node tree for matching nodes.
            d.boneCount = 0;
            bd.eyeAttach = mem.Read<uint16_t>(pawnPtr + client::C_CSPlayerPawn::m_eyeAttachment);
            bd.chestAttach = mem.Read<uint16_t>(pawnPtr + client::C_CSPlayerPawn::m_chestAttachment);
            bd.leftFootAttach = mem.Read<uint16_t>(pawnPtr + client::C_BaseCombatCharacter::m_leftFootAttachment);
            bd.rightFootAttach = mem.Read<uint16_t>(pawnPtr + client::C_BaseCombatCharacter::m_rightFootAttachment);

            Vector3 tmp;
            bd.eyeResolved = pawn.GetAttachmentWorldPosDebug(bd.eyeAttach, bd.eyePos, bd.eyeTrace);
            bd.chestResolved = pawn.GetAttachmentWorldPosDebug(bd.chestAttach, bd.chestPos, bd.chestTrace);
            bd.leftFootResolved = pawn.GetAttachmentWorldPosDebug(bd.leftFootAttach, bd.leftFootPos, bd.leftFootTrace);
            bd.rightFootResolved = pawn.GetAttachmentWorldPosDebug(bd.rightFootAttach, bd.rightFootPos, bd.rightFootTrace);

            if (bd.eyeResolved) { d.bones[d.boneCount++] = bd.eyePos; d.headPos = bd.eyePos; }
            if (bd.chestResolved) { d.bones[d.boneCount++] = bd.chestPos; }
            if (bd.leftFootResolved) { d.bones[d.boneCount++] = bd.leftFootPos; }
            if (bd.rightFootResolved) { d.bones[d.boneCount++] = bd.rightFootPos; }
            // Add pelvis/origin as fallback bone
            d.bones[d.boneCount++] = d.origin;
            d.distance = Distance3D(localOrigin, d.origin) * kUnitsToMeters;
            ++count;

            if (d.isEnemy && (nearestEnemyMeters < 0.f || d.distance < nearestEnemyMeters)) {
                nearestEnemyMeters = d.distance;
            }
        }

        gameState.nearestEnemyDist.store(nearestEnemyMeters);
        gameState.entityCount.store(count);
        overlay.PushPlayers(players, count, localTeam, view);
        WriteBoneDebugFile(boneDebugPath, players, boneDebug, count, localTeam, matrixOk, nearestEnemyMeters, localOrigin);

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

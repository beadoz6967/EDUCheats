#include <Windows.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>

#include "offsets.hpp"
#include "sdk.hpp"

// client.dll.hpp is fetched by the pre-build script from a2x/cs2-dumper.
// It provides cs2_dumper::schemas::client_dll:: namespace for chams offsets.
// __has_include lets this TU compile even on first checkout before the fetch.
#if __has_include("client.dll.hpp")
#include "client.dll.hpp"
#define EDU_HAS_CS2_SCHEMAS 1
#endif
#include "config.hpp"
#include "features/esp.hpp"
#include "features/aimbot.hpp"
#include "gui/overlay.hpp"

HMODULE g_hModule = nullptr;

static std::atomic<bool> g_running{ true };
static constexpr float kUnitsToMeters = 0.01905f;

// Bone memory layout matches CS2's model-state array element
struct BoneData {
    Vector3 pos;
    uint8_t pad[0x14]{};
};

static std::string ResolveDebugPath() {
    char buf[MAX_PATH]{};
    DWORD len = GetModuleFileNameA(g_hModule, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return "bone_debug.txt";
    std::string exe(buf, len);
    const auto slash = exe.find_last_of("\\/");
    if (slash == std::string::npos) return "bone_debug.txt";
    return exe.substr(0, slash + 1) + "bone_debug.txt";
}

static void DeleteDebugFileIfPresent(const std::string& path) {
    DeleteFileA(path.c_str());
}

static float Distance3D(const Vector3& a, const Vector3& b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

static void WriteBoneDebugFile(const std::string& path,
                               const PlayerESPData players[64], int count,
                               int localTeam, bool matrixOk,
                               float nearestEnemyMeters, const Vector3& localOrigin)
{
    if (count <= 0) { DeleteDebugFileIfPresent(path); return; }
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return;

    SYSTEMTIME st{};
    GetLocalTime(&st);
    out << "EDUCheats bone debug\n"
        << "timestamp=" << std::setfill('0')
        << std::setw(2) << st.wHour   << ":"
        << std::setw(2) << st.wMinute << ":"
        << std::setw(2) << st.wSecond << "."
        << std::setw(3) << st.wMilliseconds << "\n"
        << "players_visible=" << count << "\n"
        << "local_team=" << localTeam << "\n"
        << "matrix_ok=" << (matrixOk ? 1 : 0) << "\n"
        << "nearest_enemy_meters=" << (nearestEnemyMeters < 0.f ? 0.f : nearestEnemyMeters) << "\n"
        << std::fixed << std::setprecision(2)
        << "local_origin=" << localOrigin.x << "," << localOrigin.y << "," << localOrigin.z << "\n\n";

    for (int i = 0; i < count; ++i) {
        const PlayerESPData& p = players[i];
        out << "[player " << i << "]\n"
            << "name=" << p.name << "\n"
            << "alive=" << (p.alive ? 1 : 0) << " enemy=" << (p.isEnemy ? 1 : 0)
            << " health=" << p.health << " distance_m=" << p.distance << "\n"
            << "origin=" << p.origin.x << "," << p.origin.y << "," << p.origin.z << "\n"
            << "head=" << p.headPos.x << "," << p.headPos.y << "," << p.headPos.z << "\n"
            << "boneCount=" << p.boneCount << "\n";
        for (int b = 0; b < p.boneCount && b < 64; ++b)
            out << "bone[" << b << "]=" << p.bones[b].x << "," << p.bones[b].y << "," << p.bones[b].z << "\n";
        out << "\n";
    }
    out.flush();
}

static DWORD WINAPI MainThread(LPVOID) {
    // Step-1 injection confirmation — remove after verified
    MessageBoxA(nullptr, "EDUCheats injected", "EDUCheats",
                MB_OK | MB_ICONINFORMATION | MB_TOPMOST);

    // Wait for CS2's client.dll to be loaded before touching any game memory
    uintptr_t clientBase = 0;
    while (!clientBase && g_running.load()) {
        clientBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("client.dll"));
        if (!clientBase) Sleep(500);
    }
    if (!g_running.load()) {
        FreeLibraryAndExitThread(g_hModule, 0);
        return 0;
    }

    ESPConfig    espCfg;
    AimbotConfig aimbotCfg;
    GameState    gameState;
    Config       persist;
    persist.Load(espCfg, aimbotCfg);

    // Install Present hook — hkPresent runs on CS2's render thread from here on
    g_overlay.Install(espCfg, aimbotCfg, gameState, persist);

    const std::string boneDebugPath = ResolveDebugPath();
    int lastEntityLog = -1;
    int loggedFrames  = 0;

    while (g_running.load()) {
        uintptr_t entityListBase = *reinterpret_cast<uintptr_t*>(clientBase + offsets::dwEntityList);
        CEntityList entityList(entityListBase);

        uintptr_t localControllerPtr = *reinterpret_cast<uintptr_t*>(
            clientBase + offsets::dwLocalPlayerController);
        int localTeam = 0;
        if (localControllerPtr) {
            CCSPlayerController localCtrl(localControllerPtr);
            localTeam = localCtrl.GetTeamNum();
        }

        Vector3 localOrigin{}, eyePos{}, viewAngles{};
        uintptr_t localPawnPtr = *reinterpret_cast<uintptr_t*>(
            clientBase + offsets::dwLocalPlayerPawn);
        if (localPawnPtr) {
            C_CSPlayerPawn localPawn(localPawnPtr);
            localOrigin = localPawn.GetOrigin();

            uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(
                localPawnPtr + client::C_CSPlayerPawn::m_pGameSceneNode);
            if (sceneNode) {
                Vector3 snOrigin = *reinterpret_cast<Vector3*>(
                    sceneNode + client::CGameSceneNode::m_vecAbsOrigin);
                Vector3 viewOff  = *reinterpret_cast<Vector3*>(
                    localPawnPtr + client::C_CSPlayerPawn::m_vecViewOffset);
                eyePos = { snOrigin.x + viewOff.x, snOrigin.y + viewOff.y, snOrigin.z + viewOff.z };
            } else {
                eyePos = { localOrigin.x, localOrigin.y, localOrigin.z + 64.f };
            }
        }
        viewAngles = *reinterpret_cast<Vector3*>(clientBase + offsets::dwViewAngles);

        ViewMatrix view{};
        memcpy(&view, reinterpret_cast<void*>(clientBase + offsets::dwViewMatrix), sizeof(view));

        bool matrixOk = (view.m[3][0] != 0.f || view.m[3][1] != 0.f ||
                         view.m[3][2] != 0.f || view.m[3][3] != 0.f);
        gameState.matrixOk.store(matrixOk);

        PlayerESPData players[64]{};
        int count = 0;
        float nearestEnemyMeters = -1.f;

        for (int i = 1; i <= 128; ++i) {
            uintptr_t ctrlPtr = entityList.GetController(i);
            if (!ctrlPtr || ctrlPtr == localControllerPtr) continue;

            CCSPlayerController ctrl(ctrlPtr);
            int team = ctrl.GetTeamNum();
            if (team != 2 && team != 3) continue;

            uint32_t pawnHandle = ctrl.GetPawnHandle();
            uintptr_t pawnPtr   = entityList.HandleToPtr(pawnHandle);
            if (!pawnPtr) continue;

            C_CSPlayerPawn pawn(pawnPtr);
            if (!pawn.IsAlive()) continue;

            PlayerESPData& d = players[count];
            d.alive   = true;
            d.isEnemy = (team != localTeam);
            d.health  = std::clamp(pawn.GetHealth(), 0, 100);
            d.name    = ctrl.GetName();
            d.origin  = pawn.GetOrigin();
            d.headPos = { d.origin.x, d.origin.y, d.origin.z + 72.f };
            d.boneCount = 1;
            d.bones[0]  = d.origin;

            uintptr_t gameScene = *reinterpret_cast<uintptr_t*>(
                pawnPtr + client::C_CSPlayerPawn::m_pGameSceneNode);
            if (gameScene) {
                uintptr_t boneArray = *reinterpret_cast<uintptr_t*>(
                    gameScene + client::CGameSceneNode::m_modelState + 0x80);
                if (boneArray) {
                    BoneData rawBones[30]{};
                    memcpy(rawBones, reinterpret_cast<void*>(boneArray), sizeof(rawBones));
                    d.boneCount = 30;
                    for (int b = 0; b < 30; ++b)
                        d.bones[b] = rawBones[b].pos;
                    constexpr int kHead = 7;
                    if (kHead < d.boneCount)
                        d.headPos = d.bones[kHead];
                }
            }

            // Chams: write glow to make engine render an RGBA outline around enemies.
            // Offsets come from client.dll.hpp auto-fetched by pre-build script.
#if defined(EDU_HAS_CS2_SCHEMAS)
            if (d.isEnemy) {
                namespace cs2cli = cs2_dumper::schemas::client_dll;
                uintptr_t glowPropPtr = *reinterpret_cast<uintptr_t*>(
                    pawnPtr + cs2cli::C_CSPlayerPawn::m_pGlowProperty);
                if (glowPropPtr) {
                    float* col = reinterpret_cast<float*>(
                        glowPropPtr + cs2cli::CCSPlayerGlowProperty::m_glowColorOverride);
                    col[0] = 1.f; // R
                    col[1] = 0.f; // G
                    col[2] = 0.f; // B
                    col[3] = 1.f; // A
                    *reinterpret_cast<bool*>(
                        glowPropPtr + cs2cli::CCSPlayerGlowProperty::m_bGlowing) = true;
                }
            }
#endif

            d.distance = Distance3D(localOrigin, d.origin) * kUnitsToMeters;
            ++count;

            if (d.isEnemy && (nearestEnemyMeters < 0.f || d.distance < nearestEnemyMeters))
                nearestEnemyMeters = d.distance;
        }

        gameState.nearestEnemyDist.store(nearestEnemyMeters);
        gameState.entityCount.store(count);
        g_overlay.PushPlayers(players, count, localTeam, view);
        WriteBoneDebugFile(boneDebugPath, players, count, localTeam, matrixOk,
                           nearestEnemyMeters, localOrigin);

        if (aimbotCfg.enabled.load() && matrixOk && localPawnPtr) {
            C_CSPlayerPawn localPawn(localPawnPtr);
            if (localPawn.IsAlive() && (GetAsyncKeyState(aimbotCfg.key.load()) & 0x8000)) {
                bool  rage    = aimbotCfg.rageMode.load();
                float fov     = rage ? 360.f : aimbotCfg.fov.load();
                float smooth  = rage ? 1.f   : aimbotCfg.smooth.load();
                int   boneIdx = aimbotCfg.boneTarget.load();

                int target = aimbot::SelectTarget(players, count, eyePos, viewAngles, fov, boneIdx);
                if (target >= 0) {
                    Vector3 aimed = aimbot::CalcAngle(eyePos, players[target].bones[boneIdx]);
                    Vector3 final = aimbot::SmoothAngle(viewAngles, aimed, smooth);
                    aimbot::NormalizeAngles(final);
                    // Direct write — we're inside the process
                    *reinterpret_cast<Vector3*>(clientBase + offsets::dwViewAngles) = final;
                }
            }
        }

        ++loggedFrames;
        if (loggedFrames >= 256) {
            loggedFrames = 0;
            lastEntityLog = count; // suppress unused-variable warning
        }

        if (GetAsyncKeyState(VK_END) & 0x8000)
            g_running.store(false);

        Sleep(7);
    }

    g_overlay.Uninstall();
    persist.Save(espCfg, aimbotCfg);
    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hModule = hMod;
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}

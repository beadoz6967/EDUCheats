#pragma once
#include <cstdint>

// Generated from a2x/cs2-dumper — update when CS2 patches
// Fetched: 2026-05-14

namespace offsets {

    // client.dll — global pointers (add to client.dll base)
    constexpr uintptr_t dwEntityList           = 0x24D6FC0;  // 38604224
    constexpr uintptr_t dwLocalPlayerController = 0x230CEF0; // 36742384
    constexpr uintptr_t dwLocalPlayerPawn       = 0x204E100; // 33908480
    constexpr uintptr_t dwViewMatrix            = 0x232E8E0; // 36899552

} // namespace offsets

namespace client {

    // CCSPlayerController
    namespace CCSPlayerController {
        constexpr uintptr_t m_iszPlayerName = 0x6F4;  // 1780
        constexpr uintptr_t m_iTeamNum      = 0x3EB;  // 1003 — inherited from C_BaseEntity
        constexpr uintptr_t m_hPlayerPawn   = 0x90C;  // 2316 — CHandle<C_CSPlayerPawn>
    }

    // C_CSPlayerPawn
    namespace C_CSPlayerPawn {
        constexpr uintptr_t m_pGameSceneNode = 0x330; // 816
        constexpr uintptr_t m_iHealth        = 0x34C; // 844
        constexpr uintptr_t m_lifeState      = 0x354; // 852 — 256 = alive
        constexpr uintptr_t m_vOldOrigin     = 0x1390;// 5008 — Vector3, used as fallback origin
    }

    // CGameSceneNode
    namespace CGameSceneNode {
        constexpr uintptr_t m_vecAbsOrigin = 0xC8; // 200 — Vector3 world position
    }

} // namespace client

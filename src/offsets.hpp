#pragma once
#include <cstdint>

// Generated from a2x/cs2-dumper — update when CS2 patches
// Fetched: 2026-05-15

namespace offsets {

    // client.dll — global pointers (add to client.dll base)
    constexpr uintptr_t dwEntityList           = 0x24D4E80;
    constexpr uintptr_t dwLocalPlayerController = 0x230E5C0;
    constexpr uintptr_t dwLocalPlayerPawn       = 0x205A700;
    constexpr uintptr_t dwViewMatrix            = 0x2334850;

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
        // Attachment handles (from dumper): eye/chest
        constexpr uintptr_t m_eyeAttachment  = 0x1299; // AttachmentHandle_t (uint16)
        constexpr uintptr_t m_chestAttachment = 0x129A; // AttachmentHandle_t (uint16)
    }

    // CGameSceneNode
    namespace CGameSceneNode {
        constexpr uintptr_t m_vecAbsOrigin = 0xC8; // 200 — Vector3 world position
        constexpr uintptr_t m_modelState = 0x150; // CModelState
        constexpr uintptr_t m_pChild = 0x40; // child pointer
        constexpr uintptr_t m_pNextSibling = 0x48; // next sibling pointer
        constexpr uintptr_t m_nParentAttachmentOrBone = 0x100; // int16
    }

    // C_BaseCombatCharacter (used by player pawn inheritance)
    namespace C_BaseCombatCharacter {
        constexpr uintptr_t m_leftFootAttachment  = 0x1170; // AttachmentHandle_t
        constexpr uintptr_t m_rightFootAttachment = 0x1171; // AttachmentHandle_t
    }

} // namespace client

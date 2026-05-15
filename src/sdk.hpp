#pragma once
#include "memory.hpp"
#include "offsets.hpp"
#include <string>

struct Vector3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

struct Vector2 {
    float x = 0.f, y = 0.f;
};

// 4x4 row-major view matrix from dwViewMatrix
struct ViewMatrix {
    float m[4][4]{};
};

// Thin wrappers — all fields read on demand to stay in sync with game state.

class CGameSceneNode {
public:
    CGameSceneNode(uintptr_t base, const Memory& mem) : m_base(base), m_mem(mem) {}

    Vector3 GetAbsOrigin() const {
        return m_mem.Read<Vector3>(m_base + client::CGameSceneNode::m_vecAbsOrigin);
    }

    uintptr_t GetChild() const {
        return m_mem.Read<uintptr_t>(m_base + client::CGameSceneNode::m_pChild);
    }

    uintptr_t GetNextSibling() const {
        return m_mem.Read<uintptr_t>(m_base + client::CGameSceneNode::m_pNextSibling);
    }

    int16_t GetParentAttachmentOrBone() const {
        return m_mem.Read<int16_t>(m_base + client::CGameSceneNode::m_nParentAttachmentOrBone);
    }

private:
    uintptr_t    m_base;
    const Memory& m_mem;
};


class C_CSPlayerPawn {
public:
    C_CSPlayerPawn(uintptr_t base, const Memory& mem) : m_base(base), m_mem(mem) {}

    bool IsValid()    const { return m_base != 0; }
    uintptr_t Base()  const { return m_base; }

    int GetHealth()    const { return m_mem.Read<int>(m_base + client::C_CSPlayerPawn::m_iHealth); }
    int GetLifeState() const { return m_mem.Read<int>(m_base + client::C_CSPlayerPawn::m_lifeState); }
    bool IsAlive()     const { return GetLifeState() == 256; }

    Vector3 GetOrigin() const {
        uintptr_t sceneNode = m_mem.Read<uintptr_t>(m_base + client::C_CSPlayerPawn::m_pGameSceneNode);
        if (!sceneNode) return m_mem.Read<Vector3>(m_base + client::C_CSPlayerPawn::m_vOldOrigin);
        return CGameSceneNode(sceneNode, m_mem).GetAbsOrigin();
    }

    // Try to resolve an attachment handle to a world-space position by
    // walking the CGameSceneNode subtree and matching nodes whose
    // m_nParentAttachmentOrBone equals the attachment handle.
    // Returns true and fills out on success; false otherwise.
    bool GetAttachmentWorldPos(uint16_t attachmentHandle, Vector3& out) const {
        if (attachmentHandle == 0) return false;
        uintptr_t sceneNodePtr = m_mem.Read<uintptr_t>(m_base + client::C_CSPlayerPawn::m_pGameSceneNode);
        if (!sceneNodePtr) return false;

        // Simple stack-based traversal (depth-first). Limit iterations for safety.
        uintptr_t stack[512];
        int sp = 0;
        stack[sp++] = sceneNodePtr;
        int iter = 0;
        while (sp > 0 && iter++ < 2000) {
            uintptr_t nodePtr = stack[--sp];
            if (!nodePtr) continue;

            CGameSceneNode node(nodePtr, m_mem);
            int16_t parentAttach = node.GetParentAttachmentOrBone();
            if (parentAttach == static_cast<int16_t>(attachmentHandle)) {
                out = node.GetAbsOrigin();
                return true;
            }

            // push siblings and children
            uintptr_t sib = node.GetNextSibling();
            uintptr_t child = node.GetChild();
            if (sib) { if (sp < 512) stack[sp++] = sib; }
            if (child) { if (sp < 512) stack[sp++] = child; }
        }
        return false;
    }

private:
    uintptr_t     m_base;
    const Memory& m_mem;
};


class CCSPlayerController {
public:
    CCSPlayerController(uintptr_t base, const Memory& mem) : m_base(base), m_mem(mem) {}

    bool IsValid()   const { return m_base != 0; }
    uintptr_t Base() const { return m_base; }

    int GetTeamNum() const {
        return m_mem.Read<uint8_t>(m_base + client::CCSPlayerController::m_iTeamNum);
    }

    std::string GetName() const {
        return m_mem.ReadString(m_base + client::CCSPlayerController::m_iszPlayerName, 128);
    }

    // Resolves the CHandle to a C_CSPlayerPawn pointer via the entity list.
    // handle format: high 23 bits = serial, low 15 bits = index (actually index << ? — see GetEntity)
    uint32_t GetPawnHandle() const {
        return m_mem.Read<uint32_t>(m_base + client::CCSPlayerController::m_hPlayerPawn);
    }

private:
    uintptr_t     m_base;
    const Memory& m_mem;
};


class CEntityList {
public:
    CEntityList(uintptr_t base, const Memory& mem) : m_base(base), m_mem(mem) {}

    // Returns the controller ptr at entity index i (1-based, players at 1..64)
    uintptr_t GetController(int index) const {
        // CS2 chunked entity system:
        //   chunk_array[chunkIdx] → chunk ptr
        //   chunk[entityIdx] → entity ptr (each slot 0x10 bytes)
        uintptr_t chunk = m_mem.Read<uintptr_t>(m_base + 0x10 + 8 * (index >> 9));
        if (!chunk) return 0;
        return m_mem.Read<uintptr_t>(chunk + 0x10 * (index & 0x1FF));
    }

    // Resolve a CHandle (u32) to a pawn pointer through the entity list
    // CEntityIdentity stride = 0x70; m_pEntity at +0x00; handle low-15 bits = direct identity index
    uintptr_t HandleToPtr(uint32_t handle) const {
        if (handle == 0xFFFFFFFF || handle == 0) return 0;
        int index = handle & 0x7FFF;
        uintptr_t chunk = m_mem.Read<uintptr_t>(m_base + 0x10 + 8 * (index >> 9));
        if (!chunk) return 0;
        return m_mem.Read<uintptr_t>(chunk + 0x70 * (index & 0x1FF));
    }

private:
    uintptr_t     m_base;
    const Memory& m_mem;
};

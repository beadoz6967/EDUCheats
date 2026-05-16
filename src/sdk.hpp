#pragma once
#include "offsets.hpp"
#include <string>
#include <cstring>

// Internal DLL — all reads are direct pointer dereferences into our own VA space.
// Memory class is gone; every m_mem.Read<T>(addr) becomes *reinterpret_cast<T*>(addr).

struct Vector3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

struct Vector2 {
    float x = 0.f, y = 0.f;
};

struct ViewMatrix {
    float m[4][4]{};
};


class CGameSceneNode {
public:
    explicit CGameSceneNode(uintptr_t base) : m_base(base) {}

    Vector3 GetAbsOrigin() const {
        return *reinterpret_cast<Vector3*>(m_base + client::CGameSceneNode::m_vecAbsOrigin);
    }

    uintptr_t GetChild() const {
        return *reinterpret_cast<uintptr_t*>(m_base + client::CGameSceneNode::m_pChild);
    }

    uintptr_t GetNextSibling() const {
        return *reinterpret_cast<uintptr_t*>(m_base + client::CGameSceneNode::m_pNextSibling);
    }

    int16_t GetParentAttachmentOrBone() const {
        return *reinterpret_cast<int16_t*>(m_base + client::CGameSceneNode::m_nParentAttachmentOrBone);
    }

private:
    uintptr_t m_base;
};


class C_CSPlayerPawn {
public:
    explicit C_CSPlayerPawn(uintptr_t base) : m_base(base) {}

    bool IsValid()    const { return m_base != 0; }
    uintptr_t Base()  const { return m_base; }

    int GetHealth()    const { return *reinterpret_cast<int*>(m_base + client::C_CSPlayerPawn::m_iHealth); }
    int GetLifeState() const { return *reinterpret_cast<int*>(m_base + client::C_CSPlayerPawn::m_lifeState); }
    bool IsAlive()     const { return GetLifeState() == 256; }

    Vector3 GetOrigin() const {
        uintptr_t sceneNode = *reinterpret_cast<uintptr_t*>(m_base + client::C_CSPlayerPawn::m_pGameSceneNode);
        if (!sceneNode)
            return *reinterpret_cast<Vector3*>(m_base + client::C_CSPlayerPawn::m_vOldOrigin);
        return CGameSceneNode(sceneNode).GetAbsOrigin();
    }

    bool GetAttachmentWorldPos(uint16_t attachmentHandle, Vector3& out) const {
        if (attachmentHandle == 0) return false;
        uintptr_t sceneNodePtr = *reinterpret_cast<uintptr_t*>(
            m_base + client::C_CSPlayerPawn::m_pGameSceneNode);
        if (!sceneNodePtr) return false;

        uintptr_t stack[512];
        int sp = 0;
        stack[sp++] = sceneNodePtr;
        int iter = 0;
        while (sp > 0 && iter++ < 2000) {
            uintptr_t nodePtr = stack[--sp];
            if (!nodePtr) continue;

            CGameSceneNode node(nodePtr);
            int16_t parentAttach = node.GetParentAttachmentOrBone();
            if (parentAttach == static_cast<int16_t>(attachmentHandle)) {
                out = node.GetAbsOrigin();
                return true;
            }

            uintptr_t sib   = node.GetNextSibling();
            uintptr_t child = node.GetChild();
            if (sib   && sp < 512) stack[sp++] = sib;
            if (child && sp < 512) stack[sp++] = child;
        }
        return false;
    }

private:
    uintptr_t m_base;
};


class CCSPlayerController {
public:
    explicit CCSPlayerController(uintptr_t base) : m_base(base) {}

    bool IsValid()   const { return m_base != 0; }
    uintptr_t Base() const { return m_base; }

    int GetTeamNum() const {
        return *reinterpret_cast<uint8_t*>(m_base + client::CCSPlayerController::m_iTeamNum);
    }

    std::string GetName() const {
        const char* p = reinterpret_cast<const char*>(
            m_base + client::CCSPlayerController::m_iszPlayerName);
        return std::string(p, strnlen(p, 128));
    }

    uint32_t GetPawnHandle() const {
        return *reinterpret_cast<uint32_t*>(m_base + client::CCSPlayerController::m_hPlayerPawn);
    }

private:
    uintptr_t m_base;
};


class CEntityList {
public:
    explicit CEntityList(uintptr_t base) : m_base(base) {}

    uintptr_t GetController(int index) const {
        uintptr_t chunk = *reinterpret_cast<uintptr_t*>(m_base + 0x10 + 8 * (index >> 9));
        if (!chunk) return 0;
        return *reinterpret_cast<uintptr_t*>(chunk + 0x10 * (index & 0x1FF));
    }

    uintptr_t HandleToPtr(uint32_t handle) const {
        if (handle == 0xFFFFFFFF || handle == 0) return 0;
        int index = handle & 0x7FFF;
        uintptr_t chunk = *reinterpret_cast<uintptr_t*>(m_base + 0x10 + 8 * (index >> 9));
        if (!chunk) return 0;
        return *reinterpret_cast<uintptr_t*>(chunk + 0x70 * (index & 0x1FF));
    }

private:
    uintptr_t m_base;
};

# EDUCheats — Copilot Instructions

External CS2 ESP overlay. Reads game memory via `ReadProcessMemory`, renders via DX11 + ImGui.

## Build
MSBuild only — cmake not in PATH.
```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" build\EDUCheats.slnx /p:Configuration=Release /p:Platform=x64 /t:Build /m
```

## Project Layout
```
src/
  main.cpp          — entity scan loop, pushes PlayerESPData[64] to overlay (7ms tick)
  memory.hpp        — ReadProcessMemory wrapper
  offsets.hpp       — CS2 global pointer offsets + struct field offsets
  sdk.hpp           — CEntityList, CCSPlayerController, C_CSPlayerPawn, CGameSceneNode
  config.hpp/.cpp   — INI persistence
  features/esp.hpp  — ESPConfig, GameState, PlayerESPData (shared data types)
  gui/overlay.hpp/.cpp — DX11 swapchain, ImGui frame loop, ESP render, menu UI
```

## CS2 Entity System

### Global pointers (client.dll base + offset)
```cpp
dwEntityList            = 0x24D4E80
dwLocalPlayerController = 0x230E5C0
dwLocalPlayerPawn       = 0x205A700
dwViewMatrix            = 0x2334850
```
These change on CS2 patch. Update from `a2x/cs2-dumper` on GitHub.

### Entity list chunk layout
```
entityListBase + 0x10  →  chunk ptr array (8 bytes per ptr)
chunk index            =  entity_index >> 9
intra-chunk index      =  entity_index & 0x1FF
CEntityIdentity stride =  0x70 bytes
m_pEntity offset       =  +0x00 within identity
```

### Finding player controllers (loop var i = 1..128)
```cpp
uintptr_t chunk = Read<uintptr_t>(entityListBase + 0x10 + 8 * (i >> 9));
uintptr_t ctrl  = Read<uintptr_t>(chunk + 0x10 * (i & 0x1FF));
```
Valid controllers appear at i = 7, 14, 21, 28, ... (multiples of 7). Non-multiples return garbage filtered by team check.

### Resolving pawn from controller handle
```cpp
uint32_t handle = Read<uint32_t>(ctrl + 0x90C);   // m_hPlayerPawn
int      index  = handle & 0x7FFF;                 // direct identity index, NO -1
uintptr_t chunk = Read<uintptr_t>(entityListBase + 0x10 + 8 * (index >> 9));
uintptr_t pawn  = Read<uintptr_t>(chunk + 0x70 * (index & 0x1FF));
```

### Struct offsets
```cpp
// CCSPlayerController
m_iszPlayerName = 0x6F4
m_iTeamNum      = 0x3EB   // Read as uint8_t — reading int corrupts via neighbour bytes
m_hPlayerPawn   = 0x90C

// C_CSPlayerPawn
m_pGameSceneNode = 0x330
m_iHealth        = 0x34C
m_lifeState      = 0x354  // Read<int>: 256 = alive, 258 = dead
m_vOldOrigin     = 0x1390

// CGameSceneNode
m_vecAbsOrigin   = 0xC8
```

### Alive check
```cpp
bool IsAlive() { return Read<int>(pawn + 0x354) == 256; }
// 256 = 0x0100: lifeState byte=0 (LIFE_ALIVE), adjacent byte=1
// 258 = 0x0102: lifeState byte=2 (LIFE_DEAD)
```

## Critical Rules
- `m_iTeamNum` → always `Read<uint8_t>`, never `Read<int>`
- HandleToPtr index = `handle & 0x7FFF` with **no subtraction**
- Entity scan loop upper bound = 128 (covers 10-player lobbies)
- viewMatrix `[3][3]` can be large negative (e.g. -192) — this is normal for CS2's VP matrix
- Offsets stale after CS2 patch: global ptrs (`dw*`) change every patch, struct offsets are stable

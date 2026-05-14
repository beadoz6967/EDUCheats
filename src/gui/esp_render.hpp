#pragma once
#include "../features/esp.hpp"
#include "../sdk.hpp"

namespace esp_render {

// Renders the full ESP overlay onto ImGui's background draw list.
// Caller must already be inside an ImGui frame (NewFrame/Render bracket).
void DrawAll(const PlayerESPData players[64], int count, int localTeam,
             const ViewMatrix& view, int winW, int winH,
             const ESPConfig& cfg);

} // namespace esp_render

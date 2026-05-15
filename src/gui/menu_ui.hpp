#pragma once
#include "../config.hpp"
#include "../features/esp.hpp"

namespace menu_ui {

// Renders the Educanet-themed control panel. Must be called inside an
// ImGui frame. Saves config to disk on every toggle.
//   visibleInOut — set to false if the user clicks the [X] close box.
void Draw(ESPConfig& cfg, AimbotConfig& ab, GameState& state,
          Config& persist, bool& visibleInOut);

} // namespace menu_ui

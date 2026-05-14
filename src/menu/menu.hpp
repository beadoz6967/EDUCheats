#pragma once
#include "../config.hpp"
#include "../features/esp.hpp"
#include <atomic>

class Menu {
public:
    Menu(ESPConfig& cfg, const GameState& state, const Config& persist)
        : m_cfg(cfg), m_state(state), m_persist(persist) {}

    // Blocking — run on dedicated thread.
    void Run(std::atomic<bool>& running);

private:
    void Print() const;

    ESPConfig&       m_cfg;
    const GameState& m_state;
    const Config&    m_persist;
    bool             m_visible = false;
};

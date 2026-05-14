#pragma once
#include "../features/esp.hpp"
#include <atomic>

class Menu {
public:
    explicit Menu(ESPConfig& cfg) : m_cfg(cfg) {}

    // Blocking — run on dedicated thread.
    void Run(std::atomic<bool>& running);

private:
    void Print() const;

    ESPConfig& m_cfg;
    bool       m_visible = false;
};

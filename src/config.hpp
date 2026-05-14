#pragma once
#include "features/esp.hpp"
#include <string>

// Plain-text INI persistence beside the executable.
// All operations are silent on error — corrupt or missing files
// fall back to defaults rather than crash.
class Config {
public:
    Config();

    // Read config.ini into cfg. Missing file writes defaults and returns.
    void Load(ESPConfig& cfg) const;

    // Atomic-ish rewrite of config.ini from current cfg state.
    // Cheap (~100 bytes) so safe to call on every toggle.
    void Save(const ESPConfig& cfg) const;

    const std::string& Path() const { return m_path; }

private:
    static std::string ResolvePath();

    std::string m_path;
};

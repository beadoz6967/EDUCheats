#pragma once
#include "features/esp.hpp"
#include <string>

// Plain-text INI persistence beside the executable.
// All operations are silent on error — corrupt or missing files
// fall back to defaults rather than crash.
class Config {
public:
    Config();

    void Load(ESPConfig& esp, AimbotConfig& ab) const;
    void Save(const ESPConfig& esp, const AimbotConfig& ab) const;

    const std::string& Path() const { return m_path; }

private:
    static std::string ResolvePath();

    std::string m_path;
};

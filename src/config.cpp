#include "config.hpp"
#include <Windows.h>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

std::string Trim(std::string_view s) {
    constexpr std::string_view kWs = " \t\r\n";
    const auto first = s.find_first_not_of(kWs);
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(kWs);
    return std::string{ s.substr(first, last - first + 1) };
}

bool ParseBool(std::string_view v, bool fallback) {
    if (v == "1" || v == "true"  || v == "TRUE"  || v == "True"  || v == "on"  || v == "ON")  return true;
    if (v == "0" || v == "false" || v == "FALSE" || v == "False" || v == "off" || v == "OFF") return false;
    return fallback;
}

int ParseInt(std::string_view v, int fallback) {
    try {
        return std::stoi(std::string{ v });
    } catch (...) {
        return fallback;
    }
}

} // namespace

Config::Config() : m_path(ResolvePath()) {}

std::string Config::ResolvePath() {
    char buf[MAX_PATH]{};
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return "config.ini";

    std::string exe(buf, len);
    const auto slash = exe.find_last_of("\\/");
    if (slash == std::string::npos) return "config.ini";

    return exe.substr(0, slash + 1) + "config.ini";
}

void Config::Load(ESPConfig& cfg) const {
    std::ifstream in(m_path);
    if (!in.is_open()) {
        // File missing: write defaults so users see the template
        Save(cfg);
        return;
    }

    std::unordered_map<std::string, std::string> kv;
    std::string line;
    while (std::getline(in, line)) {
        // Strip comments
        const auto hash = line.find_first_of("#;");
        if (hash != std::string::npos) line.erase(hash);

        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = Trim(std::string_view{ line }.substr(0, eq));
        std::string val = Trim(std::string_view{ line }.substr(eq + 1));
        if (key.empty()) continue;

        kv.emplace(std::move(key), std::move(val));
    }

    auto getBool = [&](const char* key, bool fallback) -> bool {
        const auto it = kv.find(key);
        return it == kv.end() ? fallback : ParseBool(it->second, fallback);
    };
    auto getInt = [&](const char* key, int fallback) -> int {
        const auto it = kv.find(key);
        return it == kv.end() ? fallback : ParseInt(it->second, fallback);
    };

    cfg.enabled     = getBool("enabled",     cfg.enabled.load());
    cfg.nameESP     = getBool("nameESP",     cfg.nameESP.load());
    cfg.healthBar   = getBool("healthBar",   cfg.healthBar.load());
    cfg.colorMode   = getInt ("colorMode",   cfg.colorMode.load());
    cfg.distanceESP = getBool("distanceESP", cfg.distanceESP.load());
    cfg.hpNumbers   = getBool("hpNumbers",   cfg.hpNumbers.load());
    cfg.skeleton    = getBool("skeleton",    cfg.skeleton.load());
}

void Config::Save(const ESPConfig& cfg) const {
    std::ofstream out(m_path, std::ios::trunc);
    if (!out.is_open()) return;

    out << "# EDUCheats config\n"
        << "# Auto-rewritten on every toggle; manual edits during runtime will be overwritten.\n"
        << "enabled="     << (cfg.enabled.load()     ? 1 : 0) << '\n'
        << "nameESP="     << (cfg.nameESP.load()     ? 1 : 0) << '\n'
        << "healthBar="   << (cfg.healthBar.load()   ? 1 : 0) << '\n'
        << "colorMode="   <<  cfg.colorMode.load()           << '\n'
        << "distanceESP=" << (cfg.distanceESP.load() ? 1 : 0) << '\n'
        << "hpNumbers="   << (cfg.hpNumbers.load()   ? 1 : 0) << '\n'
        << "skeleton="    << (cfg.skeleton.load()    ? 1 : 0) << '\n';
}

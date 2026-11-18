#include "accessibility/a11y_config.h"

#include <algorithm>
#include <fstream>
#include <string_view>

#include "accessibility/a11y_log.h"
#include "runtime_config.h"

namespace a11y::config {
namespace {

constexpr const char* kFileName = "accessibility.ini";

Settings g_settings;

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.remove_suffix(1);
    return text;
}

bool ParseBool(std::string_view value, bool fallback) {
    if (value == "true" || value == "1" || value == "on" || value == "yes") return true;
    if (value == "false" || value == "0" || value == "off" || value == "no") return false;
    return fallback;
}

int ParseInt(std::string_view value, int fallback, int lo, int hi) {
    try {
        return std::clamp(std::stoi(std::string(value)), lo, hi);
    } catch (...) {
        return fallback;
    }
}

void Apply(std::string_view key, std::string_view value) {
    Settings& s = g_settings;
    if (key == "language") s.language = std::string(value);
    else if (key == "game_language") s.gameLanguage = std::string(value);
    else if (key == "speech") s.speech = ParseBool(value, s.speech);
    else if (key == "menu_speech") s.menuSpeech = ParseBool(value, s.menuSpeech);
    else if (key == "terrain_cues") s.terrainCues = ParseBool(value, s.terrainCues);
    else if (key == "object_cues") s.objectCues = ParseBool(value, s.objectCues);
    else if (key == "goal_beacon") s.goalBeacon = ParseBool(value, s.goalBeacon);
    else if (key == "cue_volume") s.cueVolume = ParseInt(value, s.cueVolume, 0, 100);
    else if (key == "scan_tiles") s.scanTiles = ParseInt(value, s.scanTiles, 2, 12);
    else RT_LOGF(RT_TAG_A11Y, "config: unknown key '%.*s'\n", int(key.size()), key.data());
}

const char* Bool(bool v) { return v ? "true" : "false"; }

}  // namespace

std::filesystem::path FilePath() {
    return RuntimeConfigFile::ApplicationDataDirectory() / kFileName;
}

void Load() {
    std::ifstream file(FilePath());
    if (!file.is_open()) {
        Save();
        RT_LOGF(RT_TAG_A11Y, "config: created %s\n", FilePath().string().c_str());
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        const std::string_view t = Trim(line);
        if (t.empty() || t.front() == '#' || t.front() == ';' || t.front() == '[') continue;
        const size_t eq = t.find('=');
        if (eq == std::string_view::npos) continue;
        std::string_view value = t.substr(eq + 1);
        const size_t comment = value.find('#');  // trailing "# ..." notes, as Save() writes them
        if (comment != std::string_view::npos) value = value.substr(0, comment);
        Apply(Trim(t.substr(0, eq)), Trim(value));
    }
    RT_LOGF(RT_TAG_A11Y, "config: loaded %s\n", FilePath().string().c_str());
}

void Save() {
    std::error_code ec;
    std::filesystem::create_directories(FilePath().parent_path(), ec);
    std::ofstream file(FilePath(), std::ios::trunc);
    if (!file.is_open()) {
        RT_LOGF(RT_TAG_A11Y, "config: cannot write %s\n", FilePath().string().c_str());
        return;
    }
    const Settings& s = g_settings;
    file << "# New Super Mario Bros. Wii accessibility settings. Edit while the game is closed,\n"
            "# or change them in game with the F-keys (F1 lists them).\n"
            "[accessibility]\n"
         << "language = " << s.language << "   # spoken language: auto, es, en\n"
         << "game_language = " << s.gameLanguage << "   # game text: auto, en, es, fr, de, it (restart to apply)\n"
         << "speech = " << Bool(s.speech) << "\n"
         << "menu_speech = " << Bool(s.menuSpeech) << "\n"
         << "terrain_cues = " << Bool(s.terrainCues) << "\n"
         << "object_cues = " << Bool(s.objectCues) << "\n"
         << "goal_beacon = " << Bool(s.goalBeacon) << "\n"
         << "cue_volume = " << s.cueVolume << "   # 0-100\n"
         << "scan_tiles = " << s.scanTiles << "   # 2-12, how far ahead walls and pits are found\n";
}

Settings& Get() {
    return g_settings;
}

}  // namespace a11y::config

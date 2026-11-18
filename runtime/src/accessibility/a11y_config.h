#ifndef NSMBW_ACCESSIBILITY_A11Y_CONFIG_H
#define NSMBW_ACCESSIBILITY_A11Y_CONFIG_H

#include <filesystem>
#include <string>

namespace a11y::config {

// Every user-facing accessibility setting. Saved as accessibility.ini in the game's own user-data
// folder (portable: next to the exe), so the whole install can be moved as one folder.
struct Settings {
    std::string language = "auto";      // spoken language: auto (Windows UI language), es, en
    std::string gameLanguage = "auto";  // the game's own text: auto (= spoken language), en, es, fr, de, it
    bool speech = true;
    bool terrainCues = true;     // walls, ledges, pits, ground below
    bool objectCues = true;      // enemies, items, blocks
    bool goalBeacon = true;      // a soft tick panned towards the level's goal
    bool gameSounds = true;      // use the game's own sound effects where one fits (pipes, items)
    int cueVolume = 70;          // 0..100, master level of every accessibility cue
    int scanTiles = 6;           // how far ahead walls and pits are looked for, in tiles
    bool menuSpeech = true;      // read focused menu items and new on-screen text
};

std::filesystem::path FilePath();

// Loads the file (creating it with defaults when missing). Call once at start.
void Load();
void Save();

Settings& Get();

}  // namespace a11y::config

#endif  // NSMBW_ACCESSIBILITY_A11Y_CONFIG_H

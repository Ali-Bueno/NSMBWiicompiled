# STATUS — New Super Mario Bros. Wii accessibility (NSMBWiicompiled fork)

**Last updated:** 2026-11-18. First pass built and checked by automated runs (speech muted, every
line logged); **not yet played by a blind player.**

## Identity

| | |
|---|---|
| Game | New Super Mario Bros. Wii, PAL rev 1 (SMNP01), the user's own `.rvz` |
| Engine | Static recompilation of the Wii game to native x64 (WiiCompiled translator + runtime, aurora renderer) |
| Framework | Accessibility compiled into the game DLL (`runtime/src/accessibility/`); hooks are native overrides at guest addresses (`projects/nsmbw/native/nsmbw_a11y_*.cpp`) plus a per-frame call from the player-1 `KPADRead` override |
| Speech | PRISM `prism.dll`, loaded at runtime from the exe folder (ported from the Mario Kart Wii accessibility fork) |
| Fork | `Ali-Bueno/NSMBWiicompiled`, branch `accessibility`; upstream `ral1009/NSMBWiicompiled` (remote `upstream`) |
| Play folder | `D:\code\new super mario bros wii accessibility\NSMBW Accesible\` (release build + extracted disc, portable, `LEEME.txt`); refresh it by copying `build_release\NSMBWCompiled-P1.dll`, `NSMBWCompiled.exe` and `accessibility_lang\` |
| Reference | `D:\code\decompiled games\NSMBW-Decomp` (structs, profile IDs); `D:\code\wiicompiled\KirbyRtDLAccess` (design ideas) |

## Sections

| Section | State | Notes |
|---|---|---|
| Speech pipeline (PRISM, phrase files es/en, portable config) | done | `accessibility.ini` in `UserData\` next to the exe |
| Game text language | done | `SCGetLanguage` override; follows the spoken language |
| Menus (cursor focus + window question) | done | every menu that uses the game's selection cursor |
| F10 options menu (host-drawn) | done | speaks tab, row, value; bindings screen |
| Title, course-in, lives, death, power-ups, time, star coins | done | star-coin announcement unverified in game |
| World map: world, node, cleared, "no path" | done | path directions are not listed |
| Terrain cues: walls, pits, drops, steps, lava, ground below | wip | needs a listening test and tuning |
| Enemy / item / goal / pipe cues | wip | needs a listening test |
| F2-F4 info (status, progress, nearby with pipes) | done | |
| Sound guide (Shift+F9) | done | |
| Toad-house minigames, bosses' patterns, Super Guide | todo | |
| Game's own sound effects as cues | done | pipes, items, map "no path", volume; labels resolved by the sound archive (`game/game_sound.*`) |

## Code map

- `runtime/src/accessibility/accessibility.cpp` — entry points, per-frame order, hotkey actions.
- `game/` — the engine adapter (`nsmbw_game.*`: every address and offset) and `guest_call.h` (calls guest functions with a scratch frame).
- `ui/` — `lyt.*` (pane/text reading), `menu_reader.*` (cursor focus).
- `level/` — `terrain.*`, `actors.*` (+ generated `actor_names.inc`), `entrances.*` (pipes, goal from course data), `level_monitor.*` (announcements).
- `map/world_map.*`, `audio/` (cue mixer + `cues.h` palette), `speech.*`, `localization.*` + `phrases_en.inc`, `accessibility_lang/es.ini`.
- Overrides: `projects/nsmbw/native/nsmbw_a11y_cursor.cpp`, `nsmbw_a11y_language.cpp`; tick + input sample in `nsmbw_kpad_overrides.cpp`.
- Facts and offsets: [`docs/accessibility/nsmbw-structures.md`](docs/accessibility/nsmbw-structures.md). Build and test: [`docs/accessibility/README.md`](docs/accessibility/README.md).

## Next step

Play-test with the screen reader: 1-1 to the goal with the cues on, then tune `audio/cues.h`
(levels, wall tick rate, which edges to announce) from what the player reports.

## Review

2026-11-18 adversarial review (11 findings) fixed in `0de0fc7`: shutdown order, sound guide
silencing, level state across pauses, bounded pane walks, no collision query before `dBg_c`
exists, masked bool results, retried sound lookups, hotkeys off in the options menu, rebind result
spoken, portable fallback when the exe folder is read-only. Not fixed: non-Windows builds of
`runtime/src/accessibility` (Windows-only mod).

## Known issues

- File-select slots read as "Mundo, 1, x, 05" (world and lives without words).
- The world map does not say which directions have a path; pressing one without a path says "sin camino".
- Fresh translations can drop functions reached only through pointers (one seeded so far: `0x80773220`); a new `missing_target` crash needs the address added to `function_map.txt` and the REL re-translated.
- Only PAL rev 1 is supported by the accessibility addresses (the USA build would need them mapped).

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
| Terrain cues (Kirby-style): wall hums by height + spoken height, ceiling, edges, platform above, air hum / pit / landing | wip | needs a listening test and tuning |
| Enemy radar (signature per kind, stompable vs dangerous, boss drum), items, under-item bell, goal, pipes | wip | needs a listening test |
| Combat timing: jump now / jump high / jump over, stomp and danger-below while falling | wip | arc-timed; autopilot stomps 8 enemies in 1-1, dies at a goomba pair |
| Tile camera (I J K L, U, O) | wip | untested by a player |
| F2-F4 info (status, progress, nearby with pipes) | done | |
| Sound guide (Shift+F9): categories, Page Up/Down, Home/End, F9 replay | done | |
| Toad-house minigames, bosses' patterns, Super Guide | todo | |
| Game's own sound effects as cues | done | pipes, items; labels resolved by the sound archive (`game/game_sound.*`). The map "no path" sound was dropped at the player's request (speech only) |

## Code map

- `runtime/src/accessibility/accessibility.cpp` — entry points, per-frame order, hotkey actions.
- `game/` — the engine adapter (`nsmbw_game.*`: every address and offset) and `guest_call.h` (calls guest functions with a scratch frame).
- `ui/` — `lyt.*` (pane/text reading), `menu_reader.*` (cursor focus).
- `level/` — `terrain.*`, `actors.*` (radar; generated `actor_names.inc` with Enemy/Danger/Boss), `combat.*` (jump timing from measured jump arcs), `camera.*` (tile camera), `entrances.*` (pipes, goal from course data), `level_monitor.*` (announcements).
- `audio/cue_service.*` + `cue_render.cpp`: fixed channels for hums plus a 32-voice pool (`PlayFree`) for composite one-shots; noise/warm waves, pulse, low-pass, per-part delay.
- `test_autopilot.*`: `NSMBW_A11Y_AUTOPILOT=1` plays a level from the cues alone, with a 12-frame reaction delay; `NSMBW_A11Y_LOG_COMBAT=1` traces Mario and the cued enemy frame by frame.
- `map/world_map.*`, `audio/` (cue mixer + `cues.h` palette), `speech.*`, `localization.*` + `phrases_en.inc`, `accessibility_lang/es.ini`.
- Overrides: `projects/nsmbw/native/nsmbw_a11y_cursor.cpp`, `nsmbw_a11y_language.cpp`; tick + input sample in `nsmbw_kpad_overrides.cpp`.
- Facts and offsets: [`docs/accessibility/nsmbw-structures.md`](docs/accessibility/nsmbw-structures.md). Build and test: [`docs/accessibility/README.md`](docs/accessibility/README.md).

## Next step

Chained enemies: after a stomp bounce or with two enemies in a row the jump cue comes too late
(autopilot death in 1-1 at x~2217). Options: cue from the air for a jump on landing, and use
Mario's settled walking speed instead of his decelerating speed after a landing. Then the user's
listening test, and tune `audio/cues.h`.

## Review

2026-11-18 adversarial review (11 findings) fixed in `0de0fc7`: shutdown order, sound guide
silencing, level state across pauses, bounded pane walks, no collision query before `dBg_c`
exists, masked bool results, retried sound lookups, hotkeys off in the options menu, rebind result
spoken, portable fallback when the exe folder is read-only. Not fixed: non-Windows builds of
`runtime/src/accessibility` (Windows-only mod).

## Known issues

- File-select slots read as "Mundo, 1, x, 05" (world and lives without words).
- The world map does not say which directions have a path; pressing one without a path says "sin camino" (once per real press, 1.5 s cooldown, not during the first 3 s on the map).
- Jump arcs in `level/combat.cpp` were measured walking; a running jump is higher, so cues may be slightly early when running.
- Fresh translations can drop functions reached only through pointers (one seeded so far: `0x80773220`); a new `missing_target` crash needs the address added to `function_map.txt` and the REL re-translated.
- Only PAL rev 1 is supported by the accessibility addresses (the USA build would need them mapped).

# Accessibility for blind players

This fork adds a screen reader and audio cues to NSMBWiicompiled so New Super Mario Bros. Wii can
be played without seeing the screen. Speech goes through [PRISM](https://github.com/ethindp/prism)
(NVDA, JAWS and the other readers it supports, or Windows speech when none runs).

## What it does

- **Menus**: the item under the game's selection cursor is read, with the window's question when a
  new window opens (save prompts, number of players, pause, world-map menu). The port's own F10
  options menu reads its tabs, rows and values, and the control-rebinding screen.
- **Title, course-in, world map**: the title screen, the course name and lives on the course-in
  screen, the world and each node the player stops on (level number or kind, and whether it is
  cleared). On the map, a direction with no path says "no path".
- **In a level**, spoken: power-up changes, a lost life, star coins, the time running low (100
  seconds), going into water, pipes, doors and the goal.
- **In a level**, sounds (all panned left/right to where things are):
  - walls: a tick that repeats faster as a wall gets closer (ahead, and close behind);
  - edges ahead while on the ground: a falling saw for a pit, a falling tone for a drop, a rising
    tone for a step up, a low buzz for lava or poison;
  - in the air: a tone that rises as the ground below gets closer, a low pulse when there is
    nothing below;
  - enemies: clicks, higher when the enemy is above, faster when it is close;
  - items, power-ups and star coins: a bright chirp; enterable pipes: a low bloop;
  - the goal pole: a soft tick from its side every two seconds.
- The game's own text follows the spoken language (Spanish or English, from Windows by default).

## Keys

| Key | Action |
|---|---|
| F1 | help |
| F2 | status: lives, power-up, time, star coins |
| F3 | level progress and where the goal is; on the map, the current node |
| F4 | nearby enemies, items and enterable pipes, with distances in tiles |
| F5 / F6 | terrain sounds / object sounds on or off |
| F7 / F8 | sound volume down / up |
| F9 | repeat the last thing said |
| Shift+F9 | sound guide by category: Page Down / Page Up change sound, End / Home change category, F9 replays |
| I J K L, U, O | tile camera: move a cursor a tile at a time (sounds the tile, names what is there), U describes it, O returns to Mario |

The game's own controls are unchanged (keyboard: arrows, Z or Space jump, X run, C spin; in menus
Enter or Z confirm and X or Backspace go back; Esc pauses a level and opens the map menu; F10
options).

## Settings

`UserData\accessibility.ini`, next to the game. The game is portable: its save, controls and
settings all live in `UserData\` beside the exe. Keys: `language` and `game_language` (auto, es,
en), `speech`, `menu_speech`, `terrain_cues`, `object_cues`, `goal_beacon`, `cue_volume` (0-100),
`game_sounds` (the game's own pipe, item and menu sounds; false = synthesized), `scan_tiles` (how far
ahead walls and pits are looked for). F5-F8 save their changes there.

## Building and testing

Build as in [`docs/BUILDING.md`](../BUILDING.md); this checkout also needs:

- `-DMKW_TRANSLATED_SHARD_MANIFEST=<stub>` when no Mario Kart Wii translation exists: a manifest
  that only sets the `MKW_*` variables (the NSMBW target never builds them);
- `generated_nsmbw/generated/RuntimeConfig.h` (a copy of `generated_nsmbw/RuntimeConfig.h`);
- the shard manifest regenerated after any change to `projects/nsmbw/native/`.

Test runs: `projects/nsmbw/tools/run_nsmbw.ps1` with `NSMBW_A11Y_SILENT=1` (log every spoken line
as `[accessibility] say: ...` instead of speaking), `NSMBW_A11Y_LOG_MENU=1` (every text of a new
menu), `NSMBW_A11Y_LOG_PLAYER=1` (player state, terrain probes, actors, entrances, F3/F4 output),
`NSMBW_A11Y_LOG_TERRAIN=1` (edge cues), `NSMBW_A11Y_LOG_COMBAT=1` (jump cues and a per-frame trace after each), `NSMBW_A11Y_LOG_MAP=1` (map presses, "no path"), `NSMBW_A11Y_AUTOPILOT=1` (plays a level from the cues alone). `NSMBW_TEST_INPUT` scripts presses (see `CLAUDE.md`).

Offsets and where each came from: [`nsmbw-structures.md`](nsmbw-structures.md).

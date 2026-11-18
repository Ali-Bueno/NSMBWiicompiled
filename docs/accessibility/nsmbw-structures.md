# NSMBW structures the accessibility layer reads

PAL rev 1 (SMNP01 rev 1). Every value here was checked in game with `NSMBW_A11Y_LOG_PLAYER=1`
(once-a-second dump of what the mod reads) unless marked *unverified*. Sources: NSMBW-Decomp
headers (`D:\code\decompiled games\NSMBW-Decomp`, commit `131a4d0`), `projects/nsmbw/function_map.txt`,
and the translated bodies in `build/<module>/functions/`.

All accessors live in `runtime/src/accessibility/game/nsmbw_game.cpp` (the engine adapter) and,
for the course file, `runtime/src/accessibility/level/entrances.cpp`.

## Actors

| Field | Offset | Notes |
|---|---|---|
| `fBase_c::mProfName` (u16) | 0x08 | `mUniqueID` 0x00, `mParam` 0x04; the vtable is at 0x60, not 0 (a first guess of 0x0C read 0 for every actor) |
| `fBase_c::mGroupType` (u8) | 0x0E | 2 for every actor seen in 1-1, enemies included: not usable to tell enemies apart |
| `dBaseActor_c::mLinkActor` | 0x70 | prev 0x70, next 0x74, owner 0x78 (node pointers, owner = the actor) |
| `dBaseActor_c::mPos` | 0xAC | feet, world units (16 per tile), y up |
| `dBaseActor_c::mCenterOffs` | 0xD0 | |
| `dBaseActor_c::mSpeed` | 0xE8 | |
| `dBaseActor_c::mVisible` | 0x124 | |
| `dActor_c::mBc.mFlags` | 0x274 | 0x1EC + 0x88; foot `0x001FE000`, wall R `0x15`, wall L `0x2A` |
| `dActor_c::mDirection` | 0x348 | 0 right, 1 left |
| `dActor_c::mKind` | 0x38C | 1 player, 3 enemy — but coins (401) and items (60) are 3 too |
| `dActor_c::mLayer` | 0x38F | |
| `daPlBase_c::mStatusFlags` | 0x1014 | bit 4 = out of play (dead or bubble) |
| `daPlBase_c::mStarTimer` | 0x1070 | |
| `daPlBase_c::mPowerup` | 0x1090 | 0 small … 6 ice |
| demo state (`mDemoStateMgr` current) | 0x144C | `sFStateID_c*`; name at +4, e.g. `daPlBase_c::StateID_DemoNone` |
| state (`mStateMgr` current) | 0x1498 | e.g. `dAcPy_c::StateID_Walk` |
| `daPlBase_c::mPlayerType` | 0x14D4 | character, indexes `daPyMng_c::mRest` |

Player 1 is `daPyMng_c::getPlayer(0)` (0x8005F900), called through `game/guest_call.h`. The actor
list is walked from the player's own node (no symbol for `m_actorManage`).

Profile IDs (spoken names): generated from `include/game/framework/f_profile_name.hpp` into
`runtime/src/accessibility/level/actor_names.inc`; 13 player, 51 goomba, 60 item, 401 coin seen in 1-1.

## Statics

| Symbol | Address | Use |
|---|---|---|
| `dInfo_c::m_startGameInfo` | 0x80315E90 | `mGameMode` +0x08 (u32: 2 title, 3 title replay), `mWorld1` +0x0C, `mLevel1` +0x0D |
| game-stop word | 0x8042A228 | non-zero while a pause window or map menu holds the game |
| `daPyMng_c::mPlayerType[4]` | 0x80355160 | slot → character |
| `daPyMng_c::mRest[4]` | 0x80355190 | lives per character |
| `dStageTimer_c::m_instance` | 0x8042A350 | `mTimeValue` +4, 1/4096 s |
| `dScStage_c::m_instance` | 0x8042A4A8 | `mCurrFile` +0x120E, `mCurrAreaNo` +0x120F (dScene_c 0x74 + `pad[0x1198]`; a first guess of 0x77 read a frame counter) |
| `dScStage_c::mCollectionCoin[3]` | 0x803744B0 | who took each star coin; 4 = nobody (*unverified* in game) |
| `dCourseSelectGuide_c::m_instance` | 0x80429EDC | world map node: world +1032, course +1036, type +1040 |
| `dScWMap_c::m_WorldNo` (u8) | 0x8042A52D | |
| `daWmPlayer_c::ms_instance` | 0x8042A480 | map Mario, a dBaseActor_c (pos +0xAC) |
| `dCd_c::m_instance` | 0x8042A178 | `dCdFile_c mFiles[4]`, 944 bytes each |
| `dActorData_c::l_actor_data_tbl` | 0x8030A340 | 40-byte entries, profile u16 at +0: sprite id → profile |

## Course file (`dCdFile_c`)

Read from the translated `getNextGotoP` (func_8008E3D0): entrances at +0x20, count +0x90, 20-byte
`sNextGotoData` (x +0, y +2 counted downwards, type +0x0B, area +0x0D, flags +0x10; flag 0x80 =
exit only). Sprites (`sMapActorData`, 16 bytes: id +0, x +2, y +4, area +12) at +0x24, count +0x94.
In 1-1 the start entrance is (752, 592) for a player at (760, -608): world y = -(y + 16).

## Functions called

| Function | Address | Arguments |
|---|---|---|
| `dBc_c::checkGround` | 0x800757B0 | vec*, float* out, layer, 1, -1 |
| `dBc_c::checkWall` | 0x80075FD0 | from*, to*, float* outX, layer, 1, nullptr |
| `dBc_c::checkTenjou` | 0x80075CA0 | vec*, float* out, layer, 1 |
| `dBc_c::getUnitType` / `getUnitKind` | 0x80070BA0 / 0x80070BF0 | f1 x, f2 y, r3 layer |
| `dBc_c::checkWater` | 0x80075270 | f1 x, f2 y, r3 layer, r4 float* — 3 lava, 4 poison |
| `dWmLib::IsCourseClear` | 0x800FAF50 | world, course |
| `SCGetLanguage` (overridden) | 0x801DD460 | only caller `dGameCom::GetLanguageHBM` 0x800B4630 |
| `dGameCom::SelectCursorSetup` (overridden) | 0x800B44D0 | pane, slot, forceTop — every menu cursor |

The `sc.cpp` SC overrides inherited from Mario Kart Wii sit at MKW addresses (0x801B1xxx) and are
dead for NSMBW; the real SC functions are around 0x801DD300.

## Menu text

`nw4r::lyt` panes: parent +0x0C, child list sentinel +0x14 (node = pane + 4), global matrix +0x84
(translation at +12 / +28), alpha +0xB8, flags +0xBB (bit 0 visible), name +0xBC. Text boxes are
named `T_…`; buffer pointer +0xD8, length +0xFE. Digits in some boxes are full width (U+FF10…).

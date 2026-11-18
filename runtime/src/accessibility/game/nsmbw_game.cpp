#include "accessibility/game/nsmbw_game.h"

#include "accessibility/game/guest_call.h"
#include "nsmbw_va.h"

extern "C" uint32_t g_nsmbwCurrentSceneProfile;  // nsmbw_create_next_scene_diag.cpp

namespace a11y::game {
namespace {

// Statics (PAL rev 1, NSMBW-Decomp syms.txt); NSMBW_VA maps them for other versions.
constexpr std::uint32_t kStartGameInfo = NSMBW_VA(0x80315E90u);  // dInfo_c::m_startGameInfo
constexpr std::uint32_t kStartGameModeOff = 0x08;
constexpr std::uint32_t kStartWorldOff = 0x0C;
constexpr std::uint32_t kStartLevelOff = 0x0D;
constexpr std::uint32_t kGameStopWord = NSMBW_VA(0x8042A228u);  // as nsmbw_controls.cpp
constexpr std::uint32_t kPlayerRest = NSMBW_VA(0x80355190u);    // daPyMng_c::mRest[4]
constexpr std::uint32_t kSlotPlayerType = NSMBW_VA(0x80355160u); // daPyMng_c::mPlayerType[4]
constexpr std::uint32_t kStageTimer = NSMBW_VA(0x8042A350u);    // dStageTimer_c::m_instance
constexpr std::uint32_t kStageTimerValue = 0x04;                // mTimeValue, 1/4096 s
constexpr std::uint32_t kTimerUnitShift = 12;
constexpr std::uint32_t kStageScene = NSMBW_VA(0x8042A4A8u);    // dScStage_c::m_instance
constexpr std::uint32_t kStageAreaNo = 0x120F;  // mCurrAreaNo: dScene_c (0x74) + pad[0x1198] + world, course, file

// dBg_c::m_bg_p: the level background every collision query reads; null while a stage loads.
constexpr std::uint32_t kBgInstance = NSMBW_VA(0x8042A0B0u);
constexpr std::uint8_t kGameModeTitle = 2;
constexpr std::uint8_t kGameModeTitleReplay = 3;

// dActor_c / daPlBase_c member offsets: docs/accessibility/nsmbw-structures.md.
constexpr std::uint32_t kProfName = 0x08;   // fBase_c: mUniqueID 0, mParam 4, mProfName 8; vtable at 0x60
constexpr std::uint32_t kGroupType = 0x0E;
constexpr std::uint32_t kPos = 0xAC;
constexpr std::uint32_t kCenterOffs = 0xD0;
constexpr std::uint32_t kSpeed = 0xE8;
constexpr std::uint32_t kVisible = 0x124;
constexpr std::uint32_t kBcFlags = 0x1EC + 0x88;  // mBc.mFlags
constexpr std::uint32_t kDirection = 0x348;
constexpr std::uint32_t kKind = 0x38C;
constexpr std::uint32_t kLayer = 0x38F;
constexpr std::uint32_t kStatusFlags = 0x1014;
constexpr std::uint32_t kStarTimer = 0x1070;
constexpr std::uint32_t kPowerup = 0x1090;
constexpr std::uint32_t kPlayerType = 0x14D4;
constexpr std::uint32_t kStateId = 0x1498;  // mStateMgr's current sFStateID_c*
constexpr std::uint32_t kStateIdName = 0x04;
// mDemoStateMgr sits 0x4C before mStateMgr (its 0x3C bytes, three ints and a bool; decomp
// d_a_player_base.hpp:1272-1280), so its current state is at 0x1418 + 0x34.
constexpr std::uint32_t kDemoStateId = 0x144C;

// dBc_c::mFlags masks (d_bc.hpp).
constexpr std::uint32_t kBcWallR = 0x00000015u;
constexpr std::uint32_t kBcWallL = 0x0000002Au;
constexpr std::uint32_t kBcFoot = 0x001FE000u;
constexpr int kStatusOutOfPlay = 4;
constexpr int kStatusBitsPerWord = 32;

// Collision queries' extra arguments, as the game's own actors pass them (d_a_en_shell.cpp:469,
// :482, d_a_iceball.cpp:108): line kind 1 and "any actor" -1 / nullptr.
constexpr std::uint32_t kLineKind = 1;
constexpr std::uint32_t kAnyActor = static_cast<std::uint32_t>(-1);
constexpr int kStateNameMax = 48;

// A z depth inside the playfield for query vectors (actors use 5500 for effects, but the
// collision ignores z).
constexpr float kQueryZ = 0.0f;

void WriteVec(GuestCall& call, std::uint32_t addr, float x, float y) {
    call.WriteFloat(addr, x);
    call.WriteFloat(addr + 4, y);
    call.WriteFloat(addr + 8, kQueryZ);
}

std::string ReadCString(std::uint32_t addr, int max) {
    std::string s;
    for (int i = 0; i < max; ++i) {
        std::uint8_t c = 0;
        if (!TryU8(addr + i, c) || c == 0) break;
        s.push_back(static_cast<char>(c));
    }
    return s;
}

}  // namespace

std::uint32_t SceneProfile() { return g_nsmbwCurrentSceneProfile; }

bool InScene(Scene scene) { return SceneProfile() == static_cast<std::uint32_t>(scene); }

std::uint8_t StartGameMode() {
    std::uint32_t mode = 0;
    TryU32(kStartGameInfo + kStartGameModeOff, mode);
    return static_cast<std::uint8_t>(mode);
}

bool OnTitleScreen() {
    const std::uint8_t mode = StartGameMode();
    return InScene(Scene::Stage) && (mode == kGameModeTitle || mode == kGameModeTitleReplay);
}

std::uint8_t StartWorld() {
    std::uint8_t v = 0;
    TryU8(kStartGameInfo + kStartWorldOff, v);
    return v;
}

std::uint8_t StartLevel() {
    std::uint8_t v = 0;
    TryU8(kStartGameInfo + kStartLevelOff, v);
    return v;
}

bool GamePaused() {
    std::uint32_t stop = 0;
    return TryU32(kGameStopWord, stop) && stop != 0;
}

bool StageReady() {
    std::uint32_t bg = 0;
    return TryPointer(kBgInstance, bg);
}

std::uint32_t PlayerActor(int slot) {
    if (!StageReady()) return 0;  // the scene root exists frames before its background does
    GuestCall call;
    call.Gpr(3, static_cast<std::uint32_t>(slot));
    const std::uint32_t actor = call.Invoke<NSMBW_VA(0x8005F900u)>();  // daPyMng_c::getPlayer
    return Memory::Contains(actor, kPlayerType + 4) ? actor : 0;
}

bool ReadPlayer(std::uint32_t actor, PlayerState& out) {
    if (actor == 0 || !TryVec3(actor + kPos, out.pos) || !TryVec3(actor + kSpeed, out.speed)) {
        return false;
    }
    out.actor = actor;
    TryU8(actor + kDirection, out.direction);
    TryU8(actor + kLayer, out.layer);
    TryU32(actor + kBcFlags, out.bgFlags);
    std::uint32_t v = 0;
    out.powerup = TryU32(actor + kPowerup, v) ? static_cast<int>(v) : 0;
    out.starTimer = TryU32(actor + kStarTimer, v) ? static_cast<int>(v) : 0;
    out.playerType = TryU32(actor + kPlayerType, v) ? static_cast<int>(v) : 0;
    out.onGround = (out.bgFlags & kBcFoot) != 0;
    out.wallLeft = (out.bgFlags & kBcWallL) != 0;
    out.wallRight = (out.bgFlags & kBcWallR) != 0;
    const auto stateName = [&](std::uint32_t slot) {
        std::uint32_t state = 0, name = 0;
        if (TryPointer(actor + slot, state) && TryPointer(state + kStateIdName, name)) return ReadCString(name, kStateNameMax);
        return std::string();
    };
    out.stateName = stateName(kStateId);
    out.demoStateName = stateName(kDemoStateId);
    return true;
}

namespace {
std::uint32_t g_inputHold = 0, g_inputTrig = 0;
constexpr char kDemoNone[] = "StateID_DemoNone";
}  // namespace

void RecordInput(std::uint32_t hold, std::uint32_t trig) {
    g_inputHold = hold;
    g_inputTrig = trig;
}
std::uint32_t InputTrig() { return g_inputTrig; }
std::uint32_t InputHold() { return g_inputHold; }

bool InNormalPlay(const PlayerState& player) {
    return player.demoStateName.empty() || player.demoStateName.find(kDemoNone) != std::string::npos;
}

bool PlayerOutOfPlay(std::uint32_t actor) {
    std::uint32_t word = 0;
    const std::uint32_t addr = actor + kStatusFlags + 4u * (kStatusOutOfPlay / kStatusBitsPerWord);
    return TryU32(addr, word) && ((word >> (kStatusOutOfPlay % kStatusBitsPerWord)) & 1u) != 0;
}

int Lives(int playerType) {
    std::uint32_t v = 0;
    if (playerType < 0 || playerType > 3 || !TryU32(kPlayerRest + 4u * playerType, v)) return -1;
    return static_cast<int>(v);
}

int PlayerTypeOfSlot(int slot) {
    std::uint32_t v = 0;
    if (slot < 0 || slot > 3 || !TryU32(kSlotPlayerType + 4u * slot, v)) return 0;
    return static_cast<int>(v);
}

int TimerSeconds() {
    std::uint32_t timer = 0, value = 0;
    if (!TryPointer(kStageTimer, timer) || !TryU32(timer + kStageTimerValue, value)) return -1;
    const int raw = static_cast<int>(value);
    if (raw <= 0) return -1;
    return (raw + (1 << kTimerUnitShift) - 1) >> kTimerUnitShift;
}

std::uint8_t AreaNo() {
    std::uint32_t stage = 0;
    std::uint8_t area = 0;
    if (TryPointer(kStageScene, stage)) TryU8(stage + kStageAreaNo, area);
    return area;
}

bool ReadActor(std::uint32_t actor, ActorInfo& out) {
    if (!TryVec3(actor + kPos, out.pos)) return false;
    Vec3 offs;
    TryVec3(actor + kCenterOffs, offs);
    out.center = {out.pos.x + offs.x, out.pos.y + offs.y, out.pos.z};
    out.actor = actor;
    TryU16(actor + kProfName, out.profile);
    TryU8(actor + kGroupType, out.group);
    TryU8(actor + kKind, out.kind);
    TryU8(actor + kLayer, out.layer);
    std::uint8_t visible = 0;
    TryU8(actor + kVisible, visible);
    out.visible = visible != 0;
    return true;
}

bool GroundBelow(float x, float y, std::uint8_t layer, float& height) {
    GuestCall call;
    const std::uint32_t vec = call.Scratch(0), out = call.Scratch(16);
    WriteVec(call, vec, x, y);
    call.WriteFloat(out, 0.0f);
    call.Gpr(3, vec);
    call.Gpr(4, out);
    call.Gpr(5, layer);
    call.Gpr(6, kLineKind);
    call.Gpr(7, kAnyActor);
    const bool hit = (call.Invoke<NSMBW_VA(0x800757B0u)>() & 0xFFu) != 0;  // dBc_c::checkGround
    height = call.ReadFloat(out);
    return hit;
}

bool WallBetween(float x0, float x1, float y, std::uint8_t layer, float& wallX) {
    GuestCall call;
    const std::uint32_t from = call.Scratch(0), to = call.Scratch(16), out = call.Scratch(32);
    WriteVec(call, from, x0, y);
    WriteVec(call, to, x1, y);
    call.WriteFloat(out, x1);
    call.Gpr(3, from);
    call.Gpr(4, to);
    call.Gpr(5, out);
    call.Gpr(6, layer);
    call.Gpr(7, kLineKind);
    call.Gpr(8, 0);
    const std::uint32_t hit = call.Invoke<NSMBW_VA(0x80075FD0u)>() & 0xFFu;  // dBc_c::checkWall
    wallX = call.ReadFloat(out);
    return hit != 0;
}

bool CeilingAbove(float x, float y, std::uint8_t layer, float& height) {
    GuestCall call;
    const std::uint32_t vec = call.Scratch(0), out = call.Scratch(16);
    WriteVec(call, vec, x, y);
    call.WriteFloat(out, 0.0f);
    call.Gpr(3, vec);
    call.Gpr(4, out);
    call.Gpr(5, layer);
    call.Gpr(6, kLineKind);
    const std::uint32_t hit = call.Invoke<NSMBW_VA(0x80075CA0u)>() & 0xFFu;  // dBc_c::checkTenjou
    height = call.ReadFloat(out);
    return hit != 0;
}

std::uint32_t UnitKind(float x, float y, std::uint8_t layer) {
    GuestCall call;
    call.Fpr(1, x);
    call.Fpr(2, y);
    call.Gpr(3, layer);
    return call.Invoke<NSMBW_VA(0x80070BF0u)>();  // dBc_c::getUnitKind
}

std::uint32_t UnitType(float x, float y, std::uint8_t layer) {
    GuestCall call;
    call.Fpr(1, x);
    call.Fpr(2, y);
    call.Gpr(3, layer);
    return call.Invoke<NSMBW_VA(0x80070BA0u)>();  // dBc_c::getUnitType
}

int WaterAt(float x, float y, std::uint8_t layer) {
    GuestCall call;
    const std::uint32_t out = call.Scratch(0);
    call.WriteFloat(out, 0.0f);
    call.Fpr(1, x);
    call.Fpr(2, y);
    call.Gpr(3, layer);
    call.Gpr(4, out);
    return static_cast<int>(call.Invoke<NSMBW_VA(0x80075270u)>());  // dBc_c::checkWater
}

}  // namespace a11y::game

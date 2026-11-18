#ifndef NSMBW_ACCESSIBILITY_GAME_NSMBW_GAME_H
#define NSMBW_ACCESSIBILITY_GAME_NSMBW_GAME_H

#include <cstdint>
#include <string>

#include "accessibility/guest_read.h"

// The engine adapter: every NSMBW address and struct offset the mod reads lives behind these
// accessors. Offsets and their derivation: docs/accessibility/nsmbw-structures.md.
namespace a11y::game {

// Scene profile IDs (fProf::PROFILE_NAME_e) as reported by createRoot.
enum class Scene : std::uint32_t {
    Boot = 0,
    WorldMap = 3,
    Stage = 5,  // the title screen and every level
    RestartCourseIn = 6,
    CourseIn = 7,
    Movie = 8,
    GameSetup = 10,
};

std::uint32_t SceneProfile();
bool InScene(Scene scene);

// dInfo_c::m_startGameInfo.mGameMode: 2 and 3 are the title screen and its replay demo.
std::uint8_t StartGameMode();
bool OnTitleScreen();
// The level the game is entering or playing (m_startGameInfo.mWorld1 / mLevel1, 0-based).
std::uint8_t StartWorld();
std::uint8_t StartLevel();

// dGameCom's game-stop word: non-zero while a pause window or a map menu holds the game.
bool GamePaused();

struct PlayerState {
    std::uint32_t actor = 0;
    Vec3 pos;        // feet, world units (16 per tile), y up
    Vec3 speed;
    std::uint8_t direction = 0;  // 0 right, 1 left
    std::uint8_t layer = 0;
    std::uint32_t bgFlags = 0;   // dBc_c::mFlags
    int powerup = 0;             // PLAYER_POWERUP_e
    int starTimer = 0;
    int playerType = 0;          // character, indexes daPyMng_c::mRest
    bool onGround = false;
    bool wallLeft = false, wallRight = false;
    std::string stateName;       // the player's state machine state, e.g. "StateID_Jump"
    std::string demoStateName;   // the cutscene state machine: "StateID_DemoNone" while playing
};

// Player `slot` (0 = player 1), through the game's own daPyMng_c::getPlayer.
std::uint32_t PlayerActor(int slot);
bool ReadPlayer(std::uint32_t actor, PlayerState& out);
bool PlayerOutOfPlay(std::uint32_t actor);  // dead or in a bubble

// Not in a cutscene (pipe, door, goal): the cues only make sense while the player controls Mario.
bool InNormalPlay(const PlayerState& player);

// Player 1's Wii Remote buttons as the game received them on the last frame (sideways remote:
// the screen directions are the remote's rotated d-pad).
void RecordInput(std::uint32_t hold, std::uint32_t trig);
std::uint32_t InputTrig();
std::uint32_t InputHold();
inline constexpr std::uint32_t kScreenLeft = 1u << 3;   // WPAD up (nsmbw_controls.cpp)
inline constexpr std::uint32_t kScreenRight = 1u << 2;  // WPAD down
inline constexpr std::uint32_t kScreenUp = 1u << 1;     // WPAD right
inline constexpr std::uint32_t kScreenDown = 1u << 0;   // WPAD left
inline constexpr std::uint32_t kScreenDirections = kScreenLeft | kScreenRight | kScreenUp | kScreenDown;

int Lives(int playerType);
int PlayerTypeOfSlot(int slot);  // daPyMng_c::mPlayerType
int TimerSeconds();  // -1 when the level has no running timer
std::uint8_t AreaNo();

// Actors (dBaseActor_c list): call `visit(actor)` for every live actor, starting from any one.
template <typename F>
void ForEachActor(std::uint32_t anyActor, F&& visit);

struct ActorInfo {
    std::uint32_t actor = 0;
    std::uint16_t profile = 0;
    std::uint8_t group = 0;  // fBase_c::mGroupType: 2 actor, 3 enemy
    std::uint8_t kind = 0;   // dActor_c::mKind: 3 enemy
    std::uint8_t layer = 0;
    Vec3 pos;
    Vec3 center;  // pos + mCenterOffs
    Vec3 delta;   // mPosDelta: how far it moved last frame
    bool visible = false;
};
bool ReadActor(std::uint32_t actor, ActorInfo& out);

// Background collision, through the game's own dBc_c queries (decomp d_bc.hpp).
// Ground height below (x, y): true and the surface height when there is ground.
bool GroundBelow(float x, float y, std::uint8_t layer, float& height);
// Horizontal sweep from (x0, y) to (x1, y): true and the wall's x when it hits.
bool WallBetween(float x0, float x1, float y, std::uint8_t layer, float& wallX);
bool CeilingAbove(float x, float y, std::uint8_t layer, float& height);
// Whether the point (x, y) is inside solid background (dBc_c::checkBg).
bool SolidAt(float x, float y, std::uint8_t layer);
std::uint32_t UnitKind(float x, float y, std::uint8_t layer);
std::uint32_t UnitType(float x, float y, std::uint8_t layer);
// WATER_TYPE_e: 0 none, 1 water, 2 bubble, 3 lava, 4 poison.
int WaterAt(float x, float y, std::uint8_t layer);

// Constants of the world space.
inline constexpr float kTile = 16.0f;

namespace detail {
inline constexpr std::uint32_t kLinkPrev = 0x70;  // dBaseActor_c::mLinkActor
inline constexpr std::uint32_t kLinkNext = 0x74;
inline constexpr std::uint32_t kLinkOwner = 0x78;
inline constexpr int kMaxActors = 1024;
}  // namespace detail

template <typename F>
void ForEachActor(std::uint32_t anyActor, F&& visit) {
    using namespace detail;
    std::uint32_t node = anyActor + kLinkPrev;
    for (int i = 0; i < kMaxActors; ++i) {  // back to the head of the list
        std::uint32_t prev = 0;
        if (!TryU32(node, prev) || prev == 0 || !Memory::Contains(prev, 12)) break;
        node = prev;
    }
    for (int i = 0; i < kMaxActors && node != 0; ++i) {
        std::uint32_t owner = 0, next = 0;
        if (!TryU32(node + (kLinkOwner - kLinkPrev), owner) || !TryU32(node + (kLinkNext - kLinkPrev), next)) break;
        if (owner != 0 && owner + kLinkPrev == node) visit(owner);
        node = next;
        if (node != 0 && !Memory::Contains(node, 12)) break;
    }
}

}  // namespace a11y::game

#endif  // NSMBW_ACCESSIBILITY_GAME_NSMBW_GAME_H

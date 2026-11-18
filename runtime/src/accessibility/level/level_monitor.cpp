#include "accessibility/level/level_monitor.h"

#include <cmath>

#include "accessibility/level/actors.h"
#include "accessibility/localization.h"
#include "accessibility/speech.h"
#include "nsmbw_va.h"

namespace a11y::level::monitor {
namespace {

// dScStage_c::mCollectionCoin[3] (syms.txt): who took each star coin of this course, -1 / 4 none.
constexpr std::uint32_t kCollectionCoin = NSMBW_VA(0x803744B0u);
constexpr int kStarCoins = 3;
constexpr std::uint8_t kGameModeTitle = 2;  // dInfo_c::GameMode_e
constexpr std::uint8_t kGameModeTitleReplay = 3;
constexpr int kNoCollector = 4;  // PLAYER_TYPE_e count: the "nobody" value
constexpr int kHurrySeconds = 100;  // the game's own hurry-up threshold (its music speeds up)
constexpr int kPowerupCount = 7;
constexpr float kCentreTiles = 1.0f;

// Course numbers (dInfo_c stage IDs, decomp d_info.hpp): 0-8 numbered levels, then the specials.
constexpr int kLastNumbered = 8;
constexpr int kCoinBattle = 19;
constexpr int kGhost = 20;
constexpr int kTowerFirst = 21, kTowerLast = 22;
constexpr int kCastleFirst = 23, kCastleLast = 24;
constexpr int kToadFirst = 25, kToadLast = 31;
constexpr int kEnemyFirst = 32, kEnemyLast = 34;
constexpr int kCannon = 35;
constexpr int kAirshipFirst = 36, kAirshipLast = 37;
constexpr int kStartToad = 38;
constexpr int kPeachCastle = 40;

struct LevelState {
    bool active = false;
    int powerup = -1;
    bool outOfPlay = false;
    bool hurrySaid = false;
    int starCoins[kStarCoins] = {kNoCollector, kNoCollector, kNoCollector};
    int water = -1;
    float startX = 0.0f;
    std::string demo;
    std::uint8_t area = 0xFF;
};
LevelState g_level;
std::uint32_t g_lastScene = 0xFFFFFFFFu;

void ReadStarCoins(int out[kStarCoins]) {
    for (int i = 0; i < kStarCoins; ++i) {
        std::uint32_t v = kNoCollector;
        TryU32(kCollectionCoin + 4u * i, v);
        out[i] = static_cast<int>(v);
    }
}

bool Collected(int v) { return v >= 0 && v < kNoCollector; }

int CollectedCount(const int coins[kStarCoins]) {
    int n = 0;
    for (int i = 0; i < kStarCoins; ++i) n += Collected(coins[i]) ? 1 : 0;
    return n;
}

std::string PowerName(int powerup, int starTimer) {
    const std::string name = loc::Get("power_" + std::to_string(powerup >= 0 && powerup < kPowerupCount ? powerup : 0));
    return starTimer > 0 ? name + ", " + loc::Get("star_power") : name;
}

void OnSceneChange(std::uint32_t previous, std::uint32_t scene) {
    const bool courseIn = scene == static_cast<std::uint32_t>(game::Scene::CourseIn) ||
                          scene == static_cast<std::uint32_t>(game::Scene::RestartCourseIn);
    // A course is entered from the map, or restarted after a lost life. At boot and behind the
    // title the game also loads courses, which are not announced.
    const bool fromMap = previous == static_cast<std::uint32_t>(game::Scene::WorldMap);
    const bool fromStage = previous == static_cast<std::uint32_t>(game::Scene::Stage);
    const std::uint8_t mode = game::StartGameMode();
    if (!courseIn || !(fromMap || fromStage) || mode == kGameModeTitle || mode == kGameModeTitleReplay) return;
    const int world = game::StartWorld(), level = game::StartLevel();
    std::string text = CourseName(world, level);
    // No player actor exists yet on this screen: lives come from player 1's character slot.
    const int lives = game::Lives(game::PlayerTypeOfSlot(0));
    if (lives >= 0) text += ". " + loc::Format("lives", {{"n", std::to_string(lives)}});
    speech::Say(text, true);
}

// Cutscene states worth a word (the state names are the game's own, decomp d_a_player_base.hpp).
const char* DemoPhrase(const std::string& state) {
    if (state.find("StateID_DemoGoal") != std::string::npos) return "goal";
    if (state.find("StateID_DemoInDokan") != std::string::npos) return "demo_pipe";
    if (state.find("StateID_DemoInDoor") != std::string::npos) return "demo_door";
    if (state.find("StateID_DemoInVine") != std::string::npos) return "demo_vine";
    return nullptr;
}

void TickLevel(const game::PlayerState& p) {
    if (!g_level.active) {
        g_level = LevelState{};
        g_level.active = true;
        g_level.powerup = p.powerup;
        g_level.startX = p.pos.x;
        g_level.area = game::AreaNo();
        g_level.demo = p.demoStateName;
        ReadStarCoins(g_level.starCoins);
        return;
    }
    if (game::AreaNo() != g_level.area) {  // a pipe or door into another area
        g_level.area = game::AreaNo();
        g_level.startX = p.pos.x;
    }
    if (p.powerup != g_level.powerup) {
        g_level.powerup = p.powerup;
        speech::Say(PowerName(p.powerup, 0), false);
    }
    const bool out = game::PlayerOutOfPlay(p.actor);
    if (out && !g_level.outOfPlay) speech::Say(loc::Get("dead"), true);
    g_level.outOfPlay = out;

    if (p.demoStateName != g_level.demo) {
        g_level.demo = p.demoStateName;
        if (const char* key = DemoPhrase(p.demoStateName)) speech::Say(loc::Get(key), false);
    }

    const int seconds = game::TimerSeconds();
    if (seconds > kHurrySeconds) g_level.hurrySaid = false;
    if (seconds > 0 && seconds <= kHurrySeconds && !g_level.hurrySaid) {
        g_level.hurrySaid = true;
        speech::Say(loc::Format("time_low", {{"n", std::to_string(seconds)}}), false);
    }

    int coins[kStarCoins];
    ReadStarCoins(coins);
    for (int i = 0; i < kStarCoins; ++i) {
        if (Collected(coins[i]) && !Collected(g_level.starCoins[i])) {
            speech::Say(loc::Get("star_coin_got") + " " + std::to_string(i + 1), false);
        }
        g_level.starCoins[i] = coins[i];
    }

    const int water = game::WaterAt(p.pos.x, p.pos.y + kCentreTiles * game::kTile, p.layer);
    const bool wet = water == 1;
    if (g_level.water >= 0 && wet != (g_level.water == 1)) speech::Say(loc::Get(wet ? "in_water" : "out_water"), false);
    g_level.water = water;
}

}  // namespace

std::string CourseName(int world, int level) {
    const std::string w = std::to_string(world + 1);
    const auto fmt = [&](const char* key) { return loc::Format(key, {{"world", w}, {"level", std::to_string(level + 1)}}); };
    if (level <= kLastNumbered) return fmt("course_level");
    if (level == kGhost) return fmt("course_ghost");
    if (level >= kTowerFirst && level <= kTowerLast) return fmt("course_tower");
    if (level >= kCastleFirst && level <= kCastleLast) return fmt("course_castle");
    if ((level >= kToadFirst && level <= kToadLast) || level == kStartToad) return fmt("course_toad");
    if (level >= kEnemyFirst && level <= kEnemyLast) return fmt("course_enemy");
    if (level == kCannon) return fmt("course_cannon");
    if (level >= kAirshipFirst && level <= kAirshipLast) return fmt("course_airship");
    if (level == kPeachCastle) return fmt("course_peach");
    if (level == kCoinBattle) return fmt("course_coin_battle");
    return fmt("course_other");
}

void Tick(const game::PlayerState* player) {
    const std::uint32_t scene = game::SceneProfile();
    if (scene != g_lastScene) {
        const std::uint32_t previous = g_lastScene;
        g_lastScene = scene;
        g_level.active = false;
        OnSceneChange(previous, scene);
    }
    // A pause or the options menu hides the player for a while; the level state survives it and
    // only a scene change starts it over.
    if (player == nullptr) return;
    TickLevel(*player);
}

std::string Status(const game::PlayerState& p) {
    const int lives = game::Lives(p.playerType);
    std::string text = loc::Format("status", {{"lives", std::to_string(lives < 0 ? 0 : lives)},
                                              {"power", PowerName(p.powerup, p.starTimer)}});
    const int seconds = game::TimerSeconds();
    if (seconds > 0) text += loc::Format("status_time", {{"time", std::to_string(seconds)}});
    int coins[kStarCoins];
    ReadStarCoins(coins);
    text += ". " + loc::Format("star_coins", {{"n", std::to_string(CollectedCount(coins))}});
    return text;
}

std::string Progress(const game::PlayerState& p) {
    Vec3 goal;
    const float fromStart = std::fabs(p.pos.x - g_level.startX) / game::kTile;
    if (!FindGoal(p, goal)) {
        return loc::Format("progress_no_goal", {{"tiles", std::to_string(static_cast<int>(std::lround(fromStart)))}});
    }
    const float span = goal.x - g_level.startX;
    const float done = span != 0.0f ? (p.pos.x - g_level.startX) / span : 1.0f;
    const int percent = static_cast<int>(std::lround(std::fmin(std::fmax(done, 0.0f), 1.0f) * 100.0f));
    const float dx = (goal.x - p.pos.x) / game::kTile;
    return loc::Format("progress", {{"percent", std::to_string(percent)},
                                    {"tiles", std::to_string(static_cast<int>(std::lround(std::fabs(dx))))},
                                    {"dir", loc::Get(dx < 0 ? "dir_left" : "dir_right")}});
}

}  // namespace a11y::level::monitor

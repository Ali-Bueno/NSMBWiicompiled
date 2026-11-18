#include "accessibility/level/actors.h"

#include <algorithm>
#include <cmath>

#include "accessibility/a11y_config.h"
#include "accessibility/audio/cues.h"
#include "accessibility/game/game_sound.h"
#include "accessibility/level/entrances.h"
#include "accessibility/localization.h"

namespace a11y::level {
namespace {

struct ProfileName {
    std::uint16_t profile;
    ActorCategory category;
    const char* phraseKey;
};

constexpr ProfileName kProfiles[] = {
#include "accessibility/level/actor_names.inc"
};

// Where the player is centred for distances: half a big Mario's height above the feet.
constexpr float kPlayerCentreTiles = 1.0f;
// Cue ranges, in tiles: roughly what the screen shows ahead and around.
constexpr float kEnemyRangeX = 12.0f;
constexpr float kEnemyRangeY = 7.0f;
constexpr float kItemRangeX = 8.0f;
constexpr float kItemRangeY = 6.0f;
constexpr float kUnderItemUnits = 7.0f;    // Kirby's 0.45 tile: lined up under it
constexpr float kUnderItemMaxTiles = 10.0f;
constexpr float kListRangeX = 14.0f;
constexpr float kListRangeY = 8.0f;
constexpr int kEnemyVoices = 4;  // the nearest few, staggered inside one radar pass
constexpr size_t kMaxListed = 6;
// Scanning every frame is not needed for cues that repeat at most a few times per second.
constexpr int kScanEveryFrames = 4;

const ProfileName* Lookup(std::uint16_t profile) {
    for (const ProfileName& p : kProfiles) {
        if (p.profile == profile) return &p;
    }
    return nullptr;
}

struct CueTimers {
    float enemyPass = 0.0f;
    float boss = 0.0f;
    float item = 0.0f;
    float underItem = 0.0f;
    float goal = 0.0f;
};
CueTimers g_timers;
std::vector<NearbyActor> g_cached;
int g_framesSinceScan = kScanEveryFrames;
std::uint32_t g_underItem = 0;  // the item Mario was last lined up under

int Round(float v) { return static_cast<int>(std::lround(v)); }

}  // namespace

std::string Where(int dx, int dy) {
    // Within a tile vertically counts as level with the player.
    const bool level = std::abs(dy) < 2;
    const std::string vertical = std::to_string(std::abs(dy)) + " " + loc::Get(dy < 0 ? "below" : "above");
    if (dx == 0) return level ? loc::Get("here") : vertical;
    std::string where = std::to_string(std::abs(dx)) + " " + loc::Get(dx < 0 ? "to_left" : "to_right");
    if (!level) where += ", " + vertical;
    return where;
}

namespace {

}  // namespace

ActorCategory Classify(const game::ActorInfo& info, std::string* phraseKey) {
    if (const ProfileName* p = Lookup(info.profile)) {
        if (phraseKey) *phraseKey = p->phraseKey;
        return p->category;
    }
    // No fallback on dActor_c::mKind: coins and items carry the enemy kind too (seen in 1-1:
    // EN_COIN and EN_ITEM report kind 3), so only the profiles named in the table are cued.
    if (phraseKey) phraseKey->clear();
    return ActorCategory::Unknown;
}

std::vector<NearbyActor> Scan(const game::PlayerState& player, float rangeX, float rangeY) {
    std::vector<NearbyActor> out;
    const float px = player.pos.x / game::kTile;
    const float py = player.pos.y / game::kTile + kPlayerCentreTiles;
    game::ForEachActor(player.actor, [&](std::uint32_t actor) {
        if (actor == player.actor) return;
        game::ActorInfo info;
        if (!game::ReadActor(actor, info) || info.layer != player.layer) return;
        NearbyActor n;
        n.category = Classify(info, &n.phraseKey);
        if (n.category == ActorCategory::Unknown) return;
        n.dx = info.center.x / game::kTile - px;
        n.dy = info.center.y / game::kTile - py;
        if (std::fabs(n.dx) > rangeX || std::fabs(n.dy) > rangeY) return;
        n.distance = std::hypot(n.dx, n.dy);
        n.info = info;
        out.push_back(std::move(n));
    });
    std::sort(out.begin(), out.end(), [](const NearbyActor& a, const NearbyActor& b) { return a.distance < b.distance; });
    return out;
}

bool FindGoal(const game::PlayerState& player, Vec3& goal) {
    bool found = false;
    game::ForEachActor(player.actor, [&](std::uint32_t actor) {
        if (found) return;
        game::ActorInfo info;
        if (game::ReadActor(actor, info) && Classify(info, nullptr) == ActorCategory::Goal) {
            goal = info.pos;
            found = true;
        }
    });
    return found || entrances::GoalFromCourseData(goal);
}

namespace {

void TickEnemyRadar(const std::vector<NearbyActor>& near) {
    namespace cues = audio::cues;
    // Bosses: their own drum, on their own timer.
    for (const NearbyActor& n : near) {
        if (n.category != ActorCategory::Boss) continue;
        if (g_timers.boss <= 0.0f) {
            cues::Play(cues::Boss(cues::PanFor(n.dx, kEnemyRangeX), n.dy));
            g_timers.boss = cues::BossInterval(n.distance);
        }
        break;
    }
    if (g_timers.enemyPass > 0.0f) return;
    int voice = 0;
    float nearest = kEnemyRangeX;
    for (const NearbyActor& n : near) {
        if (voice >= kEnemyVoices) break;
        if (!IsHostile(n.category) || n.category == ActorCategory::Boss) continue;
        if (std::fabs(n.dx) > kEnemyRangeX || std::fabs(n.dy) > kEnemyRangeY) continue;
        if (voice == 0) nearest = n.distance;
        cues::Composite sig = cues::EnemySignature(n.info.profile, n.category == ActorCategory::Danger,
                                                   cues::PanFor(n.dx, kEnemyRangeX), n.dy,
                                                   cues::GainFor(n.distance, kEnemyRangeX));
        for (audio::CueSpec& part : sig) part.delaySec += cues::kEnemyStaggerSec * voice;
        cues::Play(sig);
        ++voice;
    }
    g_timers.enemyPass = voice > 0 ? cues::EnemyPassInterval(nearest) : 0.0f;
}

void TickItems(const game::PlayerState& player, const std::vector<NearbyActor>& near) {
    namespace cues = audio::cues;
    const bool gameSounds = config::Get().gameSounds;
    std::uint32_t under = 0;
    for (const NearbyActor& n : near) {
        if (n.category != ActorCategory::Item && n.category != ActorCategory::StarCoin) continue;
        // Lined up under it: a bell, so Mario knows to jump straight up.
        if (under == 0 && std::fabs(n.info.center.x - player.pos.x) < kUnderItemUnits && n.dy > 0.5f &&
            n.dy < kUnderItemMaxTiles) {
            under = n.info.actor;
            if (g_underItem != under || g_timers.underItem <= 0.0f) {
                cues::Play(cues::UnderItem(n.dy));
                g_timers.underItem = cues::kUnderItemRepeatSec;
            }
        }
        if (g_timers.item > 0.0f || std::fabs(n.dx) > kItemRangeX || std::fabs(n.dy) > kItemRangeY) continue;
        if (gameSounds && game::sound::PlayAt(cues::kGameItemLabel, n.info.center.x, n.info.center.y)) {
            g_timers.item = cues::kGameItemIntervalSec;
        } else {
            cues::Play(cues::Item(cues::PanFor(n.dx, kItemRangeX), n.dy, cues::GainFor(n.distance, kItemRangeX)));
            g_timers.item = cues::kItemIntervalSec;
        }
    }
    g_underItem = under;
}

}  // namespace

void TickCues(const game::PlayerState& player, float frameSec) {
    const config::Settings& cfg = config::Get();
    if (!cfg.objectCues && !cfg.goalBeacon) return;
    if (++g_framesSinceScan >= kScanEveryFrames) {
        g_framesSinceScan = 0;
        g_cached = Scan(player, kListRangeX, kListRangeY);
    }
    g_timers.enemyPass -= frameSec;
    g_timers.boss -= frameSec;
    g_timers.item -= frameSec;
    g_timers.underItem -= frameSec;
    g_timers.goal -= frameSec;
    if (cfg.objectCues) {
        TickEnemyRadar(g_cached);
        TickItems(player, g_cached);
    }
    if (cfg.goalBeacon && g_timers.goal <= 0.0f) {
        g_timers.goal = audio::cues::kGoalIntervalSec;
        Vec3 goal;
        if (FindGoal(player, goal)) {
            const float dx = (goal.x - player.pos.x) / game::kTile;
            audio::cues::Play(audio::cues::Goal(audio::cues::PanFor(dx, kListRangeX)));
        }
    }
}

void StopCues() {
    g_cached.clear();
    g_timers = CueTimers{};
    g_underItem = 0;
}

std::string DescribeNearby(const game::PlayerState& player) {
    const std::vector<NearbyActor> list = Scan(player, kListRangeX, kListRangeY);
    std::string out;
    size_t spoken = 0;
    for (const NearbyActor& n : list) {
        if (spoken >= kMaxListed) break;
        if (n.category == ActorCategory::Coin || n.category == ActorCategory::Block) continue;
        std::string name = n.phraseKey.empty() ? loc::Get("kind_enemy") : loc::Get(n.phraseKey);
        if (n.category == ActorCategory::Danger) name += ", " + loc::Get("danger_suffix");
        const std::string entry = loc::Format("nearby_entry", {{"name", name}, {"where", Where(Round(n.dx), Round(n.dy))}});
        if (!out.empty()) out += ". ";
        out += entry;
        ++spoken;
    }
    for (const std::string& pipe : entrances::Describe(player, kListRangeX)) {
        if (!out.empty()) out += ". ";
        out += pipe;
    }
    return out.empty() ? loc::Get("nearby_none") : out;
}

}  // namespace a11y::level

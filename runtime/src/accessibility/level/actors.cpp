#include "accessibility/level/actors.h"

#include <algorithm>
#include <cmath>

#include "accessibility/a11y_config.h"
#include "accessibility/audio/cues.h"
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
constexpr float kListRangeX = 14.0f;
constexpr float kListRangeY = 8.0f;
constexpr int kEnemyVoices = 3;
constexpr size_t kMaxListed = 6;
// Scanning every frame is not needed for cues that repeat at most a few times per second.
constexpr int kScanEveryFrames = 4;

const ProfileName* Lookup(std::uint16_t profile) {
    for (const ProfileName& p : kProfiles) {
        if (p.profile == profile) return &p;
    }
    return nullptr;
}

const audio::CueChannel kEnemyChannels[kEnemyVoices] = {
    audio::CueChannel::Enemy0, audio::CueChannel::Enemy1, audio::CueChannel::Enemy2};

struct CueTimers {
    float enemy[kEnemyVoices] = {};
    float item = 0.0f;
    float goal = 0.0f;
};
CueTimers g_timers;
std::vector<NearbyActor> g_cached;
int g_framesSinceScan = kScanEveryFrames;

int Round(float v) { return static_cast<int>(std::lround(v)); }

}  // namespace

std::string Where(int dx, int dy) {
    std::string where = dx == 0 ? loc::Get("here")
                                : std::to_string(std::abs(dx)) + " " + loc::Get(dx < 0 ? "to_left" : "to_right");
    if (std::abs(dy) >= 2) where += ", " + std::to_string(std::abs(dy)) + " " + loc::Get(dy < 0 ? "below" : "above");
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

void TickCues(const game::PlayerState& player, float frameSec) {
    auto& cues = audio::CueService::Instance();
    const config::Settings& cfg = config::Get();
    if (!cfg.objectCues && !cfg.goalBeacon) return;

    if (++g_framesSinceScan >= kScanEveryFrames) {
        g_framesSinceScan = 0;
        g_cached = Scan(player, std::max(kEnemyRangeX, kListRangeX), std::max(kEnemyRangeY, kListRangeY));
    }

    for (float& t : g_timers.enemy) t -= frameSec;
    g_timers.item -= frameSec;
    g_timers.goal -= frameSec;

    if (cfg.objectCues) {
        int voice = 0;
        bool itemPlayed = false;
        for (const NearbyActor& n : g_cached) {
            if (n.category == ActorCategory::Enemy && voice < kEnemyVoices) {
                if (std::fabs(n.dx) > kEnemyRangeX || std::fabs(n.dy) > kEnemyRangeY) continue;
                if (g_timers.enemy[voice] <= 0.0f) {
                    cues.PlayOneShot(kEnemyChannels[voice],
                                     audio::cues::Enemy(audio::cues::PanFor(n.dx, kEnemyRangeX), n.dy,
                                                        audio::cues::GainFor(n.distance, kEnemyRangeX)));
                    g_timers.enemy[voice] = audio::cues::EnemyInterval(n.distance);
                }
                ++voice;
            } else if (!itemPlayed && (n.category == ActorCategory::Item || n.category == ActorCategory::StarCoin)) {
                if (std::fabs(n.dx) > kItemRangeX || std::fabs(n.dy) > kItemRangeY) continue;
                itemPlayed = true;
                if (g_timers.item <= 0.0f) {
                    cues.PlayOneShot(audio::CueChannel::Item,
                                     audio::cues::Item(audio::cues::PanFor(n.dx, kItemRangeX), n.dy,
                                                       audio::cues::GainFor(n.distance, kItemRangeX)));
                    g_timers.item = audio::cues::kItemIntervalSec;
                }
            }
        }
    }

    if (cfg.goalBeacon && g_timers.goal <= 0.0f) {
        g_timers.goal = audio::cues::kGoalIntervalSec;
        Vec3 goal;
        if (FindGoal(player, goal)) {
            const float dx = (goal.x - player.pos.x) / game::kTile;
            cues.PlayOneShot(audio::CueChannel::Goal, audio::cues::Goal(audio::cues::PanFor(dx, kListRangeX)));
        }
    }
}

void StopCues() {
    auto& cues = audio::CueService::Instance();
    for (audio::CueChannel ch : kEnemyChannels) cues.Stop(ch);
    cues.Stop(audio::CueChannel::Item);
    cues.Stop(audio::CueChannel::Goal);
    cues.Stop(audio::CueChannel::Pipe);
    g_cached.clear();
}

std::string DescribeNearby(const game::PlayerState& player) {
    const std::vector<NearbyActor> list = Scan(player, kListRangeX, kListRangeY);
    std::string out;
    size_t spoken = 0;
    for (const NearbyActor& n : list) {
        if (spoken >= kMaxListed) break;
        if (n.category == ActorCategory::Coin || n.category == ActorCategory::Block) continue;
        std::string name = n.phraseKey.empty() ? loc::Get("kind_enemy") : loc::Get(n.phraseKey);
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

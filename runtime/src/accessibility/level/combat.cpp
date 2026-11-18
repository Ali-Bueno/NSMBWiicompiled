#include "accessibility/level/combat.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <aurora/env.hpp>

#include "accessibility/a11y_config.h"
#include "accessibility/a11y_log.h"
#include "accessibility/audio/cues.h"
#include "accessibility/level/actors.h"
#include "accessibility/test_autopilot.h"

namespace a11y::level::combat {
namespace {

using game::kTile;

// Mario's jump, measured on 1-1 with the NSMBW_A11Y_LOG_COMBAT trace while walking (1.5 units
// per frame): his height above the take-off point, frame by frame after leaving the ground. A tap
// (jump held 8 frames) peaks at 44 units; a full jump (held throughout) at 82. After the table
// Mario keeps falling at the -4 units/frame cap. A faster run jumps somewhat higher, so these are
// the conservative arcs.
constexpr float kTapArc[] = {3.8f,  7.6f,  11.2f, 14.9f, 18.4f, 21.9f, 25.4f, 28.8f, 31.8f, 34.5f, 36.9f, 38.9f,
                             40.6f, 42.0f, 43.0f, 43.6f, 44.0f, 44.0f, 43.7f, 43.1f, 42.2f, 40.9f, 39.3f, 37.3f,
                             35.0f, 32.3f, 29.4f, 26.0f, 22.4f, 18.4f, 14.4f, 10.4f, 6.4f,  2.4f};
constexpr float kFullArc[] = {3.8f,  7.6f,  11.2f, 14.9f, 18.4f, 21.9f, 25.4f, 28.8f, 32.1f, 35.4f, 38.6f, 41.7f,
                              44.8f, 47.9f, 50.8f, 53.7f, 56.6f, 59.4f, 62.1f, 64.8f, 67.4f, 69.9f, 72.4f, 74.6f,
                              76.6f, 78.4f, 79.9f, 81.0f, 81.8f, 82.3f, 82.4f, 82.5f, 82.4f, 82.3f, 82.1f, 81.6f,
                              80.8f, 79.7f, 78.3f, 76.5f, 74.4f, 72.0f, 69.2f, 66.1f, 62.7f, 59.0f, 55.0f, 51.0f};
constexpr float kFallCap = 4.0f;
constexpr float kStompClearance = 4.0f;    // feet must come down from at least this far above the top

// Frames from take-off until Mario's feet come down to `height` units above his take-off point, or
// a negative value when the arc never gets that high.
template <size_t N>
float FramesToLandAt(const float (&arc)[N], float height) {
    float peak = 0.0f;
    size_t peakAt = 0;
    for (size_t i = 0; i < N; ++i) {
        if (arc[i] > peak) {
            peak = arc[i];
            peakAt = i;
        }
    }
    if (peak < height + kStompClearance) return -1.0f;
    for (size_t i = peakAt; i < N; ++i) {
        if (arc[i] <= height) return static_cast<float>(i + 1);
    }
    return static_cast<float>(N) + (arc[N - 1] - height) / kFallCap;
}
// Frames from take-off until Mario's feet are first `height` units up (clear of the top), or a
// negative value when the arc never gets that high.
template <size_t N>
float FramesToRise(const float (&arc)[N], float height) {
    for (size_t i = 0; i < N; ++i) {
        if (arc[i] >= height + kStompClearance) return static_cast<float>(i + 1);
    }
    return -1.0f;
}
template <size_t N>
float FramesToPeak(const float (&arc)[N]) {
    return static_cast<float>(std::max_element(arc, arc + N) - arc + 1);
}

// The accent is still worth giving this many frames after the ideal take-off.
constexpr float kCueLateFrames = 4.0f;
// Mario's and a small enemy's half widths together: closer than this (centre to centre) they touch.
// From the first 1-1 trace, Mario landing 13 units beside a goomba was hurt.
constexpr float kTouchUnits = 14.0f;
constexpr float kTrackFrames = 120.0f;     // beyond this an enemy is not "approaching" yet
constexpr float kMinClosingSpeed = 0.25f;  // world units per frame: slower is not "coming at you"
constexpr float kAboveMaxUnits = 80.0f;    // enemies higher than a full jump are out of reach
constexpr float kBelowMaxUnits = 96.0f;    // and lower than this are not Mario's business yet
constexpr float kLookAheadTiles = 10.0f;
// Falling: lined up when within this many units sideways, and the enemy within this many tiles below.
constexpr float kStompAlignUnits = 12.0f;
constexpr float kStompBelowTiles = 5.0f;

bool LogCombat() {
    static const bool on = [] {
        const char* v = AURORA_ENV("NSMBW_A11Y_LOG_COMBAT");
        return v != nullptr && *v == '1';
    }();
    return on;
}

// Debug trace (NSMBW_A11Y_LOG_COMBAT): Mario and the cued enemy for a while after a jump cue.
constexpr int kTraceFrames = 70;
int g_traceFrames = 0;
std::uint32_t g_traceActor = 0;

std::uint32_t g_stompCued = 0;  // the enemy of the last stomp/danger cue in this fall

// The countdown: three ticks a beat apart, then the accent on the take-off frame, for the one
// enemy whose take-off comes first. A rhythm lets the player jump on the beat instead of
// reacting to a single sound (a reaction costs about 0.2 s; a beat can be met on time).
constexpr float kBeatFrames = 15.0f;  // 0.25 s
constexpr int kBeats = 4;             // 3 ticks + the accent
std::uint32_t g_countActor = 0;
constexpr float kNoTakeOff = 1.0e9f;
float g_nextTakeOff = kNoTakeOff;  // frames until the first enemy jump, for other cues to yield
int g_beatsDone = 0;

enum class JumpKind { Tap, High, Over };

struct Candidate {
    const NearbyActor* actor = nullptr;
    float takeOff = 0.0f;  // frames from now
    float latest = 0.0f;   // the last take-off that still works
    JumpKind kind = JumpKind::Tap;
    float side = 1.0f;
    float top = 0.0f, meet = 0.0f;
};

// When to take off for this enemy, or false if it is not coming at Mario or cannot be jumped.
bool Plan(const game::PlayerState& p, const NearbyActor& n, Candidate& out) {
    if (!IsHostile(n.category) || n.category == ActorCategory::Boss) return false;
    // The enemy's top, relative to Mario's feet (its centre is half way up its body).
    const float top = n.info.pos.y + 2.0f * (n.info.center.y - n.info.pos.y) - p.pos.y;
    const float feet = n.info.pos.y - p.pos.y;
    if (feet > kAboveMaxUnits || feet < -kBelowMaxUnits) return false;
    const float dxUnits = n.info.center.x - p.pos.x;
    const float side = dxUnits > 0.0f ? 1.0f : -1.0f;
    const float closing = (p.speed.x - n.info.delta.x) * side;
    if (closing < kMinClosingSpeed) return false;
    const float meet = std::fabs(dxUnits) / closing;  // frames until Mario is over its centre
    if (meet > kTrackFrames) return false;

    // A stomp needs Mario's feet above the enemy's top by the time they touch (`touch`), and
    // coming down onto it while they still overlap; ideally over its centre (`meet`). A slow
    // enemy creeping up overlaps for long, a fast one briefly. A dangerous enemy is cleared at
    // the top of a full jump, over its centre.
    const float touch = std::max(std::fabs(dxUnits) - kTouchUnits, 0.0f) / closing;
    out = Candidate{};
    out.actor = &n;
    out.side = side;
    out.top = top;
    out.meet = meet;
    if (n.category == ActorCategory::Danger) {
        const float rise = FramesToRise(kFullArc, top);
        if (rise < 0.0f) return false;
        out.kind = JumpKind::Over;
        out.latest = touch - rise;
        out.takeOff = std::min(meet - FramesToPeak(kFullArc), out.latest);
        return true;
    }
    float land = FramesToLandAt(kTapArc, top);
    float rise = FramesToRise(kTapArc, top);
    if (land < 0.0f) {
        out.kind = JumpKind::High;
        land = FramesToLandAt(kFullArc, top);
        rise = FramesToRise(kFullArc, top);
    }
    if (land < 0.0f || rise < 0.0f) return false;  // out of reach: the radar still tells where it is
    out.latest = touch - rise;                     // later and it walks into Mario's side
    out.takeOff = std::clamp(meet - land, touch - land, out.latest);
    return true;
}

void PlayAccent(const Candidate& c) {
    const float pan = c.side * 0.7f;
    switch (c.kind) {
        case JumpKind::Over:
            audio::cues::Play(audio::cues::JumpOver(pan));
            autopilot::Note(autopilot::Cue::JumpOver);
            break;
        case JumpKind::High:
            audio::cues::Play(audio::cues::JumpHigh(pan));
            autopilot::Note(autopilot::Cue::JumpHigh);
            break;
        case JumpKind::Tap:
            audio::cues::Play(audio::cues::JumpNow(pan));
            autopilot::Note(autopilot::Cue::JumpNow);
            break;
    }
}

void TickGround(const game::PlayerState& p, const std::vector<NearbyActor>& near) {
    g_nextTakeOff = kNoTakeOff;
    Candidate best;
    bool have = false;
    for (const NearbyActor& n : near) {
        Candidate c;
        if (!Plan(p, n, c) || c.latest < -kCueLateFrames) continue;
        if (!have || c.takeOff < best.takeOff) {
            best = c;
            have = true;
        }
    }
    if (!have) {
        g_countActor = 0;
        g_beatsDone = 0;
        return;
    }
    g_nextTakeOff = best.takeOff;
    // Beats due by now: one per kBeatFrames before the take-off, the last on it.
    int due = 0;
    for (int i = 0; i < kBeats; ++i) {
        if (best.takeOff <= kBeatFrames * static_cast<float>(kBeats - 1 - i)) due = i + 1;
    }
    if (!p.onGround) due = std::min(due, kBeats - 1);
    if (best.actor->info.actor != g_countActor) {
        // A new enemy mid-countdown: start silently from where it is, so ticks never bunch up;
        // the accent always sounds.
        g_countActor = best.actor->info.actor;
        g_beatsDone = std::min(due, kBeats - 1);
        if (due < kBeats) return;
    }
    if (due <= g_beatsDone) return;
    g_beatsDone = due;
    if (due < kBeats) {
        audio::cues::Play(audio::cues::CountTick(best.side * 0.7f, due));
        return;
    }
    PlayAccent(best);
    g_traceActor = g_countActor;
    g_traceFrames = kTraceFrames;
    if (LogCombat()) {
        RT_LOGF(RT_TAG_A11Y, "combat: jump cue profile=%u kind=%d top=%.1f meet=%.1f takeoff=%.1f\n",
                best.actor->info.profile, static_cast<int>(best.kind), best.top, best.meet, best.takeOff);
    }
}

void TickAir(const game::PlayerState& p, const std::vector<NearbyActor>& near) {
    if (p.speed.y >= 0.0f) {  // still rising: nothing to line up yet
        g_stompCued = 0;
        return;
    }
    for (const NearbyActor& n : near) {
        if (!IsHostile(n.category)) continue;
        const float below = (p.pos.y - n.info.center.y) / kTile;
        const float sideways = n.info.center.x - p.pos.x;
        if (below <= 0.0f || below > kStompBelowTiles || std::fabs(sideways) > kStompAlignUnits) continue;
        if (g_stompCued == n.info.actor) return;
        g_stompCued = n.info.actor;
        if (LogCombat()) RT_LOGF(RT_TAG_A11Y, "combat: below cue profile=%u below=%.1f side=%.1f\n", n.info.profile, below, sideways);
        const float pan = std::fmax(-1.0f, std::fmin(1.0f, sideways / (2.0f * kTile)));
        audio::cues::Play(n.category == ActorCategory::Enemy ? audio::cues::StompTarget(pan)
                                                             : audio::cues::DangerBelow(pan));
        return;
    }
}

}  // namespace

void Tick(const game::PlayerState& p) {
    if (!config::Get().objectCues) return;
    const std::vector<NearbyActor> near = Scan(p, kLookAheadTiles, kLookAheadTiles);
    if (g_traceFrames > 0 && LogCombat()) {
        --g_traceFrames;
        float ex = 0.0f, ey = 0.0f;
        unsigned prof = 0;
        for (const NearbyActor& n : near) {
            if (n.info.actor == g_traceActor || (g_traceActor == 0 && prof == 0 && n.info.profile != 0)) {
                ex = n.info.pos.x;
                ey = n.info.pos.y;
                prof = n.info.profile;
            }
        }
        RT_LOGF(RT_TAG_A11Y, "combat trace: mario %.1f,%.1f v %.2f,%.2f ground %d | actor %u %.1f,%.1f\n", p.pos.x,
                p.pos.y, p.speed.x, p.speed.y, p.onGround ? 1 : 0, prof, ex, ey);
    }
    // The countdown keeps running in the air (after a stomp bounce the next enemy is often
    // already close); its accent waits for the landing, when a jump is possible again.
    TickGround(p, near);
    if (p.onGround) {
        g_stompCued = 0;
    } else {
        TickAir(p, near);
    }
}

bool EnemyJumpWithin(float frames) { return g_nextTakeOff <= frames; }

void TraceNearest() {
    g_traceActor = 0;
    g_traceFrames = kTraceFrames;
}

void Reset() {
    g_countActor = 0;
    g_beatsDone = 0;
    g_stompCued = 0;
}

}  // namespace a11y::level::combat

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

// The cue comes a player's reaction time (about 0.2 s) before the ideal take-off, and is still
// worth giving a few frames after it.
constexpr float kReactionFrames = 12.0f;
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

std::set<std::uint32_t> g_jumpCued;   // enemies already given their jump cue on this approach
std::uint32_t g_stompCued = 0;        // the enemy of the last stomp/danger cue in this fall

void TickGround(const game::PlayerState& p, const std::vector<NearbyActor>& near) {
    std::set<std::uint32_t> stillApproaching;
    for (const NearbyActor& n : near) {
        if (!IsHostile(n.category) || n.category == ActorCategory::Boss) continue;
        // The enemy's top, relative to Mario's feet (its centre is half way up its body).
        const float top = n.info.pos.y + 2.0f * (n.info.center.y - n.info.pos.y) - p.pos.y;
        const float feet = n.info.pos.y - p.pos.y;
        if (feet > kAboveMaxUnits || feet < -kBelowMaxUnits) continue;
        const float dxUnits = n.info.center.x - p.pos.x;
        const float side = dxUnits > 0.0f ? 1.0f : -1.0f;
        const float closing = (p.speed.x - n.info.delta.x) * side;
        if (closing < kMinClosingSpeed) continue;
        const float meet = std::fabs(dxUnits) / closing;  // frames until Mario is over its centre
        if (meet > kTrackFrames) continue;
        stillApproaching.insert(n.info.actor);
        if (g_jumpCued.count(n.info.actor) != 0) continue;

        // When to take off, in frames from now. A stomp needs Mario's feet above the enemy's top by
        // the time they touch (`touch`), and coming down onto it while they still overlap; ideally
        // over its centre (`meet`). A slow enemy creeping up overlaps for long, a fast one briefly.
        // A dangerous enemy is cleared at the top of a full jump, over its centre.
        const bool danger = n.category == ActorCategory::Danger;
        const float touch = std::max(std::fabs(dxUnits) - kTouchUnits, 0.0f) / closing;
        float takeOff = 0.0f, latest = 0.0f;
        bool high = false;
        if (danger) {
            high = true;
            takeOff = meet - FramesToPeak(kFullArc);
            const float rise = FramesToRise(kFullArc, top);
            if (rise < 0.0f) continue;
            latest = touch - rise;
        } else {
            float land = FramesToLandAt(kTapArc, top);
            float rise = FramesToRise(kTapArc, top);
            if (land < 0.0f) {
                high = true;
                land = FramesToLandAt(kFullArc, top);
                rise = FramesToRise(kFullArc, top);
            }
            if (land < 0.0f || rise < 0.0f) continue;  // out of reach: the radar still tells where it is
            latest = touch - rise;                     // later and it walks into Mario's side
            takeOff = std::clamp(meet - land, touch - land, latest);
        }
        // Cue a reaction time ahead of the take-off, and not once it can no longer work.
        if (takeOff > kReactionFrames || latest < kReactionFrames - kCueLateFrames) continue;


        const float pan = side * 0.7f;
        if (danger) {
            audio::cues::Play(audio::cues::JumpOver(pan));
            autopilot::Note(autopilot::Cue::JumpOver);
        } else if (high) {
            audio::cues::Play(audio::cues::JumpHigh(pan));
            autopilot::Note(autopilot::Cue::JumpHigh);
        } else {
            audio::cues::Play(audio::cues::JumpNow(pan));
            autopilot::Note(autopilot::Cue::JumpNow);
        }
        g_jumpCued.insert(n.info.actor);
        g_traceActor = n.info.actor;
        g_traceFrames = kTraceFrames;
        if (LogCombat()) {
            RT_LOGF(RT_TAG_A11Y, "combat: jump cue profile=%u high=%d top=%.1f meet=%.1f takeoff=%.1f\n", n.info.profile,
                    high ? 1 : 0, top, meet, takeOff);
        }
    }
    // An enemy leaves the cued set once it is no longer closing in: the next approach cues again.
    for (auto it = g_jumpCued.begin(); it != g_jumpCued.end();) {
        it = stillApproaching.count(*it) ? std::next(it) : g_jumpCued.erase(it);
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
    if (p.onGround) {
        g_stompCued = 0;
        TickGround(p, near);
    } else {
        g_jumpCued.clear();
        TickAir(p, near);
    }
}

void TraceNearest() {
    g_traceActor = 0;
    g_traceFrames = kTraceFrames;
}

void Reset() {
    g_jumpCued.clear();
    g_stompCued = 0;
}

}  // namespace a11y::level::combat

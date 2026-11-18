#include "accessibility/test_autopilot.h"

#include <atomic>

#include <aurora/env.hpp>

#include "accessibility/a11y_log.h"
#include "accessibility/game/nsmbw_game.h"
#include "accessibility/level/combat.h"

namespace a11y::autopilot {
namespace {

constexpr std::uint32_t kJumpButton = 1u << 8;  // "2" (jump)
constexpr std::uint32_t kRunButton = 1u << 9;   // "1" (run)
// Holding jump longer gives a higher jump; these are the hold times a player would use for a
// stomp (short hop onto the enemy), clearing a dangerous enemy or a step, and a pit.
constexpr int kStompHoldFrames = 8;
constexpr int kHighHoldFrames = 20;
constexpr int kFullHoldFrames = 40;
constexpr int kPitHoldFrames = 24;
constexpr int kPitRunFrames = 40;        // run up to the pit edge and through the jump
constexpr int kReleaseFrames = 2;        // a new jump needs the button released first
// A player reacts to a sound about 0.2 s after it; the autopilot waits as long, so a run it wins
// is one a person could win too.
constexpr int kReactionFrames = 12;
// An enemy's jump cue ends a countdown, so a player presses on the beat: allow a small error.
constexpr int kOnBeatFrames = 2;
int g_pendingIn = -1;                    // frames until the pending reaction; -1 none
int g_pendingHold = 0, g_pendingRun = 0;
const char* g_pendingWhy = "";
bool g_pendingEnemy = false;
int g_stopLeft = 0;                      // frames without pressing right

std::atomic<std::uint32_t> g_bits{0};
int g_jumpLeft = 0;
int g_runLeft = 0;
int g_releaseLeft = 0;

void StartJump(int frames, int runFrames, const char* why, bool enemy) {
    // An enemy's cue outranks a pending terrain jump: jumping at a step into an enemy is worse.
    if (g_pendingIn >= 0 && !(enemy && !g_pendingEnemy)) return;
    g_pendingEnemy = enemy;
    g_pendingIn = enemy ? kOnBeatFrames : kReactionFrames;
    g_pendingHold = frames;
    g_pendingRun = runFrames;
    g_pendingWhy = why;
    RT_LOGF(RT_TAG_A11Y, "autopilot: heard %s\n", why);
}

void Press(int frames, int runFrames, const char* why) {
    if (g_jumpLeft > 0 || g_releaseLeft > 0) {
        // Still holding an earlier jump: a person lets go and presses again for an enemy.
        if (g_pendingEnemy) {
            g_jumpLeft = 0;
            g_releaseLeft = kReleaseFrames;
            g_pendingIn = kReleaseFrames;
        }
        return;
    }
    g_jumpLeft = frames;
    level::combat::TraceNearest();
    if (runFrames > g_runLeft) g_runLeft = runFrames;
    RT_LOGF(RT_TAG_A11Y, "autopilot: jump (%s)\n", why);
}

}  // namespace

bool Enabled() {
    static const bool on = [] {
        const char* v = AURORA_ENV("NSMBW_A11Y_AUTOPILOT");
        return v != nullptr && *v == '1';
    }();
    return on;
}

void Note(Cue cue) {
    if (!Enabled()) return;
    switch (cue) {
        case Cue::JumpNow: StartJump(kStompHoldFrames, 0, "enemy", true); break;
        case Cue::JumpHigh: StartJump(kFullHoldFrames, 0, "enemy high", true); break;
        case Cue::JumpOver: StartJump(kFullHoldFrames, 0, "danger", true); break;
        case Cue::PitAhead: StartJump(kPitHoldFrames, kPitRunFrames, "pit", false); break;
        case Cue::StepAhead:
        case Cue::WallKnock: StartJump(kHighHoldFrames, 0, "wall", false); break;
        case Cue::UnderBlock:
            StartJump(kHighHoldFrames, 0, "block", false);
            break;
    }
}

void Tick(bool inLevel) {
    if (!Enabled()) return;
    if (!inLevel) {
        g_bits = 0;
        g_jumpLeft = g_runLeft = g_releaseLeft = 0;
        g_pendingIn = -1;
        g_stopLeft = 0;
        return;
    }
    if (g_pendingIn >= 0 && g_pendingIn-- == 0) Press(g_pendingHold, g_pendingRun, g_pendingWhy);
    std::uint32_t bits = g_stopLeft > 0 ? 0u : game::kScreenRight;
    if (g_stopLeft > 0) --g_stopLeft;
    if (g_jumpLeft > 0) {
        bits |= kJumpButton;
        if (--g_jumpLeft == 0) g_releaseLeft = kReleaseFrames;
    } else if (g_releaseLeft > 0) {
        --g_releaseLeft;
    }
    if (g_runLeft > 0) {
        bits |= kRunButton;
        --g_runLeft;
    }
    g_bits = bits;
}

}  // namespace a11y::autopilot

extern "C" std::uint32_t NsmbwA11yAutopilotBits() { return a11y::autopilot::g_bits.load(); }

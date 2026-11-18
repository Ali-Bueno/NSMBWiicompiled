#include "accessibility/level/blocks.h"

#include <cmath>

#include "accessibility/a11y_config.h"
#include "accessibility/audio/cues.h"
#include "accessibility/level/combat.h"
#include "accessibility/game/game_sound.h"
#include "accessibility/test_autopilot.h"

namespace a11y::level::blocks {
namespace {

using game::kTile;
namespace cues = audio::cues;

// dBc_c::getUnitType bits, read in 1-1 with NSMBW_A11Y_LOG_TERRAIN (2026-11-18): 0x5 at the
// ? blocks over the first goomba, 0x10 at the bricks beside them, 0x8000 along the coin arcs.
// The meaning of each bit is inferred from those positions, not from the game's source.
constexpr std::uint32_t kTypeQuestion = 0x4;

constexpr int kScanColumns = 6;          // each side of Mario
constexpr int kScanRows = 5;             // tiles above his feet
constexpr int kScanEveryFrames = 20;     // a full scan is ~70 tile queries
constexpr float kUnderUnits = kTile * 0.5f;  // Mario's x within the block's column
constexpr float kReactionFrames = 12.0f;     // a player's reaction to a sound, about 0.2 s
constexpr float kRiseFramesPerRow = 4.7f;
constexpr float kStandingSpeed = 0.3f;
constexpr float kEnemyYieldFrames = 75.0f;

struct Found {
    bool any = false;
    float x = 0.0f, y = 0.0f;  // block centre, world units
    float dx = 0.0f;  // tiles, block centre minus Mario
    int up = 0;       // tile rows above his feet
};

int g_frames = kScanEveryFrames;
Found g_nearest;
float g_beaconTimer = 0.0f;
float g_underTimer = 0.0f;
bool g_wasUnder = false;

Found Scan(const game::PlayerState& p) {
    Found best;
    float bestDist = 0.0f;
    const float col0 = std::floor(p.pos.x / kTile);
    for (int c = -kScanColumns; c <= kScanColumns; ++c) {
        const float x = (col0 + static_cast<float>(c)) * kTile + kTile * 0.5f;
        for (int r = 1; r <= kScanRows; ++r) {
            const float y = p.pos.y + kTile * (static_cast<float>(r) + 0.5f);
            if ((game::UnitType(x, y, p.layer) & kTypeQuestion) == 0) continue;
            const float dx = (x - p.pos.x) / kTile;
            const float dist = std::fabs(dx) + static_cast<float>(r);
            if (!best.any || dist < bestDist) {
                best = {true, x, y, dx, r};
                bestDist = dist;
            }
            break;  // the lowest in a column is the one Mario can hit
        }
    }
    return best;
}

}  // namespace

void Tick(const game::PlayerState& p, float frameSec) {
    if (!config::Get().objectCues) return;
    if (++g_frames >= kScanEveryFrames) {
        g_frames = 0;
        g_nearest = Scan(p);
    }
    g_beaconTimer -= frameSec;
    g_underTimer -= frameSec;
    if (!g_nearest.any) {
        g_wasUnder = false;
        return;
    }
    g_nearest.dx = (g_nearest.x - p.pos.x) / kTile;  // the scan is periodic; Mario is not
    // "Jump now to hit it": standing under it, or walking at it and as far from it as he will
    // travel during a reaction plus the rise of a full jump to the block. Rise frames per tile
    // row from the full-jump arc (combat.cpp): Mario's head (a tile tall) reaches the block's
    // bottom about 4.7 frames per row above the first.
    const float toward = g_nearest.dx * kTile * (p.speed.x >= 0.0f ? 1.0f : -1.0f);
    const float lead = std::fabs(p.speed.x) *
                       (kReactionFrames + kRiseFramesPerRow * static_cast<float>(g_nearest.up - 1));
    const bool standing = std::fabs(p.speed.x) < kStandingSpeed && std::fabs(g_nearest.dx * kTile) < kUnderUnits;
    const bool walkingIn = !standing && std::fabs(toward - lead) < kUnderUnits;
    // An enemy's jump comes first: a block jump then (in the air about 50 frames) would lose
    // its beat.
    const bool under = p.onGround && (standing || walkingIn) && !combat::EnemyJumpWithin(kEnemyYieldFrames);
    if (under && (!g_wasUnder || g_underTimer <= 0.0f)) {
        if (!(config::Get().gameSounds && game::sound::PlayAt(cues::kGameBlockLabel, g_nearest.x, g_nearest.y))) {
            cues::Play(cues::UnderBlock(static_cast<float>(g_nearest.up)));
        }
        g_underTimer = cues::kUnderBlockRepeatSec;
        g_beaconTimer = cues::kBlockBeaconSec;
        if (!g_wasUnder) autopilot::Note(autopilot::Cue::UnderBlock);
    }
    g_wasUnder = under;
    if (!under && g_beaconTimer <= 0.0f) {
        // Far: the soft chirp from its side (the game's sound is kept for "right under it").
        cues::Play(cues::BlockBeacon(cues::PanFor(g_nearest.dx, static_cast<float>(kScanColumns)),
                                     static_cast<float>(g_nearest.up)));
        g_beaconTimer = cues::kBlockBeaconSec;
    }
}

void Reset() {
    g_frames = kScanEveryFrames;
    g_nearest = Found{};
    g_beaconTimer = g_underTimer = 0.0f;
    g_wasUnder = false;
}

}  // namespace a11y::level::blocks

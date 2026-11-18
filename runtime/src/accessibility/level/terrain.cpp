#include "accessibility/level/terrain.h"

#include <cmath>
#include <string>

#include <aurora/env.hpp>

#include "accessibility/a11y_config.h"
#include "accessibility/a11y_log.h"
#include "accessibility/audio/cues.h"
#include "accessibility/game/game_sound.h"
#include "accessibility/localization.h"
#include "accessibility/speech.h"
#include "accessibility/test_autopilot.h"

namespace a11y::level::terrain {
namespace {

using game::kTile;
namespace cues = audio::cues;

enum class Edge { None, StepUp, Drop, Pit, Hazard };

// Geometry, in tiles unless noted.
constexpr float kProbeAboveFeet = 2.5f;    // ground probes ahead start this high: finds steps up to 2 tiles
constexpr float kBodyRowLow = 6.0f;        // world units above the feet: the walls Mario walks into
constexpr float kBodyRowHigh = 14.0f;
constexpr float kBodyHalfWidth = 0.5f;
constexpr float kStepUpTiles = 0.9f;
constexpr float kDropTiles = 1.5f;
constexpr float kPitTiles = 8.0f;          // deeper than this reads as a pit, not a drop
constexpr int kWallHeightScan = 13;        // rows climbed to find the top of a wall
constexpr float kWallHeightUnknown = 99.0f;
constexpr float kWallInsideUnits = 4.0f;   // how far into the wall column its height is measured
constexpr float kMovingSpeed = 0.3f;       // world units per frame: slower counts as standing
constexpr float kPushSpeed = 0.1f;         // at most this fast while pushing into a wall
constexpr int kUrgentColumns = 2;          // an announced edge is announced again this close
constexpr int kWaterLava = 3;              // WATER_TYPE_e (d_bc.hpp)
constexpr int kWaterPoison = 4;
constexpr float kHazardProbeDepth = 3.0f;
constexpr float kCeilingProbeTiles = 0.25f;
constexpr float kCeilingMargin = 1.0f;     // world units below a ceiling a ground probe starts
// Air: Kirby's mod waits 100 ms before calling a short hop "airborne" and re-snaps the hum's step
// only past 0.6 tiles of change, so the pitch can be counted rather than sliding.
constexpr float kAirConfirmSec = 0.1f;
constexpr float kStepHysteresis = 0.6f;
constexpr float kPlatformUnderRise = 0.6f;
// Platforms above: looked for up to this many tiles over Mario's head, from his own column.
constexpr float kPlatformAboveMaxTiles = 6.0f;
constexpr float kPlatformAboveMinTiles = 1.5f;
// A spoken wall height is repeated for the same wall only after this long.
constexpr float kWallSpeechRepeatSec = 6.0f;

struct State {
    float airSec = 0.0f;
    bool wasAir = false;
    bool lastPit = false;
    float lastGroundY = 0.0f;
    bool haveLastGround = false;
    int groundStep = -1;
    float pitTimer = 0.0f;
    Edge lastEdge = Edge::None;
    int lastEdgeColumn = 0;
    float knockTimer = 0.0f;
    int spokenWallColumn = 0x7FFFFFFF;
    float spokenWallTimer = 0.0f;
    float aboveTimer = 0.0f;
    int lastAboveUp = 0;
    float wallHeightCache[2] = {-1.0f, -1.0f};
    float wallXCache[2] = {0.0f, 0.0f};
};
State g;

bool LogTerrain() {
    static const bool on = [] {
        const char* v = AURORA_ENV("NSMBW_A11Y_LOG_TERRAIN");
        return v != nullptr && *v == '1';
    }();
    return on;
}

bool IsHazardWater(int type) { return type == kWaterLava || type == kWaterPoison; }

// Distance in tiles from Mario's side to the first wall within `range` tiles (-1 if none), swept
// through his two lowest body rows; `wallX` is where the wall face is.
float WallDistance(const game::PlayerState& p, int side, float range, float& wallX) {
    float best = -1.0f;
    for (float row : {kBodyRowLow, kBodyRowHigh}) {
        float x = 0.0f;
        if (!game::WallBetween(p.pos.x, p.pos.x + side * range * kTile, p.pos.y + row, p.layer, x)) continue;
        const float d = std::max(std::fabs(x - p.pos.x) / kTile - kBodyHalfWidth, 0.0f);
        if (best < 0.0f || d < best) {
            best = d;
            wallX = x;
        }
    }
    return best;
}

// Tiles from Mario's feet to the first gap of two free rows in the wall's column (Kirby's
// wall_height), or kWallHeightUnknown when it goes higher than the scan.
float WallHeight(const game::PlayerState& p, int side, float wallX) {
    const float x = wallX + side * kWallInsideUnits;
    bool below = !game::SolidAt(x, p.pos.y + kTile * 0.5f, p.layer);
    for (int r = 1; r <= kWallHeightScan; ++r) {
        const bool free = !game::SolidAt(x, p.pos.y + kTile * (r + 0.5f), p.layer);
        if (below && free) return static_cast<float>(r - 1);
        below = free;
    }
    return kWallHeightUnknown;
}

void SpeakWall(float height) {
    std::string text;
    if (height >= kWallHeightUnknown) {
        text = loc::Get("wall_tall");
    } else {
        const std::string n = std::to_string(static_cast<int>(std::ceil(height - 0.05f)));
        text = loc::Format(cues::WallKindFor(height) == cues::WallKind::Step ? "wall_step" : "wall_high", {{"n", n}});
    }
    speech::Say(text, false);
}

void TickWalls(const game::PlayerState& p, int ahead, float frameSec, float& aheadWall) {
    auto& service = audio::CueService::Instance();
    aheadWall = -1.0f;
    g.knockTimer -= frameSec;
    g.spokenWallTimer -= frameSec;
    for (int i = 0; i < 2; ++i) {
        const int side = i == 0 ? -1 : 1;
        const audio::CueChannel ch = side < 0 ? audio::CueChannel::WallLeft : audio::CueChannel::WallRight;
        float wallX = 0.0f;
        const float d = WallDistance(p, side, cues::kWallRangeTiles, wallX);
        if (side == ahead) aheadWall = d;
        if (d < 0.0f) {
            service.Stop(ch);
            g.wallHeightCache[i] = -1.0f;
            continue;
        }
        if (g.wallHeightCache[i] < 0.0f || std::fabs(wallX - g.wallXCache[i]) > kTile * 0.5f) {
            g.wallHeightCache[i] = WallHeight(p, side, wallX);
            g.wallXCache[i] = wallX;
        }
        const float height = g.wallHeightCache[i];
        const float closeness = (1.0f - d / cues::kWallRangeTiles) * (side == ahead ? 1.0f : cues::kWallBehindGain);
        service.SetSustained(ch, cues::WallHum(cues::WallKindFor(height), closeness, side * 0.8f));

        // Pushing into it: a knock, and once per wall its height in words.
        const bool touching = side < 0 ? p.wallLeft : p.wallRight;
        const std::uint32_t towards = side < 0 ? game::kScreenLeft : game::kScreenRight;
        if (touching && (game::InputHold() & towards) != 0 && std::fabs(p.speed.x) < kPushSpeed && p.onGround) {
            if (g.knockTimer <= 0.0f) {
                cues::Play(cues::WallKnock(side * 0.8f));
                autopilot::Note(autopilot::Cue::WallKnock);
                g.knockTimer = cues::kWallKnockCooldownSec;
            }
            const int column = static_cast<int>(std::floor(wallX / kTile));
            if (column != g.spokenWallColumn || g.spokenWallTimer <= 0.0f) {
                g.spokenWallColumn = column;
                g.spokenWallTimer = kWallSpeechRepeatSec;
                SpeakWall(height);
            }
        }
    }
}

void TickCeiling(const game::PlayerState& p) {
    auto& service = audio::CueService::Instance();
    float ceiling = 0.0f;
    if (!game::CeilingAbove(p.pos.x, p.pos.y + kTile * 0.5f, p.layer, ceiling)) {
        service.Stop(audio::CueChannel::Ceiling);
        return;
    }
    const float dist = (ceiling - p.pos.y) / kTile;
    if (dist > cues::kCeilingRangeTiles + 1.0f || dist <= 0.0f) {
        service.Stop(audio::CueChannel::Ceiling);
        return;
    }
    service.SetSustained(audio::CueChannel::Ceiling, cues::CeilingHum(1.0f - (dist - 1.0f) / cues::kCeilingRangeTiles));
}

Edge ClassifyColumn(const game::PlayerState& p, float x) {
    // The probe starts above the feet to find steps, but never inside a low ceiling over that
    // column (a block there would read as a step up).
    float start = p.pos.y + kProbeAboveFeet * kTile;
    float ceiling = 0.0f;
    if (game::CeilingAbove(x, p.pos.y + kCeilingProbeTiles * kTile, p.layer, ceiling) && ceiling < start) {
        start = ceiling - kCeilingMargin;
    }
    float h = 0.0f;
    if (!game::GroundBelow(x, start, p.layer, h)) {
        return IsHazardWater(game::WaterAt(x, p.pos.y - kHazardProbeDepth * kTile, p.layer)) ? Edge::Hazard : Edge::Pit;
    }
    const float delta = (h - p.pos.y) / kTile;
    if (IsHazardWater(game::WaterAt(x, h + kTile * 0.25f, p.layer))) return Edge::Hazard;
    if (delta < -kPitTiles) return Edge::Pit;
    if (delta <= -kDropTiles) return Edge::Drop;
    if (delta >= kStepUpTiles) return Edge::StepUp;
    return Edge::None;
}

// One or two words with each edge sound, so it does not have to be learnt.
void SayEdge(Edge e) {
    const char* key = e == Edge::Pit ? "edge_pit" : e == Edge::Drop ? "edge_drop" : e == Edge::StepUp ? "edge_step"
                                                                                                 : "edge_lava";
    speech::Say(loc::Get(key), false);
}

void PlayEdge(Edge e, int ahead, float x, float y, bool changedEdge) {
    const float pan = ahead * 0.5f;
    if (e != Edge::None && changedEdge) SayEdge(e);
    if (e == Edge::Hazard && config::Get().gameSounds && game::sound::PlayAt(cues::kGameLavaLabel, x, y)) return;
    switch (e) {
        case Edge::Pit: cues::Play(cues::EdgePit(pan)); break;
        case Edge::Drop: cues::Play(cues::EdgeDrop(pan)); break;
        case Edge::StepUp: cues::Play(cues::EdgeStep(pan)); break;
        case Edge::Hazard: cues::Play(cues::EdgeHazard(pan)); break;
        case Edge::None: break;
    }
}

void TickEdges(const game::PlayerState& p, int ahead, float aheadWall) {
    const int scan = config::Get().scanTiles;
    Edge found = Edge::None;
    int column = 0;
    for (int i = 1; i <= scan; ++i) {
        if (aheadWall >= 0.0f && i > aheadWall + kBodyHalfWidth) break;  // the wall hum covers it
        const Edge e = ClassifyColumn(p, p.pos.x + ahead * i * kTile);
        if (e != Edge::None) {
            found = e;
            column = i;
            break;
        }
    }
    const bool changed = found != g.lastEdge;
    const bool closer = found != Edge::None && column <= kUrgentColumns && g.lastEdgeColumn > kUrgentColumns;
    if (found != Edge::None && (changed || closer)) {
        PlayEdge(found, ahead, p.pos.x + ahead * column * kTile, p.pos.y, changed);
        if (found == Edge::Pit && column <= kUrgentColumns) autopilot::Note(autopilot::Cue::PitAhead);
        if (found == Edge::StepUp && column <= 1) autopilot::Note(autopilot::Cue::StepAhead);
        if (LogTerrain()) RT_LOGF(RT_TAG_A11Y, "terrain: edge %d at column %d\n", int(found), column);
    }
    g.lastEdge = found;
    g.lastEdgeColumn = found == Edge::None ? 0 : column;
}

// A surface Mario could jump up onto, over his own column.
void TickPlatformAbove(const game::PlayerState& p, float frameSec) {
    g.aboveTimer -= frameSec;
    float h = 0.0f;
    int up = 0;
    if (game::GroundBelow(p.pos.x, p.pos.y + kPlatformAboveMaxTiles * kTile, p.layer, h)) {
        const float rise = (h - p.pos.y) / kTile;
        if (rise >= kPlatformAboveMinTiles) up = static_cast<int>(std::lround(rise));
    }
    if (up != 0 && up != g.lastAboveUp && g.aboveTimer <= 0.0f) {
        cues::Play(cues::PlatformAbove(up));
        speech::Say(loc::Get("platform_above"), false);
        g.aboveTimer = cues::kPlatformAboveCooldownSec;
    }
    g.lastAboveUp = up;
}

void TickAir(const game::PlayerState& p, float frameSec) {
    auto& service = audio::CueService::Instance();
    g.airSec += frameSec;
    if (g.airSec < kAirConfirmSec) return;  // a tiny hop is not worth a sound
    float h = 0.0f;
    const bool ground = game::GroundBelow(p.pos.x, p.pos.y + kTile * 0.25f, p.layer, h);
    const float drop = (p.pos.y - h) / kTile;
    const bool pit = !ground || drop > kPitTiles * 2.0f;
    if (pit) {
        g.groundStep = -1;
        service.SetSustained(audio::CueChannel::Ground, cues::PitWind());
        g.lastPit = true;
        return;
    }
    if (g.lastPit) {
        cues::Play(cues::GroundAfterPit());
    } else if (g.haveLastGround && (h - g.lastGroundY) / kTile > kPlatformUnderRise) {
        cues::Play(cues::PlatformUnder());
    }
    g.lastPit = false;
    g.lastGroundY = h;
    g.haveLastGround = true;
    if (g.groundStep < 0 || std::fabs(drop - static_cast<float>(g.groundStep)) > kStepHysteresis) {
        g.groundStep = static_cast<int>(std::lround(std::max(drop, 0.0f)));
    }
    const int kind = IsHazardWater(game::WaterAt(p.pos.x, h + kTile * 0.25f, p.layer)) ? 2 : 0;
    service.SetSustained(audio::CueChannel::Ground, cues::GroundBelow(g.groundStep, kind));
}

void TickGround(const game::PlayerState& p, int ahead, float aheadWall, float frameSec) {
    auto& service = audio::CueService::Instance();
    service.Stop(audio::CueChannel::Ground);
    g.airSec = 0.0f;
    g.groundStep = -1;
    g.pitTimer = 0.0f;
    g.lastPit = false;
    g.haveLastGround = false;
    TickEdges(p, ahead, aheadWall);
    TickPlatformAbove(p, frameSec);
}

// Debug (NSMBW_A11Y_LOG_TERRAIN): the tile codes in the column above Mario, on change.
void LogTilesAbove(const game::PlayerState& p) {
    static std::uint32_t last[5] = {};
    for (int dy = 1; dy <= 5; ++dy) {
        const float y = p.pos.y + kTile * (dy + 0.5f);
        const std::uint32_t type = game::UnitType(p.pos.x, y, p.layer);
        const std::uint32_t kind = game::UnitKind(p.pos.x, y, p.layer);
        const std::uint32_t key = type ^ (kind * 2654435761u);
        if (key == last[dy - 1]) continue;
        last[dy - 1] = key;
        if (type != 0 || kind != 0) {
            RT_LOGF(RT_TAG_A11Y, "tile above dy=%d x=%.0f type=0x%08X kind=0x%08X\n", dy, p.pos.x, type, kind);
        }
    }
}

}  // namespace

void Tick(const game::PlayerState& p, float frameSec) {
    if (LogTerrain()) LogTilesAbove(p);
    if (!config::Get().terrainCues) {
        Stop();
        return;
    }
    const int facing = p.direction == 0 ? 1 : -1;
    const int ahead = std::fabs(p.speed.x) > kMovingSpeed ? (p.speed.x > 0 ? 1 : -1) : facing;
    float aheadWall = -1.0f;
    TickWalls(p, ahead, frameSec, aheadWall);
    TickCeiling(p);
    if (p.onGround) {
        TickGround(p, ahead, aheadWall, frameSec);
    } else {
        g.lastEdge = Edge::None;
        g.lastEdgeColumn = 0;
        TickAir(p, frameSec);
    }
    g.wasAir = !p.onGround;
}

void Stop() {
    auto& service = audio::CueService::Instance();
    service.Stop(audio::CueChannel::WallLeft);
    service.Stop(audio::CueChannel::WallRight);
    service.Stop(audio::CueChannel::Ground);
    service.Stop(audio::CueChannel::Ceiling);
    g = State{};
}

}  // namespace a11y::level::terrain

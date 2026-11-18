#include "accessibility/level/terrain.h"

#include <cmath>

#include <aurora/env.hpp>

#include "accessibility/a11y_config.h"
#include "accessibility/a11y_log.h"
#include "accessibility/audio/cues.h"

namespace a11y::level::terrain {
namespace {

using game::kTile;

enum class Edge { None, StepUp, Drop, Pit, Hazard };

// Geometry thresholds, in tiles.
constexpr float kProbeAboveFeet = 2.5f;   // ground probes start this high: finds steps up to 2 tiles
constexpr float kWallProbeHeight = 0.5f;  // walls are swept through the lowest body tile
constexpr float kBodyHalfWidth = 0.5f;
constexpr float kStepUpTiles = 0.9f;
constexpr float kDropTiles = 1.5f;
constexpr float kPitTiles = 8.0f;         // deeper than this reads as a pit, not a drop
constexpr float kWallBehindTiles = 2.0f;
constexpr float kAirToneMinTiles = 0.75f; // below this the landing is immediate: no tone
constexpr float kMovingSpeed = 0.3f;      // world units per frame: slower counts as standing
// Distances (in columns) at which an edge already announced is announced again.
constexpr int kUrgentColumns = 2;
// WATER_TYPE_e values that hurt (d_bc.hpp): lava and poison.
constexpr int kWaterLava = 3;
constexpr int kWaterPoison = 4;
constexpr float kHazardProbeDepth = 3.0f;

struct State {
    float wallTimer[2] = {};  // 0 left, 1 right
    Edge lastEdge = Edge::None;
    int lastEdgeColumn = 0;
    float noGroundTimer = 0.0f;
    bool airTone = false;
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

// Distance in tiles from the player's side to the first wall within `range` tiles, or -1.
float WallDistance(const game::PlayerState& p, int side, float range) {
    const float y = p.pos.y + kWallProbeHeight * kTile;
    float wallX = 0.0f;
    if (!game::WallBetween(p.pos.x, p.pos.x + side * range * kTile, y, p.layer, wallX)) return -1.0f;
    return std::max(std::fabs(wallX - p.pos.x) / kTile - kBodyHalfWidth, 0.0f);
}

void TickWalls(const game::PlayerState& p, int ahead, float frameSec, float& aheadWall) {
    auto& cues = audio::CueService::Instance();
    aheadWall = -1.0f;
    for (int i = 0; i < 2; ++i) {
        const int side = i == 0 ? -1 : 1;
        const float range = side == ahead ? static_cast<float>(config::Get().scanTiles) : kWallBehindTiles;
        const float d = WallDistance(p, side, range);
        if (side == ahead) aheadWall = d;
        g.wallTimer[i] -= frameSec;
        if (d < 0.0f || g.wallTimer[i] > 0.0f) continue;
        const audio::CueChannel ch = side < 0 ? audio::CueChannel::WallLeft : audio::CueChannel::WallRight;
        cues.PlayOneShot(ch, audio::cues::WallTick(side * 0.85f));
        g.wallTimer[i] = audio::cues::WallTickInterval(d);
    }
}

Edge ClassifyColumn(const game::PlayerState& p, float x) {
    float h = 0.0f;
    const bool ground = game::GroundBelow(x, p.pos.y + kProbeAboveFeet * kTile, p.layer, h);
    if (!ground) {
        return IsHazardWater(game::WaterAt(x, p.pos.y - kHazardProbeDepth * kTile, p.layer)) ? Edge::Hazard : Edge::Pit;
    }
    const float delta = (h - p.pos.y) / kTile;
    if (IsHazardWater(game::WaterAt(x, h + kTile * 0.25f, p.layer))) return Edge::Hazard;
    if (delta < -kPitTiles) return Edge::Pit;
    if (delta <= -kDropTiles) return Edge::Drop;
    if (delta >= kStepUpTiles) return Edge::StepUp;
    return Edge::None;
}

void PlayEdge(Edge e, int ahead) {
    auto& cues = audio::CueService::Instance();
    const float pan = ahead * 0.6f;
    switch (e) {
        case Edge::Pit: cues.PlayOneShot(audio::CueChannel::Edge, audio::cues::Pit(pan)); break;
        case Edge::Drop: cues.PlayOneShot(audio::CueChannel::Edge, audio::cues::Drop(pan)); break;
        case Edge::StepUp: cues.PlayOneShot(audio::CueChannel::Edge, audio::cues::StepUp(pan)); break;
        case Edge::Hazard: cues.PlayOneShot(audio::CueChannel::Hazard, audio::cues::Hazard(pan)); break;
        case Edge::None: break;
    }
}

void TickEdges(const game::PlayerState& p, int ahead, float aheadWall) {
    const int scan = config::Get().scanTiles;
    Edge found = Edge::None;
    int column = 0;
    for (int i = 1; i <= scan; ++i) {
        if (aheadWall >= 0.0f && i > aheadWall + kBodyHalfWidth) break;  // the wall comes first
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
        PlayEdge(found, ahead);
        if (LogTerrain()) RT_LOGF(RT_TAG_A11Y, "terrain: edge %d at column %d\n", int(found), column);
    }
    g.lastEdge = found;
    g.lastEdgeColumn = found == Edge::None ? 0 : column;
}

void TickAir(const game::PlayerState& p, float frameSec) {
    auto& cues = audio::CueService::Instance();
    float h = 0.0f;
    const bool ground = game::GroundBelow(p.pos.x, p.pos.y + kTile * 0.25f, p.layer, h);
    const float dist = (p.pos.y - h) / kTile;
    if (!ground || dist > kPitTiles * 2.0f) {
        if (g.airTone) cues.Stop(audio::CueChannel::Ground);
        g.airTone = false;
        g.noGroundTimer -= frameSec;
        if (g.noGroundTimer <= 0.0f) {
            cues.PlayOneShot(audio::CueChannel::Hazard, audio::cues::NoGroundBelow());
            g.noGroundTimer = audio::cues::kNoGroundPulseSec;
        }
        return;
    }
    g.noGroundTimer = 0.0f;
    if (dist < kAirToneMinTiles) {
        if (g.airTone) cues.Stop(audio::CueChannel::Ground);
        g.airTone = false;
        return;
    }
    cues.SetSustained(audio::CueChannel::Ground, audio::cues::GroundBelow(dist));
    g.airTone = true;
}

}  // namespace

void Tick(const game::PlayerState& p, float frameSec) {
    if (!config::Get().terrainCues) {
        Stop();
        return;
    }
    const int facing = p.direction == 0 ? 1 : -1;
    const int ahead = std::fabs(p.speed.x) > kMovingSpeed ? (p.speed.x > 0 ? 1 : -1) : facing;
    float aheadWall = -1.0f;
    TickWalls(p, ahead, frameSec, aheadWall);
    if (p.onGround) {
        if (g.airTone) audio::CueService::Instance().Stop(audio::CueChannel::Ground);
        g.airTone = false;
        g.noGroundTimer = 0.0f;
        TickEdges(p, ahead, aheadWall);
    } else {
        g.lastEdge = Edge::None;
        g.lastEdgeColumn = 0;
        TickAir(p, frameSec);
    }
}

void Stop() {
    auto& cues = audio::CueService::Instance();
    cues.Stop(audio::CueChannel::WallLeft);
    cues.Stop(audio::CueChannel::WallRight);
    cues.Stop(audio::CueChannel::Ground);
    cues.Stop(audio::CueChannel::Edge);
    g = State{};
}

}  // namespace a11y::level::terrain

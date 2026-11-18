#include "accessibility/level/camera.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "accessibility/audio/cues.h"
#include "accessibility/level/actors.h"
#include "accessibility/level/entrances.h"
#include "accessibility/localization.h"
#include "accessibility/speech.h"

namespace a11y::level::camera {
namespace {

using game::kTile;
namespace cues = audio::cues;

constexpr int kMaxOffset = 40;          // tiles from Mario the cursor may go
constexpr float kAutoHomeSec = 8.0f;    // idle time before it snaps back
constexpr float kPanRangeTiles = 8.0f;
constexpr float kSameTileUnits = kTile * 0.75f;
constexpr int kWaterPlain = 1, kWaterBubble = 2, kWaterLava = 3, kWaterPoison = 4;  // WATER_TYPE_e
constexpr float kGroundLookTiles = 30.0f;

int g_dx = 0, g_dy = 0;
float g_idle = 0.0f;

enum class Tile { Open, Solid, Water, Hazard };

// Centre of the cursor's tile, in world units.
void CellCentre(const game::PlayerState& p, float& x, float& y) {
    x = (std::floor(p.pos.x / kTile) + static_cast<float>(g_dx)) * kTile + kTile * 0.5f;
    y = (std::floor(p.pos.y / kTile) + static_cast<float>(g_dy)) * kTile + kTile * 0.5f;
}

Tile TileAt(const game::PlayerState& p, float x, float y) {
    const int water = game::WaterAt(x, y, p.layer);
    if (water == kWaterLava || water == kWaterPoison) return Tile::Hazard;
    if (game::SolidAt(x, y, p.layer)) return Tile::Solid;
    if (water == kWaterPlain || water == kWaterBubble) return Tile::Water;
    return Tile::Open;
}

void PlayTile(Tile t) {
    const float pan = std::clamp(static_cast<float>(g_dx) / kPanRangeTiles, -1.0f, 1.0f) * 0.8f;
    const float semis = static_cast<float>(std::clamp(g_dy, -12, 12));
    switch (t) {
        case Tile::Open: cues::Play(cues::CameraOpen(pan, semis)); break;
        case Tile::Solid: cues::Play(cues::CameraSolid(pan, semis)); break;
        case Tile::Water: cues::Play(cues::CameraWater(pan, semis)); break;
        case Tile::Hazard: cues::Play(cues::CameraHazard(pan, semis)); break;
    }
}

const char* TileKey(Tile t) {
    switch (t) {
        case Tile::Open: return "tile_open";
        case Tile::Solid: return "tile_solid";
        case Tile::Water: return "tile_water";
        case Tile::Hazard: return "tile_hazard";
    }
    return "tile_open";
}

// Names of what stands in the cursor's tile: actors, then an enterable pipe.
std::string ThingsAt(const game::PlayerState& p, float x, float y) {
    std::string out;
    const auto add = [&](const std::string& name) {
        if (!out.empty()) out += ", ";
        out += name;
    };
    for (const NearbyActor& n : Scan(p, static_cast<float>(kMaxOffset), static_cast<float>(kMaxOffset))) {
        if (std::fabs(n.info.center.x - x) > kSameTileUnits || std::fabs(n.info.center.y - y) > kSameTileUnits) continue;
        add(n.phraseKey.empty() ? loc::Get("kind_enemy") : loc::Get(n.phraseKey));
    }
    for (const entrances::Pipe& pipe : entrances::PipesNear(p, static_cast<float>(kMaxOffset))) {
        if (std::fabs(pipe.x - x) <= kTile && std::fabs(pipe.y - y) <= kTile) add(loc::Get("kind_pipe"));
    }
    return out;
}

}  // namespace

void Move(const game::PlayerState& p, int dx, int dy) {
    g_idle = 0.0f;
    if (std::abs(g_dx + dx) > kMaxOffset || std::abs(g_dy + dy) > kMaxOffset) {
        cues::Play(cues::CameraLimit());
        return;
    }
    g_dx += dx;
    g_dy += dy;
    float x = 0.0f, y = 0.0f;
    CellCentre(p, x, y);
    PlayTile(TileAt(p, x, y));
    if (g_dx == 0 && g_dy == 0) cues::Play(cues::CameraHome());
    const std::string things = ThingsAt(p, x, y);
    if (!things.empty()) speech::Say(things, true);
}

void Describe(const game::PlayerState& p) {
    g_idle = 0.0f;
    float x = 0.0f, y = 0.0f;
    CellCentre(p, x, y);
    const Tile t = TileAt(p, x, y);
    std::string text = (g_dx == 0 && g_dy == 0) ? loc::Get("camera_mario") : Where(g_dx, g_dy);
    text += ": " + loc::Get(TileKey(t));
    if (t == Tile::Open || t == Tile::Water) {
        float h = 0.0f;
        if (game::GroundBelow(x, y, p.layer, h) && (y - h) / kTile < kGroundLookTiles) {
            const int below = static_cast<int>(std::lround((y - h) / kTile - 0.5f));
            text += ", " + (below <= 0 ? loc::Get("camera_ground_here")
                                       : loc::Format("camera_ground_below", {{"n", std::to_string(below)}}));
        } else {
            text += ", " + loc::Get("camera_nothing_below");
        }
    }
    const std::string things = ThingsAt(p, x, y);
    if (!things.empty()) text += ". " + things;
    speech::Say(text, true);
}

void Home() {
    g_dx = g_dy = 0;
    g_idle = 0.0f;
    cues::Play(cues::CameraHome());
}

void Tick(float frameSec) {
    if (g_dx == 0 && g_dy == 0) return;
    g_idle += frameSec;
    if (g_idle >= kAutoHomeSec) Home();
}

}  // namespace a11y::level::camera

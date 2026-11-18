#ifndef NSMBW_ACCESSIBILITY_LEVEL_TERRAIN_H
#define NSMBW_ACCESSIBILITY_LEVEL_TERRAIN_H

#include "accessibility/game/nsmbw_game.h"

// Terrain cues from the game's own collision queries: walls to each side, pits / drops / steps
// ahead while on the ground, and the ground below while in the air.
namespace a11y::level::terrain {

void Tick(const game::PlayerState& player, float frameSec);
void Stop();

}  // namespace a11y::level::terrain

#endif  // NSMBW_ACCESSIBILITY_LEVEL_TERRAIN_H

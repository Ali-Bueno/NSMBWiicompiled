#ifndef NSMBW_ACCESSIBILITY_LEVEL_BLOCKS_H
#define NSMBW_ACCESSIBILITY_LEVEL_BLOCKS_H

#include "accessibility/game/nsmbw_game.h"

// Hittable blocks in the background tiles: ? blocks (which hold an item or a coin) and bricks.
// The nearest ? block sounds from its side and height now and then; a bell rings while Mario
// stands right under one, so he knows to stop and jump.
namespace a11y::level::blocks {

void Tick(const game::PlayerState& player, float frameSec);
void Reset();

}  // namespace a11y::level::blocks

#endif  // NSMBW_ACCESSIBILITY_LEVEL_BLOCKS_H

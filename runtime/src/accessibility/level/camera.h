#ifndef NSMBW_ACCESSIBILITY_LEVEL_CAMERA_H
#define NSMBW_ACCESSIBILITY_LEVEL_CAMERA_H

#include "accessibility/game/nsmbw_game.h"

// The tile camera: a cursor moved one tile at a time around Mario (I J K L), which sounds what is
// in that tile (solid, open, water, lava) and names what stands there; U describes it in words,
// O brings it back to Mario. It returns by itself after a while without use.
namespace a11y::level::camera {

void Move(const game::PlayerState& player, int dx, int dy);
void Describe(const game::PlayerState& player);
void Home();
void Tick(float frameSec);

}  // namespace a11y::level::camera

#endif  // NSMBW_ACCESSIBILITY_LEVEL_CAMERA_H

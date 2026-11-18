#ifndef NSMBW_ACCESSIBILITY_LEVEL_COMBAT_H
#define NSMBW_ACCESSIBILITY_LEVEL_COMBAT_H

#include "accessibility/game/nsmbw_game.h"

// Timing help for dealing with enemies: "jump now" when an enemy closing in on the ground will be
// under Mario's feet at the top of a jump (or "jump over it" for one that hurts from above), and,
// while falling, a ding when Mario is lined up over a stompable enemy or a buzz over a harmful one.
namespace a11y::level::combat {

void Tick(const game::PlayerState& player);
void Reset();
// Debug (NSMBW_A11Y_LOG_COMBAT): trace Mario and the nearest actor for a while.
void TraceNearest();

}  // namespace a11y::level::combat

#endif  // NSMBW_ACCESSIBILITY_LEVEL_COMBAT_H

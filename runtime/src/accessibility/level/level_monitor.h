#ifndef NSMBW_ACCESSIBILITY_LEVEL_LEVEL_MONITOR_H
#define NSMBW_ACCESSIBILITY_LEVEL_LEVEL_MONITOR_H

#include <string>

#include "accessibility/game/nsmbw_game.h"

// Spoken state changes in and around a level: the course name on the course-in screen, power-up
// changes, a lost life, the time running low, star coins, water. Also answers the info hotkeys.
namespace a11y::level::monitor {

// Once per frame; `player` is null outside a playable level.
void Tick(const game::PlayerState* player);

std::string CourseName(int world, int level);  // 0-based, as dInfo_c stores them
std::string Status(const game::PlayerState& player);
std::string Progress(const game::PlayerState& player);

}  // namespace a11y::level::monitor

#endif  // NSMBW_ACCESSIBILITY_LEVEL_LEVEL_MONITOR_H

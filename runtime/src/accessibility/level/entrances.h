#ifndef NSMBW_ACCESSIBILITY_LEVEL_ENTRANCES_H
#define NSMBW_ACCESSIBILITY_LEVEL_ENTRANCES_H

#include <string>
#include <vector>

#include "accessibility/game/nsmbw_game.h"

// Enterable pipes, from the course file's entrance list (dCdFile_c next-goto data): pipes are
// background tiles, not actors, so this is the only place they can be found.
namespace a11y::level::entrances {

enum class PipeKind { Down, Up, Side };  // which way the player goes into it

struct Pipe {
    float x = 0.0f, y = 0.0f;  // world units, y up
    PipeKind kind = PipeKind::Down;
};

std::vector<Pipe> PipesNear(const game::PlayerState& player, float rangeTiles);

// One soft cue for the nearest enterable pipe, repeated while it is in range.
void TickCue(const game::PlayerState& player, float frameSec);

// The goal pole of the current area from the course's sprite list (it only exists as an actor
// once it is near the screen).
bool GoalFromCourseData(Vec3& goal);

// Debug (NSMBW_A11Y_LOG_PLAYER): every entrance of the current course file.
void LogAll();

// Spoken lines for F4, nearest first.
std::vector<std::string> Describe(const game::PlayerState& player, float rangeTiles);

}  // namespace a11y::level::entrances

#endif  // NSMBW_ACCESSIBILITY_LEVEL_ENTRANCES_H

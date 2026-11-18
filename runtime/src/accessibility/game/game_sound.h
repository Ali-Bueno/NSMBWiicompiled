#ifndef NSMBW_ACCESSIBILITY_GAME_GAME_SOUND_H
#define NSMBW_ACCESSIBILITY_GAME_GAME_SOUND_H

#include <cstdint>

// The game's own sound effects, played through its sound engine on the game thread. Sounds are
// named by their label in the disc's sound archive ("SE_SYS_INVALID"); the archive itself turns
// the label into an ID, so no ID table has to be guessed.
namespace a11y::game::sound {

// A menu-style sound, not positioned.
bool PlaySystem(const char* label);

// A sound at a world position (world units, y up): the game pans it by where that point is on
// screen, the way it pans the sounds of the actors there.
bool PlayAt(const char* label, float x, float y);

// Debug: logs the IDs a few labels resolve to.
void LogLabels();

}  // namespace a11y::game::sound

#endif  // NSMBW_ACCESSIBILITY_GAME_GAME_SOUND_H

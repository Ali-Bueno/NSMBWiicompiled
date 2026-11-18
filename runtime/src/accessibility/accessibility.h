#ifndef NSMBW_ACCESSIBILITY_ACCESSIBILITY_H
#define NSMBW_ACCESSIBILITY_ACCESSIBILITY_H

#include <cstdint>

struct AuroraEvent;

// The accessibility mod's entry points. Nothing may throw out of them: they run inside guest
// code, which cannot unwind.
namespace a11y {

void Init();                                  // product start, before the guest runs
void HandleEvents(const AuroraEvent* events); // host event thread
void Shutdown();                              // process exit

}  // namespace a11y

// Game thread, from the player-1 KPADRead override: once per game frame.
extern "C" void NsmbwAccessibilityGameTick();
// Game thread, from the same override: player 1's Wii Remote buttons as the game receives them.
extern "C" void NsmbwA11yInputSample(std::uint32_t hold, std::uint32_t trig);
// Game thread, from the SelectCursorSetup override (nsmbw_a11y_cursor.cpp).
extern "C" void NsmbwA11yOnCursorPane(std::uint32_t pane, std::uint32_t slot);

#endif  // NSMBW_ACCESSIBILITY_ACCESSIBILITY_H

#ifndef NSMBW_ACCESSIBILITY_TEST_AUTOPILOT_H
#define NSMBW_ACCESSIBILITY_TEST_AUTOPILOT_H

#include <cstdint>

// Test only (NSMBW_A11Y_AUTOPILOT=1): plays a level using nothing but the cues a blind player
// hears. It holds right and reacts to each cue the way the sound guide tells a player to, so a run
// that reaches the goal shows the cues carry enough information and arrive in time. Does nothing
// when the variable is unset.
namespace a11y::autopilot {

enum class Cue { JumpNow, JumpHigh, JumpOver, PitAhead, StepAhead, WallKnock, UnderBlock };

bool Enabled();
void Note(Cue cue);
// Game thread, once per frame while in normal play.
void Tick(bool inLevel);

}  // namespace a11y::autopilot

// Bits ORed into player 1's buttons (nsmbw_kpad_overrides.cpp).
extern "C" std::uint32_t NsmbwA11yAutopilotBits();

#endif  // NSMBW_ACCESSIBILITY_TEST_AUTOPILOT_H

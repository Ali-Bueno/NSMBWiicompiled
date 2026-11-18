#ifndef NSMBW_ACCESSIBILITY_SOUND_GUIDE_H
#define NSMBW_ACCESSIBILITY_SOUND_GUIDE_H

// The sound guide (Shift+F9): a small menu of every cue, by category, that says what a sound
// means and then plays it. Page Down / Page Up move through the sounds, End / Home through the
// categories, F9 plays the current one again, Shift+F9 closes it.
namespace a11y::sound_guide {

void Toggle();
bool IsOpen();
void Next(int delta);
void NextCategory(int delta);
void Replay();

// Advances pending speech/sound timing; true while the guide is open (gameplay cues stay quiet).
bool Tick(float frameSec);

}  // namespace a11y::sound_guide

#endif  // NSMBW_ACCESSIBILITY_SOUND_GUIDE_H

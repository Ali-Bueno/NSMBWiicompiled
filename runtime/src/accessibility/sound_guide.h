#ifndef NSMBW_ACCESSIBILITY_SOUND_GUIDE_H
#define NSMBW_ACCESSIBILITY_SOUND_GUIDE_H

// Shift+F9: says what each cue means, then plays it, one after another.
namespace a11y::sound_guide {

void Start();
// Advances the guide; true while it is playing (gameplay cues stay quiet meanwhile).
bool Tick(float frameSec);

}  // namespace a11y::sound_guide

#endif  // NSMBW_ACCESSIBILITY_SOUND_GUIDE_H

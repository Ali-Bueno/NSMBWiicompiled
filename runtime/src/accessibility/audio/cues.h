#ifndef NSMBW_ACCESSIBILITY_AUDIO_CUES_H
#define NSMBW_ACCESSIBILITY_AUDIO_CUES_H

#include <algorithm>
#include <cmath>

#include "accessibility/audio/cue_service.h"

// The mod's whole sound palette: one function per cue, so every pitch, length and level the
// player learns lives in this file (the sound guide, F9 with Shift, plays them all).
namespace a11y::audio::cues {

inline float Semitones(float n) { return std::pow(2.0f, n / 12.0f); }

// Walls: a short tick repeated faster as the wall gets closer (parking-sensor style).
inline CueSpec WallTick(float pan) {
    CueSpec c;
    c.shape = Waveform::Triangle;
    c.frequencyHz = 330.0f;
    c.amplitude = 0.35f;
    c.pan = pan;
    c.durationSec = 0.04f;
    return c;
}
// Seconds between wall ticks for a wall `tiles` away.
inline float WallTickInterval(float tiles) { return 0.10f + 0.09f * std::max(tiles, 0.0f); }

// A pit (no ground, or a fall deeper than the screen) ahead: a falling saw.
inline CueSpec Pit(float pan) {
    CueSpec c;
    c.shape = Waveform::Saw;
    c.frequencyHz = 620.0f;
    c.sweepToHz = 140.0f;
    c.amplitude = 0.30f;
    c.pan = pan;
    c.durationSec = 0.25f;
    return c;
}
// A drop of a few tiles ahead: a falling sine.
inline CueSpec Drop(float pan) {
    CueSpec c;
    c.shape = Waveform::Sine;
    c.frequencyHz = 560.0f;
    c.sweepToHz = 330.0f;
    c.amplitude = 0.35f;
    c.pan = pan;
    c.durationSec = 0.15f;
    return c;
}
// A step up ahead: a rising sine.
inline CueSpec StepUp(float pan) {
    CueSpec c;
    c.shape = Waveform::Sine;
    c.frequencyHz = 330.0f;
    c.sweepToHz = 560.0f;
    c.amplitude = 0.35f;
    c.pan = pan;
    c.durationSec = 0.12f;
    return c;
}
// Lava, spikes or poison where the player is heading.
inline CueSpec Hazard(float pan) {
    CueSpec c;
    c.shape = Waveform::Saw;
    c.frequencyHz = 150.0f;
    c.sweepToHz = 95.0f;
    c.amplitude = 0.35f;
    c.pan = pan;
    c.durationSec = 0.22f;
    return c;
}

// In the air: a held tone whose pitch rises as the ground below gets closer.
inline CueSpec GroundBelow(float tiles) {
    CueSpec c;
    c.shape = Waveform::Sine;
    c.frequencyHz = std::max(880.0f * std::pow(2.0f, -std::max(tiles, 0.0f) / 4.0f), 110.0f);
    c.amplitude = 0.14f;
    return c;
}
// In the air with nothing below: a low pulse, repeated by the caller.
inline CueSpec NoGroundBelow() {
    CueSpec c;
    c.shape = Waveform::Square;
    c.frequencyHz = 92.0f;
    c.amplitude = 0.22f;
    c.durationSec = 0.12f;
    return c;
}
inline constexpr float kNoGroundPulseSec = 0.30f;

// An enemy: a click panned to its side, higher when it is above the player.
inline CueSpec Enemy(float pan, float tilesAbove, float gain) {
    CueSpec c;
    c.shape = Waveform::Square;
    c.frequencyHz = 1100.0f * Semitones(std::clamp(tilesAbove, -12.0f, 12.0f));
    c.amplitude = 0.30f * gain;
    c.pan = pan;
    c.durationSec = 0.05f;
    return c;
}
inline float EnemyInterval(float tiles) { return tiles < 3.0f ? 0.40f : 0.85f; }

// An item, power-up, star coin or red coin: a bright upward chirp.
inline CueSpec Item(float pan, float tilesAbove, float gain) {
    CueSpec c;
    c.shape = Waveform::Sine;
    c.frequencyHz = 1568.0f * Semitones(std::clamp(tilesAbove, -12.0f, 12.0f));
    c.sweepToHz = c.frequencyHz * Semitones(5.0f);
    c.amplitude = 0.25f * gain;
    c.pan = pan;
    c.durationSec = 0.09f;
    return c;
}
inline constexpr float kItemIntervalSec = 1.1f;
// The game's own sounds used as cues (labels in the disc's sound archive). They repeat less often
// than the synthesized ones: they are longer and carry their own meaning.
inline constexpr char kGameItemLabel[] = "SE_OBJ_ITEM_APPEAR";
inline constexpr float kGameItemIntervalSec = 2.0f;
inline constexpr char kGamePipeLabel[] = "SE_PLY_DOKAN_IN_OUT";
inline constexpr float kGamePipeIntervalSec = 2.6f;
inline constexpr char kGameInvalidLabel[] = "SE_SYS_INVALID";
inline constexpr char kGameCursorLabel[] = "SE_SYS_CURSOR";

// The goal pole: a soft tick from its side.
inline CueSpec Goal(float pan) {
    CueSpec c;
    c.shape = Waveform::Triangle;
    c.frequencyHz = 784.0f;
    c.amplitude = 0.14f;
    c.pan = pan;
    c.durationSec = 0.06f;
    return c;
}
inline constexpr float kGoalIntervalSec = 2.0f;

// An enterable pipe: a low hollow bloop, panned to it, higher when it is above the player.
inline CueSpec Pipe(float pan, float tilesAbove) {
    CueSpec c;
    c.shape = Waveform::Sine;
    c.frequencyHz = 196.0f * Semitones(std::clamp(tilesAbove, -8.0f, 8.0f));
    c.sweepToHz = c.frequencyHz * Semitones(7.0f);
    c.amplitude = 0.35f;
    c.pan = pan;
    c.durationSec = 0.16f;
    return c;
}

// Feedback for hotkeys (volume changes).
inline CueSpec Confirm() {
    CueSpec c;
    c.shape = Waveform::Sine;
    c.frequencyHz = 880.0f;
    c.amplitude = 0.3f;
    c.durationSec = 0.07f;
    return c;
}

// Pan for something `dx` tiles to the side, full at `range` tiles.
inline float PanFor(float dx, float range) { return std::clamp(dx / range, -1.0f, 1.0f) * 0.9f; }
// Loudness falloff with distance, never fully silent inside the range.
inline float GainFor(float distance, float range) {
    return std::clamp(1.0f - distance / range, 0.25f, 1.0f);
}

}  // namespace a11y::audio::cues

#endif  // NSMBW_ACCESSIBILITY_AUDIO_CUES_H

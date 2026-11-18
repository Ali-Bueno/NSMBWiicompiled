#ifndef NSMBW_ACCESSIBILITY_AUDIO_CUES_H
#define NSMBW_ACCESSIBILITY_AUDIO_CUES_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "accessibility/audio/cue_service.h"

// The mod's whole sound palette: one function per cue, so every pitch, length and level the
// player learns lives in this file (the sound guide, Shift+F9, plays them all). The design follows
// the Kirby Return to Dream Land accessibility mod: side is pan, height is pitch (about a semitone
// per tile), continuous information is a quiet hum, and changes are short knocks and chimes.
namespace a11y::audio::cues {

using Composite = std::vector<CueSpec>;

inline float Semitones(float n) { return std::pow(2.0f, n / 12.0f); }
inline constexpr float kLoudnessCurve = 0.6f;  // 0.12 -> 0.28, 0.2 -> 0.38, 0.5 -> 0.66

// Plays every part of a composite sound on the free voices.
inline void Play(const Composite& parts) {
    for (const CueSpec& part : parts) CueService::Instance().PlayFree(part);
}

inline CueSpec Tone(Waveform shape, float f0, float f1, float sec, float gain, float pan, float delay = 0.0f) {
    CueSpec c;
    c.shape = shape;
    c.frequencyHz = f0;
    c.sweepToHz = f1 != f0 ? f1 : 0.0f;
    c.durationSec = sec;
    // Quiet cues were too quiet next to the game's music (player report, 2026-11-18): a power
    // curve lifts the soft ones much more than the loud ones, keeping their order.
    c.amplitude = std::pow(std::clamp(gain, 0.0f, 1.0f), kLoudnessCurve);
    c.pan = pan;
    c.delaySec = delay;
    return c;
}

// Pan for something `dx` tiles to the side, full at `range` tiles.
inline float PanFor(float dx, float range) { return std::clamp(dx / range, -1.0f, 1.0f) * 0.9f; }
// Loudness falloff with distance, never fully silent inside the range.
inline float GainFor(float distance, float range) { return std::clamp(1.0f - distance / range, 0.25f, 1.0f); }

inline void Append(Composite& to, const Composite& more) { to.insert(to.end(), more.begin(), more.end()); }

// A wooden knock: the body, an inharmonic overtone and a short noise click.
inline Composite WoodKnock(float f, float gain, float pan, float delay = 0.0f) {
    CueSpec click = Tone(Waveform::Noise, 1.0f, 1.0f, 0.008f, gain * 0.2f, pan, delay);
    click.lowpassHz = 3200.0f;
    return {Tone(Waveform::Sine, f, f * 0.82f, 0.045f, gain, pan, delay),
            Tone(Waveform::Sine, f * 2.76f, f * 2.3f, 0.025f, gain * 0.3f, pan, delay), click};
}

// ---- Ground and air ---------------------------------------------------------------------------

// In the air: a quiet, dull hum two semitones lower per tile of drop to the ground below.
// `kind`: 0 ground, 1 a thin platform, 2 something that hurts.
inline CueSpec GroundBelow(int dropTiles, int kind) {
    CueSpec c;
    c.shape = kind == 2 ? Waveform::Saw : kind == 1 ? Waveform::Triangle : Waveform::Sine;
    c.frequencyHz = std::max(523.0f * std::pow(2.0f, -static_cast<float>(std::max(dropTiles, 0)) / 6.0f), 98.0f);
    c.amplitude = kind == 2 ? 0.2f : 0.18f;
    c.lowpassHz = kind == 2 ? 1400.0f : 900.0f;
    return c;
}
// In the air over a hole (nothing below): held, gusting wind on the ground-hum channel.
inline CueSpec PitWind() {
    CueSpec c;
    c.shape = Waveform::Noise;
    c.frequencyHz = 1.0f;
    c.amplitude = 0.3f;
    c.lowpassHz = 600.0f;
    c.pulseHz = 1.5f;
    c.pulseDepth = 0.5f;
    c.attackSec = 0.08f;
    return c;
}
// The sound guide plays the held wind for a moment.
inline Composite Pit() {
    CueSpec c = PitWind();
    c.durationSec = 1.2f;
    return {c};
}
// In the air, a higher platform has come in under Mario.
inline Composite PlatformUnder() { return WoodKnock(560.0f, 0.4f, 0.0f); }
// In the air, ground again after a pit: a two-note "safe".
inline Composite GroundAfterPit() {
    return {Tone(Waveform::Triangle, 784.0f, 784.0f, 0.06f, 0.3f, 0.0f),
            Tone(Waveform::Triangle, 1175.0f, 1175.0f, 0.08f, 0.3f, 0.0f, 0.07f)};
}

// ---- Edges ahead (on the ground) --------------------------------------------------------------

// Each edge has its own family, so none is mistaken for another:
// a pit is a low noise rumble (a hole), a drop is wood knocks walking down a stair, a step up is
// knocks walking up, lava is the game's lava splash (synthesized: a rough low buzz).
// A hole ahead: a gust of wind (the player asked for wind for holes, 2026-11-18).
inline Composite EdgePit(float pan) {
    CueSpec gust = Tone(Waveform::Noise, 1.0f, 1.0f, 0.45f, 0.5f, pan);
    gust.lowpassHz = 700.0f;
    gust.attackSec = 0.15f;
    gust.releaseSec = 0.35f;
    gust.pulseHz = 3.0f;
    gust.pulseDepth = 0.4f;
    return {gust};
}
inline Composite EdgeHazard(float pan) {
    CueSpec buzz = Tone(Waveform::Saw, 90.0f, 90.0f, 0.3f, 0.35f, pan);
    buzz.pulseHz = 12.0f;
    buzz.pulseDepth = 0.8f;
    return {buzz};
}
inline Composite EdgeDrop(float pan) {
    Composite c = WoodKnock(520.0f, 0.4f, pan);
    Append(c, WoodKnock(390.0f, 0.36f, pan, 0.11f));
    Append(c, WoodKnock(290.0f, 0.32f, pan, 0.22f));
    return c;
}
inline Composite EdgeStep(float pan) {
    Composite c = WoodKnock(390.0f, 0.4f, pan);
    Append(c, WoodKnock(520.0f, 0.4f, pan, 0.11f));
    return c;
}
inline constexpr char kGameLavaLabel[] = "SE_OBJ_CMN_SPLASH_LAVA";

// A platform above Mario that he can jump onto: two knocks, a semitone higher per tile up.
inline Composite PlatformAbove(int tilesUp) {
    const float f = 1250.0f * Semitones(static_cast<float>(tilesUp));
    Composite c = WoodKnock(f, 0.22f, 0.0f);
    Append(c, WoodKnock(f, 0.18f, 0.0f, 0.07f));
    Append(c, WoodKnock(f, 0.14f, 0.0f, 0.14f));
    return c;
}
inline constexpr float kPlatformAboveCooldownSec = 1.1f;

// ---- Walls and ceiling ------------------------------------------------------------------------

// What a wall is to Mario, by its height (tiles to the first gap he fits through).
enum class WallKind { Step, HighJump, Wall };
inline constexpr float kStepMaxTiles = 2.2f;      // a hop gets over it
inline constexpr float kHighJumpMaxTiles = 5.2f;  // a full (running) jump gets over it

inline WallKind WallKindFor(float heightTiles) {
    if (heightTiles <= kStepMaxTiles) return WallKind::Step;
    if (heightTiles <= kHighJumpMaxTiles) return WallKind::HighJump;
    return WallKind::Wall;
}

// A held hum per side, louder as the wall gets closer: ticking for a step, a slow throb for a
// wall you need a big jump for, a steady low hum for a real wall.
inline CueSpec WallHum(WallKind kind, float closeness, float pan) {
    CueSpec c;
    c.pan = pan;
    c.lowpassHz = 1800.0f;
    const float shaped = std::clamp(closeness, 0.0f, 1.0f);
    switch (kind) {
        case WallKind::Step:
            c.shape = Waveform::Triangle;
            c.frequencyHz = 330.0f;
            c.pulseHz = 4.0f;
            c.pulseDepth = 0.8f;
            c.pluck = true;
            c.amplitude = shaped * 0.5f;
            break;
        case WallKind::HighJump:
            c.shape = Waveform::Warm;
            c.frequencyHz = 247.0f;
            c.pulseHz = 2.5f;
            c.pulseDepth = 0.7f;
            c.amplitude = shaped * 0.42f;
            break;
        case WallKind::Wall:
            c.shape = Waveform::Warm;
            c.frequencyHz = 196.0f;
            c.amplitude = shaped * 0.36f;
            break;
    }
    return c;
}
inline constexpr float kWallRangeTiles = 5.0f;
inline constexpr float kWallBehindGain = 0.75f;

// Walking into a wall.
inline Composite WallKnock(float pan) { return {Tone(Waveform::Square, 150.0f, 110.0f, 0.07f, 0.55f, pan)}; }
inline constexpr float kWallKnockCooldownSec = 0.35f;

// A ceiling above: filtered noise, louder and brighter as it gets lower.
inline CueSpec CeilingHum(float closeness) {
    CueSpec c;
    c.shape = Waveform::Noise;
    c.frequencyHz = 400.0f;
    const float k = std::clamp(closeness, 0.0f, 1.0f);
    c.amplitude = 0.1f + 0.2f * k;
    c.lowpassHz = 350.0f + 650.0f * k;
    return c;
}
inline constexpr float kCeilingRangeTiles = 6.0f;

// ---- Enemies ----------------------------------------------------------------------------------

// Each enemy kind has its own short signature (derived from its profile, so the same kind always
// sounds the same); a stompable enemy's signature follows a bright click, a dangerous one has no
// click and a rough low voice, so the two classes never sound alike.
inline Composite EnemySignature(std::uint16_t profile, bool dangerous, float pan, float tilesAbove, float gain) {
    const float shift = Semitones(std::clamp(tilesAbove, -12.0f, 12.0f));
    std::uint32_t h = 2166136261u;  // FNV-1a of the profile id
    h = (h ^ (profile & 0xFFu)) * 16777619u;
    h = (h ^ (profile >> 8)) * 16777619u;
    Composite c;
    if (dangerous) {
        const float f = 160.0f * Semitones(static_cast<float>(h % 12)) * shift;
        c.push_back(Tone(Waveform::Saw, f, f, 0.22f, gain * 0.9f, pan));
        CueSpec rasp = Tone(Waveform::Noise, 1.0f, 1.0f, 0.22f, gain * 0.25f, pan);
        rasp.lowpassHz = 900.0f;
        c.push_back(rasp);
        return c;
    }
    c.push_back(Tone(Waveform::Square, 3200.0f, 2400.0f, 0.014f, std::max(gain * 1.2f, 0.16f), pan));
    static constexpr Waveform kWaves[] = {Waveform::Sine, Waveform::Square, Waveform::Triangle};
    const Waveform w = kWaves[(h / 25) % 3];
    const float f = 300.0f * Semitones(static_cast<float>(h % 25)) * shift;
    const float t = 0.045f;  // after the click
    switch ((h / 75) % 5) {
        case 0: c.push_back(Tone(w, f, f, 0.07f, gain, pan, t)); break;
        case 1:
            c.push_back(Tone(w, f, f, 0.05f, gain, pan, t));
            c.push_back(Tone(w, f, f, 0.05f, gain, pan, t + 0.08f));
            break;
        case 2:
            for (int i = 0; i < 3; ++i) c.push_back(Tone(w, f, f, 0.035f, gain, pan, t + 0.045f * i));
            break;
        case 3: c.push_back(Tone(w, f * 0.75f, f * 1.25f, 0.12f, gain, pan, t)); break;
        default: c.push_back(Tone(w, f * 1.25f, f * 0.75f, 0.12f, gain, pan, t)); break;
    }
    return c;
}
// Seconds between radar passes, and between enemies inside one pass.
inline float EnemyPassInterval(float nearestTiles) { return nearestTiles < 3.0f ? 0.55f : 1.0f; }
inline constexpr float kEnemyStaggerSec = 0.25f;

// A boss: a low double drum with a hit, pitched by height.
inline Composite Boss(float pan, float tilesAbove) {
    const float s = Semitones(std::clamp(tilesAbove, -12.0f, 12.0f));
    CueSpec hit = Tone(Waveform::Noise, 1.0f, 1.0f, 0.06f, 0.3f, pan);
    hit.lowpassHz = 180.0f;
    return {Tone(Waveform::Sine, 90.0f * s, 60.0f * s, 0.14f, 0.6f, pan),
            Tone(Waveform::Sine, 90.0f * s, 60.0f * s, 0.14f, 0.6f, pan, 0.17f), hit};
}
inline float BossInterval(float tiles) { return tiles < 4.0f ? 0.8f : tiles < 8.0f ? 1.0f : 1.4f; }

// The approach sensor: a short pip from the enemy's side, repeated faster as the moment to jump
// nears (a buzzier pip for a dangerous one).
inline Composite ApproachBeep(float pan, bool danger) {
    CueSpec c = Tone(danger ? Waveform::Saw : Waveform::Square, 660.0f, 660.0f, 0.03f, 0.2f, pan);
    c.lowpassHz = 2200.0f;
    return {c};
}
// In the air, not yet over the enemy below: a high blip from its side.
inline Composite AimBeep(float pan, bool danger) {
    return {Tone(danger ? Waveform::Saw : Waveform::Sine, 1320.0f, 1320.0f, 0.025f, 0.22f, pan)};
}
inline constexpr float kDangerBelowRepeatSec = 0.3f;
// A demo of the sensor speeding up, for the sound guide.
inline Composite ApproachDemo(Composite accent) {
    Composite c;
    float t = 0.0f;
    for (float gap : {0.45f, 0.35f, 0.26f, 0.18f, 0.12f, 0.09f}) {
        Composite b = ApproachBeep(0.5f, false);
        for (CueSpec& x : b) x.delaySec = t;
        Append(c, b);
        t += gap;
    }
    for (CueSpec& a : accent) a.delaySec += t;
    Append(c, accent);
    return c;
}
inline Composite AimDemo() {
    Composite c;
    float t = 0.0f;
    for (float gap : {0.3f, 0.22f, 0.15f, 0.1f}) {
        Composite b = AimBeep(0.6f, false);
        for (CueSpec& x : b) x.delaySec = t;
        Append(c, b);
        t += gap;
    }
    Composite d = {Tone(Waveform::Sine, 1760.0f, 2349.0f, 0.07f, 0.30f, 0.0f, t)};
    Append(c, d);
    return c;
}

// The countdown to a jump: three short ticks, each a step higher (beat 1-3), a quarter second
// apart, then one of the jump cues below on the beat where Mario should leave the ground.
inline Composite CountTick(float pan, int beat) {
    // Unfiltered noise: a hi-hat click, the only sound of its kind, a little louder each beat.
    return {Tone(Waveform::Noise, 1.0f, 1.0f, 0.025f, 0.16f + 0.06f * static_cast<float>(beat), pan)};
}
// A bright major chord: the moment to jump.
inline Composite Chord(float root, float gain, float pan, float delay = 0.0f) {
    return {Tone(Waveform::Square, root, root, 0.13f, gain, pan, delay),
            Tone(Waveform::Square, root * Semitones(4.0f), root * Semitones(4.0f), 0.13f, gain * 0.8f, pan, delay),
            Tone(Waveform::Square, root * Semitones(7.0f), root * Semitones(7.0f), 0.13f, gain * 0.8f, pan, delay)};
}
inline Composite Countdown(Composite accent) {
    Composite c;
    for (int beat = 1; beat <= 3; ++beat) {
        Composite tick = CountTick(0.5f, beat);
        for (CueSpec& t : tick) t.delaySec = 0.25f * static_cast<float>(beat - 1);
        Append(c, tick);
    }
    for (CueSpec& a : accent) a.delaySec += 0.75f;
    Append(c, accent);
    return c;
}

// "Jump now": a stompable enemy walking into Mario will be under his feet if he jumps now.
inline Composite JumpNow(float pan) { return Chord(1047.0f, 0.22f, pan); }
// "Jump high now": the enemy is up on a ledge; only a full jump (button held) lands on it.
inline Composite JumpHigh(float pan) {
    Composite c = Chord(1047.0f, 0.22f, pan);
    Append(c, Chord(1568.0f, 0.22f, pan, 0.14f));
    return c;
}
// "Jump over it": the same moment for an enemy that hurts from above.
inline Composite JumpOver(float pan) {
    // A harsh rising tritone: jump, but not onto it.
    return {Tone(Waveform::Saw, 330.0f, 990.0f, 0.2f, 0.28f, pan),
            Tone(Waveform::Saw, 330.0f * Semitones(6.0f), 990.0f * Semitones(6.0f), 0.2f, 0.22f, pan)};
}
// Falling right onto a stompable enemy: a bright ding.
inline Composite StompTarget(float pan) { return {Tone(Waveform::Sine, 1760.0f, 2349.0f, 0.07f, 0.30f, pan)}; }
// Falling onto something that hurts: a falling buzz, steer away.
inline Composite DangerBelow(float pan) { return {Tone(Waveform::Saw, 330.0f, 165.0f, 0.15f, 0.35f, pan)}; }

// ---- Items, goal, pipes -----------------------------------------------------------------------

// An item, power-up or star coin (synthesized fallback for the game's own item sound).
inline Composite Item(float pan, float tilesAbove, float gain) {
    const float f = 1568.0f * Semitones(std::clamp(tilesAbove, -12.0f, 12.0f));
    return {Tone(Waveform::Sine, f, f * Semitones(5.0f), 0.09f, 0.25f * gain, pan)};
}
inline constexpr float kItemIntervalSec = 1.1f;
// A ? block, as the game sounds a coin popping out of one.
inline constexpr char kGameBlockLabel[] = "SE_OBJ_COIN_POP_UP";
inline constexpr float kMovingItemIntervalSec = 0.35f;
// A ? block nearby: a soft two-note chirp from its side, higher the higher it is.
inline Composite BlockBeacon(float pan, float tilesUp) {
    const float f = 988.0f * Semitones(std::clamp(tilesUp, 0.0f, 8.0f));
    const float g = f * Semitones(5.0f);
    return {Tone(Waveform::Square, f, f, 0.05f, 0.12f, pan), Tone(Waveform::Square, g, g, 0.08f, 0.12f, pan, 0.06f)};
}
inline constexpr float kBlockBeaconSec = 1.6f;
// A coin nearby: a short metallic ting (two inharmonic partials), higher the higher it is.
inline Composite CoinTing(float pan, float tilesUp) {
    const float f = 2400.0f * Semitones(std::clamp(tilesUp, -2.0f, 8.0f));
    return {Tone(Waveform::Sine, f, f, 0.05f, 0.14f, pan), Tone(Waveform::Sine, f * 1.47f, f * 1.47f, 0.035f, 0.08f, pan)};
}
inline constexpr float kCoinNearSec = 0.4f;
inline constexpr float kCoinFarSec = 0.9f;
// Right under a ? block: the same chirp, louder and twice, centred: stop and jump to hit it.
inline Composite UnderBlock(float tilesUp) {
    Composite c = BlockBeacon(0.0f, tilesUp);
    Append(c, BlockBeacon(0.0f, tilesUp));
    for (size_t i = 0; i < c.size(); ++i) {
        c[i].amplitude = 0.3f;
        if (i >= 2) c[i].delaySec += 0.18f;
    }
    return c;
}
inline constexpr float kUnderBlockRepeatSec = 1.5f;
// An item on the move, ticking fast (demo for the sound guide).
inline Composite MovingItemDemo() {
    Composite c;
    for (int i = 0; i < 4; ++i) {
        Composite t = Item(0.6f, 0.0f, 1.0f);
        for (CueSpec& x : t) x.delaySec = kMovingItemIntervalSec * static_cast<float>(i);
        Append(c, t);
    }
    return c;
}
// Lined up right under an item: a rising bell, lower the higher the item is.
inline Composite UnderItem(float tilesAbove) {
    const float f = 2093.0f * Semitones(-std::clamp(tilesAbove, 0.0f, 12.0f));
    return {Tone(Waveform::Sine, f, f * 2.0f, 0.08f, 0.22f, 0.0f),
            Tone(Waveform::Sine, f * 2.0f, f * 2.0f, 0.12f, 0.18f, 0.0f, 0.08f),
            Tone(Waveform::Sine, f * 3.0f, f * 3.0f, 0.12f, 0.06f, 0.0f, 0.08f)};
}
inline constexpr float kUnderItemRepeatSec = 1.2f;

// The game's own sounds used as cues (labels in the disc's sound archive). They repeat less often
// than the synthesized ones: they are longer and carry their own meaning.
inline constexpr char kGameItemLabel[] = "SE_OBJ_ITEM_APPEAR";
inline constexpr float kGameItemIntervalSec = 2.0f;
inline constexpr char kGamePipeLabel[] = "SE_PLY_DOKAN_IN_OUT";
inline constexpr float kGamePipeIntervalSec = 2.6f;
inline constexpr char kGameCursorLabel[] = "SE_SYS_CURSOR";

// The goal pole: a soft tick from its side.
inline Composite Goal(float pan) {
    CueSpec a = Tone(Waveform::Sine, 523.0f, 523.0f, 0.6f, 0.16f, pan);
    a.releaseSec = 0.5f;
    CueSpec b = Tone(Waveform::Sine, 523.0f * 2.76f, 523.0f * 2.76f, 0.3f, 0.06f, pan);
    b.releaseSec = 0.3f;
    return {a, b};
}
inline constexpr float kGoalIntervalSec = 2.0f;

// An enterable pipe (synthesized fallback): a low hollow bloop, higher when above.
inline Composite Pipe(float pan, float tilesAbove) {
    const float f = 196.0f * Semitones(std::clamp(tilesAbove, -8.0f, 8.0f));
    return {Tone(Waveform::Sine, f, f * Semitones(7.0f), 0.16f, 0.35f, pan)};
}

// ---- Tile camera ------------------------------------------------------------------------------

inline Composite CameraOpen(float pan, float semis) {
    const float f = 1100.0f * Semitones(semis);
    return {Tone(Waveform::Triangle, f, f, 0.03f, 0.12f, pan)};
}
inline Composite CameraSolid(float pan, float semis) {
    const float s = Semitones(semis);
    return {Tone(Waveform::Square, 150.0f * s, 110.0f * s, 0.07f, 0.45f, pan)};
}
inline Composite CameraWater(float pan, float semis) {
    const float f = 600.0f * Semitones(semis);
    return {Tone(Waveform::Sine, f, f * 1.5f, 0.06f, 0.25f, pan),
            Tone(Waveform::Sine, f * 1.2f, f * 1.8f, 0.05f, 0.2f, pan, 0.07f)};
}
inline Composite CameraHazard(float pan, float semis) {
    const float f = 220.0f * Semitones(semis);
    return {Tone(Waveform::Saw, f, f, 0.12f, 0.35f, pan)};
}
inline Composite CameraHome() { return {Tone(Waveform::Sine, 880.0f, 1320.0f, 0.1f, 0.25f, 0.0f)}; }
inline Composite CameraLimit() { return {Tone(Waveform::Sine, 120.0f, 90.0f, 0.1f, 0.4f, 0.0f)}; }

// ---- Feedback ---------------------------------------------------------------------------------

inline Composite Confirm() { return {Tone(Waveform::Sine, 880.0f, 880.0f, 0.07f, 0.3f, 0.0f)}; }

}  // namespace a11y::audio::cues

#endif  // NSMBW_ACCESSIBILITY_AUDIO_CUES_H

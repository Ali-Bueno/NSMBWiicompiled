#include "accessibility/audio/waveform.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace a11y::audio {
namespace {

// Equal-RMS trims. For peak A the RMS is A for a square, A/sqrt(2) for a sine and A/sqrt(3) for a
// triangle or saw; normalising to the triangle keeps every trim at or below full scale.
constexpr float kTrimTriangle = 1.0f;
constexpr float kTrimSaw = 1.0f;
constexpr float kTrimSine = 0.816496580927726f;    // sqrt(2/3)
constexpr float kTrimSquare = 0.577350269189626f;  // 1/sqrt(3)

constexpr float kTwoPi = 6.283185307179586f;
// Noise and the warm hum, trimmed by ear to sit at the triangle's loudness.
constexpr float kTrimNoise = 0.6f;
constexpr float kWarm2 = 0.45f;
constexpr float kWarm3 = 0.25f;
constexpr float kTrimWarm = 0.6f;
constexpr float kQuarterPi = 0.785398163397448f;

}  // namespace

float SampleWaveform(Waveform shape, float phase) {
    switch (shape) {
        case Waveform::Sine:
            return std::sin(phase * kTwoPi) * kTrimSine;
        case Waveform::Square:
            return (phase < 0.5f ? 1.0f : -1.0f) * kTrimSquare;
        case Waveform::Saw:
            return (phase * 2.0f - 1.0f) * kTrimSaw;
        case Waveform::Noise: {
            // xorshift: cheap, and the mixer renders on one thread.
            static std::uint32_t state = 0x9E3779B9u;
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return (static_cast<float>(state) / 2147483648.0f - 1.0f) * kTrimNoise;
        }
        case Waveform::Warm:
            return (std::sin(phase * kTwoPi) + kWarm2 * std::sin(2.0f * phase * kTwoPi) +
                    kWarm3 * std::sin(3.0f * phase * kTwoPi)) * kTrimWarm;
        case Waveform::Triangle:
        default:
            return (phase < 0.5f ? phase * 4.0f - 1.0f : 3.0f - phase * 4.0f) * kTrimTriangle;
    }
}

void PanGains(float pan, float& left, float& right) {
    const float angle = (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * kQuarterPi;
    left = std::cos(angle);
    right = std::sin(angle);
}

float SoftLimit(float value, float knee) {
    const float magnitude = std::fabs(value);
    const float headroom = 1.0f - knee;
    if (magnitude <= knee || headroom <= 0.0f) {
        return std::clamp(value, -1.0f, 1.0f);
    }
    // tanh meets the linear region with matching slope, so nothing below the knee is touched and
    // the curve above it approaches full scale without ever reaching it.
    const float limited = knee + headroom * std::tanh((magnitude - knee) / headroom);
    return value < 0.0f ? -limited : limited;
}

}  // namespace a11y::audio

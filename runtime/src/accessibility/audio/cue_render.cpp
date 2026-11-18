#include "accessibility/audio/cue_service.h"

#include <algorithm>
#include <cmath>

namespace a11y::audio {
namespace {
constexpr float kInt16Peak = 32767.0f;
constexpr float kPluckRiseSec = 0.005f;
constexpr float kPluckDecaySec = 0.045f / 3.0f;  // about 95% decayed 45 ms after the rise
constexpr float kTwoPiRender = 6.2831853f;
}  // namespace

bool CueService::SamplePlayable(const Sample& sample, float pos) {
    // Linear interpolation needs both the current sample and its successor.
    return pos >= 0.0f && static_cast<size_t>(pos) + 1 < sample.mono.size();
}

void CueService::Render(int frames) {
    mMix.assign(static_cast<size_t>(frames) * kChannels, 0.0f);
    const float invFrames = 1.0f / static_cast<float>(frames);
    const float secPerSample = 1.0f / static_cast<float>(kSampleRate);
    float loudestVoice = 0.0f;  // limiter knee, so one voice alone passes through untouched

    for (Voice& voice : mVoices) {
        if (!voice.active) {
            continue;
        }
        // Re-steering glides across the block; a sweep follows the cue's duration instead.
        const bool sweeping = voice.sweepPerSample != 1.0f;
        const float freqStep = sweeping ? 0.0f : (voice.targetFreq - voice.freq) * invFrames;
        const float ampStep = (voice.targetAmp - voice.amp) * invFrames;
        const float gainLStep = (voice.targetGainL - voice.gainL) * invFrames;
        const float gainRStep = (voice.targetGainR - voice.gainR) * invFrames;
        float voicePeak = 0.0f;

        const float pulseStep = voice.pulseHz * secPerSample;
        for (int i = 0; i < frames; ++i) {
            if (voice.delaySec > 0.0f) {  // a queued part of a composite sound: not started yet
                voice.delaySec -= secPerSample;
                continue;
            }
            // A sample plays to its own end; only synthesized one-shots run on the timer.
            if (!voice.sample && !voice.sustained && !voice.stopping) {
                voice.remainingSec -= secPerSample;
                if (voice.remainingSec <= 0.0f) {
                    voice.stopping = true;
                }
            }
            const float target = voice.stopping ? 0.0f : 1.0f;
            const float step = target > voice.env ? voice.attackStep : voice.releaseStep;
            voice.env += std::clamp(target - voice.env, -step, step);

            float raw = 0.0f;
            if (voice.sample) {
                const std::vector<float>& pcm = voice.sample->mono;
                if (SamplePlayable(*voice.sample, voice.samplePos)) {
                    const size_t index = static_cast<size_t>(voice.samplePos);
                    const float frac = voice.samplePos - static_cast<float>(index);
                    raw = pcm[index] + (pcm[index + 1] - pcm[index]) * frac;
                    voice.samplePos += voice.rate;
                } else {
                    voice.stopping = true;
                }
            } else {
                raw = SampleWaveform(voice.shape, voice.phase);
                voice.phase += voice.freq * secPerSample;
                voice.phase -= std::floor(voice.phase);
            }
            if (voice.lowpassCoef > 0.0f) {
                voice.lowpassState += voice.lowpassCoef * (raw - voice.lowpassState);
                raw = voice.lowpassState;
            }
            float pulse = 1.0f;
            if (voice.pulseHz > 0.0f) {
                voice.pulsePhase += pulseStep;
                voice.pulsePhase -= std::floor(voice.pulsePhase);
                float shape;
                if (voice.pluck) {
                    const float t = voice.pulsePhase / voice.pulseHz;  // seconds into this cycle
                    shape = t < kPluckRiseSec ? t / kPluckRiseSec : std::exp(-(t - kPluckRiseSec) / kPluckDecaySec);
                } else {
                    shape = 0.5f - 0.5f * std::cos(voice.pulsePhase * kTwoPiRender);
                }
                pulse = 1.0f - voice.pulseDepth + voice.pulseDepth * shape;
            }
            const float sample = raw * voice.amp * voice.env * voice.channelVolume * pulse;
            const float left = sample * voice.gainL;
            const float right = sample * voice.gainR;
            mMix[static_cast<size_t>(i) * kChannels] += left;
            mMix[static_cast<size_t>(i) * kChannels + 1] += right;
            voicePeak = std::max(voicePeak, std::max(std::fabs(left), std::fabs(right)));

            if (sweeping) {
                // Hold the final pitch throughout release, including within a queued block.
                const float next = voice.freq * voice.sweepPerSample;
                voice.freq = voice.sweepPerSample > 1.0f ? std::min(next, voice.targetFreq)
                                                       : std::max(next, voice.targetFreq);
            } else {
                voice.freq += freqStep;
            }
            voice.amp += ampStep;
            voice.gainL += gainLStep;
            voice.gainR += gainRStep;
        }

        if (!sweeping) {
            voice.freq = voice.targetFreq;
        }
        voice.amp = voice.targetAmp;
        voice.gainL = voice.targetGainL;
        voice.gainR = voice.targetGainR;
        if (voice.stopping && voice.env <= 0.0f) {
            voice.active = false;
        }
        loudestVoice = std::max(loudestVoice, voicePeak);
    }

    mOut.resize(mMix.size());
    const float knee = std::min(loudestVoice, 1.0f);
    for (size_t i = 0; i < mMix.size(); ++i) {
        mOut[i] = static_cast<int16_t>(SoftLimit(mMix[i], knee) * kInt16Peak);
    }
}

}  // namespace a11y::audio

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace synthmath {

inline constexpr int levelControlChange = 70;
inline constexpr int cutoffControlChange = 71;
inline constexpr int resonanceControlChange = 72;
inline constexpr int attackControlChange = 73;
inline constexpr int decayControlChange = 74;
inline constexpr int sustainControlChange = 75;
inline constexpr int releaseControlChange = 76;
inline constexpr int lfoDepthControlChange = 77;

inline double midiNoteToFrequency(int note) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

inline float controlChangeToLevel(int value) {
    return static_cast<float>(value) / 127.0F;
}

// Maps a CC value onto the normalized 0..1 range of a parameter, so a skewed parameter follows its skew.
inline float controlChangeToNormalized(int value) {
    return static_cast<float>(value) / 127.0F;
}

inline float velocityToAmplitude(int velocity) {
    return static_cast<float>(velocity) / 127.0F;
}

enum class Waveform { sine, saw, square, noise };

// Noise peaks louder than the other shapes at the same level, so it is scaled down.
inline constexpr float noiseGain = 0.5F;

inline float sineSample(double phase) {
    return static_cast<float>(std::sin(2.0 * std::numbers::pi * phase));
}

inline float sawSample(double phase) {
    return static_cast<float>(2.0 * phase - 1.0);
}

inline float squareSample(double phase) {
    return phase < 0.5 ? 1.0F : -1.0F;
}

// xorshift32: the state must not be zero.
inline float noiseSample(std::uint32_t &state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(static_cast<double>(state) * (2.0 / 4294967295.0) - 1.0);
}

inline float waveSample(Waveform waveform, double phase, std::uint32_t &noiseState) {
    switch (waveform) {
    case Waveform::sine:
        return sineSample(phase);
    case Waveform::saw:
        return sawSample(phase);
    case Waveform::square:
        return squareSample(phase);
    case Waveform::noise:
        return noiseSample(noiseState) * noiseGain;
    }
    return 0.0F;
}

// Low-pass filter: the topology-preserving-transform (zero-delay feedback) state-variable filter.
// Chosen over a 4-pole ladder because it is 2-pole, cheap, has an exact tan() cutoff and stays stable
// for any cutoff below Nyquist and any positive damping, which a Chamberlin SVF does not near Nyquist.
inline constexpr float minCutoffHz = 20.0F;
inline constexpr float maxCutoffHz = 20000.0F;
inline constexpr double cutoffNyquistRatio = 0.45;
// Damping k = 1 / Q. Resonance 0 is critically damped (Q 0.5); resonance 1 stops at Q 20, short of
// k = 0 where the filter would self-oscillate.
inline constexpr double maxDamping = 2.0;
inline constexpr double minDamping = 0.05;

// Normalized 0..1 -> 20..20000 Hz on a log scale, clamped to 0.45 * sampleRate.
inline float cutoffParamToHz(float norm, double sampleRate) {
    const double clamped = std::clamp(static_cast<double>(norm), 0.0, 1.0);
    const double hz = minCutoffHz * std::pow(static_cast<double>(maxCutoffHz / minCutoffHz), clamped);
    return static_cast<float>(std::min(hz, cutoffNyquistRatio * sampleRate));
}

struct FilterState {
    float ic1eq = 0.0F;
    float ic2eq = 0.0F;
};

struct FilterCoefficients {
    float a1 = 0.0F;
    float a2 = 0.0F;
    float a3 = 0.0F;
};

// The cutoff is clamped to 0.45 * sampleRate here, so any input is safe. Resonance is clamped to 0..1.
inline FilterCoefficients filterCoefficients(float cutoffHz, float resonance, double sampleRate) {
    const double cutoff = std::clamp(static_cast<double>(cutoffHz), 1.0, cutoffNyquistRatio * sampleRate);
    const double res = std::clamp(static_cast<double>(resonance), 0.0, 1.0);
    const double g = std::tan(std::numbers::pi * cutoff / sampleRate);
    const double k = maxDamping + (minDamping - maxDamping) * res;
    const double a1 = 1.0 / (1.0 + g * (g + k));
    const double a2 = g * a1;
    return {static_cast<float>(a1), static_cast<float>(a2), static_cast<float>(g * a2)};
}

inline float filterTick(FilterState &state, const FilterCoefficients &c, float input) {
    const float v3 = input - state.ic2eq;
    const float v1 = c.a1 * state.ic1eq + c.a2 * v3;
    const float v2 = state.ic2eq + c.a2 * state.ic1eq + c.a3 * v3;
    state.ic1eq = 2.0F * v1 - state.ic1eq;
    state.ic2eq = 2.0F * v2 - state.ic2eq;
    return v2;
}

// LFO on the cutoff. It moves the cutoff in octaves, so the sweep sounds the same at any cutoff;
// a fixed Hz offset would swamp a low cutoff and vanish at a high one.
enum class LfoShape { sine, saw, square };

inline constexpr double lfoMinRateHz = 0.05;
inline constexpr double lfoMaxRateHz = 20.0;
// Full depth moves the cutoff by plus or minus this many octaves.
inline constexpr double lfoMaxOctaves = 2.0;

inline double lfoPhaseIncrement(double rateHz, double sampleRate) {
    return rateHz / sampleRate;
}

// Reuses the oscillator shapes, so the maths exists once. The result is in [-1, 1].
inline float lfoValue(LfoShape shape, double phase) {
    switch (shape) {
    case LfoShape::sine:
        return sineSample(phase);
    case LfoShape::saw:
        return sawSample(phase);
    case LfoShape::square:
        return squareSample(phase);
    }
    return 0.0F;
}

inline double applyOctaveOffset(double baseHz, double octaves) {
    return baseHz * std::pow(2.0, octaves);
}

// Offsets the cutoff by lfoValue * depth * lfoMaxOctaves, then clamps to 0.45 * sampleRate.
inline float modulatedCutoffHz(float baseHz, float lfoValue, float depth, double sampleRate) {
    const double octaves = static_cast<double>(lfoValue) * static_cast<double>(depth) * lfoMaxOctaves;
    const double hz = applyOctaveOffset(static_cast<double>(baseHz), octaves);
    return static_cast<float>(std::min(hz, cutoffNyquistRatio * sampleRate));
}

// Amplitude envelope. Each timed segment is a one-pole move towards a target placed beyond its end
// value, which gives the exponential shape and still arrives in finite time. The time is the time
// from the full distance (1 -> sustain, 1 -> 0); a segment that starts nearer its end takes less.
enum class AdsrStage { idle, attack, decay, sustain, release };

struct AdsrState {
    AdsrStage stage = AdsrStage::idle;
    float level = 0.0F;
};

struct AdsrParams {
    float attackSeconds = 0.01F;
    float decaySeconds = 0.1F;
    float sustain = 0.5F;
    float releaseSeconds = 0.2F;
};

inline constexpr double adsrOvershoot = 0.3;
// The sustain level follows its parameter with this time constant, so turning the knob does not click.
inline constexpr double adsrSustainSeconds = 0.005;

// Retriggering keeps the current level, so a still-ringing note never restarts from zero.
inline void adsrGateOn(AdsrState &state) {
    state.stage = AdsrStage::attack;
}

inline void adsrGateOff(AdsrState &state) {
    if (state.stage != AdsrStage::idle) {
        state.stage = AdsrStage::release;
    }
}

// Per-sample coefficient of a segment whose remaining distance to the target shrinks by `ratio`
// over `seconds`.
inline float adsrCoefficient(double ratio, double seconds, double sampleRate) {
    const double samples = std::max(seconds * sampleRate, 1.0);
    return static_cast<float>(1.0 - std::pow(ratio, 1.0 / samples));
}

inline float adsrTick(AdsrState &state, const AdsrParams &params, double sampleRate) {
    const auto o = adsrOvershoot;
    const auto sustain = std::clamp(params.sustain, 0.0F, 1.0F);
    switch (state.stage) {
    case AdsrStage::idle:
        state.level = 0.0F;
        break;
    case AdsrStage::attack:
        state.level += (static_cast<float>(1.0 + o) - state.level) *
                       adsrCoefficient(o / (1.0 + o), params.attackSeconds, sampleRate);
        if (state.level >= 1.0F) {
            state.level = 1.0F;
            state.stage = AdsrStage::decay;
        }
        break;
    case AdsrStage::decay:
        if (state.level <= sustain) {
            state.stage = AdsrStage::sustain;
        } else {
            state.level += (sustain - static_cast<float>(o) - state.level) *
                           adsrCoefficient(o / (1.0 - sustain + o), params.decaySeconds, sampleRate);
            if (state.level <= sustain) {
                state.level = sustain;
                state.stage = AdsrStage::sustain;
            }
        }
        break;
    case AdsrStage::sustain:
        state.level += (sustain - state.level) * adsrCoefficient(0.01, adsrSustainSeconds, sampleRate);
        break;
    case AdsrStage::release:
        state.level +=
            (-static_cast<float>(o) - state.level) * adsrCoefficient(o / (1.0 + o), params.releaseSeconds, sampleRate);
        if (state.level <= 0.0F) {
            state.level = 0.0F;
            state.stage = AdsrStage::idle;
        }
        break;
    }
    state.level = std::clamp(state.level, 0.0F, 1.0F);
    return state.level;
}

} // namespace synthmath

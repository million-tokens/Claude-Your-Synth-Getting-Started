#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace synthmath {

inline constexpr int levelControlChange = 70;
inline constexpr int cutoffControlChange = 71;
inline constexpr int resonanceControlChange = 72;

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

} // namespace synthmath

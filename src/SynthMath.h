#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>

namespace synthmath {

inline constexpr int levelControlChange = 70;

inline double midiNoteToFrequency(int note) {
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

inline float controlChangeToLevel(int value) {
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

} // namespace synthmath

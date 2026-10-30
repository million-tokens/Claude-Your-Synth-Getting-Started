#pragma once

#include <cmath>

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

} // namespace synthmath

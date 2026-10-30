#include "SynthMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

using Catch::Approx;

TEST_CASE("MIDI note to frequency") {
    CHECK(synthmath::midiNoteToFrequency(69) == Approx(440.0));
    CHECK(synthmath::midiNoteToFrequency(81) == Approx(880.0));
}

TEST_CASE("MIDI CC to level") {
    CHECK(synthmath::controlChangeToLevel(0) == Approx(0.0F));
    CHECK(synthmath::controlChangeToLevel(127) == Approx(1.0F));
    CHECK(synthmath::controlChangeToLevel(64) == Approx(0.5F).margin(0.01));
}

TEST_CASE("Velocity to amplitude") {
    CHECK(synthmath::velocityToAmplitude(127) == Approx(1.0F));
    CHECK(synthmath::velocityToAmplitude(100) > synthmath::velocityToAmplitude(50));
}

TEST_CASE("Waveform shapes") {
    CHECK(synthmath::sawSample(0.0) == -1.0F);
    CHECK(synthmath::sawSample(0.5) == 0.0F);
    CHECK(synthmath::squareSample(0.25) == 1.0F);
    CHECK(synthmath::squareSample(0.75) == -1.0F);
    CHECK(synthmath::sineSample(0.25) == Approx(1.0F).margin(1e-6));
}

TEST_CASE("Every waveform stays bounded over a cycle") {
    using synthmath::Waveform;
    for (const auto waveform : {Waveform::sine, Waveform::saw, Waveform::square, Waveform::noise}) {
        std::uint32_t noiseState = 1;
        for (int i = 0; i < 1000; ++i) {
            const auto sample = synthmath::waveSample(waveform, i / 1000.0, noiseState);
            CHECK(!std::isnan(sample));
            CHECK(sample >= -1.05F);
            CHECK(sample <= 1.05F);
        }
    }
}

TEST_CASE("Noise is bounded, centred and repeatable") {
    constexpr int count = 10000;
    const auto generate = [] {
        std::uint32_t state = 12345;
        std::vector<float> samples;
        for (int i = 0; i < count; ++i) {
            samples.push_back(synthmath::noiseSample(state));
        }
        return samples;
    };
    const auto first = generate();
    double sum = 0.0;
    for (const auto sample : first) {
        CHECK(sample >= -1.0F);
        CHECK(sample <= 1.0F);
        sum += sample;
    }
    CHECK(std::abs(sum / count) < 0.05);
    CHECK(first == generate());
}

TEST_CASE("Cutoff parameter maps logarithmically and clamps") {
    CHECK(synthmath::cutoffParamToHz(0.0F, 48000.0) == Approx(20.0F).margin(1.0));
    CHECK(synthmath::cutoffParamToHz(1.0F, 48000.0) == Approx(20000.0F).margin(1.0));
    CHECK(synthmath::cutoffParamToHz(0.5F, 48000.0) == Approx(std::sqrt(20.0F * 20000.0F)).margin(1.0));
    CHECK(synthmath::cutoffParamToHz(1.0F, 32000.0) == Approx(0.45F * 32000.0F));
    float previous = 0.0F;
    for (int i = 0; i <= 100; ++i) {
        const auto hz = synthmath::cutoffParamToHz(static_cast<float>(i) / 100.0F, 48000.0);
        CHECK(hz > previous);
        previous = hz;
    }
}

TEST_CASE("Filter coefficients clamp the cutoff to 0.45 * sampleRate") {
    const auto high = synthmath::filterCoefficients(1.0e9F, 0.5F, 32000.0);
    const auto limit = synthmath::filterCoefficients(0.45F * 32000.0F, 0.5F, 32000.0);
    CHECK(high.a1 == Approx(limit.a1));
    CHECK(high.a2 == Approx(limit.a2));
    CHECK(std::isfinite(synthmath::filterCoefficients(-5.0F, 7.0F, 44100.0).a3));
}

namespace {
double harmonicMagnitude(const std::vector<float> &signal, std::size_t begin, double frequency, double sampleRate) {
    double re = 0.0;
    double im = 0.0;
    for (std::size_t i = begin; i < signal.size(); ++i) {
        const double angle = 2.0 * std::numbers::pi * frequency * static_cast<double>(i) / sampleRate;
        re += signal[i] * std::cos(angle);
        im += signal[i] * std::sin(angle);
    }
    return std::sqrt(re * re + im * im);
}

std::vector<float> filteredSaw(float cutoffHz, float resonance, double sampleRate) {
    const auto coefficients = synthmath::filterCoefficients(cutoffHz, resonance, sampleRate);
    synthmath::FilterState state;
    std::vector<float> out;
    double phase = 0.0;
    for (int i = 0; i < static_cast<int>(sampleRate); ++i) {
        out.push_back(synthmath::filterTick(state, coefficients, synthmath::sawSample(phase)));
        phase += 110.0 / sampleRate;
        phase -= std::floor(phase);
    }
    return out;
}
} // namespace

TEST_CASE("Low-pass filter removes high harmonics and keeps low ones") {
    constexpr double sampleRate = 48000.0;
    const auto closed = filteredSaw(500.0F, 0.0F, sampleRate);
    const auto open = filteredSaw(20000.0F, 0.0F, sampleRate);
    const std::size_t begin = closed.size() / 2;
    const auto db = [&](const std::vector<float> &signal, double frequency) {
        return 20.0 * std::log10(harmonicMagnitude(signal, begin, frequency, sampleRate));
    };
    CHECK(db(open, 1760.0) - db(closed, 1760.0) >= 18.0);
    CHECK(std::abs(db(open, 220.0) - db(closed, 220.0)) <= 5.0);
}

TEST_CASE("Filter stays finite and bounded on noise") {
    for (const float cutoff : {20.0F, 500.0F, 5000.0F, 21000.0F}) {
        for (const float resonance : {0.0F, 0.5F, 0.9F, 1.0F}) {
            for (const double sampleRate : {44100.0, 48000.0, 96000.0}) {
                const auto coefficients = synthmath::filterCoefficients(cutoff, resonance, sampleRate);
                synthmath::FilterState state;
                std::uint32_t noiseState = 1;
                for (int i = 0; i < 20000; ++i) {
                    const auto out = synthmath::filterTick(state, coefficients, synthmath::noiseSample(noiseState));
                    REQUIRE(std::isfinite(out));
                    REQUIRE(std::abs(out) < 100.0F);
                }
            }
        }
    }
}

namespace {
constexpr double adsrRate = 48000.0;

float tickFor(synthmath::AdsrState &state, const synthmath::AdsrParams &params, double seconds) {
    float level = state.level;
    for (int i = 0; i < static_cast<int>(seconds * adsrRate); ++i) {
        level = synthmath::adsrTick(state, params, adsrRate);
    }
    return level;
}
} // namespace

TEST_CASE("ADSR follows attack, decay, sustain and release") {
    const synthmath::AdsrParams params{0.01F, 0.1F, 0.5F, 0.2F};
    synthmath::AdsrState state;
    synthmath::adsrGateOn(state);
    CHECK(tickFor(state, params, 0.01) == Approx(1.0F).margin(0.02));
    CHECK(tickFor(state, params, 0.1) == Approx(0.5F).margin(0.02));
    for (int i = 0; i < 48000; ++i) {
        CHECK(synthmath::adsrTick(state, params, adsrRate) == Approx(0.5F).margin(0.001));
    }
    synthmath::adsrGateOff(state);
    CHECK(tickFor(state, params, 0.2) < 0.01F);
}

TEST_CASE("ADSR stays within 0..1 over a grid of parameters") {
    for (const float attack : {0.001F, 0.05F, 5.0F}) {
        for (const float decay : {0.001F, 0.05F, 5.0F}) {
            for (const float sustain : {0.0F, 0.3F, 1.0F}) {
                for (const float release : {0.001F, 0.05F, 10.0F}) {
                    const synthmath::AdsrParams params{attack, decay, sustain, release};
                    synthmath::AdsrState state;
                    synthmath::adsrGateOn(state);
                    for (int i = 0; i < 48000; ++i) {
                        const float level = synthmath::adsrTick(state, params, adsrRate);
                        REQUIRE(level >= 0.0F);
                        REQUIRE(level <= 1.0F);
                    }
                    synthmath::adsrGateOff(state);
                    for (int i = 0; i < 48000; ++i) {
                        const float level = synthmath::adsrTick(state, params, adsrRate);
                        REQUIRE(level >= 0.0F);
                        REQUIRE(level <= 1.0F);
                    }
                }
            }
        }
    }
}

TEST_CASE("ADSR retrigger restarts the attack from the current level") {
    const synthmath::AdsrParams params{0.01F, 0.1F, 0.5F, 0.2F};
    synthmath::AdsrState state;
    synthmath::adsrGateOn(state);
    while (state.level < 0.5F) {
        synthmath::adsrTick(state, params, adsrRate);
    }
    const float before = state.level;
    synthmath::adsrGateOn(state);
    CHECK(synthmath::adsrTick(state, params, adsrRate) > before);
}

TEST_CASE("LFO phase increment completes a cycle at the given rate") {
    CHECK(synthmath::lfoPhaseIncrement(2.0, 48000.0) * 24000.0 == Approx(1.0).margin(1e-9));
}

TEST_CASE("LFO shapes reuse the waveform functions") {
    using synthmath::LfoShape;
    for (const double phase : {0.0, 0.25, 0.5, 0.75}) {
        CHECK(synthmath::lfoValue(LfoShape::sine, phase) == synthmath::sineSample(phase));
        CHECK(synthmath::lfoValue(LfoShape::saw, phase) == synthmath::sawSample(phase));
        CHECK(synthmath::lfoValue(LfoShape::square, phase) == synthmath::squareSample(phase));
    }
}

TEST_CASE("Octave offset scales the frequency by powers of two") {
    CHECK(synthmath::applyOctaveOffset(440.0, 1.0) == Approx(880.0).margin(1e-9));
    CHECK(synthmath::applyOctaveOffset(440.0, -1.0) == Approx(220.0).margin(1e-9));
    CHECK(synthmath::applyOctaveOffset(440.0, 0.0) == Approx(440.0).margin(1e-9));
}

TEST_CASE("Modulated cutoff: depth 0 changes nothing, full depth stays under the clamp") {
    constexpr double sampleRate = 48000.0;
    for (const float base : {20.0F, 500.0F, 5000.0F, 20000.0F}) {
        for (const float lfo : {-1.0F, -0.5F, 0.0F, 0.5F, 1.0F}) {
            if (base < 0.45 * sampleRate) {
                CHECK(synthmath::modulatedCutoffHz(base, lfo, 0.0F, sampleRate) == base);
            }
            CHECK(synthmath::modulatedCutoffHz(base, lfo, 1.0F, sampleRate) <= 0.45 * sampleRate);
        }
    }
    CHECK(synthmath::modulatedCutoffHz(1000.0F, 1.0F, 1.0F, sampleRate) == Approx(4000.0F));
    CHECK(synthmath::modulatedCutoffHz(1000.0F, -1.0F, 1.0F, sampleRate) == Approx(250.0F));
}

TEST_CASE("Filter stays finite and bounded with the cutoff moved every sample by the LFO") {
    using synthmath::LfoShape;
    for (const auto shape : {LfoShape::sine, LfoShape::saw, LfoShape::square}) {
        for (const float cutoff : {20.0F, 500.0F, 5000.0F, 21000.0F}) {
            for (const float resonance : {0.0F, 0.5F, 0.9F, 1.0F}) {
                for (const double sampleRate : {44100.0, 48000.0, 96000.0}) {
                    synthmath::FilterState state;
                    std::uint32_t noiseState = 1;
                    double phase = 0.0;
                    const double increment = synthmath::lfoPhaseIncrement(20.0, sampleRate);
                    for (int i = 0; i < 20000; ++i) {
                        const auto hz =
                            synthmath::modulatedCutoffHz(cutoff, synthmath::lfoValue(shape, phase), 1.0F, sampleRate);
                        const auto coefficients = synthmath::filterCoefficients(hz, resonance, sampleRate);
                        const auto out = synthmath::filterTick(state, coefficients, synthmath::noiseSample(noiseState));
                        REQUIRE(std::isfinite(out));
                        REQUIRE(std::abs(out) < 100.0F);
                        phase += increment;
                        phase -= std::floor(phase);
                    }
                }
            }
        }
    }
}

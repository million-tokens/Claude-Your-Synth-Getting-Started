#include "SynthMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
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

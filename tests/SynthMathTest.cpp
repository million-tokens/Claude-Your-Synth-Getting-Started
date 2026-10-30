#include "SynthMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

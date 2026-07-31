#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Product name is MiniSynth", "[smoke]") {
    REQUIRE(std::string(MINISYNTH_PRODUCT_NAME) == "MiniSynth");
}

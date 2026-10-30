include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

add_executable(MiniSynthTests
    ProductNameTest.cpp
    SynthMathTest.cpp
)

target_include_directories(MiniSynthTests PRIVATE ${CMAKE_SOURCE_DIR}/src)

get_target_property(MINISYNTH_PRODUCT_NAME MiniSynth JUCE_PRODUCT_NAME)

target_compile_definitions(MiniSynthTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        MINISYNTH_PRODUCT_NAME="${MINISYNTH_PRODUCT_NAME}"
)

target_link_libraries(MiniSynthTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)
catch_discover_tests(MiniSynthTests)

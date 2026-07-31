cmake_minimum_required(VERSION 3.22)
project(MiniSynth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_console_app(MiniSynth
    PRODUCT_NAME "MiniSynth"
)

target_sources(MiniSynth PRIVATE
    src/Main.cpp
)

target_compile_definitions(MiniSynth
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(MiniSynth
    PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

enable_testing()
add_subdirectory(tests)

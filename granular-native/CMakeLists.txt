cmake_minimum_required(VERSION 3.22)

project(GRANULAR VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "macOS deployment target")
if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64"
        CACHE STRING "macOS universal architectures")
endif()

# ----------------------------------------------------------------------------
# JUCE (fetched via CMake; same pinned tag as ds4-native)
# ----------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
# Formats: on iOS build Standalone + AUv3 (for AUM); on desktop the usual set.
# ----------------------------------------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(GRANULAR_FORMATS Standalone AUv3)
else()
    set(GRANULAR_FORMATS Standalone AU VST3)
endif()

juce_add_plugin(GRANULAR
    COMPANY_NAME            "scire-h"
    COMPANY_WEBSITE         "https://github.com/scire-h/scire-h"
    PLUGIN_MANUFACTURER_CODE Scir
    PLUGIN_CODE             Grnl
    FORMATS                 ${GRANULAR_FORMATS}
    PRODUCT_NAME            "GRANULAR"
    PLUGIN_NAME             "GRANULAR"
    BUNDLE_ID               com.scireh.granular
    DESCRIPTION             "5-track granular synthesiser (native port of granular.html)"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    HARDENED_RUNTIME_ENABLED TRUE
    REQUIRES_FULL_SCREEN    FALSE
)

juce_generate_juce_header(GRANULAR)

target_sources(GRANULAR PRIVATE
    Source/PluginProcessor.cpp
)

target_compile_definitions(GRANULAR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(GRANULAR
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

cmake_minimum_required(VERSION 3.22)

project(DS4M VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS deployment target")
# Build universal binaries on Apple Silicon out of the box.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64"
        CACHE STRING "macOS universal architectures")
endif()

# ----------------------------------------------------------------------------
# JUCE (fetched via CMake; pin to a tested release tag)
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
# Plug-in target
# ----------------------------------------------------------------------------
juce_add_plugin(DS4M
    COMPANY_NAME            "ULT-SOUND Clone"
    COMPANY_WEBSITE         "https://github.com/scire-h/scire-h"
    PLUGIN_MANUFACTURER_CODE Ults
    PLUGIN_CODE             Ds4m
    FORMATS                 Standalone AU VST3
    PRODUCT_NAME            "ULT-SOUND DS-4M"
    PLUGIN_NAME             "DS-4M"
    BUNDLE_ID               com.ultsound.ds4m
    DESCRIPTION             "Component-level model of the ULT-SOUND DS-4M drum synth"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    HARDENED_RUNTIME_ENABLED TRUE
)

juce_generate_juce_header(DS4M)

target_sources(DS4M PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Voice.cpp
    Source/DriftedVCO.cpp
    Source/NoiseVoice.cpp
    Source/ExpEnvelope.cpp
    Source/PanelLookAndFeel.cpp
    Source/ChannelStripComponent.cpp
    Source/MasterStripComponent.cpp
)

target_compile_definitions(DS4M PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(DS4M
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_plugin_client
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

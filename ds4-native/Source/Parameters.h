#pragma once
#include <juce_core/juce_core.h>

/* Central place for parameter IDs / channel labels / value tables.
   All UI binding and processor reading go through these constants so a
   typo can't desynchronise the editor from the DSP. */
namespace P {

constexpr int kNumChannels = 4;

inline juce::String chPrefix(int chIdx) {
    return "ch" + juce::String(chIdx + 1) + "_";
}

/* The CYMBAL / CYMBAL / SNARE / NOISE silk-screen pills on the original
   panel. They drive the noise voice characteristic for each channel. */
constexpr const char* kChannelLabels[kNumChannels] = {
    "CYMBAL", "CYMBAL", "SNARE", "NOISE"
};

/* Parameter ID stems (the channel prefix is prepended at runtime). */
namespace pid {
    constexpr const char* vco         = "vco";          // VCO tuning 0..1
    constexpr const char* beatTune    = "beat_tune";    // 0..1 -> 0..200 cents
    constexpr const char* octave      = "octave";       // 1..5
    constexpr const char* attack      = "attack";       // 0..1
    constexpr const char* sustain     = "sustain";      // 0..1
    constexpr const char* waveform    = "waveform";     // 0:sine 1:tri 2:sqr 3:saw
    constexpr const char* noiseOn     = "noise_on";     // bool
    constexpr const char* lfoRate     = "lfo_rate";     // 0..1
    constexpr const char* lfoDepth    = "lfo_depth";    // 0..1
    constexpr const char* lfoOn       = "lfo_on";       // bool
    constexpr const char* sweep       = "sweep";        // 0..1 (width)
    constexpr const char* sweepDir    = "sweep_dir";    // 0:up 1:off 2:down
    constexpr const char* output      = "output";       // 0..1
    constexpr const char* sense       = "sense";        // 0..1 (sense level)
    constexpr const char* multiVCO    = "multi_vco";    // bool
}

namespace gid {
    constexpr const char* masterVolume = "master_volume";
    constexpr const char* beatSense    = "beat_sense";
}

}  // namespace P

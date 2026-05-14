#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

/* Factory presets for the DS-4M plug-in.

   Each preset is a flat list of (paramID -> value) entries that
   `Presets::apply` writes into the APVTS. The APVTS broadcasts changes
   to the editor so the UI reflects the loaded patch immediately. */

namespace Presets {

struct Entry {
    juce::String id;
    float        value;
};

struct Preset {
    juce::String name;
    std::vector<Entry> entries;
};

const std::vector<Preset>& factoryPresets();

void apply (juce::AudioProcessorValueTreeState& apvts, int index);

}  // namespace Presets

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "PluginProcessor.h"
#include "ChannelStripComponent.h"
#include "MasterStripComponent.h"
#include "PanelLookAndFeel.h"

class DS4MEditor : public juce::AudioProcessorEditor {
public:
    explicit DS4MEditor (DS4MProcessor&);
    ~DS4MEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    DS4MProcessor& processor;
    PanelLookAndFeel laf;

    std::array<std::unique_ptr<ChannelStripComponent>, P::kNumChannels> strips;
    std::unique_ptr<MasterStripComponent> master;

    /* Top-bar preset selector. */
    juce::ComboBox  presetBox;
    juce::TextButton helpButton { "?" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DS4MEditor)
};

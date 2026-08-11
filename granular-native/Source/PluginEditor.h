#pragma once
// v0.1 editor: the generic parameter panel wrapped with a 1-bit header.
// The full custom UI (pattern grid, knobs, strips in the web app's 1-bit
// Macintosh style) is a later phase — see docs/PORT_PLAN.md.

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class GranularAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit GranularAudioProcessorEditor (GranularAudioProcessor& p)
        : AudioProcessorEditor (&p), generic (p)
    {
        addAndMakeVisible (generic);
        setResizable (true, true);
        setSize (560, 640);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::white);
        g.setColour (juce::Colours::black);
        g.fillRect (getLocalBounds().removeFromTop (28));
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (16.0f, juce::Font::bold)));
        g.drawText ("GRANULAR  ·  5-track granulator (v0.1 shell)",
                    getLocalBounds().removeFromTop (28), juce::Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (28);
        generic.setBounds (r);
    }

private:
    juce::GenericAudioProcessorEditor generic;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GranularAudioProcessorEditor)
};

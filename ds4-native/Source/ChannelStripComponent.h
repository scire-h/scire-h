#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Parameters.h"

class PanelLookAndFeel;
class DS4MProcessor;

/* One channel strip -- VCO / ATTACK / LFO / OUTPUT sections plus the
   sense / multi-vco / noise toggles at the left. */
class ChannelStripComponent : public juce::Component {
public:
    ChannelStripComponent (DS4MProcessor& proc, int channelIndex,
                           PanelLookAndFeel& laf);
    ~ChannelStripComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /* Fired by the editor when 1..4 keys are pressed -- runs the
       pad button's onClick action and the button's visual flash. */
    void triggerPadButton();

private:
    DS4MProcessor& processor;
    int            channelIndex;
    PanelLookAndFeel& laf;

    using SAtt   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt   = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ChAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    juce::Slider   vcoKnob,    beatTuneSlider, attackKnob, sustainSlider;
    juce::Slider   lfoLevelKnob, lfoRateSlider;
    juce::Slider   outputKnob, sweepSlider;
    juce::Slider   senseKnob;
    juce::ComboBox waveformBox, sweepDirBox, octaveBox;
    juce::ToggleButton noiseToggle, lfoToggle, multiToggle;
    juce::TextButton padButton;

    std::vector<std::unique_ptr<SAtt>>  sAtts;
    std::vector<std::unique_ptr<BAtt>>  bAtts;
    std::vector<std::unique_ptr<ChAtt>> cAtts;
};

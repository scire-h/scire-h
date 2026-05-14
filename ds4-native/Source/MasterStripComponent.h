#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Parameters.h"

class PanelLookAndFeel;
class DS4MProcessor;

class MasterStripComponent : public juce::Component {
public:
    MasterStripComponent (DS4MProcessor&, PanelLookAndFeel&);
    void paint   (juce::Graphics&) override;
    void resized () override;
private:
    DS4MProcessor& processor;
    PanelLookAndFeel& laf;

    juce::Slider       masterVolumeSlider;
    juce::ToggleButton beatSenseToggle;

    using SAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SAtt> sAtt;
    std::unique_ptr<BAtt> bAtt;
};

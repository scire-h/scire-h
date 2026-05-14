#include "MasterStripComponent.h"
#include "PanelLookAndFeel.h"
#include "PluginProcessor.h"

MasterStripComponent::MasterStripComponent (DS4MProcessor& proc,
                                            PanelLookAndFeel& lookFeel)
    : processor (proc), laf (lookFeel)
{
    masterVolumeSlider.setSliderStyle (juce::Slider::LinearVertical);
    masterVolumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    masterVolumeSlider.setRange (0.0, 1.0, 0.0);
    addAndMakeVisible (masterVolumeSlider);

    beatSenseToggle.setButtonText ("BEAT SENSE");
    addAndMakeVisible (beatSenseToggle);

    sAtt = std::make_unique<SAtt> (processor.apvts, P::gid::masterVolume, masterVolumeSlider);
    bAtt = std::make_unique<BAtt> (processor.apvts, P::gid::beatSense,    beatSenseToggle);
}

void MasterStripComponent::paint (juce::Graphics& g) {
    auto r = getLocalBounds();
    PanelLookAndFeel::paintColouredStrip (g, r, PanelLookAndFeel::kCyan);

    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (juce::FontOptions ("Helvetica Neue", 14.0f, juce::Font::bold)));
    g.drawText ("ULT'SOUND", r.removeFromTop (20), juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions ("Helvetica Neue", 16.0f, juce::Font::bold)));
    g.drawText ("DS-4M",     r.removeFromTop (22), juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions ("Helvetica Neue", 9.0f,  juce::Font::bold)));
    PanelLookAndFeel::paintPill (g, r.removeFromTop (18).reduced (10, 0), "CUSTOM");
}

void MasterStripComponent::resized () {
    auto r = getLocalBounds().reduced (10, 80);
    masterVolumeSlider.setBounds (r.removeFromTop (160).reduced (20, 0));
    beatSenseToggle.setBounds   (r.removeFromBottom (30));
}

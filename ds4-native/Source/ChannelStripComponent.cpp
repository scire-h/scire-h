#include "ChannelStripComponent.h"
#include "PanelLookAndFeel.h"
#include "PluginProcessor.h"

namespace {
    void styleRotary (juce::Slider& s) {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (0.0, 1.0, 0.0);
    }
    void styleVertical (juce::Slider& s) {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (0.0, 1.0, 0.0);
    }
}

ChannelStripComponent::ChannelStripComponent (DS4MProcessor& proc, int chIdx,
                                              PanelLookAndFeel& lookFeel)
    : processor (proc), channelIndex (chIdx), laf (lookFeel)
{
    auto& apvts = processor.apvts;
    const auto p = P::chPrefix (chIdx);

    /* Knobs. */
    styleRotary (vcoKnob);       addAndMakeVisible (vcoKnob);
    styleRotary (attackKnob);    addAndMakeVisible (attackKnob);
    styleRotary (lfoLevelKnob);  addAndMakeVisible (lfoLevelKnob);
    styleRotary (outputKnob);    addAndMakeVisible (outputKnob);
    styleRotary (senseKnob);     addAndMakeVisible (senseKnob);

    /* Sliders. */
    styleVertical (beatTuneSlider); addAndMakeVisible (beatTuneSlider);
    styleVertical (sustainSlider);  addAndMakeVisible (sustainSlider);
    styleVertical (lfoRateSlider);  addAndMakeVisible (lfoRateSlider);
    styleVertical (sweepSlider);    addAndMakeVisible (sweepSlider);

    /* Combo boxes. */
    waveformBox.addItem ("∿  sine",     1);
    waveformBox.addItem ("△  triangle", 2);
    waveformBox.addItem ("⊓  square",   3);
    waveformBox.addItem ("⋀  saw",       4);
    addAndMakeVisible (waveformBox);

    sweepDirBox.addItem ("UP",   1);
    sweepDirBox.addItem ("OFF",  2);
    sweepDirBox.addItem ("DOWN", 3);
    addAndMakeVisible (sweepDirBox);

    for (int o = 1; o <= 5; ++o) octaveBox.addItem ("Oct " + juce::String (o), o);
    addAndMakeVisible (octaveBox);

    /* Toggles. */
    noiseToggle.setButtonText  ("NOISE");
    lfoToggle.setButtonText    ("LFO");
    multiToggle.setButtonText  ("MULTI");
    addAndMakeVisible (noiseToggle);
    addAndMakeVisible (lfoToggle);
    addAndMakeVisible (multiToggle);

    /* Pad button: triggers the channel when clicked. */
    padButton.setButtonText ("PAD " + juce::String (chIdx + 1));
    padButton.onClick = [this] { processor.triggerChannel (channelIndex, 0.9f); };
    addAndMakeVisible (padButton);

    /* Attachments -- one per parameter, owned by this component. */
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::vco,       vcoKnob));
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::beatTune,  beatTuneSlider));
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::attack,    attackKnob));
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::sustain,   sustainSlider));
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::lfoDepth,  lfoLevelKnob));
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::lfoRate,   lfoRateSlider));
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::output,    outputKnob));
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::sweep,     sweepSlider));
    sAtts.push_back (std::make_unique<SAtt> (apvts, p + P::pid::sense,     senseKnob));

    cAtts.push_back (std::make_unique<ChAtt> (apvts, p + P::pid::waveform,  waveformBox));
    cAtts.push_back (std::make_unique<ChAtt> (apvts, p + P::pid::sweepDir,  sweepDirBox));
    cAtts.push_back (std::make_unique<ChAtt> (apvts, p + P::pid::octave,    octaveBox));

    bAtts.push_back (std::make_unique<BAtt> (apvts, p + P::pid::noiseOn,   noiseToggle));
    bAtts.push_back (std::make_unique<BAtt> (apvts, p + P::pid::lfoOn,     lfoToggle));
    bAtts.push_back (std::make_unique<BAtt> (apvts, p + P::pid::multiVCO,  multiToggle));
}

ChannelStripComponent::~ChannelStripComponent() = default;

void ChannelStripComponent::paint (juce::Graphics& g) {
    auto r = getLocalBounds();
    /* Five painted regions: sense / VCO / ATTACK / LFO / OUTPUT. */
    const int senseW = 70;
    const int rest   = r.getWidth() - senseW;
    const int colW   = rest / 4;

    auto sense  = r.removeFromLeft (senseW);
    auto vco    = r.removeFromLeft (colW);
    auto attack = r.removeFromLeft (colW);
    auto lfo    = r.removeFromLeft (colW);
    auto outCol = r;

    PanelLookAndFeel::paintColouredStrip (g, sense,  PanelLookAndFeel::kCyan);
    PanelLookAndFeel::paintColouredStrip (g, vco,    PanelLookAndFeel::kCyan);
    PanelLookAndFeel::paintColouredStrip (g, attack, PanelLookAndFeel::kYellow);
    PanelLookAndFeel::paintColouredStrip (g, lfo,    PanelLookAndFeel::kYellow);
    PanelLookAndFeel::paintColouredStrip (g, outCol, PanelLookAndFeel::kYellow);

    PanelLookAndFeel::paintPill (g, vco   .reduced (8).removeFromTop (16), "VCO");
    PanelLookAndFeel::paintPill (g, attack.reduced (8).removeFromTop (16), "ATTACK");
    PanelLookAndFeel::paintPill (g, lfo   .reduced (8).removeFromTop (16), "LFO");
    PanelLookAndFeel::paintPill (g, outCol.reduced (8).removeFromTop (16), "OUTPUT");

    /* Channel pill at the bottom of the ATTACK column. */
    auto attackBottom = attack.reduced (8).removeFromBottom (20);
    PanelLookAndFeel::paintPill (g, attackBottom.removeFromTop (16),
                                 P::kChannelLabels[channelIndex]);

    /* Ch label in the SENSE column. */
    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (juce::FontOptions ("Helvetica Neue", 10.0f, juce::Font::bold)));
    g.drawText ("Ch" + juce::String (channelIndex + 1),
                sense.reduced (6).removeFromTop (14),
                juce::Justification::topLeft);
}

void ChannelStripComponent::resized() {
    auto r       = getLocalBounds();
    const int senseW = 70;
    const int rest   = r.getWidth() - senseW;
    const int colW   = rest / 4;

    auto sense  = r.removeFromLeft (senseW).reduced (6, 22);
    auto vco    = r.removeFromLeft (colW).reduced (6, 22);
    auto attack = r.removeFromLeft (colW).reduced (6, 22);
    auto lfo    = r.removeFromLeft (colW).reduced (6, 22);
    auto outCol = r.reduced (6, 22);

    /* SENSE column. */
    {
        auto col = sense;
        senseKnob.setBounds   (col.removeFromTop (54).reduced (4));
        multiToggle.setBounds (col.removeFromTop (22));
        noiseToggle.setBounds (col.removeFromTop (22));
        padButton.setBounds   (col.removeFromBottom (44).reduced (2));
    }
    /* VCO column. */
    {
        auto col = vco;
        vcoKnob.setBounds       (col.removeFromTop (60));
        col.removeFromTop (4);
        beatTuneSlider.setBounds (col.removeFromTop (90).reduced (col.getWidth()/2 - 10, 0));
        col.removeFromTop (4);
        octaveBox.setBounds      (col.removeFromTop (22));
    }
    /* ATTACK column. */
    {
        auto col = attack;
        attackKnob.setBounds    (col.removeFromTop (60));
        col.removeFromTop (4);
        sustainSlider.setBounds (col.removeFromTop (90).reduced (col.getWidth()/2 - 10, 0));
        col.removeFromBottom (22);    // leave room for channel pill
        waveformBox.setBounds   (col.removeFromTop (22));
    }
    /* LFO column. */
    {
        auto col = lfo;
        lfoLevelKnob.setBounds  (col.removeFromTop (60));
        col.removeFromTop (4);
        lfoRateSlider.setBounds (col.removeFromTop (90).reduced (col.getWidth()/2 - 10, 0));
        col.removeFromTop (4);
        lfoToggle.setBounds     (col.removeFromTop (22));
    }
    /* OUTPUT column. */
    {
        auto col = outCol;
        outputKnob.setBounds    (col.removeFromTop (60));
        col.removeFromTop (4);
        sweepSlider.setBounds   (col.removeFromTop (90).reduced (col.getWidth()/2 - 10, 0));
        col.removeFromTop (4);
        sweepDirBox.setBounds   (col.removeFromTop (22));
    }
}

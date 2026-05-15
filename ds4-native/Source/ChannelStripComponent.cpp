#include "ChannelStripComponent.h"
#include "PanelLookAndFeel.h"
#include "PluginProcessor.h"
#include <cmath>

namespace {
    /* Show a little value bubble while the user drags any control.
       Makes the otherwise-mute knobs and faders readable. */
    void enableValueBubble (juce::Slider& s, juce::Component* parent) {
        s.setPopupDisplayEnabled (true, true, parent);
    }

    void styleRotary (juce::Slider& s, juce::Component* parent) {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (0.0, 1.0, 0.0);
        enableValueBubble (s, parent);
    }
    void styleVertical (juce::Slider& s, juce::Component* parent) {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (0.0, 1.0, 0.0);
        enableValueBubble (s, parent);
    }

    /* ------- Human-readable value formatters for each control ------- */

    juce::String fmtNoteName (double v) {
        /* 0..1 maps to -12..+12 semitones from A3 (= MIDI 57). */
        const int totalSemis = (int) std::round ((v - 0.5) * 24.0);
        const int midi = 57 + totalSemis;
        static const char* names[] = { "C","C#","D","D#","E","F","F#",
                                       "G","G#","A","A#","B" };
        const int idx = ((midi % 12) + 12) % 12;
        const int oct = midi / 12 - 1;
        return juce::String (names[idx]) + juce::String (oct);
    }
    juce::String fmtBeatTune (double v) {
        return juce::String ((int) std::round (v * 200.0)) + " ct";
    }
    juce::String fmtAttack (double v) {
        const double ms = 1.0 + std::pow (v, 1.4) * 349.0;
        if (ms < 10.0)  return juce::String (ms, 2) + " ms";
        if (ms < 100.0) return juce::String (ms, 1) + " ms";
        return juce::String ((int) std::round (ms)) + " ms";
    }
    juce::String fmtSustain (double v) {
        const double sec = 0.03 + std::pow (v, 1.4) * 4.47;
        if (sec < 1.0) return juce::String ((int) std::round (sec * 1000.0)) + " ms";
        return juce::String (sec, 2) + " s";
    }
    juce::String fmtLfoRate (double v) {
        const double hz = 0.8 * std::pow (22.0 / 0.8, v);
        if (hz < 1.0)  return juce::String (hz, 2) + " Hz";
        if (hz < 10.0) return juce::String (hz, 1) + " Hz";
        return juce::String ((int) std::round (hz)) + " Hz";
    }
    juce::String fmtPercent (double v) {
        return juce::String ((int) std::round (v * 100.0)) + " %";
    }
    juce::String fmtSweep (double v) {
        return juce::String (v * 6.0, 1) + " oct";
    }

    void installFormatter (juce::Slider& s,
                           juce::String (*fn)(double)) {
        s.textFromValueFunction = fn;
        /* default valueFromTextFunction strips the suffix and parses, which is fine */
    }
}

ChannelStripComponent::ChannelStripComponent (DS4MProcessor& proc, int chIdx,
                                              PanelLookAndFeel& lookFeel)
    : processor (proc), channelIndex (chIdx), laf (lookFeel)
{
    auto& apvts = processor.apvts;
    const auto p = P::chPrefix (chIdx);

    /* Knobs. */
    styleRotary (vcoKnob,       this); addAndMakeVisible (vcoKnob);
    styleRotary (attackKnob,    this); addAndMakeVisible (attackKnob);
    styleRotary (lfoLevelKnob,  this); addAndMakeVisible (lfoLevelKnob);
    styleRotary (outputKnob,    this); addAndMakeVisible (outputKnob);
    styleRotary (senseKnob,     this); addAndMakeVisible (senseKnob);

    /* Sliders. */
    styleVertical (beatTuneSlider, this); addAndMakeVisible (beatTuneSlider);
    styleVertical (sustainSlider,  this); addAndMakeVisible (sustainSlider);
    styleVertical (lfoRateSlider,  this); addAndMakeVisible (lfoRateSlider);
    styleVertical (sweepSlider,    this); addAndMakeVisible (sweepSlider);

    /* Human-readable value bubbles. The first three are the ones the
       user singled out as confusing -- VCO TUNING shows note names
       (C2..A5), BEAT TUNE shows cents, SWEEP WIDTH shows octaves.
       The rest follow the same pattern for consistency. */
    installFormatter (vcoKnob,        fmtNoteName);
    installFormatter (beatTuneSlider, fmtBeatTune);
    installFormatter (attackKnob,     fmtAttack);
    installFormatter (sustainSlider,  fmtSustain);
    installFormatter (lfoLevelKnob,   fmtPercent);
    installFormatter (lfoRateSlider,  fmtLfoRate);
    installFormatter (outputKnob,     fmtPercent);
    installFormatter (sweepSlider,    fmtSweep);
    installFormatter (senseKnob,      fmtPercent);

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

void ChannelStripComponent::triggerPadButton() {
    /* Used by the editor when the user presses 1..4 on the keyboard.
       triggerClick() runs onClick (which fires the channel) and also
       does the JUCE built-in button-press visual flash. */
    padButton.triggerClick();
}

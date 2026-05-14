#include "PluginEditor.h"
#include "Presets.h"

DS4MEditor::DS4MEditor (DS4MProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&laf);

    for (int i = 0; i < P::kNumChannels; ++i) {
        strips[(size_t)i] = std::make_unique<ChannelStripComponent> (processor, i, laf);
        addAndMakeVisible (strips[(size_t)i].get());
    }
    master = std::make_unique<MasterStripComponent> (processor, laf);
    addAndMakeVisible (master.get());

    /* Preset selector. */
    presetBox.setTextWhenNothingSelected ("Preset...");
    presetBox.addItem ("-- factory --", -2);
    presetBox.setItemEnabled (-2, false);
    int id = 1;
    for (const auto& pr : Presets::factoryPresets())
        presetBox.addItem (pr.name, id++);
    presetBox.onChange = [this] {
        const int sel = presetBox.getSelectedId();
        if (sel > 0) Presets::apply (processor.apvts, sel - 1);
    };
    addAndMakeVisible (presetBox);

    helpButton.setTooltip (
        "ULT-SOUND DS-4M -- click a PAD button on each channel strip or "
        "send MIDI notes C1..D#1 to trigger the four drums. Routing audio "
        "to the 'Trigger In' side-chain (L = Ch1, R = Ch2) lets a real "
        "piezo or any percussive audio drive the synth.");
    addAndMakeVisible (helpButton);

    setResizable (true, true);
    setSize (1100, 640);
    setResizeLimits (820, 480, 1800, 1100);
}

DS4MEditor::~DS4MEditor() {
    setLookAndFeel (nullptr);
}

void DS4MEditor::paint (juce::Graphics& g) {
    g.fillAll (juce::Colour (0xFF1A1A1A));

    auto r = getLocalBounds().reduced (12);
    g.setColour (PanelLookAndFeel::kPanel);
    g.fillRoundedRectangle (r.toFloat(), 6.0f);
    g.setColour (PanelLookAndFeel::kInkLine);
    g.drawRoundedRectangle (r.toFloat(), 6.0f, 2.0f);

    g.setColour (juce::Colour (0xFF555555));
    g.setFont (juce::Font (juce::FontOptions ("Helvetica Neue", 9.0f, juce::Font::plain)));
    g.drawText ("TOYO GAKKI CO.,LTD.",
                r.removeFromBottom (16),
                juce::Justification::right);
}

void DS4MEditor::resized () {
    auto r = getLocalBounds().reduced (18);

    /* Top bar: preset selector + help button (40 px high). */
    auto top = r.removeFromTop (32);
    helpButton.setBounds (top.removeFromRight (32));
    top.removeFromRight (6);
    presetBox.setBounds  (top.removeFromRight (180));

    r.removeFromTop (6);

    /* Master strip on the right, fixed width. */
    auto masterArea = r.removeFromRight (110);
    master->setBounds (masterArea);

    /* Two rows of two channel strips. */
    auto top2 = r.removeFromTop (r.getHeight() / 2);
    auto bot  = r;
    const int half = top2.getWidth() / 2;
    strips[0]->setBounds (top2.removeFromLeft (half));
    strips[1]->setBounds (top2);
    strips[2]->setBounds (bot.removeFromLeft (half));
    strips[3]->setBounds (bot);
}

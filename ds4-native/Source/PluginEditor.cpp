#include "PluginEditor.h"

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

    setResizable (true, true);
    setSize (1100, 600);
    setResizeLimits (820, 460, 1800, 1100);
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

    /* Maker stamp. */
    g.setColour (juce::Colour (0xFF555555));
    g.setFont (juce::Font (juce::FontOptions ("Helvetica Neue", 9.0f, juce::Font::plain)));
    g.drawText ("TOYO GAKKI CO.,LTD.",
                r.removeFromBottom (16),
                juce::Justification::right);
}

void DS4MEditor::resized () {
    auto r = getLocalBounds().reduced (18);

    /* Master strip on the right, fixed width. */
    auto masterArea = r.removeFromRight (110);
    master->setBounds (masterArea);

    /* Two rows of two channel strips. */
    auto top = r.removeFromTop (r.getHeight() / 2);
    auto bot = r;
    const int half = top.getWidth() / 2;
    strips[0]->setBounds (top.removeFromLeft (half));
    strips[1]->setBounds (top);
    strips[2]->setBounds (bot.removeFromLeft (half));
    strips[3]->setBounds (bot);
}

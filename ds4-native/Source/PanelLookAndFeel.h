#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/* A LookAndFeel that paints rotary knobs, sliders and switches in the
   style of the DS-4M front panel: metallic silver knobs with a
   black indicator line, black-bordered yellow/cyan painted regions and
   pill-shaped section labels. */
class PanelLookAndFeel : public juce::LookAndFeel_V4 {
public:
    static const juce::Colour kCyan;
    static const juce::Colour kYellow;
    static const juce::Colour kPanel;
    static const juce::Colour kRed;
    static const juce::Colour kInkLine;

    PanelLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int height) override;

    /* Helpers used by composite components. */
    static void paintPill (juce::Graphics& g, juce::Rectangle<int> r,
                           const juce::String& text);
    static void paintColouredStrip (juce::Graphics& g, juce::Rectangle<int> r,
                                    juce::Colour fill);
};

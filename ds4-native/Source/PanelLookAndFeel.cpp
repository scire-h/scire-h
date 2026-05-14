#include "PanelLookAndFeel.h"

const juce::Colour PanelLookAndFeel::kCyan    = juce::Colour (0xFF5DC6C8);
const juce::Colour PanelLookAndFeel::kYellow  = juce::Colour (0xFFC4C248);
const juce::Colour PanelLookAndFeel::kPanel   = juce::Colour (0xFF111111);
const juce::Colour PanelLookAndFeel::kRed     = juce::Colour (0xFFD92020);
const juce::Colour PanelLookAndFeel::kInkLine = juce::Colour (0xFF050505);

PanelLookAndFeel::PanelLookAndFeel() {
    setColour (juce::Slider::backgroundColourId,  juce::Colour (0x30000000));
    setColour (juce::Slider::trackColourId,       juce::Colour (0x80000000));
    setColour (juce::Slider::thumbColourId,       juce::Colours::black);
    setColour (juce::Label::textColourId,         juce::Colours::black);
    setColour (juce::ToggleButton::textColourId,  juce::Colours::black);
}

void PanelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y,
                                         int width, int height,
                                         float sliderPos,
                                         float startAngle, float endAngle,
                                         juce::Slider&)
{
    const float radius   = (float)juce::jmin (width, height) * 0.45f;
    const float cx       = (float)x + (float)width  * 0.5f;
    const float cy       = (float)y + (float)height * 0.5f;
    const float angle    = startAngle + sliderPos * (endAngle - startAngle);

    /* Metallic body. */
    juce::ColourGradient bg (juce::Colours::white,           cx - radius * 0.3f, cy - radius * 0.4f,
                             juce::Colour (0xFF888888),      cx + radius * 0.4f, cy + radius * 0.4f, true);
    g.setGradientFill (bg);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (kInkLine);
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.4f);

    /* Centre dimple. */
    const float dimR = radius * 0.30f;
    juce::ColourGradient dimple (juce::Colours::white,       cx - dimR * 0.3f, cy - dimR * 0.4f,
                                 juce::Colour (0xFFB0B0B0),  cx + dimR * 0.4f, cy + dimR * 0.4f, true);
    g.setGradientFill (dimple);
    g.fillEllipse (cx - dimR, cy - dimR, dimR * 2.0f, dimR * 2.0f);

    /* Indicator. */
    juce::Path needle;
    needle.addRoundedRectangle (-1.5f, -radius, 3.0f, radius * 0.6f, 1.0f);
    g.setColour (kInkLine);
    g.fillPath (needle, juce::AffineTransform::rotation (angle).translated (cx, cy));
}

void PanelLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y,
                                         int width, int height,
                                         float sliderPos, float /*minPos*/, float /*maxPos*/,
                                         const juce::Slider::SliderStyle style,
                                         juce::Slider&)
{
    juce::Rectangle<float> track ((float)x + (float)width * 0.5f - 2.0f,
                                  (float)y + 2.0f, 4.0f, (float)height - 4.0f);
    g.setColour (juce::Colour (0x90000000));
    g.fillRoundedRectangle (track, 1.0f);

    /* Cap. */
    const float capW = (float)width * 1.4f;
    const float capH = 14.0f;
    const float capX = (float)x + (float)width * 0.5f - capW * 0.5f;
    const float capY = (style == juce::Slider::LinearVertical)
                     ? sliderPos - capH * 0.5f
                     : (float)y + (float)height * 0.5f - capH * 0.5f;
    juce::Rectangle<float> cap (capX, capY, capW, capH);
    juce::ColourGradient cg (juce::Colour (0xFF555555), capX, capY,
                             juce::Colour (0xFF111111), capX, capY + capH, false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (cap, 2.0f);
    g.setColour (kInkLine);
    g.drawRoundedRectangle (cap, 2.0f, 1.0f);
}

void PanelLookAndFeel::drawToggleButton (juce::Graphics& g,
                                         juce::ToggleButton& b,
                                         bool /*hl*/, bool /*down*/)
{
    auto r = b.getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (kInkLine);
    g.drawRoundedRectangle (r, 3.0f, 1.0f);

    const float h = r.getHeight();
    const float pos = b.getToggleState() ? 0.05f : 0.55f;
    juce::Rectangle<float> lever (r.getCentreX() - 4.0f,
                                  r.getY() + pos * h,
                                  8.0f, h * 0.40f);
    juce::ColourGradient cg (juce::Colour (0xFFCFCFCF), lever.getX(), lever.getY(),
                             juce::Colour (0xFF555555), lever.getX(), lever.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (lever, 2.0f);
    g.setColour (kInkLine);
    g.drawRoundedRectangle (lever, 2.0f, 0.8f);
}

juce::Font PanelLookAndFeel::getLabelFont (juce::Label&) {
    return juce::Font (juce::FontOptions ("Helvetica Neue", 9.5f, juce::Font::bold));
}

juce::Font PanelLookAndFeel::getTextButtonFont (juce::TextButton&, int) {
    return juce::Font (juce::FontOptions ("Helvetica Neue", 11.0f, juce::Font::bold));
}

void PanelLookAndFeel::paintPill (juce::Graphics& g,
                                  juce::Rectangle<int> r,
                                  const juce::String& text)
{
    auto rf = r.toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0x30FFFFFF));
    g.fillRoundedRectangle (rf, rf.getHeight() * 0.5f);
    g.setColour (kInkLine);
    g.drawRoundedRectangle (rf, rf.getHeight() * 0.5f, 1.0f);
    g.setColour (juce::Colours::black);
    g.setFont (juce::Font (juce::FontOptions ("Helvetica Neue", 9.5f, juce::Font::bold)));
    g.drawText (text, rf, juce::Justification::centred);
}

void PanelLookAndFeel::paintColouredStrip (juce::Graphics& g,
                                           juce::Rectangle<int> r,
                                           juce::Colour fill)
{
    auto rf = r.toFloat();
    juce::ColourGradient cg (fill.brighter (0.10f), rf.getX(), rf.getY(),
                             fill.darker (0.10f),  rf.getX(), rf.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (rf, 3.0f);
    g.setColour (kInkLine);
    g.drawRoundedRectangle (rf, 3.0f, 1.4f);
}

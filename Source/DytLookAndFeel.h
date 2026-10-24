#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Discover Your Tone design system, as code.
namespace Dyt
{
    const juce::Colour white          { 0xffffffff };
    const juce::Colour ink            { 0xff0d0d0d };
    const juce::Colour signalBlue     { 0xff5bb8d4 };
    const juce::Colour signalBlueLite { 0xffe8f6fb };
    const juce::Colour steel          { 0xffc9ced3 };

    juce::Font font (float height, bool semiBold = false, float kerning = 0.0f);

    // The DYT wave motif.
    juce::Path wave (juce::Rectangle<float> area, float cycles = 2.0f);
}

class DytLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DytLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
};

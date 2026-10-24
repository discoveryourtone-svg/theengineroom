#include "DytLookAndFeel.h"
#include "BinaryData.h"

namespace
{
    juce::Typeface::Ptr regularFace()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor (BinaryData::Jost400_ttf, BinaryData::Jost400_ttfSize);
        return tf;
    }
    juce::Typeface::Ptr semiBoldFace()
    {
        static auto tf = juce::Typeface::createSystemTypefaceFor (BinaryData::Jost600_ttf, BinaryData::Jost600_ttfSize);
        return tf;
    }
}

juce::Font Dyt::font (float height, bool semiBold, float kerning)
{
    return juce::Font (juce::FontOptions (semiBold ? semiBoldFace() : regularFace())
                           .withHeight (height)
                           .withKerningFactor (kerning));
}

juce::Path Dyt::wave (juce::Rectangle<float> r, float cycles)
{
    juce::Path p;
    const int steps = 64;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float x = r.getX() + t * r.getWidth();
        const float y = r.getCentreY() - std::sin (t * cycles * juce::MathConstants<float>::twoPi) * r.getHeight() * 0.5f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    return p;
}

//==============================================================================
DytLookAndFeel::DytLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, Dyt::ink);
    setColour (juce::ComboBox::backgroundColourId, Dyt::white);
    setColour (juce::ComboBox::outlineColourId, Dyt::ink);
    setColour (juce::ComboBox::arrowColourId, Dyt::signalBlue);
    setColour (juce::PopupMenu::backgroundColourId, Dyt::white);
    setColour (juce::PopupMenu::textColourId, Dyt::ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Dyt::signalBlueLite);
    setColour (juce::PopupMenu::highlightedTextColourId, Dyt::ink);
    setColour (juce::TooltipWindow::backgroundColourId, Dyt::white);
    setColour (juce::TooltipWindow::textColourId, Dyt::ink);
}

juce::Typeface::Ptr DytLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    return f.isBold() ? semiBoldFace() : regularFace();
}

//==============================================================================
void DytLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    auto bounds = Rectangle<int> (x, y, w, h).toFloat();
    const float ringR = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const float knobR = ringR - 10.0f;
    const auto  c     = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    auto arc = [&] (float from, float to, float r)
    {
        Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
        return p;
    };

    // Track
    g.setColour (Dyt::ink.withAlpha (0.12f));
    g.strokePath (arc (startAngle, endAngle, ringR), PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // Sweet-spot band (beginner guide rail)
    if (s.getProperties().contains ("sweetFrom"))
    {
        const float a0 = startAngle + (float) s.getProperties()["sweetFrom"] * (endAngle - startAngle);
        const float a1 = startAngle + (float) s.getProperties()["sweetTo"]   * (endAngle - startAngle);
        g.setColour (Dyt::signalBlue.withAlpha (0.28f));
        g.strokePath (arc (a0, a1, ringR), PathStrokeType (9.0f, PathStrokeType::curved, PathStrokeType::butt));
    }

    // Value
    g.setColour (Dyt::signalBlue);
    g.strokePath (arc (startAngle, angle, ringR), PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // Knob shadow + body
    g.setColour (Dyt::ink.withAlpha (0.10f));
    g.fillEllipse (Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c.translated (0.0f, 3.0f)));

    g.setColour (Dyt::ink);
    g.fillEllipse (Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c));

    // Subtle grip ridges
    g.setColour (Colours::white.withAlpha (0.07f));
    for (int i = 0; i < 24; ++i)
    {
        const float a = (float) i / 24.0f * MathConstants<float>::twoPi;
        g.drawLine (Line<float> (c.getPointOnCircumference (knobR * 0.86f, a), c.getPointOnCircumference (knobR * 0.98f, a)), 1.2f);
    }

    // Cap
    g.setColour (Colour (0xff1e1e1e));
    g.fillEllipse (Rectangle<float> (knobR * 1.5f, knobR * 1.5f).withCentre (c));

    // Pointer
    g.setColour (Colours::white);
    g.drawLine (Line<float> (c.getPointOnCircumference (knobR * 0.30f, angle), c.getPointOnCircumference (knobR * 0.86f, angle)), 3.0f);
}

//==============================================================================
void DytLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.75f);
    g.setColour (Dyt::white);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver() ? Dyt::signalBlue : Dyt::ink);
    g.drawRoundedRectangle (r, 6.0f, 1.5f);

    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.5f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (Dyt::signalBlue);
    g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font DytLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Dyt::font (13.0f); }
juce::Font DytLookAndFeel::getPopupMenuFont()               { return Dyt::font (14.0f); }

void DytLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void DytLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Dyt::white);
    g.setColour (Dyt::ink);
    g.drawRect (0, 0, w, h, 1);
}

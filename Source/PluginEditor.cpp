#include "PluginEditor.h"

using namespace juce;

namespace
{
    constexpr float kSweep = MathConstants<float>::pi * 0.75f;   // +/-135 degrees

    const char* kModeNames[] = { "OVERDRIVE", "DISTORTION", "FUZZ" };
    const char* kModeShort[] = { "OD", "DIST", "FUZZ" };
    const char* kModeHints[] =
    {
        "Overdrive: warm, smooth grit, like a small amp turned up. Your clean sound stays mixed in.",
        "Distortion: tighter and heavier. Squared-off waves give long sustain and chunky palm-mutes.",
        "Fuzz: wild, woolly and lopsided. Notes sputter as they fade. Think 60s rock and big riffs."
    };

    // Beginner guide rail on the DRIVE knob, per mode (0..1 of the travel).
    const float kSweetSpots[3][2] = { { 0.30f, 0.62f }, { 0.45f, 0.75f }, { 0.55f, 0.85f } };

    void drawCentredText (Graphics& g, const String& t, const Font& f, Rectangle<float> r, Colour c)
    {
        g.setColour (c);
        g.setFont (f);
        g.drawText (t, r, Justification::centred, false);
    }
}

//==============================================================================
//  BoostGauge
//==============================================================================
void BoostGauge::setReading (float pushDb, bool engaged)
{
    // Map: -30 dB..0 dB  -> VAC 30..0,   0..+39 dB -> BOOST 0..30
    float s = pushDb <= 0.0f ? jmax (pushDb, -30.0f) : jmin (pushDb / 1.3f, 30.0f);
    target = engaged ? s : -30.0f;
}

void BoostGauge::advance (float dt)
{
    // Spring-damper needle: a touch of overshoot, like a real gauge.
    const float k = 220.0f, damping = 2.0f * std::sqrt (k) * 0.55f;
    const int steps = 4;
    const float h = dt / (float) steps;
    const float before = value;
    for (int i = 0; i < steps; ++i)
    {
        velocity += (k * (target - value) - damping * velocity) * h;
        value += velocity * h;
    }
    value = jlimit (-33.0f, 33.0f, value);   // the needle pin
    if (std::abs (value - before) > 0.01f) repaint();
}

void BoostGauge::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;

    // Bezel shadow, bezel, face
    g.setColour (Dyt::ink.withAlpha (0.10f));
    g.fillEllipse (Rectangle<float> (R * 2.0f - 4.0f, R * 2.0f - 4.0f).withCentre (c.translated (0.0f, 3.0f)));
    g.setColour (Dyt::ink);
    g.fillEllipse (Rectangle<float> (R * 2.0f - 4.0f, R * 2.0f - 4.0f).withCentre (c));
    g.setColour (Dyt::white);
    const float faceR = R - 9.0f;
    g.fillEllipse (Rectangle<float> (faceR * 2.0f, faceR * 2.0f).withCentre (c));

    auto angleFor = [] (float s) { return s / 30.0f * kSweep; };

    // Zones
    auto arc = [&] (float s0, float s1, float r)
    {
        Path p; p.addCentredArc (c.x, c.y, r, r, 0.0f, angleFor (s0), angleFor (s1), true); return p;
    };
    g.setColour (Dyt::ink.withAlpha (0.15f));
    g.strokePath (arc (-30.0f, 0.0f, faceR - 4.0f), PathStrokeType (2.0f));
    g.setColour (Dyt::signalBlue);
    g.strokePath (arc (0.0f, 30.0f, faceR - 4.0f), PathStrokeType (4.0f));

    // Ticks
    for (int s = -30; s <= 30; s += 2)
    {
        const bool major = (s % 10) == 0;
        const float a = angleFor ((float) s);
        const float r0 = faceR - 8.0f, r1 = r0 - (major ? 11.0f : 5.0f);
        g.setColour (Dyt::ink.withAlpha (major ? 1.0f : 0.55f));
        g.drawLine (Line<float> (c.getPointOnCircumference (r0, a), c.getPointOnCircumference (r1, a)), major ? 2.0f : 1.0f);
    }

    // Numerals (vac counts down to 0, boost counts up)
    for (int s = -30; s <= 30; s += 10)
    {
        const auto p = c.getPointOnCircumference (faceR - 30.0f, angleFor ((float) s));
        drawCentredText (g, String (std::abs (s)), Dyt::font (14.0f, true),
                         Rectangle<float> (30.0f, 18.0f).withCentre (p), Dyt::ink);
    }

    // Zone labels sit in the dead zone at the bottom, clear of the needle.
    const float labelY = c.y + faceR * 0.70f;
    drawCentredText (g, "VAC",   Dyt::font (10.5f, true, 0.12f), Rectangle<float> (44.0f, 14.0f).withCentre ({ c.x - 24.0f, labelY }), Dyt::ink.withAlpha (0.6f));
    drawCentredText (g, "BOOST", Dyt::font (10.5f, true, 0.12f), Rectangle<float> (50.0f, 14.0f).withCentre ({ c.x + 26.0f, labelY }), Dyt::signalBlue);
    g.setColour (Dyt::ink.withAlpha (0.2f));
    g.drawLine (c.x, labelY - 5.0f, c.x, labelY + 5.0f, 1.0f);

    // Current circuit, printed on the dial like a maker's mark
    drawCentredText (g, modeName, Dyt::font (9.5f, true, 0.2f),
                     Rectangle<float> (100.0f, 12.0f).withCentre (c.translated (0.0f, -faceR * 0.34f)), Dyt::ink.withAlpha (0.55f));

    // Needle
    const float a = angleFor (value);
    Path needle;
    needle.addTriangle (-2.6f, 12.0f, 2.6f, 12.0f, 0.0f, -(faceR - 12.0f));
    needle.addRoundedRectangle (-2.6f, 4.0f, 5.2f, 12.0f, 1.5f);
    g.setColour (Dyt::ink.withAlpha (0.18f));
    g.fillPath (needle, AffineTransform::rotation (a).translated (c.x + 1.5f, c.y + 2.5f));
    g.setColour (Dyt::ink);
    g.fillPath (needle, AffineTransform::rotation (a).translated (c.x, c.y));

    // Hub
    g.setColour (Dyt::signalBlue);
    g.fillEllipse (Rectangle<float> (16.0f, 16.0f).withCentre (c));
    g.setColour (Dyt::ink);
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (c));
}

//==============================================================================
//  ModeSwitch
//==============================================================================
ModeSwitch::ModeSwitch (RangedAudioParameter& p)
    : attachment (p, [this] (float v)
      {
          selected = jlimit (0, 2, roundToInt (v));
          repaint();
          if (onModeChanged) onModeChanged();
      })
{
    attachment.sendInitialUpdate();
}

int ModeSwitch::segmentAt (Point<int> p) const
{
    if (! getLocalBounds().contains (p)) return -1;
    return jlimit (0, 2, p.x * 3 / jmax (1, getWidth()));
}

String ModeSwitch::hintAt (Point<int> p) const
{
    const int s = segmentAt (p);
    return s >= 0 ? kModeHints[s] : String();
}

void ModeSwitch::mouseDown (const MouseEvent& e)
{
    const int s = segmentAt (e.getPosition());
    if (s >= 0 && s != selected)
        attachment.setValueAsCompleteGesture ((float) s);
}

void ModeSwitch::mouseMove (const MouseEvent& e) { hovered = segmentAt (e.getPosition()); repaint(); }
void ModeSwitch::mouseExit (const MouseEvent&)   { hovered = -1; repaint(); }

void ModeSwitch::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float segW = r.getWidth() / 3.0f;

    for (int i = 0; i < 3; ++i)
    {
        auto seg = Rectangle<float> (r.getX() + segW * (float) i, r.getY(), segW, r.getHeight());
        if (i == selected)
        {
            Path p;
            p.addRoundedRectangle (seg.getX(), seg.getY(), seg.getWidth(), seg.getHeight(), 7.0f, 7.0f,
                                   i == 0, i == 2, i == 0, i == 2);
            g.setColour (Dyt::ink);
            g.fillPath (p);
        }
        else if (i == hovered)
        {
            g.setColour (Dyt::signalBlueLite);
            g.fillRect (seg.reduced (0.0f, 1.0f));
        }

        // Little indicator light in each segment
        const auto dot = Rectangle<float> (6.0f, 6.0f).withCentre ({ seg.getX() + 16.0f, seg.getCentreY() });
        g.setColour (i == selected ? Dyt::signalBlue : Dyt::ink.withAlpha (0.18f));
        g.fillEllipse (dot);

        drawCentredText (g, kModeShort[i], Dyt::font (13.0f, true, 0.14f), seg.withTrimmedLeft (12.0f),
                         i == selected ? Dyt::white : Dyt::ink.withAlpha (0.75f));
    }

    g.setColour (Dyt::ink);
    g.drawRoundedRectangle (r, 7.0f, 1.5f);
    for (int i = 1; i < 3; ++i)
        g.drawLine (r.getX() + segW * (float) i, r.getY(), r.getX() + segW * (float) i, r.getBottom(), 1.0f);
}

//==============================================================================
//  Footswitch
//==============================================================================
Footswitch::Footswitch (RangedAudioParameter& bypass)
    : attachment (bypass, [this] (float v) { bypassed = v >= 0.5f; repaint(); if (onChange) onChange(); })
{
    attachment.sendInitialUpdate();
}

void Footswitch::mouseDown (const MouseEvent&)
{
    pressed = true;
    attachment.setValueAsCompleteGesture (bypassed ? 0.0f : 1.0f);
    repaint();
}

void Footswitch::mouseUp (const MouseEvent&) { pressed = false; repaint(); }

void Footswitch::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f - 2.0f;

    // Hex nut
    Path nut;
    nut.addPolygon (c, 6, R, MathConstants<float>::pi / 6.0f);
    g.setColour (Dyt::ink.withAlpha (0.10f));
    g.fillPath (nut, AffineTransform::translation (0.0f, 3.0f));
    g.setColour (Dyt::steel);
    g.fillPath (nut);
    g.setColour (Dyt::ink);
    g.strokePath (nut, PathStrokeType (1.5f));

    // Domed cap
    const float capR = R * (pressed ? 0.60f : 0.64f);
    ColourGradient grad (Colour (0xfffafafa), c.x - capR * 0.4f, c.y - capR * 0.5f,
                         Colour (0xffa9b0b6), c.x + capR * 0.6f, c.y + capR * 0.8f, true);
    g.setGradientFill (grad);
    g.fillEllipse (Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
    g.setColour (Dyt::ink);
    g.drawEllipse (Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c), 1.5f);
}

//==============================================================================
//  LearnButton
//==============================================================================
void LearnButton::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.75f);
    const bool over = isMouseOver();
    g.setColour (over ? Dyt::signalBlueLite : Dyt::white);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (over ? Dyt::signalBlue : Dyt::ink);
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);
    drawCentredText (g, CharPointer_UTF8 ("HOW IT WORKS \xe2\x86\x92"), Dyt::font (11.5f, true, 0.1f), r, Dyt::ink);
}

void LearnButton::mouseUp (const MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()))
        URL ("https://discoveryourtone.com").launchInDefaultBrowser();
}

//==============================================================================
//  PedalFace
//==============================================================================
PedalFace::PedalFace (EngineRoomProcessor& p)
    : proc (p),
      modeSwitch (*p.apvts.getParameter (ParamID::mode)),
      driveAtt (p.apvts, ParamID::drive, drive),
      toneAtt  (p.apvts, ParamID::tone,  tone),
      levelAtt (p.apvts, ParamID::level, level),
      footswitch (*p.apvts.getParameter (ParamID::bypass)),
      licenceChip (p.getLicence())
{
    setLookAndFeel (&lnf);
    hint = defaultHint;

    addAndMakeVisible (gauge);
    gauge.getProperties().set ("hint", "The gauge shows how hard you're pushing the circuit. In VAC it's clean. Swing into BOOST and it breaks up into dirt.");

    addAndMakeVisible (modeSwitch);
    modeSwitch.onModeChanged = [this] { updateSweetSpot(); };

    auto setupKnob = [this] (Slider& s, const char* hintText)
    {
        s.setSliderStyle (Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (220);
        s.setPopupDisplayEnabled (true, true, this);
        s.getProperties().set ("hint", hintText);
        addAndMakeVisible (s);
    };
    setupKnob (drive, "Drive: how hard you push the circuit. Turn right for more grit. The blue band is the sweet spot for this mode.");
    setupKnob (tone,  "Tone: left is darker and warmer, right is brighter and sharper.");
    setupKnob (level, "Level: overall volume. At 5 it matches your volume with the pedal off.");
    drive.setDoubleClickReturnValue (true, 5.0);
    tone.setDoubleClickReturnValue (true, 5.5);
    level.setDoubleClickReturnValue (true, 5.0);
    updateSweetSpot();

    addAndMakeVisible (footswitch);
    footswitch.getProperties().set ("hint", "On/Off: click to switch the pedal in or out. The light shows when it's on.");
    footswitch.onChange = [this] { repaint(); };

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        starterTones.addItem (proc.getProgramName (i).fromFirstOccurrenceOf ("Starter: ", false, false), i + 1);
    starterTones.setTextWhenNothingSelected ("Pick a starter tone");
    if (proc.getChosenProgram() >= 0)
        starterTones.setSelectedId (proc.getChosenProgram() + 1, dontSendNotification);
    starterTones.onChange = [this]
    {
        if (const int id = starterTones.getSelectedId(); id > 0)
            proc.setCurrentProgram (id - 1);
    };
    starterTones.getProperties().set ("hint", "Starter tones: ready-made settings from the Jump Start Guitarist course. Pick one, then tweak.");
    addAndMakeVisible (starterTones);

    learn.getProperties().set ("hint", "Opens a plain-English walkthrough of what's happening inside this pedal.");
    addAndMakeVisible (learn);

    licenceChip.getProperties().set ("hint", "Your licence. Click to activate, or to see your founder details.");
    licenceChip.onClick = [this] { showActivation(); };
    addAndMakeVisible (licenceChip);
    proc.getLicence().addChangeListener (this);

    ledOn = footswitch.isEngaged();
    addMouseListener (this, true);
    startTimerHz (60);

    if (proc.getLicence().getStatus() == LicenceManager::Status::trialEnded)
        showActivation();
}

void PedalFace::showActivation()
{
    if (activation == nullptr)
    {
        activation = std::make_unique<ActivationPanel> (proc.getLicence());
        SafePointer<PedalFace> safe (this);
        activation->onClose = [safe] { MessageManager::callAsync ([safe] { if (safe) safe->hideActivation(); }); };
        addAndMakeVisible (*activation);
        activation->setBounds (getLocalBounds());
    }
    activation->toFront (true);
}

void PedalFace::hideActivation()
{
    activation.reset();
    licenceChip.repaint();
}

PedalFace::~PedalFace()
{
    proc.getLicence().removeChangeListener (this);
    activation.reset();
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void PedalFace::updateSweetSpot()
{
    const int m = modeSwitch.getSelected();
    drive.getProperties().set ("sweetFrom", kSweetSpots[m][0]);
    drive.getProperties().set ("sweetTo",   kSweetSpots[m][1]);
    gauge.setModeName (kModeNames[m]);
    drive.repaint();
}

void PedalFace::resized()
{
    gauge.setBounds (128, 108, 184, 184);
    modeSwitch.setBounds (70, 310, 300, 34);

    drive.setBounds (Rectangle<int> (124, 124).withCentre ({ 220, 418 }));
    tone.setBounds  (Rectangle<int> (86, 86).withCentre ({ 104, 426 }));
    level.setBounds (Rectangle<int> (86, 86).withCentre ({ 336, 426 }));

    starterTones.setBounds (42, 588, 132, 28);
    footswitch.setBounds (Rectangle<int> (62, 62).withCentre ({ 220, 600 }));
    learn.setBounds (266, 588, 132, 28);
    licenceChip.setBounds (24, 647, 186, 16);
    if (activation) activation->setBounds (getLocalBounds());
}

void PedalFace::setHint (const String& h)
{
    const auto next = h.isNotEmpty() ? h : defaultHint;
    if (next != hint) { hint = next; repaint (40, 496, 360, 44); }
}

void PedalFace::mouseMove (const MouseEvent& e)
{
    if (e.originalComponent == &modeSwitch)
        return setHint (modeSwitch.hintAt (e.getEventRelativeTo (&modeSwitch).getPosition()));

    for (auto* c = e.originalComponent; c != nullptr && c != this; c = c->getParentComponent())
        if (c->getProperties().contains ("hint"))
            return setHint (c->getProperties()["hint"].toString());

    setHint ({});
}

void PedalFace::mouseExit (const MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getEventRelativeTo (this).getPosition()))
        setHint ({});
}

void PedalFace::timerCallback()
{
    gauge.setReading (proc.getPushDb(), footswitch.isEngaged());
    gauge.advance (1.0f / 60.0f);

    if (const bool on = footswitch.isEngaged(); on != ledOn)
    {
        ledOn = on;
        repaint (200, 536, 40, 30);
    }
}

void PedalFace::simulateReading (float pushDb, int frames)
{
    for (int i = 0; i < frames; ++i)
    {
        gauge.setReading (pushDb, footswitch.isEngaged());
        gauge.advance (1.0f / 60.0f);
    }
}

void PedalFace::paintKnobLabels (Graphics& g, Slider& s, const String& name, const String& left, const String& right)
{
    const auto b = s.getBounds().toFloat();
    drawCentredText (g, name, Dyt::font (14.0f, true, 0.14f),
                     Rectangle<float> (b.getWidth() + 40.0f, 18.0f).withCentre ({ b.getCentreX(), 494.0f }), Dyt::ink);

    const auto small = Dyt::font (9.5f, true, 0.1f);
    const float r = b.getWidth() * 0.5f;
    const auto lp = b.getCentre().getPointOnCircumference (r - 2.0f, MathConstants<float>::pi * 1.25f);
    const auto rp = b.getCentre().getPointOnCircumference (r - 2.0f, MathConstants<float>::pi * 2.75f);
    drawCentredText (g, left,  small, Rectangle<float> (50.0f, 12.0f).withCentre (lp.translated (-4.0f, 10.0f)), Dyt::ink.withAlpha (0.5f));
    drawCentredText (g, right, small, Rectangle<float> (50.0f, 12.0f).withCentre (rp.translated ( 4.0f, 10.0f)), Dyt::ink.withAlpha (0.5f));
}

void PedalFace::paint (Graphics& g)
{
    const auto W = (float) baseWidth, H = (float) baseHeight;
    g.fillAll (Dyt::white);

    // Frame system: Signal Blue bars top and bottom
    g.setColour (Dyt::signalBlue);
    g.fillRect (0.0f, 0.0f, W, 8.0f);
    g.fillRect (0.0f, H - 8.0f, W, 8.0f);

    // Enclosure
    const auto body = Rectangle<float> (24.0f, 26.0f, W - 48.0f, 612.0f);
    g.setColour (Dyt::ink.withAlpha (0.06f));
    g.fillRoundedRectangle (body.translated (0.0f, 6.0f), 20.0f);
    g.setColour (Dyt::white);
    g.fillRoundedRectangle (body, 20.0f);
    g.setColour (Dyt::ink);
    g.drawRoundedRectangle (body, 20.0f, 2.0f);

    // Corner screws
    for (auto pt : { body.getTopLeft().translated (16, 16), body.getTopRight().translated (-16, 16),
                     body.getBottomLeft().translated (16, -16), body.getBottomRight().translated (-16, -16) })
    {
        g.setColour (Dyt::steel);
        g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (pt));
        g.setColour (Dyt::ink.withAlpha (0.6f));
        g.drawEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (pt), 1.0f);
        g.drawLine (pt.x - 2.5f, pt.y, pt.x + 2.5f, pt.y, 1.0f);
    }

    // Headline: ink words, final word in Signal Blue, full-stop signature
    {
        const auto f = Dyt::font (27.0f, true, 0.06f);
        const String a ("THE ENGINE "), b ("ROOM.");
        const float wa = GlyphArrangement::getStringWidth (f, a);
        const float wb = GlyphArrangement::getStringWidth (f, b);
        const float x0 = W * 0.5f - (wa + wb) * 0.5f;
        g.setFont (f);
        g.setColour (Dyt::ink);
        g.drawText (a, Rectangle<float> (x0, 44.0f, wa + 4.0f, 34.0f), Justification::centredLeft, false);
        g.setColour (Dyt::signalBlue);
        g.drawText (b, Rectangle<float> (x0 + wa, 44.0f, wb + 4.0f, 34.0f), Justification::centredLeft, false);
    }
    drawCentredText (g, CharPointer_UTF8 ("OVERDRIVE \xc2\xb7 DISTORTION \xc2\xb7 FUZZ"), Dyt::font (10.5f, true, 0.2f),
                     { 0.0f, 78.0f, W, 16.0f }, Dyt::ink.withAlpha (0.5f));

    // Knob labels
    paintKnobLabels (g, tone,  "TONE",  "DARK",  "BRIGHT");
    paintKnobLabels (g, drive, "DRIVE", "CLEAN", "DIRTY");
    paintKnobLabels (g, level, "LEVEL", "QUIET", "LOUD");

    // Hint strip: the pedal explains itself
    {
        const auto strip = Rectangle<float> (42.0f, 508.0f, W - 84.0f, 32.0f);
        g.setColour (Dyt::signalBlueLite);
        g.fillRoundedRectangle (strip, 8.0f);
        g.setColour (Dyt::signalBlue);
        g.fillRoundedRectangle (strip.withWidth (4.0f), 2.0f);
        g.setColour (Dyt::ink);
        g.setFont (Dyt::font (11.5f));
        g.drawFittedText (hint, strip.reduced (14.0f, 2.0f).toNearestInt(), Justification::centredLeft, 2, 0.9f);
    }

    // Status LED
    {
        const auto ledC = Point<float> (220.0f, 552.0f);
        if (ledOn)
        {
            g.setColour (Dyt::signalBlue.withAlpha (0.25f));
            g.fillEllipse (Rectangle<float> (22.0f, 22.0f).withCentre (ledC));
        }
        g.setColour (ledOn ? Dyt::signalBlue : Dyt::ink.withAlpha (0.12f));
        g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (ledC));
        g.setColour (Dyt::ink);
        g.drawEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (ledC), 1.2f);
    }

    drawCentredText (g, "STARTER TONES", Dyt::font (9.5f, true, 0.16f), { 42.0f, 572.0f, 132.0f, 14.0f }, Dyt::ink.withAlpha (0.55f));
    drawCentredText (g, "LEARN",         Dyt::font (9.5f, true, 0.16f), { 266.0f, 572.0f, 132.0f, 14.0f }, Dyt::ink.withAlpha (0.55f));

    // Footer: wave motif + wordmark
    {
        const auto f = Dyt::font (10.5f, true, 0.22f);
        const String mark ("DISCOVER YOUR TONE");
        const float mw = GlyphArrangement::getStringWidth (f, mark);
        const float right = W - 24.0f;
        g.setColour (Dyt::ink);
        g.setFont (f);
        g.drawText (mark, Rectangle<float> (right - mw - 2.0f, 647.0f, mw + 4.0f, 16.0f), Justification::centredRight, false);
        g.setColour (Dyt::signalBlue);
        g.strokePath (Dyt::wave ({ right - mw - 40.0f, 650.0f, 30.0f, 9.0f }, 1.5f),
                      PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

//==============================================================================
//  Editor: fixed-aspect, resizable, scales the pedal face
//==============================================================================
EngineRoomEditor::EngineRoomEditor (EngineRoomProcessor& p)
    : AudioProcessorEditor (p), face (p)
{
    addAndMakeVisible (face);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) PedalFace::baseWidth / PedalFace::baseHeight);
    setResizeLimits ((int) (PedalFace::baseWidth * 0.7), (int) (PedalFace::baseHeight * 0.7),
                     PedalFace::baseWidth * 2, PedalFace::baseHeight * 2);
    setSize (PedalFace::baseWidth, PedalFace::baseHeight);
}

void EngineRoomEditor::resized()
{
    face.setBounds (0, 0, PedalFace::baseWidth, PedalFace::baseHeight);
    face.setTransform (AffineTransform::scale ((float) getWidth() / (float) PedalFace::baseWidth));
}

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "DytLookAndFeel.h"
#include "ActivationPanel.h"

//==============================================================================
// The boost gauge: shows how hard the circuit is being pushed, live.
// VAC side = clean headroom. BOOST side = breaking up into dirt.
class BoostGauge : public juce::Component
{
public:
    void setReading (float pushDb, bool engaged);
    void setModeName (const juce::String& m) { if (m != modeName) { modeName = m; repaint(); } }
    void advance (float dt);
    void paint (juce::Graphics&) override;

private:
    float target = -30.0f, value = -30.0f, velocity = 0.0f;
    juce::String modeName;
};

//==============================================================================
class ModeSwitch : public juce::Component
{
public:
    ModeSwitch (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    juce::String hintAt (juce::Point<int>) const;
    int  getSelected() const { return selected; }
    std::function<void()> onModeChanged;

private:
    int segmentAt (juce::Point<int>) const;
    juce::ParameterAttachment attachment;
    int selected = 0, hovered = -1;
};

//==============================================================================
class Footswitch : public juce::Component
{
public:
    Footswitch (juce::RangedAudioParameter& bypass);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isEngaged() const { return ! bypassed; }
    std::function<void()> onChange;

private:
    juce::ParameterAttachment attachment;
    bool bypassed = false, pressed = false;
};

//==============================================================================
class LearnButton : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }
};

//==============================================================================
// The pedal face, laid out at a fixed 440 x 680 and scaled to the window.
class PedalFace : public juce::Component, private juce::Timer, private juce::ChangeListener
{
public:
    explicit PedalFace (EngineRoomProcessor&);
    ~PedalFace() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    static constexpr int baseWidth = 440, baseHeight = 680;

    // For offline snapshots: run the gauge as if audio were playing.
    void simulateReading (float pushDb, int frames);

    void showActivation();
    void hideActivation();
    ActivationPanel* getActivationPanel() { return activation.get(); }

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override { licenceChip.repaint(); }
    void updateSweetSpot();
    void setHint (const juce::String&);
    void paintKnobLabels (juce::Graphics&, juce::Slider&, const juce::String& name,
                          const juce::String& left, const juce::String& right);

    EngineRoomProcessor& proc;
    DytLookAndFeel lnf;

    BoostGauge gauge;
    ModeSwitch modeSwitch;
    juce::Slider drive, tone, level;
    juce::AudioProcessorValueTreeState::SliderAttachment driveAtt, toneAtt, levelAtt;
    Footswitch footswitch;
    juce::ComboBox starterTones;
    LearnButton learn;
    LicenceChip licenceChip;
    std::unique_ptr<ActivationPanel> activation;

    juce::String hint;
    const juce::String defaultHint { "Hover over anything to see what it does." };
    bool ledOn = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalFace)
};

//==============================================================================
class EngineRoomEditor : public juce::AudioProcessorEditor
{
public:
    explicit EngineRoomEditor (EngineRoomProcessor&);
    void paint (juce::Graphics& g) override { g.fillAll (Dyt::white); }
    void resized() override;
    PedalFace& getFace() { return face; }

private:
    PedalFace face;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineRoomEditor)
};

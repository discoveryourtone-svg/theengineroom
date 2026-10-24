#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Licence.h"

class PillButton : public juce::Button
{
public:
    PillButton (const juce::String& text, bool filled) : juce::Button (text), filled (filled) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    bool filled;
};

//==============================================================================
// Small status line on the pedal: "FOUNDER #007 · SAM LEE" or "TRIAL · 12 DAYS LEFT"
class LicenceChip : public juce::Component
{
public:
    explicit LicenceChip (LicenceManager& m) : licence (m) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }
    std::function<void()> onClick;
private:
    LicenceManager& licence;
};

//==============================================================================
class ActivationPanel : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private juce::ChangeListener
{
public:
    explicit ActivationPanel (LicenceManager&);
    ~ActivationPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dragging = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;

    std::function<void()> onClose;

    // Used by the offline test tool
    void setKeyText (const juce::String& t) { keyBox.setText (t, false); }
    void pressActivate() { tryActivate (keyBox.getText()); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void refresh();
    void tryActivate (const juce::String& text);

    LicenceManager& licence;
    juce::TextEditor keyBox;
    PillButton activateBtn { "ACTIVATE", true };
    PillButton loadBtn     { "LOAD LICENCE FILE", false };
    PillButton pasteBtn    { "PASTE KEY", false };
    PillButton closeBtn    { "KEEP TRYING IT", false };
    PillButton removeBtn   { "REMOVE FROM THIS COMPUTER", false };
    juce::HyperlinkButton lostKey { "Lost your key? discoveryourtone.com/support",
                                    juce::URL ("https://discoveryourtone.com/support") };

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String message;
    bool messageIsError = false, dragging = false;
};

#include "ActivationPanel.h"
#include "DytLookAndFeel.h"

using namespace juce;

namespace
{
    constexpr float kCardX = 36.0f, kCardY = 96.0f, kCardW = 368.0f, kCardH = 478.0f;

    void headline (Graphics& g, const String& ink, const String& blue, float y, float size, float centreX)
    {
        const auto f = Dyt::font (size, true, 0.04f);
        const float wa = GlyphArrangement::getStringWidth (f, ink);
        const float wb = GlyphArrangement::getStringWidth (f, blue);
        const float x0 = centreX - (wa + wb) * 0.5f;
        g.setFont (f);
        g.setColour (Dyt::ink);
        g.drawText (ink, Rectangle<float> (x0, y, wa + 4.0f, size + 8.0f), Justification::centredLeft, false);
        g.setColour (Dyt::signalBlue);
        g.drawText (blue, Rectangle<float> (x0 + wa, y, wb + 4.0f, size + 8.0f), Justification::centredLeft, false);
    }
}

//==============================================================================
void PillButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    if (filled)
    {
        g.setColour (down ? Dyt::ink.brighter (0.25f) : (over ? Dyt::ink.brighter (0.12f) : Dyt::ink));
        g.fillRoundedRectangle (r, rad);
    }
    else
    {
        g.setColour (over ? Dyt::signalBlueLite : Dyt::white);
        g.fillRoundedRectangle (r, rad);
        g.setColour (over ? Dyt::signalBlue : Dyt::ink);
        g.drawRoundedRectangle (r, rad, 1.5f);
    }
    g.setColour (filled ? Dyt::white : Dyt::ink);
    g.setFont (Dyt::font (filled ? 13.0f : 11.0f, true, 0.12f));
    g.drawText (getButtonText(), r, Justification::centred, false);
}

//==============================================================================
void LicenceChip::paint (Graphics& g)
{
    String text;
    Colour dot = Dyt::signalBlue;
    switch (licence.getStatus())
    {
        case LicenceManager::Status::licensed:
        {
            const auto info = licence.getInfo();
            text = info.badge() + String (CharPointer_UTF8 (" \xc2\xb7 ")) + info.name.toUpperCase();
            break;
        }
        case LicenceManager::Status::trial:
        {
            const int d = licence.getTrialDaysLeft();
            text = "TRIAL " + String (CharPointer_UTF8 ("\xc2\xb7 ")) + String (d) + (d == 1 ? " DAY" : " DAYS") + " LEFT "
                   + String (CharPointer_UTF8 ("\xc2\xb7 ")) + "ACTIVATE";
            dot = Dyt::ink.withAlpha (0.3f);
            break;
        }
        case LicenceManager::Status::trialEnded:
            text = String ("TRIAL ENDED ") + String (CharPointer_UTF8 ("\xc2\xb7 ")) + "ACTIVATE";
            dot = Dyt::ink;
            break;
    }

    const auto b = getLocalBounds().toFloat();
    g.setColour (dot);
    g.fillEllipse (Rectangle<float> (6.0f, 6.0f).withCentre ({ 4.0f, b.getCentreY() }));
    g.setColour (isMouseOver() ? Dyt::signalBlue : Dyt::ink.withAlpha (0.7f));
    g.setFont (Dyt::font (9.5f, true, 0.12f));
    g.drawFittedText (text, b.withTrimmedLeft (13.0f).toNearestInt(), Justification::centredLeft, 1, 0.8f);
}

//==============================================================================
ActivationPanel::ActivationPanel (LicenceManager& m) : licence (m)
{
    keyBox.setMultiLine (true, true);
    keyBox.setReturnKeyStartsNewLine (false);
    keyBox.setFont (Dyt::font (13.0f));
    keyBox.setTextToShowWhenEmpty ("Paste your licence key here. It starts with DYT-ER-", Dyt::ink.withAlpha (0.4f));
    keyBox.setColour (TextEditor::backgroundColourId, Dyt::white);
    keyBox.setColour (TextEditor::textColourId, Dyt::ink);
    keyBox.setColour (TextEditor::outlineColourId, Dyt::ink);
    keyBox.setColour (TextEditor::focusedOutlineColourId, Dyt::signalBlue);
    keyBox.setColour (TextEditor::highlightColourId, Dyt::signalBlueLite);
    keyBox.setColour (CaretComponent::caretColourId, Dyt::ink);
    keyBox.setIndents (10, 10);
    keyBox.onReturnKey = [this] { tryActivate (keyBox.getText()); };
    addAndMakeVisible (keyBox);

    activateBtn.onClick = [this] { tryActivate (keyBox.getText()); };
    pasteBtn.onClick = [this]
    {
        const auto clip = SystemClipboard::getTextFromClipboard();
        keyBox.setText (clip, false);
        tryActivate (clip);
    };
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Choose your Engine Room licence file",
                                                 File::getSpecialLocation (File::userHomeDirectory).getChildFile ("Downloads"),
                                                 "*.dytlicence;*.txt");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      tryActivate (fc.getResult().loadFileAsString());
                              });
    };
    closeBtn.onClick  = [this] { if (onClose) onClose(); };
    removeBtn.onClick = [this]
    {
        licence.deactivate();
        message = "Licence removed from this computer. Your key still works if you want it back.";
        messageIsError = false;
        repaint();
    };

    lostKey.setFont (Dyt::font (11.5f), false, Justification::centred);
    lostKey.setColour (HyperlinkButton::textColourId, Dyt::ink.withAlpha (0.6f));

    for (auto* c : std::initializer_list<Component*> { &activateBtn, &loadBtn, &pasteBtn, &closeBtn, &removeBtn, &lostKey })
        addAndMakeVisible (c);

    licence.addChangeListener (this);
    refresh();
}

ActivationPanel::~ActivationPanel()
{
    licence.removeChangeListener (this);
}

void ActivationPanel::tryActivate (const String& text)
{
    const auto err = licence.activate (text);
    messageIsError = err.isNotEmpty();
    if (messageIsError)
        message = err;
    else
    {
        const auto info = licence.getInfo();
        message = info.founder ? "Welcome aboard, Founder #" + String (info.serial).paddedLeft ('0', 3) + ". Find your sound."
                               : "You're all set. Find your sound.";
        keyBox.clear();
    }
    refresh();
}

void ActivationPanel::filesDropped (const StringArray& files, int, int)
{
    dragging = false;
    if (files.size() > 0)
        tryActivate (File (files[0]).loadFileAsString());
    repaint();
}

void ActivationPanel::refresh()
{
    const bool licensed = licence.isLicensed();
    keyBox.setVisible (! licensed);
    activateBtn.setVisible (! licensed);
    loadBtn.setVisible (! licensed);
    pasteBtn.setVisible (! licensed);
    removeBtn.setVisible (licensed);
    closeBtn.setButtonText (licensed ? "DONE" : (licence.getStatus() == LicenceManager::Status::trial ? "KEEP TRYING IT" : "NOT NOW"));
    resized();
    repaint();
}

void ActivationPanel::resized()
{
    const int x = (int) kCardX + 28, w = (int) kCardW - 56;
    keyBox.setBounds (x, 214, w, 104);
    activateBtn.setBounds (x, 370, w, 42);
    loadBtn.setBounds (x, 422, w / 2 - 5, 34);
    pasteBtn.setBounds (x + w / 2 + 5, 422, w / 2 - 5, 34);
    removeBtn.setBounds (x + 30, 422, w - 60, 34);
    closeBtn.setBounds (x + 60, 472, w - 120, 34);
    lostKey.setBounds (x, 524, w, 22);
}

void ActivationPanel::paint (Graphics& g)
{
    g.fillAll (Dyt::white.withAlpha (0.94f));

    const auto card = Rectangle<float> (kCardX, kCardY, kCardW, kCardH);
    g.setColour (Dyt::ink.withAlpha (0.06f));
    g.fillRoundedRectangle (card.translated (0.0f, 6.0f), 18.0f);
    g.setColour (dragging ? Dyt::signalBlueLite : Dyt::white);
    g.fillRoundedRectangle (card, 18.0f);
    g.setColour (dragging ? Dyt::signalBlue : Dyt::ink);
    g.drawRoundedRectangle (card, 18.0f, 2.0f);

    const float cx = card.getCentreX();
    const auto textArea = Rectangle<float> (kCardX + 28.0f, 0.0f, kCardW - 56.0f, 0.0f);
    auto line = [&] (const String& t, float y, float h, const Font& f, Colour c)
    {
        g.setColour (c);
        g.setFont (f);
        g.drawFittedText (t, textArea.withY (y).withHeight (h).toNearestInt(), Justification::centred, 3, 0.9f);
    };

    if (licence.isLicensed())
    {
        const auto info = licence.getInfo();
        headline (g, "You're ", "in.", 122.0f, 26.0f, cx);
        line ("The Engine Room is licensed on this computer.", 162.0f, 20.0f, Dyt::font (13.0f), Dyt::ink.withAlpha (0.7f));

        // Badge plate
        const auto plate = Rectangle<float> (cx - 120.0f, 206.0f, 240.0f, 120.0f);
        g.setColour (Dyt::signalBlueLite);
        g.fillRoundedRectangle (plate, 14.0f);
        g.setColour (Dyt::signalBlue);
        g.drawRoundedRectangle (plate, 14.0f, 1.5f);
        line (info.badge(), 222.0f, 34.0f, Dyt::font (26.0f, true, 0.1f), Dyt::signalBlue);
        line (info.name, 262.0f, 20.0f, Dyt::font (15.0f, true), Dyt::ink);
        line (info.email + String (CharPointer_UTF8 ("  \xc2\xb7  ")) + info.issued, 286.0f, 18.0f, Dyt::font (11.5f), Dyt::ink.withAlpha (0.6f));
        line ("Your key works on all your own computers.", 340.0f, 18.0f, Dyt::font (12.0f), Dyt::ink.withAlpha (0.7f));
        if (message.isNotEmpty())
            line (message, 372.0f, 36.0f, Dyt::font (12.5f, true), messageIsError ? Dyt::ink : Dyt::signalBlue);
    }
    else
    {
        headline (g, "Unlock The Engine ", "Room.", 122.0f, 25.0f, cx);
        String status;
        if (licence.getStatus() == LicenceManager::Status::trial)
        {
            const int d = licence.getTrialDaysLeft();
            status = "Free trial: " + String (d) + (d == 1 ? " day" : " days") + " left. Everything works until then.";
        }
        else
            status = "Your free trial has ended. Until you activate, the pedal drops to clean for a few seconds each minute.";
        line (status, 158.0f, 40.0f, Dyt::font (12.5f), Dyt::ink.withAlpha (0.75f));

        line ("Or drop your .dytlicence file anywhere on this card.", 324.0f, 16.0f, Dyt::font (11.0f), Dyt::ink.withAlpha (0.5f));
        if (message.isNotEmpty())
            line (message, 340.0f, 30.0f, Dyt::font (12.0f, true), messageIsError ? Dyt::ink : Dyt::signalBlue);
    }

    // Brand footer inside the card
    g.setColour (Dyt::signalBlue);
    g.fillRect (card.getX() + 28.0f, card.getBottom() - 14.0f, card.getWidth() - 56.0f, 3.0f);
}

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>

// ─────────────────────────────────────────────────────────────────────────────
//  Licensing for The Engine Room.
//
//  Keys are Ed25519-signed by Discover Your Tone and checked offline against
//  the public key in LicencePublicKey.h. No internet, no server, no account.
//
//  Key format:  DYT-ER-<base64url payload>.<base64url signature>
//  Payload:     1|ER|<F=founder,S=standard>|<serial>|<name>|<email>|<issued>
// ─────────────────────────────────────────────────────────────────────────────

struct LicenceInfo
{
    bool valid = false;
    juce::String error;        // friendly reason when not valid
    bool founder = false;
    int serial = 0;
    juce::String name, email, issued, key;

    juce::String badge() const   // e.g. "FOUNDER #007"
    {
        return founder ? "FOUNDER #" + juce::String (serial).paddedLeft ('0', 3)
                       : "LICENCE #" + juce::String (serial);
    }

    static LicenceInfo fromText (const juce::String& anyText);   // key or .dytlicence contents
};

class LicenceManager : public juce::ChangeBroadcaster
{
public:
    enum class Status { licensed = 0, trial, trialEnded };

    static constexpr int trialDays = 14;

    LicenceManager();

    Status getStatus() const noexcept   { return (Status) status.load(); }
    bool   isLicensed() const noexcept  { return getStatus() == Status::licensed; }
    int    getTrialDaysLeft() const;
    LicenceInfo getInfo() const;

    // Returns an empty string on success, otherwise a friendly error.
    juce::String activate (const juce::String& keyOrFileText);
    void deactivate();

    static juce::File getStorageFile();
    static void setStorageFileForTesting (const juce::File&);   // tests only

private:
    void load();
    void save() const;
    void refreshStatus();

    mutable juce::CriticalSection lock;
    LicenceInfo info;
    juce::Time trialStart, lastSeen;
    std::atomic<int> status { (int) Status::trial };

    JUCE_DECLARE_NON_COPYABLE (LicenceManager)
};

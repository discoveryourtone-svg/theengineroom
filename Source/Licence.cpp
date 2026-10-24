#include "Licence.h"
#include "LicencePublicKey.h"

extern "C"
{
   #include "../ThirdParty/monocypher/monocypher-ed25519.h"
}

namespace
{
    constexpr const char* kPrefix = "DYT-ER-";

    // Serials refunded or leaked. Add here and ship an update to revoke them.
    constexpr int kRevokedFounder[]  = { 0 };
    constexpr int kRevokedStandard[] = { 0 };

    juce::File& testFileOverride()
    {
        static juce::File f;
        return f;
    }

    // Strict, canonical base64url decoding (no padding, no stray bits).
    bool decodeB64u (const juce::String& s, juce::MemoryBlock& out)
    {
        static const juce::String alphabet ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
        if (s.length() % 4 == 1) return false;

        out.reset();
        juce::uint32 acc = 0;
        int bits = 0;
        for (auto c : s)
        {
            const int v = alphabet.indexOfChar (c);
            if (v < 0) return false;
            acc = (acc << 6) | (juce::uint32) v;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                const juce::uint8 byte = (juce::uint8) ((acc >> bits) & 0xff);
                out.append (&byte, 1);
            }
        }
        return (acc & ((1u << bits) - 1u)) == 0;   // leftover bits must be zero
    }

    template <size_t N>
    bool contains (const int (&list)[N], int v)
    {
        for (auto x : list) if (x != 0 && x == v) return true;
        return false;
    }
}

//==============================================================================
LicenceInfo LicenceInfo::fromText (const juce::String& anyText)
{
    LicenceInfo r;

    // Find the key, wherever it is (a pasted email, a .dytlicence file, ...)
    const int start = anyText.indexOf (kPrefix);
    if (start < 0)
    {
        r.error = anyText.trim().isEmpty() ? "Paste your licence key first."
                                           : "That doesn't look like an Engine Room key. It starts with DYT-ER-";
        return r;
    }

    juce::String key;
    for (int i = start; i < anyText.length(); ++i)
    {
        const auto c = anyText[i];
        if (juce::CharacterFunctions::isLetterOrDigit (c) || c == '-' || c == '_' || c == '.')
            key << c;
        else if (! juce::CharacterFunctions::isWhitespace (c))
            break;                                  // keys may be wrapped over lines
        else if (c == '\n' && i + 1 < anyText.length() && anyText[i + 1] == '\n')
            break;
    }

    const auto body = key.fromFirstOccurrenceOf (kPrefix, false, false);
    const auto p = body.upToFirstOccurrenceOf (".", false, false);
    const auto s = body.fromFirstOccurrenceOf (".", false, false);

    juce::MemoryBlock payload, sig;
    if (! body.containsChar ('.') || ! decodeB64u (p, payload) || ! decodeB64u (s, sig) || sig.getSize() != 64)
    {
        r.error = "That key looks incomplete. Copy the whole thing, from DYT-ER- to the end.";
        return r;
    }

    if (crypto_ed25519_check ((const uint8_t*) sig.getData(), LicencePublicKey::bytes,
                              (const uint8_t*) payload.getData(), payload.getSize()) != 0)
    {
        r.error = "That key isn't valid. Check for typos, or paste it straight from your email.";
        return r;
    }

    const auto fields = juce::StringArray::fromTokens (payload.toString(), "|", "");
    if (fields.size() != 7 || fields[0] != "1" || fields[1] != "ER"
        || (fields[2] != "F" && fields[2] != "S"))
    {
        r.error = "That key is for a different product or a newer version. Update the plugin and try again.";
        return r;
    }

    r.founder = fields[2] == "F";
    r.serial  = fields[3].getIntValue();
    r.name    = fields[4];
    r.email   = fields[5];
    r.issued  = fields[6];
    r.key     = key;

    if (contains (r.founder ? kRevokedFounder : kRevokedStandard, r.serial))
    {
        r.error = "This key has been switched off. Get in touch at discoveryourtone.com if that's a surprise.";
        return r;
    }

    r.valid = true;
    return r;
}

//==============================================================================
juce::File LicenceManager::getStorageFile()
{
    if (testFileOverride() != juce::File())
        return testFileOverride();

    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    dir = dir.getChildFile ("Application Support");
   #endif
    return dir.getChildFile ("Discover Your Tone").getChildFile ("The Engine Room").getChildFile ("licence.json");
}

void LicenceManager::setStorageFileForTesting (const juce::File& f) { testFileOverride() = f; }

LicenceManager::LicenceManager()
{
    load();
}

void LicenceManager::load()
{
    const juce::ScopedLock sl (lock);
    const auto now = juce::Time::getCurrentTime();
    auto json = juce::JSON::parse (getStorageFile());

    info = LicenceInfo::fromText (json.getProperty ("key", "").toString());

    const auto ts = json.getProperty ("trialStart", "").toString();
    const auto ls = json.getProperty ("lastSeen", "").toString();
    trialStart = ts.isNotEmpty() ? juce::Time::fromISO8601 (ts) : now;
    lastSeen   = ls.isNotEmpty() ? juce::Time::fromISO8601 (ls) : now;

    if (now > lastSeen) lastSeen = now;
    refreshStatus();
    save();
}

void LicenceManager::save() const
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("key", info.valid ? info.key : juce::String());
    obj->setProperty ("trialStart", trialStart.toISO8601 (true));
    obj->setProperty ("lastSeen", lastSeen.toISO8601 (true));

    const auto f = getStorageFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (juce::var (obj.release())));
}

void LicenceManager::refreshStatus()
{
    if (info.valid)
        status = (int) Status::licensed;
    else
    {
        // Clock wound back more than a day = trial over.
        const bool clockRewound = juce::Time::getCurrentTime() < lastSeen - juce::RelativeTime::days (1);
        status = (int) ((getTrialDaysLeft() > 0 && ! clockRewound) ? Status::trial : Status::trialEnded);
    }
}

int LicenceManager::getTrialDaysLeft() const
{
    const juce::ScopedLock sl (lock);
    const double used = (juce::Time::getCurrentTime() - trialStart).inDays();
    return juce::jlimit (0, trialDays, (int) std::ceil ((double) trialDays - used));
}

LicenceInfo LicenceManager::getInfo() const
{
    const juce::ScopedLock sl (lock);
    return info;
}

juce::String LicenceManager::activate (const juce::String& text)
{
    auto candidate = LicenceInfo::fromText (text);
    if (! candidate.valid)
        return candidate.error;

    {
        const juce::ScopedLock sl (lock);
        info = candidate;
        refreshStatus();
        save();
    }
    sendChangeMessage();
    return {};
}

void LicenceManager::deactivate()
{
    {
        const juce::ScopedLock sl (lock);
        info = {};
        refreshStatus();
        save();
    }
    sendChangeMessage();
}

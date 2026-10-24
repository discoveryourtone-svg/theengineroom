// Offline test tool: renders a guitar riff through each mode, checks bypass,
// measures CPU, and snapshots the interface to PNG.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

using namespace juce;

static AudioBuffer<float> makeRiff (double sr, float seconds)
{
    // Karplus-Strong plucks: a little E-power-chord riff with single notes.
    const int n = (int) (sr * seconds);
    AudioBuffer<float> buf (2, n);
    buf.clear();
    Random rng (42);

    struct Note { float t, hz, amp, len; };
    std::vector<Note> notes;
    const float e2 = 82.41f, b2 = 123.47f, e3 = 164.81f, g2 = 98.0f, a2 = 110.0f, d3 = 146.83f;
    auto chord = [&] (float t, float root, float fifth, float oct, float len)
    {
        notes.push_back ({ t, root, 0.30f, len });
        notes.push_back ({ t + 0.006f, fifth, 0.24f, len });
        notes.push_back ({ t + 0.012f, oct, 0.20f, len });
    };
    chord (0.00f, e2, b2, e3, 0.9f);
    chord (1.00f, g2, d3, g2 * 2, 0.45f);
    chord (1.50f, a2, e3, a2 * 2, 0.9f);
    for (int i = 0; i < 6; ++i) notes.push_back ({ 2.6f + i * 0.16f, e2, 0.28f, 0.14f }); // chugs
    notes.push_back ({ 3.7f, 329.63f, 0.25f, 0.5f });
    notes.push_back ({ 4.2f, 392.0f, 0.25f, 0.5f });
    notes.push_back ({ 4.7f, 440.0f, 0.25f, 1.8f });   // long note: hear the sustain / sputter

    for (auto& nt : notes)
    {
        const int start = (int) (nt.t * sr);
        const int period = (int) (sr / nt.hz);
        std::vector<float> line ((size_t) period);
        for (auto& v : line) v = (rng.nextFloat() * 2.0f - 1.0f);
        const int len = (int) (nt.len * sr);
        const float decay = nt.len < 0.3f ? 0.990f : 0.9975f;
        size_t idx = 0;
        for (int i = 0; i < len && start + i < n; ++i)
        {
            const size_t nxt = (idx + 1) % line.size();
            const float out = line[idx];
            line[idx] = decay * 0.5f * (line[idx] + line[nxt]);
            idx = nxt;
            const float fadeOut = i > len - 400 ? (float) (len - i) / 400.0f : 1.0f;
            const float s = out * nt.amp * fadeOut;
            buf.addSample (0, start + i, s);
            buf.addSample (1, start + i, s);
        }
    }
    return buf;
}

static void writeWav (const File& f, const AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static void setParam (EngineRoomProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static float runThrough (EngineRoomProcessor& p, AudioBuffer<float>& buf, int block, float& maxPush)
{
    MidiBuffer midi;
    maxPush = -60.0f;
    for (int pos = 0; pos < buf.getNumSamples(); pos += block)
    {
        const int len = jmin (block, buf.getNumSamples() - pos);
        AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, len);
        p.processBlock (view, midi);
        maxPush = jmax (maxPush, p.getPushDb());
    }
    return buf.getMagnitude (0, buf.getNumSamples());
}

//==============================================================================
static int failures = 0;
static void check (bool ok, const String& what)
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (! ok) ++failures;
}

static void writeLicenceState (const File& f, const String& key, RelativeTime trialAge, RelativeTime lastSeenOffset = {})
{
    auto obj = std::make_unique<DynamicObject>();
    obj->setProperty ("key", key);
    obj->setProperty ("trialStart", (Time::getCurrentTime() - trialAge).toISO8601 (true));
    obj->setProperty ("lastSeen", (Time::getCurrentTime() + lastSeenOffset).toISO8601 (true));
    f.replaceWithText (JSON::toString (var (obj.release())));
}

static void snapshot (Component& c, const File& f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 2.0f);
    f.deleteFile();
    FileOutputStream fos (f);
    PNGImageFormat().writeImageToStream (img, fos);
}

static void runLicenceTests (const File& out, const File& ledger, const AudioBuffer<float>& dry, double sr, int block)
{
    std::cout << "\nLicence tests\n";
    const auto store = LicenceManager::getStorageFile();

    // Pull a real issued key (and its name) from the issuing tool's ledger
    StringArray rows;
    ledger.readLines (rows);
    rows.removeEmptyStrings();
    check (rows.size() >= 2, "ledger has at least one issued key");
    const auto key  = rows[1].fromLastOccurrenceOf (",", false, false).trim();
    const auto name = StringArray::fromTokens (rows[1], ",", "\"")[2];

    // 1. Fresh install = 14-day trial
    store.deleteFile();
    {
        LicenceManager m;
        check (m.getStatus() == LicenceManager::Status::trial, "fresh install starts the trial");
        check (m.getTrialDaysLeft() == 14, "trial has 14 days");
    }

    // 2. Valid key
    {
        LicenceManager m;
        const auto err = m.activate (key);
        check (err.isEmpty() && m.isLicensed(), "issued key activates");
        const auto info = m.getInfo();
        check (info.founder && info.serial == 1 && info.name == name, "key carries founder #001 and buyer name (" + info.badge() + ", " + info.name + ")");
    }
    {
        LicenceManager m;
        check (m.isLicensed(), "licence survives restart");
    }
    {
        // Every key in the ledger (made by both the Python tool and Licence Desk) must work,
        // including names with accents.
        int ok = 0, total = 0;
        for (int i = 1; i < rows.size(); ++i)
        {
            const auto fields = StringArray::fromTokens (rows[i], ",", "\"");
            const auto info = LicenceInfo::fromText (rows[i].fromLastOccurrenceOf (",", false, false));
            ++total;
            if (info.valid && info.name == fields[2] && info.serial == fields[0].getIntValue()) ++ok;
            else std::cout << "        mismatch: " << rows[i] << "\n";
        }
        check (ok == total, "all " + String (total) + " ledger keys verify in the plugin with the right name and number");
    }

    // 3. Things that must be rejected / accepted
    auto rejects = [] (const String& text) { return ! LicenceInfo::fromText (text).valid; };
    String tampered = key;
    const int sigPos = key.indexOfChar ('.') + 10;
    tampered = key.replaceSection (sigPos, 1, key[sigPos] == 'A' ? "B" : "A");
    check (rejects (tampered), "one changed character in the signature is rejected");

    String payloadTamper = key;
    payloadTamper = key.replaceSection (12, 1, key[12] == 'A' ? "B" : "A");
    check (rejects (payloadTamper), "one changed character in the buyer details is rejected");
    check (rejects (key.dropLastCharacters (5)), "truncated key is rejected");
    check (rejects ("hello"), "random text is rejected");
    check (rejects (key.replace ("DYT-ER-", "DYT-XX-")), "wrong product prefix is rejected");

    int sweep = 0, accepted = 0;
    for (int i = 7; i < key.length(); ++i)
    {
        if (key[i] == '.') continue;
        ++sweep;
        if (! rejects (key.replaceSection (i, 1, key[i] == 'Q' ? "R" : "Q"))) ++accepted;
    }
    check (accepted == 0, "every single-character edit rejected (" + String (sweep) + " tried)");

    const auto email = "Hey Sam,\n\nHere's your key:\n\n" + key.substring (0, 60) + "\n" + key.substring (60) + "\n\nFind your sound,\nKyle";
    check (LicenceInfo::fromText (email).valid, "key pasted with the surrounding email text (and a line wrap) still works");
    const auto fileText = "# The Engine Room\n# Founder #001\n\n" + key + "\n";
    check (LicenceInfo::fromText (fileText).valid, ".dytlicence file contents work");

    // 4. Trial ended / clock tricks
    writeLicenceState (store, {}, RelativeTime::days (20));
    {
        LicenceManager m;
        check (m.getStatus() == LicenceManager::Status::trialEnded, "trial ends after 14 days");
    }
    writeLicenceState (store, {}, RelativeTime::days (2), RelativeTime::days (10));
    {
        LicenceManager m;
        check (m.getStatus() == LicenceManager::Status::trialEnded, "winding the clock back ends the trial");
    }
    writeLicenceState (store, key, RelativeTime::days (200));
    {
        LicenceManager m;
        check (m.isLicensed(), "licensed users never hit the trial limit");
    }

    // 5. After the trial, the pedal drops to clean for 4 s every minute
    writeLicenceState (store, {}, RelativeTime::days (30));
    {
        EngineRoomProcessor p;
        p.apvts.getParameter (ParamID::drive)->setValueNotifyingHost (0.8f);
        p.prepareToPlay (sr, block);
        const int lat = p.getLatencySamples();
        AudioBuffer<float> in (2, (int) sr * 61);
        for (int i = 0; i < in.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch)
                in.setSample (ch, i, dry.getSample (ch, i % dry.getNumSamples()));
        auto buf = in;
        MidiBuffer midi;
        for (int pos = 0; pos < buf.getNumSamples(); pos += block)
        {
            AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, jmin (block, buf.getNumSamples() - pos));
            p.processBlock (view, midi);
        }
        auto maxDiff = [&] (double t0, double t1)
        {
            double d = 0;
            for (int i = (int) (t0 * sr); i < (int) (t1 * sr); ++i)
                d = jmax (d, (double) std::abs (buf.getSample (0, i) - in.getSample (0, i - lat)));
            return d;
        };
        check (maxDiff (56.2, 59.8) < 1e-6, "expired trial: clean for 56-60 s");
        check (maxDiff (10.0, 50.0) > 0.01, "expired trial: pedal works the rest of the minute");
    }

    // 6. Snapshots of the activation flow
    store.deleteFile();
    {
        EngineRoomProcessor p;
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        auto& face = dynamic_cast<EngineRoomEditor*> (ed.get())->getFace();
        face.simulateReading (14.0f, 120);
        snapshot (*ed, out.getChildFile ("ui_trial_chip.png"));
        face.showActivation();
        snapshot (*ed, out.getChildFile ("ui_activate_trial.png"));
        face.getActivationPanel()->setKeyText ("DYT-ER-not-a-real-key");
        face.getActivationPanel()->pressActivate();
        snapshot (*ed, out.getChildFile ("ui_activate_error.png"));
        face.getActivationPanel()->setKeyText (email);
        face.getActivationPanel()->pressActivate();
        check (p.getLicence().isLicensed(), "activation through the panel works");
        snapshot (*ed, out.getChildFile ("ui_activate_founder.png"));
        face.hideActivation();
        snapshot (*ed, out.getChildFile ("ui_licensed_chip.png"));
    }
    writeLicenceState (store, {}, RelativeTime::days (30));
    {
        EngineRoomProcessor p;
        std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
        snapshot (*ed, out.getChildFile ("ui_trial_ended.png"));
    }

    std::cout << (failures == 0 ? "All licence tests passed\n" : "SOME LICENCE TESTS FAILED\n");
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File out (argc > 1 ? argv[1] : File::getCurrentWorkingDirectory().getFullPathName());
    out.createDirectory();

    // Never touch the real licence file from tests
    LicenceManager::setStorageFileForTesting (out.getChildFile ("test_licence.json"));
    out.getChildFile ("test_licence.json").deleteFile();

    const double sr = 48000.0;
    const int block = 256;
    const auto dry = makeRiff (sr, 7.0f);
    writeWav (out.getChildFile ("00_clean_DI.wav"), dry, sr);
    std::cout << "Clean DI peak " << Decibels::gainToDecibels (dry.getMagnitude (0, dry.getNumSamples())) << " dBFS, RMS "
              << Decibels::gainToDecibels (dry.getRMSLevel (0, 0, dry.getNumSamples())) << " dBFS\n";

    struct Case { const char* file; int prog; };
    const Case cases[] = { { "01_overdrive_first_crunch.wav", 0 }, { "02_overdrive_boost.wav", 1 },
                           { "03_distortion_rock_riff.wav", 2 }, { "04_distortion_wall.wav", 3 },
                           { "05_fuzz_fight.wav", 4 }, { "06_fuzz_sputter.wav", 5 } };

    for (auto& c : cases)
    {
        EngineRoomProcessor p;
        p.setCurrentProgram (c.prog);
        p.prepareToPlay (sr, block);
        auto buf = dry;
        float maxPush = 0.0f;
        const float peak = runThrough (p, buf, block, maxPush);
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                finite &= std::isfinite (buf.getSample (ch, i));
        writeWav (out.getChildFile (c.file), buf, sr);
        std::cout << p.getProgramName (c.prog).paddedRight (' ', 26) << " peak " << String (Decibels::gainToDecibels (peak), 1)
                  << " dBFS  RMS " << String (Decibels::gainToDecibels (buf.getRMSLevel (0, 0, buf.getNumSamples())), 1)
                  << " dBFS  gauge max " << String (maxPush, 1) << " dB  finite=" << (finite ? "yes" : "NO") << "\n";
    }

    // Drive sweep across all modes into one file (mode switch while playing)
    {
        EngineRoomProcessor p;
        p.prepareToPlay (sr, block);
        AudioBuffer<float> all (2, dry.getNumSamples() * 3);
        MidiBuffer midi;
        for (int m = 0; m < 3; ++m)
        {
            setParam (p, ParamID::mode, (float) m);
            auto buf = dry;
            for (int pos = 0; pos < buf.getNumSamples(); pos += block)
            {
                const int len = jmin (block, buf.getNumSamples() - pos);
                setParam (p, ParamID::drive, 10.0f * (float) pos / (float) buf.getNumSamples());
                AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, len);
                p.processBlock (view, midi);
            }
            for (int ch = 0; ch < 2; ++ch)
                all.copyFrom (ch, m * dry.getNumSamples(), buf, ch, 0, buf.getNumSamples());
        }
        writeWav (out.getChildFile ("07_drive_sweep_all_modes.wav"), all, sr);
    }

    // Bypass must be a latency-compensated copy of the input
    {
        EngineRoomProcessor p;
        setParam (p, ParamID::bypass, 1.0f);
        p.prepareToPlay (sr, block);
        auto buf = dry;
        float mp;
        runThrough (p, buf, block, mp);
        const int lat = p.getLatencySamples();
        double err = 0.0;
        for (int i = lat; i < buf.getNumSamples(); ++i)
            err = jmax (err, (double) std::abs (buf.getSample (0, i) - dry.getSample (0, i - lat)));
        std::cout << "Bypass: latency " << lat << " samples, max error vs input " << err
                  << (err < 1e-6 ? "  PASS" : "  FAIL") << "\n";
    }

    // CPU: 60 s of audio, worst mode (fuzz, full drive)
    {
        EngineRoomProcessor p;
        setParam (p, ParamID::mode, 2.0f);
        setParam (p, ParamID::drive, 10.0f);
        p.prepareToPlay (sr, block);
        AudioBuffer<float> buf (2, (int) sr * 60);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            buf.setSample (0, i, dry.getSample (0, i % dry.getNumSamples())),
            buf.setSample (1, i, dry.getSample (1, i % dry.getNumSamples()));
        const auto t0 = Time::getMillisecondCounterHiRes();
        float mp;
        runThrough (p, buf, block, mp);
        const auto ms = Time::getMillisecondCounterHiRes() - t0;
        std::cout << "CPU: 60 s stereo processed in " << String (ms, 1) << " ms ("
                  << String (100.0 * ms / 60000.0, 2) << "% of one core, unoptimised build may vary)\n";
    }

    // Interface snapshots
    {
        struct Shot { const char* file; int prog; float push; bool bypass; };
        const Shot shots[] = { { "ui_overdrive.png", 0, 12.0f, false },
                               { "ui_distortion.png", 2, 30.0f, false },
                               { "ui_fuzz.png", 4, 38.0f, false },
                               { "ui_bypassed.png", 0, 0.0f, true } };
        for (auto& s : shots)
        {
            EngineRoomProcessor p;
            p.setCurrentProgram (s.prog);
            setParam (p, ParamID::bypass, s.bypass ? 1.0f : 0.0f);
            std::unique_ptr<AudioProcessorEditor> ed (p.createEditor());
            auto* er = dynamic_cast<EngineRoomEditor*> (ed.get());
            er->getFace().simulateReading (s.push, 120);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            out.getChildFile (s.file).deleteFile();
            FileOutputStream fos (out.getChildFile (s.file));
            PNGImageFormat().writeImageToStream (img, fos);
        }
        std::cout << "UI snapshots written\n";
    }
    if (argc > 2)
        runLicenceTests (out, File (argv[2]), dry, sr, block);
    return failures == 0 ? 0 : 1;
}

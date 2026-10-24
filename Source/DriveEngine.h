#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <optional>

// ─────────────────────────────────────────────────────────────────────────────
//  DriveEngine — the three Engine Room circuits.
//
//  OVERDRIVE   Mid-hump soft clipping in the feedback loop. The clean signal is
//              always mixed back in, so your playing stays clear. (Classic
//              green-pedal topology.)
//  DISTORTION  Tight low cut, big gain, hard diode clipping to ground. Flat-top
//              squared-off waves = sustain and chug.
//  FUZZ        Two-transistor style: huge gain, lopsided (asymmetric) clipping
//              and a natural gate that sputters as notes die away.
//
//  All clipping runs 4x oversampled to keep it smooth and alias-free.
// ─────────────────────────────────────────────────────────────────────────────

class DriveEngine
{
public:
    enum class Mode { overdrive = 0, distortion, fuzz };

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // All in 0..1 except mode.
    void setDrive (float d)  { driveTarget = juce::jlimit (0.0f, 1.0f, d); }
    void setTone  (float t)  { toneTarget  = juce::jlimit (0.0f, 1.0f, t); }
    void setOutputGain (float linear) { outGainTarget = linear; }
    void setMode (Mode m);

    void process (juce::AudioBuffer<float>& buffer);

    int  getLatencySamples() const;

    // How hard the clipping stage is being pushed, in dB relative to the clip
    // point (0 dB = just starting to break up). Read by the boost gauge.
    float getPushDb() const noexcept { return pushDb.load (std::memory_order_relaxed); }

private:
    struct OnePole
    {
        float a = 0.0f, z = 0.0f;
        void setLowpass (float cutoffHz, float fs)
        {
            a = std::exp (-juce::MathConstants<float>::twoPi * cutoffHz / fs);
        }
        inline float lp (float x) noexcept { z = x + a * (z - x); return z; }
        inline float hp (float x) noexcept { return x - lp (x); }
        void reset() noexcept { z = 0.0f; }
    };

    struct ChannelState
    {
        OnePole preHp, preLp, postLp, dcBlock, toneLp, toneLp2, gateEnv;
        float fuzzEnv = 0.0f;
    };

    float shape (float x, float gain, ChannelState& st) noexcept;
    void  updateFilters();
    float modeTrim (float drive) const noexcept;
    static float gainForDrive (Mode m, float drive) noexcept;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    std::vector<ChannelState> channels;

    double baseRate = 44100.0, osRate = 176400.0;
    Mode mode = Mode::overdrive;
    std::optional<Mode> pendingMode;

    float driveTarget = 0.5f, toneTarget = 0.5f, outGainTarget = 1.0f;
    juce::SmoothedValue<float> driveSmoothed, toneSmoothed, outGainSmoothed;
    juce::SmoothedValue<float> modeFade; // short dip when switching modes
    float lastToneCutoff = -1.0f;

    float pushEnv = 0.0f, pushAttack = 0.0f, pushRelease = 0.0f;
    std::atomic<float> pushDb { -60.0f };
};

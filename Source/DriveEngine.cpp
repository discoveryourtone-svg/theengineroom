#include "DriveEngine.h"

namespace
{
    constexpr int   kOversamplingFactor = 2;      // 2^2 = 4x
    constexpr float kModeFadeSeconds    = 0.012f;

    inline float smoothstep (float e0, float e1, float x) noexcept
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
        return t * t * (3.0f - 2.0f * t);
    }

    // Knee of each clipper, in the engine's internal units. Used so the gauge
    // reads "0" at the point each circuit starts to break up.
    inline float kneeFor (DriveEngine::Mode m) noexcept
    {
        switch (m)
        {
            case DriveEngine::Mode::overdrive:  return 0.45f;
            case DriveEngine::Mode::distortion: return 0.8f;
            case DriveEngine::Mode::fuzz:       return 0.8f;
        }
        return 1.0f;
    }
}

//==============================================================================
void DriveEngine::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    baseRate = sampleRate;

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannels, (size_t) kOversamplingFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) maxBlockSize);
    osRate = baseRate * oversampler->getOversamplingFactor();

    channels.assign ((size_t) numChannels, ChannelState{});

    if (pendingMode.has_value()) { mode = *pendingMode; pendingMode.reset(); }

    driveSmoothed.reset (baseRate, 0.05);
    toneSmoothed.reset (baseRate, 0.05);
    outGainSmoothed.reset (baseRate, 0.05);
    modeFade.reset (baseRate, kModeFadeSeconds);

    driveSmoothed.setCurrentAndTargetValue (driveTarget);
    toneSmoothed.setCurrentAndTargetValue (toneTarget);
    outGainSmoothed.setCurrentAndTargetValue (outGainTarget);
    modeFade.setCurrentAndTargetValue (1.0f);

    // Gauge ballistics, like a real needle: quick-ish rise, slow fall.
    pushAttack  = std::exp (-1.0f / (0.010f * (float) baseRate));
    pushRelease = std::exp (-1.0f / (0.350f * (float) baseRate));

    lastToneCutoff = -1.0f;
    updateFilters();
    reset();
}

void DriveEngine::reset()
{
    if (oversampler) oversampler->reset();
    for (auto& c : channels)
    {
        c.preHp.reset(); c.preLp.reset(); c.postLp.reset(); c.dcBlock.reset();
        c.toneLp.reset(); c.toneLp2.reset(); c.gateEnv.reset();
        c.fuzzEnv = 0.0f;
    }
    pushEnv = 0.0f;
    pushDb.store (-60.0f);
}

int DriveEngine::getLatencySamples() const
{
    return oversampler ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
}

void DriveEngine::setMode (Mode m)
{
    const Mode heading = pendingMode.value_or (mode);
    if (m == heading) return;

    if (m == mode) { pendingMode.reset(); modeFade.setTargetValue (1.0f); }
    else           { pendingMode = m;     modeFade.setTargetValue (0.0f); }
}

//==============================================================================
float DriveEngine::gainForDrive (Mode m, float d) noexcept
{
    switch (m)
    {
        case Mode::overdrive:  return 1.0f + 119.0f * std::pow (d, 2.2f);   // 1x .. 120x
        case Mode::distortion: return 3.0f * std::pow (300.0f, d);           // 3x .. 900x
        case Mode::fuzz:       return 8.0f * std::pow (100.0f, d);           // 8x .. 800x
    }
    return 1.0f;
}

// Auto level-matching: keeps "LEVEL 5" close to your bypassed volume
// whatever the drive setting, so turning up DRIVE adds grit, not just volume.
float DriveEngine::modeTrim (float d) const noexcept
{
    switch (mode)
    {
        case Mode::overdrive:  return std::pow (0.09f, d);
        case Mode::distortion: return 0.17f + 0.4f * (1.0f - d) * (1.0f - d) * (1.0f - d);
        case Mode::fuzz:       return 0.22f;
    }
    return 1.0f;
}

void DriveEngine::updateFilters()
{
    const auto fs = (float) osRate;
    for (auto& c : channels)
    {
        switch (mode)
        {
            case Mode::overdrive:
                c.preHp.setLowpass (720.0f, fs);   // clipping branch only: the mid hump
                c.postLp.setLowpass (6000.0f, fs);
                break;
            case Mode::distortion:
                c.preHp.setLowpass (90.0f, fs);
                c.postLp.setLowpass (4800.0f, fs);
                break;
            case Mode::fuzz:
                c.preHp.setLowpass (45.0f, fs);
                c.postLp.setLowpass (4200.0f, fs);
                break;
        }
        c.dcBlock.setLowpass (12.0f, fs);
        c.gateEnv.setLowpass (12.0f, fs);
    }
}

//==============================================================================
float DriveEngine::shape (float x, float gain, ChannelState& st) noexcept
{
    switch (mode)
    {
        case Mode::overdrive:
        {
            // Clean + soft-clipped boosted mids (diodes in the op-amp feedback).
            const float knee = kneeFor (mode);
            const float branch = st.preHp.hp (x) * (gain - 1.0f);
            const float clipped = knee * std::tanh (branch / knee);
            return st.postLp.lp (x + clipped);
        }

        case Mode::distortion:
        {
            // Hard-ish diodes to ground: flat tops, lots of sustain.
            const float v = st.preHp.hp (x) * gain;
            const float a = std::abs (v);
            const float y = v / std::pow (1.0f + std::pow (a, 2.5f), 0.4f);
            return st.postLp.lp (y);
        }

        case Mode::fuzz:
        {
            // Envelope-driven bias shift = the lopsided, spitty fuzz character.
            const float v = st.preHp.hp (x) * gain;
            st.fuzzEnv = st.gateEnv.lp (std::abs (x));
            const float bias = 0.18f + 0.25f * juce::jlimit (0.0f, 1.0f, st.fuzzEnv * 8.0f);
            const float a = v + bias;
            float y = a > 0.0f ? std::tanh (a)
                               : -0.55f * (1.0f - std::exp (a / 0.55f));
            y = st.dcBlock.hp (y);
            // Natural gate: dying notes break up and sputter out.
            y *= smoothstep (0.0008f, 0.006f, st.fuzzEnv);
            return st.postLp.lp (y);
        }
    }
    return x;
}

//==============================================================================
void DriveEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = juce::jmin (buffer.getNumChannels(), (int) channels.size());
    if (numSamples == 0 || numChannels == 0) return;

    // Mode switch happens once the fade-out has reached silence.
    if (pendingMode.has_value() && modeFade.getCurrentValue() <= 0.0f)
    {
        mode = *pendingMode;
        pendingMode.reset();
        updateFilters();
        for (auto& c : channels) { c.preHp.reset(); c.postLp.reset(); c.dcBlock.reset(); }
        modeFade.setTargetValue (1.0f);
    }

    driveSmoothed.setTargetValue (driveTarget);
    toneSmoothed.setTargetValue (toneTarget);
    outGainSmoothed.setTargetValue (outGainTarget);

    const float driveStart = driveSmoothed.getCurrentValue();
    driveSmoothed.skip (numSamples);
    const float driveEnd = driveSmoothed.getCurrentValue();
    const float gStart = gainForDrive (mode, driveStart);
    const float gEnd   = gainForDrive (mode, driveEnd);

    // Measure push before clipping (input envelope x gain), mono-summed.
    {
        const float knee = kneeFor (mode);
        float env = pushEnv;
        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                s = std::max (s, std::abs (buffer.getSample (ch, i)));
            const float g = gStart + (gEnd - gStart) * ((float) i / (float) numSamples);
            const float v = s * g * (mode == Mode::overdrive ? 0.6f : 1.0f);
            const float coeff = v > env ? pushAttack : pushRelease;
            env = v + coeff * (env - v);
        }
        pushEnv = env;
        pushDb.store (juce::Decibels::gainToDecibels (env / knee, -60.0f), std::memory_order_relaxed);
    }

    // ── Oversampled clipping ────────────────────────────────────────────────
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numChannels, (size_t) numSamples);
    auto osBlock = oversampler->processSamplesUp (block);
    const int osSamples = (int) osBlock.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* d = osBlock.getChannelPointer ((size_t) ch);
        auto& st = channels[(size_t) ch];
        for (int i = 0; i < osSamples; ++i)
        {
            const float g = gStart + (gEnd - gStart) * ((float) i / (float) osSamples);
            d[i] = shape (d[i], g, st);
        }
    }

    oversampler->processSamplesDown (block);

    // ── Tone + level at base rate ───────────────────────────────────────────
    const float trim = modeTrim (driveEnd);
    for (int i = 0; i < numSamples; ++i)
    {
        const float t = toneSmoothed.getNextValue();
        const float out = outGainSmoothed.getNextValue() * modeFade.getNextValue() * trim;

        // 500 Hz (dark) .. 12 kHz (bright), recomputed only when it moves.
        const float cutoff = 500.0f * std::pow (24.0f, t);
        if (std::abs (cutoff - lastToneCutoff) > 1.0f)
        {
            for (auto& c : channels)
            {
                c.toneLp.setLowpass (cutoff, (float) baseRate);
                c.toneLp2.setLowpass (cutoff, (float) baseRate);
            }
            lastToneCutoff = cutoff;
        }

        const float presence = juce::jmax (0.0f, t - 0.5f) * 0.6f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& st = channels[(size_t) ch];
            const float x  = buffer.getSample (ch, i);
            const float lp = st.toneLp2.lp (st.toneLp.lp (x));
            buffer.setSample (ch, i, (lp + presence * (x - lp)) * out);
        }
    }
}

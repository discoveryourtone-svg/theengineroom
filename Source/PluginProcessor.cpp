#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iterator>

namespace
{
    struct StarterTone { const char* name; int mode; float drive, tone, level; };

    // Each one maps to a lesson moment in Jump Start Guitarist.
    const StarterTone kStarterTones[] =
    {
        { "Starter: First Crunch",   0, 3.5f, 6.0f, 5.0f },
        { "Starter: Boost the Amp",  0, 1.0f, 6.5f, 7.0f },
        { "Starter: Rock Riff",      1, 6.0f, 5.5f, 5.0f },
        { "Starter: Wall of Sound",  1, 8.5f, 4.5f, 4.5f },
        { "Starter: Fuzz Fight",     2, 7.0f, 4.5f, 5.0f },
        { "Starter: Sputter",        2, 2.5f, 5.0f, 5.5f },
    };
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EngineRoomProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto oneDecimal = AudioParameterFloatAttributes()
                          .withStringFromValueFunction ([] (float v, int) { return String (v, 1); });

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::drive, 1 }, "Drive",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f, oneDecimal));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::tone, 1 }, "Tone",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.5f, oneDecimal));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::level, 1 }, "Level",
                                                        NormalisableRange<float> (0.0f, 10.0f, 0.01f), 5.0f, oneDecimal));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode, 1 }, "Mode",
                                                         StringArray { "Overdrive", "Distortion", "Fuzz" }, 0));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamID::bypass, 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

float EngineRoomProcessor::levelToGain (float level)
{
    // 5 = same volume as bypassed, 10 = +12 dB, 0 = -40 dB.
    const float db = level >= 5.0f ? (level - 5.0f) * 2.4f
                                   : -40.0f * std::pow ((5.0f - level) / 5.0f, 1.6f);
    return juce::Decibels::decibelsToGain (db);
}

//==============================================================================
EngineRoomProcessor::EngineRoomProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EngineRoom", createLayout())
{
    driveParam  = apvts.getRawParameterValue (ParamID::drive);
    toneParam   = apvts.getRawParameterValue (ParamID::tone);
    levelParam  = apvts.getRawParameterValue (ParamID::level);
    modeParam   = apvts.getRawParameterValue (ParamID::mode);
    bypassParam = apvts.getRawParameterValue (ParamID::bypass);
}

bool EngineRoomProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void EngineRoomProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = getTotalNumOutputChannels();

    engine.setMode ((DriveEngine::Mode) (int) modeParam->load());
    engine.setDrive (driveParam->load() / 10.0f);
    engine.setTone (toneParam->load() / 10.0f);
    engine.setOutputGain (levelToGain (levelParam->load()));
    engine.prepare (sampleRate, samplesPerBlock, numCh);

    const int latency = engine.getLatencySamples();
    setLatencySamples (latency);

    dryDelay.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    dryDelay.setMaximumDelayInSamples (juce::jmax (1, latency + 1));
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();
    dryBuffer.setSize (numCh, samplesPerBlock);

    currentSampleRate = sampleRate;
    samplesPlayed = 0;

    wetMix.reset (sampleRate, 0.02);
    wetMix.setCurrentAndTargetValue (isEngaged() ? 1.0f : 0.0f);
}

void EngineRoomProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    engine.setMode ((DriveEngine::Mode) (int) modeParam->load());
    engine.setDrive (driveParam->load() / 10.0f);
    engine.setTone (toneParam->load() / 10.0f);
    engine.setOutputGain (levelToGain (levelParam->load()));
    wetMix.setTargetValue (isEngaged() && ! inDemoPause() ? 1.0f : 0.0f);
    samplesPlayed += numSamples;

    // Latency-matched dry copy, so bypass is a click-free crossfade.
    if (dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize (numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* src = buffer.getReadPointer (ch);
        auto* dst = dryBuffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            dryDelay.pushSample (ch, src[i]);
            dst[i] = dryDelay.popSample (ch);
        }
    }

    engine.process (buffer);

    if (! wetMix.isSmoothing() && wetMix.getTargetValue() >= 1.0f)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float w = wetMix.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, w * buffer.getSample (ch, i) + (1.0f - w) * dryBuffer.getSample (ch, i));
    }
}

bool EngineRoomProcessor::inDemoPause() const noexcept
{
    if (licence->getStatus() != LicenceManager::Status::trialEnded)
        return false;
    const double t = std::fmod ((double) samplesPlayed / currentSampleRate, 60.0);
    return t >= 56.0;
}

//==============================================================================
int EngineRoomProcessor::getNumPrograms() { return (int) std::size (kStarterTones); }

const juce::String EngineRoomProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? kStarterTones[index].name : "";
}

void EngineRoomProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms())) return;
    currentProgram = index;
    const auto& t = kStarterTones[index];

    auto set = [this] (const char* id, float plainValue)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
    };
    set (ParamID::mode, (float) t.mode);
    set (ParamID::drive, t.drive);
    set (ParamID::tone, t.tone);
    set (ParamID::level, t.level);
}

juce::AudioProcessorParameter* EngineRoomProcessor::getBypassParameter() const
{
    return apvts.getParameter (ParamID::bypass);
}

void EngineRoomProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EngineRoomProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", -1);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* EngineRoomProcessor::createEditor()
{
    return new EngineRoomEditor (*this);
}

#ifndef ER_STANDALONE_TOOL
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EngineRoomProcessor();
}
#endif

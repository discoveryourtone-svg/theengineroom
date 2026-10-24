#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DriveEngine.h"
#include "Licence.h"

namespace ParamID
{
    inline constexpr const char* drive  = "drive";
    inline constexpr const char* tone   = "tone";
    inline constexpr const char* level  = "level";
    inline constexpr const char* mode   = "mode";
    inline constexpr const char* bypass = "bypass";
}

class EngineRoomProcessor : public juce::AudioProcessor
{
public:
    EngineRoomProcessor();
    ~EngineRoomProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "The Engine Room"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Starter tones (the same ones taught in the course)
    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, currentProgram); }
    int getChosenProgram() const { return currentProgram; }   // -1 = none picked yet
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // For the UI
    juce::AudioProcessorValueTreeState apvts;
    float getPushDb() const noexcept { return engine.getPushDb(); }
    bool  isEngaged() const noexcept { return bypassParam != nullptr && bypassParam->load() < 0.5f; }

    LicenceManager& getLicence() { return *licence; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static float levelToGain (float level010);

private:
    DriveEngine engine;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> wetMix;

    std::atomic<float>* driveParam  = nullptr;
    std::atomic<float>* toneParam   = nullptr;
    std::atomic<float>* levelParam  = nullptr;
    std::atomic<float>* modeParam   = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    int currentProgram = -1;

    // After the trial: the pedal drops to clean for the last 4 s of every minute.
    juce::SharedResourcePointer<LicenceManager> licence;
    juce::int64 samplesPlayed = 0;
    double currentSampleRate = 44100.0;
    bool inDemoPause() const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EngineRoomProcessor)
};

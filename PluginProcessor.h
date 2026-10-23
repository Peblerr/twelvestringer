#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "TwelveStringEngine.h"

namespace ParamID
{
    inline constexpr const char* octave  = "octave";
    inline constexpr const char* focus   = "focus";
    inline constexpr const char* sparkle = "sparkle";
    inline constexpr const char* unison  = "unison";
    inline constexpr const char* detune  = "detune";
    inline constexpr const char* drift   = "drift";
    inline constexpr const char* strum   = "strum";
    inline constexpr const char* width   = "width";
    inline constexpr const char* dry     = "dry";
    inline constexpr const char* output  = "output";
    inline constexpr const char* live    = "live";
}

class TwelveStringerProcessor : public juce::AudioProcessor
{
public:
    TwelveStringerProcessor();
    ~TwelveStringerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    twelve::Params readParams() const;

    twelve::TwelveStringEngine engine;
    int currentProgram = 0;

    std::atomic<float>* pOctave  = nullptr;
    std::atomic<float>* pFocus   = nullptr;
    std::atomic<float>* pSparkle = nullptr;
    std::atomic<float>* pUnison  = nullptr;
    std::atomic<float>* pDetune  = nullptr;
    std::atomic<float>* pDrift   = nullptr;
    std::atomic<float>* pStrum   = nullptr;
    std::atomic<float>* pWidth   = nullptr;
    std::atomic<float>* pDry     = nullptr;
    std::atomic<float>* pOutput  = nullptr;
    std::atomic<float>* pLive    = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwelveStringerProcessor)
};

#pragma once
#include <JuceHeader.h>
#include <array>
#include "FXAudioVisualState.h"
class MusiquePitchShiftProcessor : public juce::AudioProcessor
{
public:
    MusiquePitchShiftProcessor();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return parameters; }
    const fx::AudioVisualState& getVisualState() const noexcept { return visualState; }
private:
    static constexpr int maxVoices = 4;

    float readSampleWithDelay(int channel, float delaySamples) const noexcept;
    static float grainEnvelope(float phase) noexcept;

    juce::AudioProcessorValueTreeState parameters;
    fx::AudioVisualState visualState;
    std::array<std::vector<float>, 2> ring;
    int writePos = 0;
    double sampleRate = 44100.0;
    int ringSize = 0;
    int grainSizeSamples = 2048;
    int baseDelaySamples = 256;
    int latencySamples = 1280;
    std::array<float, maxVoices> grainPhases {};
    float currentRatio = 1.0f;
    float currentMix = 1.0f;
    float currentOutputGain = 1.0f;
    float currentFormant = 0.5f;
    float currentSpread = 0.65f;
    float currentDetune = 10.0f;
    std::array<float, 2> formantLowState {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusiquePitchShiftProcessor)
};

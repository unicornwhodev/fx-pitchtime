#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "FXAudioVisualState.h"
#include "PitchTimeEngines.h"

struct PitchTimeSnapshot
{
    std::array<float, pitchtime::visualBinCount> bins {};
    float primary = 0.0f;
    float secondary = 0.0f;
    float tertiary = 0.0f;
    float quaternary = 0.0f;
    float detectedMidi = 0.0f;
    float targetMidi = 0.0f;
    float confidence = 0.0f;
    float latencyMs = 0.0f;
    bool flagA = false;
    bool flagB = false;
};

class MusiquePitchShiftProcessor : public juce::AudioProcessor
{
public:
    enum EngineIndex
    {
        pitchShift = 0,
        timeStretch,
        autoTune,
        formantShift,
        vibrato,
        numEngines
    };

    using SyncDivision = pitchtime::SyncDivision;

    MusiquePitchShiftProcessor();
    ~MusiquePitchShiftProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static const std::array<SyncDivision, 9>& getSyncDivisions() noexcept;
    static juce::StringArray getAllParameterIds();
    static void normalisePresetObject(juce::var& preset);

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override
    {
#if MUSIQUE_PITCHTIME_DSP_TESTS
        return "Musique PitchTime";
#else
        return JucePlugin_Name;
#endif
    }

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
    PitchTimeSnapshot getPitchTimeSnapshot() const noexcept;
    juce::String getSyncLabelForNormalised(float normalised) const;
    void postExternalStateChange();

private:
    static void ensureStateParamValue(juce::ValueTree& state, const char* paramId, const juce::var& value);
    static juce::var readStateParamValue(const juce::ValueTree& state, const char* paramId, const juce::var& fallback);
    static void normaliseStateTree(juce::ValueTree& state);
    static double resolveHostBpm(juce::AudioProcessor& processor);

    void resetAllEngines();
    void clearSnapshot() noexcept;
    void storeSnapshot(const pitchtime::PitchTimeEngineSnapshot& snapshot, float latencyMs) noexcept;
    pitchtime::PitchTimeParams buildParameterSnapshot() const;
    void updateLatencyForParams(const pitchtime::PitchTimeParams& params, double bpm);

    juce::AudioProcessorValueTreeState parameters;
    fx::AudioVisualState visualState;
    juce::AudioBuffer<float> wetBuffer;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputSmoothed;
    double preparedSampleRate = 44100.0;
    int lastEngineIndex = -1;
    int lastLatencySamples = 0;

    pitchtime::PitchShiftEngine pitchShiftEngineState;
    pitchtime::TimeStretchEngine timeStretchEngineState;
    pitchtime::AutoTuneEngine autoTuneEngineState;
    pitchtime::FormantShiftEngine formantShiftEngineState;
    pitchtime::VibratoEngine vibratoEngineState;

    std::array<std::atomic<float>, pitchtime::visualBinCount> visualBins {};
    std::atomic<float> visualPrimary { 0.0f };
    std::atomic<float> visualSecondary { 0.0f };
    std::atomic<float> visualTertiary { 0.0f };
    std::atomic<float> visualQuaternary { 0.0f };
    std::atomic<float> visualDetectedMidi { 0.0f };
    std::atomic<float> visualTargetMidi { 0.0f };
    std::atomic<float> visualConfidence { 0.0f };
    std::atomic<float> visualLatencyMs { 0.0f };
    std::atomic<bool> visualFlagA { false };
    std::atomic<bool> visualFlagB { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusiquePitchShiftProcessor)
};

#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace pitchtime
{

constexpr int maxPitchVoices = 4;
constexpr int visualBinCount = 8;

struct SyncDivision
{
    float beats;
    const char* label;
};

struct PitchTimeParams
{
    int engine = 0;
    int variant = 0;
    bool sync = false;
    bool bypass = false;
    bool mono = false;
    bool stereoStack = true;
    float mix = 100.0f;
    float outputDb = 0.0f;

    float pitch = 0.0f;
    float fine = 0.0f;
    float formant = 0.5f;
    float voice = 1.0f;
    int voiceCount = 1;
    float spread = 55.0f;
    float detune = 7.0f;
    float octave = 0.0f;
    int snappedOctave = 0;

    float stretchRatio = 100.0f;
    float stretchWindow = 220.0f;
    float stretchGrain = 50.0f;
    float stretchTransient = 50.0f;
    float stretchTone = 50.0f;
    float stretchSpread = 40.0f;
    float stretchSmooth = 60.0f;

    float tuneAmount = 80.0f;
    float tuneSpeed = 55.0f;
    float tuneHumanize = 25.0f;
    int tuneKey = 0;
    int tuneScale = 1;
    float tuneFormant = 55.0f;
    int tuneRange = 3;

    float fmtShift = 0.0f;
    float fmtFine = 0.0f;
    float fmtFocus = 55.0f;
    float fmtBrightness = 50.0f;
    float fmtBody = 50.0f;
    float fmtUnvoiced = 50.0f;
    float fmtSpread = 20.0f;

    float vibRate = 3.2f;
    float vibDepth = 35.0f;
    float vibSpread = 40.0f;
    int vibShape = 0;
    float vibTone = 50.0f;
    float vibRise = 20.0f;
    float vibDetune = 20.0f;
};

struct PitchTimeEngineSnapshot
{
    std::array<float, visualBinCount> bins {};
    float primary = 0.0f;
    float secondary = 0.0f;
    float tertiary = 0.0f;
    float quaternary = 0.0f;
    float detectedMidi = 0.0f;
    float targetMidi = 0.0f;
    float confidence = 0.0f;
    bool flagA = false;
    bool flagB = false;
};

inline const std::array<SyncDivision, 9>& getSyncDivisions() noexcept
{
    static constexpr std::array<SyncDivision, 9> divisions {{
        { 0.125f, "1/32" },
        { 1.0f / 6.0f, "1/16T" },
        { 0.25f, "1/16" },
        { 1.0f / 3.0f, "1/8T" },
        { 0.5f, "1/8" },
        { 2.0f / 3.0f, "1/4T" },
        { 1.0f, "1/4" },
        { 2.0f, "1/2" },
        { 4.0f, "1 BAR" }
    }};
    return divisions;
}

inline float dbToGain(float dB) noexcept
{
    return std::pow(10.0f, dB / 20.0f);
}

inline float semitonesToRatio(float semitones) noexcept
{
    return std::pow(2.0f, semitones / 12.0f);
}

inline float clamp01(float value) noexcept
{
    return juce::jlimit(0.0f, 1.0f, value);
}

inline float normaliseValue(float value, float minValue, float maxValue) noexcept
{
    if (maxValue <= minValue)
        return 0.0f;
    return clamp01((value - minValue) / (maxValue - minValue));
}

inline int quantiseChoice(float value, int minValue, int maxValue) noexcept
{
    return juce::jlimit(minValue, maxValue, (int) std::round(value));
}

inline int quantiseVoiceCount(float voiceRaw) noexcept
{
    return quantiseChoice(voiceRaw, 1, maxPitchVoices);
}

inline int snapOctave(float octaveRaw) noexcept
{
    return quantiseChoice(octaveRaw, -2, 2);
}

inline float frequencyToMidi(float hz) noexcept
{
    return hz > 0.0f ? 69.0f + 12.0f * std::log2(hz / 440.0f) : 0.0f;
}

inline float midiToFrequency(float midi) noexcept
{
    return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f);
}

inline float centsFromRatio(float ratio) noexcept
{
    return 1200.0f * std::log2(juce::jmax(1.0e-6f, ratio));
}

inline float onePoleCoeff(float cutoffHz, double sampleRate) noexcept
{
    const float safeRate = juce::jmax(1.0f, (float) sampleRate);
    const float safeCutoff = juce::jlimit(10.0f, safeRate * 0.48f, cutoffHz);
    return std::exp(-juce::MathConstants<float>::twoPi * safeCutoff / safeRate);
}

inline int resolveSyncIndex(float rawValue, float minValue, float maxValue) noexcept
{
    const auto& divisions = getSyncDivisions();
    const float normalised = juce::jlimit(0.0f, 0.999f, normaliseValue(rawValue, minValue, maxValue) * 0.999f);
    return juce::jlimit(0, (int) divisions.size() - 1,
                        (int) std::floor(normalised * (float) divisions.size()));
}

inline float resolveSyncedRateHz(float rawValue, float minValue, float maxValue, double bpm) noexcept
{
    const auto& divisions = getSyncDivisions();
    const int index = resolveSyncIndex(rawValue, minValue, maxValue);
    const float beats = divisions[(size_t) index].beats;
    return (float) (bpm / 60.0 / juce::jmax(0.03125f, beats));
}

inline float resolveSyncedMilliseconds(float rawValue, float minValue, float maxValue, double bpm) noexcept
{
    const auto& divisions = getSyncDivisions();
    const int index = resolveSyncIndex(rawValue, minValue, maxValue);
    const float beats = divisions[(size_t) index].beats;
    return (float) (60000.0 / juce::jmax(1.0, bpm) * beats);
}

inline int resolveLengthSamples(float rawValue,
                                float minValue,
                                float maxValue,
                                bool sync,
                                double bpm,
                                double sampleRate,
                                int minSamples,
                                int maxSamples) noexcept
{
    if (!sync)
        return juce::jlimit(minSamples, maxSamples, (int) std::round(rawValue * 0.001 * sampleRate));

    const double milliseconds = resolveSyncedMilliseconds(rawValue, minValue, maxValue, bpm);
    return juce::jlimit(minSamples, maxSamples, (int) std::round(milliseconds * 0.001 * sampleRate));
}

inline float resolveRateHz(float rawValue,
                           float minValue,
                           float maxValue,
                           bool sync,
                           double bpm) noexcept
{
    return sync ? resolveSyncedRateHz(rawValue, minValue, maxValue, bpm) : rawValue;
}

inline float grainEnvelope(float phase) noexcept
{
    return 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
}

inline float triangleWave(float phase) noexcept
{
    const float wrapped = phase - std::floor(phase);
    return 1.0f - 4.0f * std::abs(wrapped - 0.5f);
}

inline float softClip(float sample) noexcept
{
    return std::tanh(sample);
}

inline float hermiteRead(const std::vector<float>& ring, int ringSize, float readPos) noexcept
{
    if (ringSize <= 4)
        return 0.0f;

    while (readPos < 0.0f)
        readPos += (float) ringSize;
    while (readPos >= (float) ringSize)
        readPos -= (float) ringSize;

    const int x1 = ((int) std::floor(readPos)) % ringSize;
    const int x0 = (x1 + ringSize - 1) % ringSize;
    const int x2 = (x1 + 1) % ringSize;
    const int x3 = (x1 + 2) % ringSize;
    const float frac = readPos - (float) x1;

    const float xm1 = ring[(size_t) x0];
    const float x00 = ring[(size_t) x1];
    const float x01 = ring[(size_t) x2];
    const float x02 = ring[(size_t) x3];

    const float c0 = x00;
    const float c1 = 0.5f * (x01 - xm1);
    const float c2 = xm1 - 2.5f * x00 + 2.0f * x01 - 0.5f * x02;
    const float c3 = 0.5f * (x02 - xm1) + 1.5f * (x00 - x01);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

inline float simpleTilt(float sample, float tilt, float& lowState, double sampleRate, float cutoffBase = 900.0f) noexcept
{
    const float cutoff = juce::jlimit(120.0f, (float) sampleRate * 0.45f, cutoffBase + std::abs(tilt) * 2600.0f);
    const float coeff = onePoleCoeff(cutoff, sampleRate);
    lowState = lowState * coeff + sample * (1.0f - coeff);
    const float low = lowState;
    const float high = sample - low;
    const float lowGain = tilt >= 0.0f ? 1.0f - 0.45f * tilt : 1.0f + 0.65f * -tilt;
    const float highGain = tilt >= 0.0f ? 1.0f + 1.1f * tilt : 1.0f - 0.5f * -tilt;
    return low * lowGain + high * highGain;
}

inline juce::String noteNameFromMidi(float midiValue)
{
    static constexpr const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    if (!std::isfinite(midiValue))
        return "--";
    const int note = ((int) std::round(midiValue) % 12 + 12) % 12;
    const int octave = (int) std::floor((midiValue + 3.0f) / 12.0f) - 1;
    return juce::String(names[note]) + juce::String(octave);
}

class PitchShiftEngine
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        grainSizeSamples = juce::jlimit(1024, 4096, (int) std::round(sampleRate * 0.04));
        if ((grainSizeSamples & 1) != 0)
            ++grainSizeSamples;

        baseDelaySamples = juce::jmax(64, grainSizeSamples / 8);
        latencySamples = baseDelaySamples + grainSizeSamples / 2;
        ringSize = juce::jmax((int) std::round(sampleRate * 0.45),
                              latencySamples + grainSizeSamples * 2 + 4096);

        for (auto& channelRing : ring)
            channelRing.assign((size_t) ringSize, 0.0f);
        for (auto& channelRing : antiAliasRing)
            channelRing.assign((size_t) ringSize, 0.0f);

        reset();
    }

    void reset()
    {
        for (auto& channelRing : ring)
            std::fill(channelRing.begin(), channelRing.end(), 0.0f);
        for (auto& channelRing : antiAliasRing)
            std::fill(channelRing.begin(), channelRing.end(), 0.0f);

        writePos = 0;
        grainPhases = { 0.0f, 0.21f, 0.49f, 0.73f };
        antiAliasState = { 0.0f, 0.0f };
        colourLowState = { 0.0f, 0.0f };
        phaseDrift = 0.0f;
    }

    int getLatencySamples() const noexcept { return latencySamples; }

    double getTailSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) (latencySamples + grainSizeSamples) / sampleRate : 0.0;
    }

    void process(juce::AudioBuffer<float>& buffer,
                 const PitchTimeParams& params,
                 PitchTimeEngineSnapshot& snapshot)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || ringSize <= 4)
            return;

        const float semitoneShift = params.pitch + (float) params.snappedOctave * 12.0f + params.fine * 0.01f;
        const float ratio = juce::jlimit(0.25f, 4.0f, semitonesToRatio(semitoneShift));
        const int numVoices = params.voiceCount;
        const bool stereoStack = params.stereoStack && numChannels > 1;
        const float spreadNorm = clamp01(params.spread / 100.0f);
        const float detuneNorm = clamp01(params.detune / 18.0f);
        const float perceptualSpread = 0.82f * std::pow(spreadNorm, 1.6f);
        const float perceptualDetune = 18.0f * std::pow(detuneNorm, 1.35f);
        const float maxVoiceRatio = ratio * semitonesToRatio(perceptualDetune / 100.0f);
        const float antiAliasCutoff = juce::jlimit(800.0f, (float) sampleRate * 0.45f,
                                                   (float) sampleRate * 0.45f / juce::jmax(1.0f, maxVoiceRatio));
        const float antiAliasCoeff = onePoleCoeff(antiAliasCutoff, sampleRate);
        const float colourTilt = (params.formant - 0.5f) * 2.0f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float inputLeft = buffer.getSample(0, sample);
            const float inputRight = buffer.getSample(numChannels > 1 ? 1 : 0, sample);

            ring[0][(size_t) writePos] = inputLeft;
            antiAliasState[0] = antiAliasState[0] * antiAliasCoeff + inputLeft * (1.0f - antiAliasCoeff);
            antiAliasRing[0][(size_t) writePos] = antiAliasState[0];

            ring[1][(size_t) writePos] = inputRight;
            antiAliasState[1] = antiAliasState[1] * antiAliasCoeff + inputRight * (1.0f - antiAliasCoeff);
            antiAliasRing[1][(size_t) writePos] = antiAliasState[1];

            float wetLeft = 0.0f;
            float wetRight = 0.0f;

            for (int voiceIndex = 0; voiceIndex < numVoices; ++voiceIndex)
            {
                const float voiceNorm = numVoices == 1
                    ? 0.0f
                    : juce::jmap((float) voiceIndex, 0.0f, (float) (numVoices - 1), -1.0f, 1.0f);
                const float detuneCents = numVoices > 1 ? voiceNorm * perceptualDetune : 0.0f;
                const float voiceRatio = juce::jlimit(0.25f, 4.0f, ratio * semitonesToRatio(detuneCents / 100.0f));
                const float ratioDelta = voiceRatio - 1.0f;
                const float jitter = numVoices > 1 ? std::fmod(phaseOffsets[(size_t) voiceIndex] + phaseDrift, 1.0f) : 0.0f;
                const float phaseA = std::fmod(grainPhases[(size_t) voiceIndex] + jitter, 1.0f);
                const float phaseB = std::fmod(phaseA + 0.5f, 1.0f);
                const float phaseAdvance = std::abs(ratioDelta) > 1.0e-4f
                    ? juce::jlimit(1.0f / (float) grainSizeSamples, 0.35f, std::abs(ratioDelta) / (float) grainSizeSamples)
                    : (numVoices > 1 ? 1.0f / (float) grainSizeSamples : 0.0f);

                float voiceLeft = readRingSample(ring, 0, (float) latencySamples);
                float voiceRight = readRingSample(ring, 1, (float) latencySamples);

                if (std::abs(ratioDelta) > 1.0e-4f || numVoices > 1)
                {
                    const float delayA = ratioDelta >= 0.0f
                        ? (float) baseDelaySamples + (1.0f - phaseA) * (float) grainSizeSamples
                        : (float) baseDelaySamples + phaseA * (float) grainSizeSamples;
                    const float delayB = ratioDelta >= 0.0f
                        ? (float) baseDelaySamples + (1.0f - phaseB) * (float) grainSizeSamples
                        : (float) baseDelaySamples + phaseB * (float) grainSizeSamples;
                    const float envA = grainEnvelope(phaseA);
                    const float envB = grainEnvelope(phaseB);
                    const float envNorm = juce::jmax(1.0e-5f, envA + envB);
                    const auto& source = voiceRatio > 1.05f ? antiAliasRing : ring;
                    voiceLeft = (readRingSample(source, 0, delayA) * envA + readRingSample(source, 0, delayB) * envB) / envNorm;
                    voiceRight = (readRingSample(source, 1, delayA) * envA + readRingSample(source, 1, delayB) * envB) / envNorm;
                }

                const float pan = stereoStack ? voiceNorm * perceptualSpread : 0.0f;
                const float panLeft = std::sqrt(0.5f * (1.0f - pan));
                const float panRight = std::sqrt(0.5f * (1.0f + pan));
                wetLeft += voiceLeft * panLeft;
                wetRight += voiceRight * panRight;

                grainPhases[(size_t) voiceIndex] += phaseAdvance;
                if (grainPhases[(size_t) voiceIndex] >= 1.0f)
                    grainPhases[(size_t) voiceIndex] -= 1.0f;
            }

            const float normalisation = numVoices == 1
                ? juce::MathConstants<float>::sqrt2
                : 1.0f / std::sqrt((float) numVoices);
            wetLeft *= normalisation;
            wetRight *= normalisation;

            wetLeft = simpleTilt(wetLeft, colourTilt, colourLowState[0], sampleRate, 850.0f);
            wetRight = simpleTilt(wetRight, colourTilt, colourLowState[1], sampleRate, 850.0f);

            buffer.setSample(0, sample, wetLeft);
            if (numChannels > 1)
                buffer.setSample(1, sample, wetRight);

            writePos = (writePos + 1) % ringSize;
            phaseDrift = std::fmod(phaseDrift + 0.00017f * (0.35f + spreadNorm), 1.0f);
        }

        snapshot.primary = semitoneShift;
        snapshot.secondary = (float) numVoices;
        snapshot.tertiary = params.spread;
        snapshot.quaternary = params.detune;
        snapshot.flagA = stereoStack;
        snapshot.flagB = params.voiceCount > 1;
        snapshot.bins[0] = normaliseValue(semitoneShift, -24.0f, 24.0f);
        snapshot.bins[1] = normaliseValue((float) numVoices, 1.0f, 4.0f);
        snapshot.bins[2] = clamp01(params.spread / 100.0f);
        snapshot.bins[3] = clamp01(params.detune / 18.0f);
        snapshot.bins[4] = clamp01(params.formant);
        snapshot.bins[5] = clamp01((ratio - 0.25f) / 3.75f);
    }

private:
    float readRingSample(const std::array<std::vector<float>, 2>& source,
                         int channel,
                         float delaySamples) const noexcept
    {
        if (ringSize <= 4)
            return 0.0f;
        const float clampedDelay = juce::jlimit(1.0f, (float) ringSize - 3.0f, delaySamples);
        float readPos = (float) writePos - clampedDelay;
        while (readPos < 0.0f)
            readPos += (float) ringSize;
        return hermiteRead(source[(size_t) channel], ringSize, readPos);
    }

    double sampleRate = 44100.0;
    int ringSize = 0;
    int grainSizeSamples = 2048;
    int baseDelaySamples = 256;
    int latencySamples = 1280;
    int writePos = 0;
    std::array<std::vector<float>, 2> ring;
    std::array<std::vector<float>, 2> antiAliasRing;
    std::array<float, 2> antiAliasState { 0.0f, 0.0f };
    std::array<float, 2> colourLowState { 0.0f, 0.0f };
    std::array<float, maxPitchVoices> grainPhases { 0.0f, 0.21f, 0.49f, 0.73f };
    const std::array<float, maxPitchVoices> phaseOffsets { 0.0f, 0.17f, 0.33f, 0.47f };
    float phaseDrift = 0.0f;
};

class TimeStretchEngine
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        ringSize = juce::jmax((int) std::round(sampleRate * 0.75), 32768);
        for (auto& channelRing : ring)
            channelRing.assign((size_t) ringSize, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& channelRing : ring)
            std::fill(channelRing.begin(), channelRing.end(), 0.0f);
        writePos = 0;
        phase = 0.0f;
        toneLowState = { 0.0f, 0.0f };
        scatterPhase = 0.0f;
        lastLatencySamples = 512;
    }

    int computeLatencySamples(const PitchTimeParams& params, double bpm) const noexcept
    {
        return resolveLengthSamples(params.stretchWindow, 15.0f, 1000.0f, params.sync, bpm,
                                    sampleRate, 192, juce::jmax(192, (int) std::round(sampleRate * 1.5)));
    }

    double getTailSeconds(const PitchTimeParams& params, double bpm) const noexcept
    {
        return sampleRate > 0.0 ? (double) computeLatencySamples(params, bpm) / sampleRate : 0.0;
    }

    void process(juce::AudioBuffer<float>& buffer,
                 const PitchTimeParams& params,
                 double bpm,
                 PitchTimeEngineSnapshot& snapshot)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || ringSize <= 4)
            return;

        const float ratio = juce::jlimit(0.5f, 2.0f, params.stretchRatio / 100.0f);
        const int windowSamples = computeLatencySamples(params, bpm);
        const float toneTilt = (params.stretchTone - 50.0f) / 50.0f;
        const float transientBlend = clamp01(params.stretchTransient / 100.0f);
        const float spreadSamples = params.stretchSpread * 0.25f;
        const float smooth = clamp01(params.stretchSmooth / 100.0f);
        const float scatter = clamp01(params.stretchGrain / 100.0f);
        const float variantBias = params.variant == 1 ? 0.84f : (params.variant == 2 ? 1.18f : 1.0f);
        const float phaseAdvance = juce::jlimit(1.0e-5f, 0.2f, 1.0f / ((float) windowSamples * ratio * variantBias));

        lastLatencySamples = windowSamples;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float inputLeft = buffer.getSample(0, sample);
            const float inputRight = buffer.getSample(numChannels > 1 ? 1 : 0, sample);
            ring[0][(size_t) writePos] = inputLeft;
            ring[1][(size_t) writePos] = inputRight;

            const float grainPhaseA = std::fmod(phase, 1.0f);
            const float grainPhaseB = std::fmod(grainPhaseA + 0.5f, 1.0f);
            const float envA = std::pow(grainEnvelope(grainPhaseA), 0.7f + (1.0f - smooth) * 1.2f);
            const float envB = std::pow(grainEnvelope(grainPhaseB), 0.7f + (1.0f - smooth) * 1.2f);
            const float envNorm = juce::jmax(1.0e-5f, envA + envB);

            const float scatterOffset = params.variant == 2
                ? std::sin(scatterPhase) * scatter * 0.25f * (float) windowSamples
                : std::sin(scatterPhase) * scatter * 0.08f * (float) windowSamples;
            const float delayA = (float) windowSamples + (1.0f - grainPhaseA) * (float) windowSamples + scatterOffset;
            const float delayB = (float) windowSamples + (1.0f - grainPhaseB) * (float) windowSamples - scatterOffset * 0.7f;
            const float dryDelay = (float) windowSamples;

            float wetLeft = ((readRingSample(0, delayA) * envA) + (readRingSample(0, delayB) * envB)) / envNorm;
            float wetRight = ((readRingSample(1, delayA + spreadSamples) * envA)
                            + (readRingSample(1, delayB - spreadSamples) * envB)) / envNorm;

            const float dryLeft = readRingSample(0, dryDelay);
            const float dryRight = readRingSample(1, dryDelay);
            wetLeft = juce::jmap(transientBlend, wetLeft, dryLeft * 0.4f + wetLeft * 0.6f);
            wetRight = juce::jmap(transientBlend, wetRight, dryRight * 0.4f + wetRight * 0.6f);

            wetLeft = simpleTilt(wetLeft, toneTilt, toneLowState[0], sampleRate, 1000.0f);
            wetRight = simpleTilt(wetRight, toneTilt, toneLowState[1], sampleRate, 1000.0f);

            buffer.setSample(0, sample, wetLeft);
            if (numChannels > 1)
                buffer.setSample(1, sample, wetRight);

            writePos = (writePos + 1) % ringSize;
            phase += phaseAdvance;
            if (phase >= 1.0f)
                phase -= 1.0f;
            scatterPhase = std::fmod(scatterPhase + 0.031f + scatter * 0.022f, juce::MathConstants<float>::twoPi);
        }

        snapshot.primary = ratio;
        snapshot.secondary = (float) windowSamples * 1000.0f / (float) sampleRate;
        snapshot.tertiary = params.stretchGrain;
        snapshot.quaternary = params.stretchSmooth;
        snapshot.flagA = params.sync;
        snapshot.flagB = params.variant == 2;
        snapshot.bins[0] = normaliseValue(ratio, 0.5f, 2.0f);
        snapshot.bins[1] = normaliseValue((float) windowSamples, 192.0f, (float) juce::roundToInt(sampleRate * 1.5));
        snapshot.bins[2] = clamp01(params.stretchGrain / 100.0f);
        snapshot.bins[3] = clamp01(params.stretchTransient / 100.0f);
        snapshot.bins[4] = clamp01(params.stretchTone / 100.0f);
        snapshot.bins[5] = clamp01(params.stretchSpread / 100.0f);
        snapshot.bins[6] = clamp01(params.stretchSmooth / 100.0f);
    }

private:
    float readRingSample(int channel, float delaySamples) const noexcept
    {
        const float clampedDelay = juce::jlimit(1.0f, (float) ringSize - 3.0f, delaySamples);
        float readPos = (float) writePos - clampedDelay;
        while (readPos < 0.0f)
            readPos += (float) ringSize;
        return hermiteRead(ring[(size_t) channel], ringSize, readPos);
    }

    double sampleRate = 44100.0;
    int ringSize = 0;
    int writePos = 0;
    int lastLatencySamples = 512;
    std::array<std::vector<float>, 2> ring;
    std::array<float, 2> toneLowState { 0.0f, 0.0f };
    float phase = 0.0f;
    float scatterPhase = 0.0f;
};

class AutoTuneEngine
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        grainSizeSamples = juce::jlimit(512, 2048, (int) std::round(sampleRate * 0.03));
        if ((grainSizeSamples & 1) != 0)
            ++grainSizeSamples;

        baseDelaySamples = juce::jmax(96, grainSizeSamples / 6);
        latencySamples = baseDelaySamples + grainSizeSamples / 2;
        ringSize = juce::jmax((int) std::round(sampleRate * 0.4),
                              latencySamples + grainSizeSamples * 2 + 4096);
        analysisSize = 2048;
        analysisRing.assign((size_t) analysisSize, 0.0f);
        for (auto& channelRing : ring)
            channelRing.assign((size_t) ringSize, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& channelRing : ring)
            std::fill(channelRing.begin(), channelRing.end(), 0.0f);
        std::fill(analysisRing.begin(), analysisRing.end(), 0.0f);
        analysisWritePos = 0;
        analysisFilled = 0;
        writePos = 0;
        grainPhase = 0.0f;
        colourLowState = { 0.0f, 0.0f };
        currentRatio = 1.0f;
        currentWetConfidence = 0.0f;
        lastDetectedMidi = 0.0f;
        lastTargetMidi = 0.0f;
        lastConfidence = 0.0f;
    }

    int getLatencySamples() const noexcept { return latencySamples; }

    double getTailSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) (latencySamples + grainSizeSamples) / sampleRate : 0.0;
    }

    void process(juce::AudioBuffer<float>& buffer,
                 const PitchTimeParams& params,
                 PitchTimeEngineSnapshot& snapshot)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || ringSize <= 4)
            return;

        const auto estimate = estimatePitch(params.tuneRange);
        lastDetectedMidi = estimate.detectedMidi;
        lastConfidence = estimate.confidence;
        const bool trackingActive = estimate.valid && estimate.confidence >= 0.25f;

        const float amount = clamp01(params.tuneAmount / 100.0f);
        const float humanize = clamp01(params.tuneHumanize / 100.0f);
        const float speedNorm = clamp01(params.tuneSpeed / 100.0f);
        const float variantAmount = params.variant == 0 ? 0.68f : (params.variant == 1 ? 0.88f : 1.0f);
        const float speedLerp = params.variant == 0 ? (0.12f + speedNorm * 0.42f)
                                                   : (params.variant == 1 ? 0.26f + speedNorm * 0.54f
                                                                          : 0.38f + speedNorm * 0.58f);

        float desiredRatio = 1.0f;
        float desiredConfidenceWet = 0.0f;
        if (trackingActive)
        {
            const float allowedMidi = snapMidiToScale(estimate.detectedMidi, params.tuneKey, params.tuneScale);
            const float distance = allowedMidi - estimate.detectedMidi;
            const float humanizeAmount = humanize * clamp01(1.0f - std::abs(distance) / 1.5f);
            const float targetMidi = estimate.detectedMidi + distance * amount * variantAmount * (1.0f - 0.55f * humanizeAmount);
            lastTargetMidi = params.variant == 2 ? allowedMidi : targetMidi;
            desiredRatio = semitonesToRatio((lastTargetMidi - estimate.detectedMidi));
            desiredConfidenceWet = clamp01((estimate.confidence - 0.18f) / 0.55f);
        }
        else
        {
            lastTargetMidi = 0.0f;
        }

        currentRatio += (desiredRatio - currentRatio) * speedLerp;
        currentWetConfidence += (desiredConfidenceWet - currentWetConfidence) * (0.14f + speedNorm * 0.32f);
        currentRatio = juce::jlimit(0.5f, 2.0f, currentRatio);
        currentWetConfidence = clamp01(currentWetConfidence);

        const float ratioDelta = currentRatio - 1.0f;
        const float phaseAdvance = std::abs(ratioDelta) > 1.0e-4f
            ? juce::jlimit(1.0f / (float) grainSizeSamples, 0.28f, std::abs(ratioDelta) / (float) grainSizeSamples)
            : 0.0f;
        const float formantTilt = juce::jmap(clamp01(params.tuneFormant / 100.0f), 0.0f, 1.0f, 0.55f, -0.45f)
                                * std::abs(centsFromRatio(currentRatio)) / 700.0f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float inputLeft = buffer.getSample(0, sample);
            const float inputRight = buffer.getSample(numChannels > 1 ? 1 : 0, sample);
            const float mono = 0.5f * (inputLeft + inputRight);

            ring[0][(size_t) writePos] = inputLeft;
            ring[1][(size_t) writePos] = inputRight;

            analysisRing[(size_t) analysisWritePos] = mono;
            analysisWritePos = (analysisWritePos + 1) % analysisSize;
            analysisFilled = juce::jmin(analysisSize, analysisFilled + 1);

            const float dryLeft = readRingSample(0, (float) latencySamples);
            const float dryRight = readRingSample(1, (float) latencySamples);
            float wetLeft = dryLeft;
            float wetRight = dryRight;

            if (std::abs(ratioDelta) > 1.0e-4f && currentWetConfidence > 0.02f)
            {
                const float phaseA = grainPhase;
                const float phaseB = std::fmod(phaseA + 0.5f, 1.0f);
                const float delayA = ratioDelta >= 0.0f
                    ? (float) baseDelaySamples + (1.0f - phaseA) * (float) grainSizeSamples
                    : (float) baseDelaySamples + phaseA * (float) grainSizeSamples;
                const float delayB = ratioDelta >= 0.0f
                    ? (float) baseDelaySamples + (1.0f - phaseB) * (float) grainSizeSamples
                    : (float) baseDelaySamples + phaseB * (float) grainSizeSamples;
                const float envA = grainEnvelope(phaseA);
                const float envB = grainEnvelope(phaseB);
                const float envNorm = juce::jmax(1.0e-5f, envA + envB);
                wetLeft = (readRingSample(0, delayA) * envA + readRingSample(0, delayB) * envB) / envNorm;
                wetRight = (readRingSample(1, delayA) * envA + readRingSample(1, delayB) * envB) / envNorm;
                grainPhase += phaseAdvance;
                if (grainPhase >= 1.0f)
                    grainPhase -= 1.0f;
            }

            wetLeft = simpleTilt(wetLeft, formantTilt, colourLowState[0], sampleRate, 1150.0f);
            wetRight = simpleTilt(wetRight, formantTilt, colourLowState[1], sampleRate, 1150.0f);
            wetLeft = juce::jmap(currentWetConfidence, dryLeft, wetLeft);
            wetRight = juce::jmap(currentWetConfidence, dryRight, wetRight);

            buffer.setSample(0, sample, wetLeft);
            if (numChannels > 1)
                buffer.setSample(1, sample, wetRight);

            writePos = (writePos + 1) % ringSize;
        }

        snapshot.primary = centsFromRatio(currentRatio);
        snapshot.secondary = lastDetectedMidi;
        snapshot.tertiary = lastTargetMidi;
        snapshot.quaternary = currentWetConfidence;
        snapshot.detectedMidi = lastDetectedMidi;
        snapshot.targetMidi = lastTargetMidi;
        snapshot.confidence = lastConfidence;
        snapshot.flagA = trackingActive;
        snapshot.flagB = currentWetConfidence > 0.25f;
        snapshot.bins[0] = normaliseValue(snapshot.primary, -400.0f, 400.0f);
        snapshot.bins[1] = normaliseValue(lastDetectedMidi, 36.0f, 84.0f);
        snapshot.bins[2] = normaliseValue(lastTargetMidi, 36.0f, 84.0f);
        snapshot.bins[3] = clamp01(lastConfidence);
        snapshot.bins[4] = clamp01(currentWetConfidence);
        snapshot.bins[5] = clamp01(params.tuneAmount / 100.0f);
    }

private:
    struct PitchEstimate
    {
        bool valid = false;
        float detectedMidi = 0.0f;
        float confidence = 0.0f;
    };

    float readRingSample(int channel, float delaySamples) const noexcept
    {
        const float clampedDelay = juce::jlimit(1.0f, (float) ringSize - 3.0f, delaySamples);
        float readPos = (float) writePos - clampedDelay;
        while (readPos < 0.0f)
            readPos += (float) ringSize;
        return hermiteRead(ring[(size_t) channel], ringSize, readPos);
    }

    PitchEstimate estimatePitch(int rangeMode) const
    {
        PitchEstimate estimate;
        if (analysisFilled < 512 || sampleRate <= 0.0)
            return estimate;

        const int frameSize = juce::jmin(analysisFilled, 1024);
        std::vector<float> frame((size_t) frameSize, 0.0f);
        for (int index = 0; index < frameSize; ++index)
        {
            const int srcIndex = (analysisWritePos - frameSize + index + analysisSize) % analysisSize;
            frame[(size_t) index] = analysisRing[(size_t) srcIndex];
        }

        double mean = 0.0;
        for (float sample : frame)
            mean += sample;
        mean /= (double) frameSize;

        double energy = 0.0;
        int zeroCrossings = 0;
        for (float& sample : frame)
        {
            sample = (float) ((double) sample - mean);
            energy += (double) sample * (double) sample;
        }

        for (int index = 1; index < frameSize; ++index)
            if ((frame[(size_t) (index - 1)] >= 0.0f) != (frame[(size_t) index] >= 0.0f))
                ++zeroCrossings;

        if (energy < 1.0e-3)
            return estimate;

        if (zeroCrossings > frameSize / 6)
            return estimate;

        float minHz = 60.0f;
        float maxHz = 1600.0f;
        switch (rangeMode)
        {
            case 0: minHz = 70.0f; maxHz = 400.0f; break;
            case 1: minHz = 110.0f; maxHz = 900.0f; break;
            case 2: minHz = 220.0f; maxHz = 1600.0f; break;
            default: break;
        }

        const int minLag = juce::jlimit(8, frameSize / 2, (int) std::floor(sampleRate / maxHz));
        const int maxLag = juce::jlimit(minLag + 1, frameSize - 2, (int) std::ceil(sampleRate / minHz));

        float bestScore = 0.0f;
        float secondBestScore = 0.0f;
        int bestLag = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double corr = 0.0;
            double normA = 0.0;
            double normB = 0.0;
            const int limit = frameSize - lag;
            for (int index = 0; index < limit; ++index)
            {
                const double a = frame[(size_t) index];
                const double b = frame[(size_t) (index + lag)];
                corr += a * b;
                normA += a * a;
                normB += b * b;
            }

            const double denom = std::sqrt(normA * normB);
            if (denom <= 1.0e-9)
                continue;

            const float score = (float) juce::jlimit(-1.0, 1.0, corr / denom);
            if (score > bestScore)
            {
                secondBestScore = bestScore;
                bestScore = score;
                bestLag = lag;
            }
            else if (score > secondBestScore)
            {
                secondBestScore = score;
            }
        }

        if (bestLag <= 0 || bestScore < 0.28f)
            return estimate;

        const float frequency = (float) sampleRate / (float) bestLag;
        if (!std::isfinite(frequency) || frequency < minHz || frequency > maxHz * 1.08f)
            return estimate;

        const float contrast = juce::jmax(0.0f, bestScore - secondBestScore);
        const float periodicityConfidence = clamp01((bestScore - 0.22f) / 0.62f)
                                          * clamp01((contrast - 0.006f) / 0.18f);
        const float pureToneBoost = clamp01((bestScore - 0.78f) / 0.16f) * 0.85f;
        estimate.confidence = juce::jmax(periodicityConfidence, pureToneBoost);
        if (estimate.confidence < 0.12f)
            return estimate;

        estimate.valid = true;
        estimate.detectedMidi = frequencyToMidi(frequency);
        return estimate;
    }

    static float snapMidiToScale(float midiValue, int key, int scale)
    {
        static const std::array<std::array<int, 12>, 4> intervalSets {{
            { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
            { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 },
            { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 },
            { 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0 }
        }};

        const int scaleIndex = juce::jlimit(0, (int) intervalSets.size() - 1, scale);
        const auto& intervals = intervalSets[(size_t) scaleIndex];
        float bestMidi = std::round(midiValue);
        float bestDistance = std::numeric_limits<float>::max();
        const int rounded = (int) std::round(midiValue);

        for (int candidate = rounded - 18; candidate <= rounded + 18; ++candidate)
        {
            const int noteClass = ((candidate - key) % 12 + 12) % 12;
            if (intervals[(size_t) noteClass] == 0)
                continue;

            const float distance = std::abs((float) candidate - midiValue);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestMidi = (float) candidate;
            }
        }

        return bestMidi;
    }

    double sampleRate = 44100.0;
    int ringSize = 0;
    int analysisSize = 2048;
    int grainSizeSamples = 1024;
    int baseDelaySamples = 192;
    int latencySamples = 704;
    int writePos = 0;
    int analysisWritePos = 0;
    int analysisFilled = 0;
    std::array<std::vector<float>, 2> ring;
    std::vector<float> analysisRing;
    std::array<float, 2> colourLowState { 0.0f, 0.0f };
    float grainPhase = 0.0f;
    float currentRatio = 1.0f;
    float currentWetConfidence = 0.0f;
    float lastDetectedMidi = 0.0f;
    float lastTargetMidi = 0.0f;
    float lastConfidence = 0.0f;
};

class FormantShiftEngine
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        window.clear();
        window.resize((size_t) fftSize, 0.0f);
        for (int index = 0; index < fftSize; ++index)
        {
            const float hann = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * (float) index / (float) (fftSize - 1));
            window[(size_t) index] = std::sqrt(hann);
        }

        for (auto& channel : channels)
        {
            channel.inputRing.assign((size_t) fftSize, 0.0f);
            channel.outputRing.assign((size_t) fftSize, 0.0f);
            channel.fftData.assign((size_t) (2 * fftSize), 0.0f);
            channel.envDisplay.fill(0.0f);
            channel.shiftedDisplay.fill(0.0f);
            channel.ringPos = 0;
            channel.hopCounter = 0;
        }

        reset();
    }

    void reset()
    {
        for (auto& channel : channels)
        {
            std::fill(channel.inputRing.begin(), channel.inputRing.end(), 0.0f);
            std::fill(channel.outputRing.begin(), channel.outputRing.end(), 0.0f);
            std::fill(channel.fftData.begin(), channel.fftData.end(), 0.0f);
            channel.envDisplay.fill(0.0f);
            channel.shiftedDisplay.fill(0.0f);
            channel.ringPos = 0;
            channel.hopCounter = 0;
        }
    }

    int getLatencySamples() const noexcept { return hopSize; }

    double getTailSeconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) fftSize / sampleRate : 0.0;
    }

    void process(juce::AudioBuffer<float>& buffer,
                 const PitchTimeParams& params,
                 PitchTimeEngineSnapshot& snapshot)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0)
            return;

        const float shiftSemitones = params.fmtShift + params.fmtFine * 0.01f;
        const float spreadSemitones = params.fmtSpread * 0.02f;
        const float focus = clamp01(params.fmtFocus / 100.0f);
        const float brightness = (params.fmtBrightness - 50.0f) / 50.0f;
        const float body = (params.fmtBody - 50.0f) / 50.0f;
        const float unvoiced = (params.fmtUnvoiced - 50.0f) / 50.0f;
        const float robotBlend = params.variant == 2 ? 0.48f : 0.0f;
        const float warpBlend = params.variant == 0 ? 0.72f : (params.variant == 1 ? 0.92f : 1.0f);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float leftShift = shiftSemitones - (numChannels > 1 ? spreadSemitones : 0.0f);
            buffer.setSample(0, sample, processSample(channels[0], buffer.getSample(0, sample),
                                                      leftShift, focus, brightness, body, unvoiced, robotBlend, warpBlend));

            if (numChannels > 1)
            {
                const float rightShift = shiftSemitones + spreadSemitones;
                buffer.setSample(1, sample, processSample(channels[1], buffer.getSample(1, sample),
                                                          rightShift, focus, brightness, body, unvoiced, robotBlend, warpBlend));
            }
        }

        for (int index = 0; index < 4; ++index)
        {
            snapshot.bins[(size_t) index] = channels[0].envDisplay[(size_t) index];
            snapshot.bins[(size_t) (index + 4)] = channels[0].shiftedDisplay[(size_t) index];
        }
        snapshot.primary = shiftSemitones;
        snapshot.secondary = params.fmtFocus;
        snapshot.tertiary = params.fmtBrightness;
        snapshot.quaternary = params.fmtBody;
        snapshot.flagA = params.variant == 2;
        snapshot.flagB = std::abs(shiftSemitones) > 0.25f;
    }

private:
    struct ChannelState
    {
        std::vector<float> inputRing;
        std::vector<float> outputRing;
        std::vector<float> fftData;
        std::array<float, 4> envDisplay {};
        std::array<float, 4> shiftedDisplay {};
        int ringPos = 0;
        int hopCounter = 0;
    };

    float processSample(ChannelState& state,
                        float inputSample,
                        float shiftSemitones,
                        float focus,
                        float brightness,
                        float body,
                        float unvoiced,
                        float robotBlend,
                        float warpBlend)
    {
        const float outputSample = state.outputRing[(size_t) state.ringPos];
        state.outputRing[(size_t) state.ringPos] = 0.0f;
        state.inputRing[(size_t) state.ringPos] = inputSample;

        state.ringPos = (state.ringPos + 1) % fftSize;
        ++state.hopCounter;
        if (state.hopCounter >= hopSize)
        {
            state.hopCounter = 0;
            processFrame(state, shiftSemitones, focus, brightness, body, unvoiced, robotBlend, warpBlend);
        }

        return outputSample;
    }

    void processFrame(ChannelState& state,
                      float shiftSemitones,
                      float focus,
                      float brightness,
                      float body,
                      float unvoiced,
                      float robotBlend,
                      float warpBlend)
    {
        constexpr int fftBins = fftSize / 2 + 1;
        std::array<float, fftBins> magnitudes {};
        std::array<float, fftBins> envelope {};
        std::array<float, fftBins> warpedEnvelope {};

        std::fill(state.fftData.begin(), state.fftData.end(), 0.0f);
        for (int sample = 0; sample < fftSize; ++sample)
        {
            const int srcIndex = (state.ringPos + sample) % fftSize;
            state.fftData[(size_t) sample] = state.inputRing[(size_t) srcIndex] * window[(size_t) sample];
        }

        fft.performRealOnlyForwardTransform(state.fftData.data());

        for (int bin = 0; bin < fftBins; ++bin)
        {
            const float real = state.fftData[(size_t) (bin * 2)];
            const float imag = state.fftData[(size_t) (bin * 2 + 1)];
            magnitudes[(size_t) bin] = std::sqrt(real * real + imag * imag) + 1.0e-5f;
        }

        const int smoothingBins = juce::jlimit(3, 40, (int) std::round(36.0f - focus * 30.0f));
        for (int bin = 0; bin < fftBins; ++bin)
        {
            int count = 0;
            double sum = 0.0;
            for (int offset = -smoothingBins; offset <= smoothingBins; ++offset)
            {
                const int neighbour = juce::jlimit(0, fftBins - 1, bin + offset);
                sum += magnitudes[(size_t) neighbour];
                ++count;
            }
            envelope[(size_t) bin] = (float) (sum / juce::jmax(1, count));
        }

        const float shiftRatio = semitonesToRatio(shiftSemitones);
        for (int bin = 0; bin < fftBins; ++bin)
        {
            const float sourceIndex = (float) bin / juce::jmax(0.25f, shiftRatio);
            const int indexA = juce::jlimit(0, fftBins - 1, (int) std::floor(sourceIndex));
            const int indexB = juce::jlimit(0, fftBins - 1, indexA + 1);
            const float frac = sourceIndex - (float) indexA;
            const float warped = juce::jmap(frac, envelope[(size_t) indexA], envelope[(size_t) indexB]);
            warpedEnvelope[(size_t) bin] = juce::jmap(warpBlend, envelope[(size_t) bin], warped);
        }

        for (int bin = 0; bin < fftBins; ++bin)
        {
            const float real = state.fftData[(size_t) (bin * 2)];
            const float imag = state.fftData[(size_t) (bin * 2 + 1)];
            const float magnitude = magnitudes[(size_t) bin];
            const float phase = std::atan2(imag, real);
            const float spectralDetail = magnitude / juce::jmax(1.0e-5f, envelope[(size_t) bin]);
            const float detailBlend = robotBlend > 0.0f ? std::pow(spectralDetail, 1.0f - robotBlend) : spectralDetail;
            const float normFreq = (float) bin / (float) (fftBins - 1);
            const float brightGain = std::pow(2.0f, brightness * normFreq * 0.55f);
            const float bodyGain = std::pow(2.0f, -body * normFreq * 0.45f + body * (1.0f - normFreq) * 0.25f);
            const float unvoicedGain = std::pow(2.0f, unvoiced * juce::jlimit(0.0f, 1.0f, (normFreq - 0.45f) * 1.8f) * 0.45f);
            const float newMagnitude = detailBlend * warpedEnvelope[(size_t) bin] * brightGain * bodyGain * unvoicedGain;

            state.fftData[(size_t) (bin * 2)] = newMagnitude * std::cos(phase);
            state.fftData[(size_t) (bin * 2 + 1)] = newMagnitude * std::sin(phase);
        }

        fft.performRealOnlyInverseTransform(state.fftData.data());

        const float inverseScale = 1.0f / (float) fftSize;
        for (int sample = 0; sample < fftSize; ++sample)
        {
            const int dstIndex = (state.ringPos + sample) % fftSize;
            const float reconstructed = state.fftData[(size_t) sample] * inverseScale * window[(size_t) sample];
            state.outputRing[(size_t) dstIndex] += reconstructed;
        }

        for (int group = 0; group < 4; ++group)
        {
            const int start = group * (fftBins / 4);
            const int end = group == 3 ? fftBins : (group + 1) * (fftBins / 4);
            float envSum = 0.0f;
            float shiftedSum = 0.0f;
            for (int bin = start; bin < end; ++bin)
            {
                envSum += envelope[(size_t) bin];
                shiftedSum += warpedEnvelope[(size_t) bin];
            }
            state.envDisplay[(size_t) group] = clamp01(envSum / (float) (end - start) * 0.12f);
            state.shiftedDisplay[(size_t) group] = clamp01(shiftedSum / (float) (end - start) * 0.12f);
        }
    }

    static constexpr int fftOrder = 9;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hopSize = fftSize / 2;

    double sampleRate = 44100.0;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window;
    std::array<ChannelState, 2> channels;
};

class VibratoEngine
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate;
        ringSize = juce::jmax((int) std::round(sampleRate * 0.25), 16384);
        for (auto& channelRing : ring)
            channelRing.assign((size_t) ringSize, 0.0f);
        reset();
    }

    void reset()
    {
        for (auto& channelRing : ring)
            std::fill(channelRing.begin(), channelRing.end(), 0.0f);
        writePos = 0;
        toneLowState = { 0.0f, 0.0f };
        phase = 0.0f;
        wowPhase = 0.0f;
        riseState = 0.0f;
        lastLatencySamples = 256;
    }

    int computeLatencySamples(const PitchTimeParams& params, double bpm) const noexcept
    {
        const float rateHz = resolveRateHz(params.vibRate, 0.1f, 12.0f, params.sync, bpm);
        juce::ignoreUnused(rateHz);
        const float depthMs = 0.3f + clamp01(params.vibDepth / 100.0f) * 11.0f;
        const float detuneMs = clamp01(params.vibDetune / 100.0f) * 2.8f;
        const float baseDelayMs = 7.0f + depthMs * 1.2f;
        return juce::jlimit(32, ringSize - 4, (int) std::round((baseDelayMs + depthMs + detuneMs) * 0.001 * sampleRate));
    }

    double getTailSeconds(const PitchTimeParams& params, double bpm) const noexcept
    {
        return sampleRate > 0.0 ? (double) computeLatencySamples(params, bpm) / sampleRate : 0.0;
    }

    void process(juce::AudioBuffer<float>& buffer,
                 const PitchTimeParams& params,
                 double bpm,
                 PitchTimeEngineSnapshot& snapshot)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || ringSize <= 4)
            return;

        const float rateHz = resolveRateHz(params.vibRate, 0.1f, 12.0f, params.sync, bpm);
        const float depthMs = 0.3f + clamp01(params.vibDepth / 100.0f) * 11.0f;
        const float spread = clamp01(params.vibSpread / 100.0f);
        const float toneTilt = (params.vibTone - 50.0f) / 50.0f;
        const float detuneMs = clamp01(params.vibDetune / 100.0f) * 2.8f;
        const float riseTimeMs = 15.0f + clamp01(params.vibRise / 100.0f) * 1500.0f;
        const float riseCoeff = std::exp(-1.0f / juce::jmax(1.0f, riseTimeMs * 0.001f * (float) sampleRate));
        const float depthSamples = depthMs * 0.001f * (float) sampleRate;
        const float detuneSamples = detuneMs * 0.001f * (float) sampleRate;
        const float baseDelaySamples = (7.0f + depthMs * 1.2f) * 0.001f * (float) sampleRate;
        const float phaseDelta = rateHz / (float) sampleRate;
        const float wowDelta = (0.33f + depthMs * 0.03f) / (float) sampleRate;
        lastLatencySamples = computeLatencySamples(params, bpm);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float inputLeft = buffer.getSample(0, sample);
            const float inputRight = buffer.getSample(numChannels > 1 ? 1 : 0, sample);
            ring[0][(size_t) writePos] = inputLeft;
            ring[1][(size_t) writePos] = inputRight;

            riseState = riseState * riseCoeff + (1.0f - riseCoeff);

            const float lfoLeft = lfoValue(params.vibShape, phase);
            const float lfoRight = lfoValue(params.vibShape, phase + spread * 0.5f);
            const float wow = params.variant == 1 ? std::sin(juce::MathConstants<float>::twoPi * wowPhase) * 0.18f : 0.0f;
            const float choir = params.variant == 2 ? 0.45f + 0.35f * std::sin(juce::MathConstants<float>::twoPi * (phase + 0.19f)) : 0.0f;

            const float delayLeft = baseDelaySamples + depthSamples * riseState * (0.5f + 0.5f * (lfoLeft + wow))
                                  + (params.variant == 2 ? detuneSamples * choir : 0.0f);
            const float delayRight = baseDelaySamples + depthSamples * riseState * (0.5f + 0.5f * (lfoRight - wow))
                                   + (params.variant == 2 ? detuneSamples * (1.0f - choir) : 0.0f);

            float wetLeft = readRingSample(0, delayLeft);
            float wetRight = readRingSample(1, delayRight);

            if (params.variant == 2)
            {
                wetLeft = 0.72f * wetLeft + 0.28f * readRingSample(0, delayLeft + detuneSamples * 0.6f);
                wetRight = 0.72f * wetRight + 0.28f * readRingSample(1, delayRight + detuneSamples * 0.6f);
            }

            wetLeft = simpleTilt(wetLeft, toneTilt, toneLowState[0], sampleRate, 1600.0f);
            wetRight = simpleTilt(wetRight, toneTilt, toneLowState[1], sampleRate, 1600.0f);

            buffer.setSample(0, sample, wetLeft);
            if (numChannels > 1)
                buffer.setSample(1, sample, wetRight);

            writePos = (writePos + 1) % ringSize;
            phase += phaseDelta;
            if (phase >= 1.0f)
                phase -= 1.0f;
            wowPhase += wowDelta;
            if (wowPhase >= 1.0f)
                wowPhase -= 1.0f;
        }

        snapshot.primary = rateHz;
        snapshot.secondary = params.vibDepth;
        snapshot.tertiary = params.vibSpread;
        snapshot.quaternary = riseState;
        snapshot.flagA = params.sync;
        snapshot.flagB = params.variant == 2;
        snapshot.bins[0] = normaliseValue(rateHz, 0.1f, 12.0f);
        snapshot.bins[1] = clamp01(params.vibDepth / 100.0f);
        snapshot.bins[2] = clamp01(params.vibSpread / 100.0f);
        snapshot.bins[3] = clamp01(params.vibRise / 100.0f);
        snapshot.bins[4] = 0.5f + 0.5f * lfoValue(params.vibShape, phase);
        snapshot.bins[5] = 0.5f + 0.5f * lfoValue(params.vibShape, phase + spread * 0.5f);
    }

private:
    float readRingSample(int channel, float delaySamples) const noexcept
    {
        const float clampedDelay = juce::jlimit(1.0f, (float) ringSize - 3.0f, delaySamples);
        float readPos = (float) writePos - clampedDelay;
        while (readPos < 0.0f)
            readPos += (float) ringSize;
        return hermiteRead(ring[(size_t) channel], ringSize, readPos);
    }

    static float lfoValue(int shape, float phaseValue) noexcept
    {
        const float wrapped = phaseValue - std::floor(phaseValue);
        switch (shape)
        {
            case 1: return triangleWave(wrapped);
            case 2:
            {
                const float tri = triangleWave(wrapped);
                return juce::jlimit(-1.0f, 1.0f, tri * 0.7f + std::sin(juce::MathConstants<float>::twoPi * wrapped) * 0.3f);
            }
            default:
                return std::sin(juce::MathConstants<float>::twoPi * wrapped);
        }
    }

    double sampleRate = 44100.0;
    int ringSize = 0;
    int writePos = 0;
    int lastLatencySamples = 256;
    std::array<std::vector<float>, 2> ring;
    std::array<float, 2> toneLowState { 0.0f, 0.0f };
    float phase = 0.0f;
    float wowPhase = 0.0f;
    float riseState = 0.0f;
};

} // namespace pitchtime

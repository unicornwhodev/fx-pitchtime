#include "PluginProcessor.h"
#include "FXComponents.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
struct Runner
{
    int checks = 0;
    int failures = 0;

    void expect(bool condition, const std::string& name)
    {
        ++checks;
        if (condition)
        {
            std::cout << "[PASS] " << name << '\n';
            return;
        }

        ++failures;
        std::cout << "[FAIL] " << name << '\n';
    }
};

void setParameter(MusiquePitchShiftProcessor& processor, const juce::String& id, float value)
{
    auto* parameter = processor.getAPVTS().getParameter(id);
    if (parameter == nullptr)
    {
        std::cerr << "Missing parameter: " << id << '\n';
        std::exit(2);
    }

    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float getParameterValue(MusiquePitchShiftProcessor& processor, const juce::String& id)
{
    if (auto* raw = processor.getAPVTS().getRawParameterValue(id))
        return raw->load();

    std::cerr << "Missing parameter: " << id << '\n';
    std::exit(2);
}

void prepare(MusiquePitchShiftProcessor& processor, int numInputs, int numOutputs, double sampleRate, int maximumBlockSize)
{
    processor.setPlayConfigDetails(numInputs, numOutputs, sampleRate, maximumBlockSize);
    processor.prepareToPlay(sampleRate, maximumBlockSize);
}

juce::AudioBuffer<float> makeStereoSine(int samples, double sampleRate, double frequency, float amplitude = 0.12f)
{
    juce::AudioBuffer<float> buffer(2, samples);
    constexpr double pi = 3.14159265358979323846;
    for (int index = 0; index < samples; ++index)
    {
        const double t = (double) index / sampleRate;
        const float left = (float) (amplitude * std::sin(2.0 * pi * frequency * t));
        const float right = (float) (amplitude * std::sin(2.0 * pi * frequency * t + 0.31));
        buffer.setSample(0, index, left);
        buffer.setSample(1, index, right);
    }
    return buffer;
}

juce::AudioBuffer<float> makeMonoSine(int samples, double sampleRate, double frequency, float amplitude = 0.12f)
{
    juce::AudioBuffer<float> buffer(1, samples);
    constexpr double pi = 3.14159265358979323846;
    for (int index = 0; index < samples; ++index)
    {
        const double t = (double) index / sampleRate;
        buffer.setSample(0, index, (float) (amplitude * std::sin(2.0 * pi * frequency * t)));
    }
    return buffer;
}

juce::AudioBuffer<float> makeStereoHarmonic(int samples, double sampleRate, double baseFrequency)
{
    juce::AudioBuffer<float> buffer(2, samples);
    constexpr double pi = 3.14159265358979323846;
    for (int index = 0; index < samples; ++index)
    {
        const double t = (double) index / sampleRate;
        const float sample = (float) (
            0.12 * std::sin(2.0 * pi * baseFrequency * t)
          + 0.06 * std::sin(2.0 * pi * baseFrequency * 2.0 * t)
          + 0.03 * std::sin(2.0 * pi * baseFrequency * 3.0 * t));
        buffer.setSample(0, index, sample);
        buffer.setSample(1, index, sample * 0.92f);
    }
    return buffer;
}

juce::AudioBuffer<float> makeStereoNoise(int samples, float amplitude = 0.08f)
{
    juce::AudioBuffer<float> buffer(2, samples);
    juce::Random rng(42);
    for (int channel = 0; channel < 2; ++channel)
        for (int index = 0; index < samples; ++index)
            buffer.setSample(channel, index, rng.nextFloat() * amplitude * 2.0f - amplitude);
    return buffer;
}

juce::AudioBuffer<float> makeImpulse(int samples, int channels = 2, float amplitude = 0.8f)
{
    juce::AudioBuffer<float> buffer(channels, samples);
    buffer.clear();
    for (int channel = 0; channel < channels; ++channel)
        buffer.setSample(channel, 0, amplitude);
    return buffer;
}

juce::AudioBuffer<float> makeSilence(int samples, int channels = 2)
{
    juce::AudioBuffer<float> buffer(channels, samples);
    buffer.clear();
    return buffer;
}

float maxAbs(const juce::AudioBuffer<float>& buffer)
{
    float value = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            value = juce::jmax(value, std::abs(buffer.getSample(channel, sample)));
    return value;
}

bool isFiniteBuffer(const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            if (!std::isfinite(buffer.getSample(channel, sample)))
                return false;
    return true;
}

float differenceEnergy(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    const int channels = juce::jmin(a.getNumChannels(), b.getNumChannels());
    const int samples = juce::jmin(a.getNumSamples(), b.getNumSamples());
    float sum = 0.0f;
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < samples; ++sample)
            sum += std::abs(a.getSample(channel, sample) - b.getSample(channel, sample));
    return sum / (float) juce::jmax(1, channels * samples);
}

juce::ValueTree copyStateTree(MusiquePitchShiftProcessor& processor)
{
    juce::MemoryBlock stateData;
    processor.getStateInformation(stateData);
    auto xml = juce::AudioProcessor::getXmlFromBinary(stateData.getData(), (int) stateData.getSize());
    if (xml == nullptr)
    {
        std::cerr << "Failed to decode state XML\n";
        std::exit(2);
    }

    return juce::ValueTree::fromXml(*xml);
}

void loadStateTree(MusiquePitchShiftProcessor& processor, const juce::ValueTree& state)
{
    auto xml = state.createXml();
    if (xml == nullptr)
    {
        std::cerr << "Failed to encode state XML\n";
        std::exit(2);
    }

    juce::MemoryBlock stateData;
    juce::AudioProcessor::copyXmlToBinary(*xml, stateData);
    processor.setStateInformation(stateData.getData(), (int) stateData.getSize());
}

void removeParameterFromState(juce::ValueTree& state, const juce::String& id)
{
    for (int index = state.getNumChildren() - 1; index >= 0; --index)
    {
        auto child = state.getChild(index);
        if (child.hasType("PARAM") && child.getProperty("id").toString() == id)
            state.removeChild(index, nullptr);
    }
}

void process(MusiquePitchShiftProcessor& processor, juce::AudioBuffer<float>& buffer)
{
    juce::MidiBuffer midi;
    processor.processBlock(buffer, midi);
}
}

int main()
{
    Runner runner;
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "pitch", 7.0f);
        setParameter(processor, "voice", 3.0f);
        setParameter(processor, "detune", 9.0f);

        auto state = copyStateTree(processor);
        removeParameterFromState(state, "engine");
        removeParameterFromState(state, "variant");
        removeParameterFromState(state, "sync");
        loadStateTree(processor, state);

        runner.expect((int) std::round(getParameterValue(processor, "engine")) == 0, "legacy state defaults to pitch shift engine");
        runner.expect(getParameterValue(processor, "sync") < 0.5f, "legacy state injects sync=false");
        runner.expect((int) std::round(getParameterValue(processor, "variant")) == 2, "legacy state derives harmony variant when legacy pitch already exceeds 5 st");
        runner.expect(std::abs(getParameterValue(processor, "pitch") - 7.0f) < 0.001f, "legacy state preserves pitch parameter");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "engine", 4.0f);

        juce::DynamicObject::Ptr object = new juce::DynamicObject();
        object->setProperty("name", "LegacyPreset");
        object->setProperty("pitch", 12.0f);
        object->setProperty("voice", 2.0f);
        object->setProperty("stereo_stack", true);
        object->setProperty("mix", 42.0f);
        juce::var preset(object.get());
        MusiquePitchShiftProcessor::normalisePresetObject(preset);
        fx::preset::applyToAPVTS(processor.getAPVTS(), preset);
        processor.postExternalStateChange();

        runner.expect((int) std::round(getParameterValue(processor, "engine")) == 0, "legacy preset JSON normalises back to pitch engine");
        runner.expect((int) std::round(getParameterValue(processor, "variant")) == 2, "legacy preset JSON derives harmony variant");
        runner.expect(std::abs(getParameterValue(processor, "mix") - 42.0f) < 0.001f, "legacy preset JSON keeps legacy scalar fields");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "engine", 0.0f);
        setParameter(processor, "pitch", 12.0f);
        setParameter(processor, "voice", 4.0f);
        setParameter(processor, "detune", 18.0f);
        setParameter(processor, "spread", 100.0f);
        setParameter(processor, "mix", 100.0f);

        float peak = 0.0f;
        for (int block = 0; block < 80; ++block)
        {
            auto buffer = block == 0 ? makeImpulse(blockSize) : makeSilence(blockSize);
            process(processor, buffer);
            peak = juce::jmax(peak, maxAbs(buffer));
            runner.expect(isFiniteBuffer(buffer), "pitch shift buffer stays finite under 4-voice extreme ratio");
        }

        runner.expect(peak > 0.001f, "pitch shift produces audible output");
        runner.expect(peak < 2.2f, "pitch shift stays bounded at 4 voices");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 1, 1, sampleRate, blockSize);
        setParameter(processor, "engine", 0.0f);
        setParameter(processor, "pitch", -5.0f);
        float monoPeak = 0.0f;
        for (int block = 0; block < 10; ++block)
        {
            auto monoBuffer = makeMonoSine(blockSize, sampleRate, 220.0);
            process(processor, monoBuffer);
            monoPeak = juce::jmax(monoPeak, maxAbs(monoBuffer));
            runner.expect(isFiniteBuffer(monoBuffer), "mono buffer processing stays finite");
        }

        juce::AudioProcessor::BusesLayout monoLayout;
        monoLayout.inputBuses.add(juce::AudioChannelSet::mono());
        monoLayout.outputBuses.add(juce::AudioChannelSet::mono());
        juce::AudioProcessor::BusesLayout stereoLayout;
        stereoLayout.inputBuses.add(juce::AudioChannelSet::stereo());
        stereoLayout.outputBuses.add(juce::AudioChannelSet::stereo());

        runner.expect(processor.isBusesLayoutSupported(monoLayout), "mono->mono layout is supported");
        runner.expect(processor.isBusesLayoutSupported(stereoLayout), "stereo->stereo layout is supported");
        runner.expect(monoPeak > 0.0001f, "mono processing becomes audible after latency fill");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "engine", 1.0f);
        setParameter(processor, "stretch_ratio", 160.0f);
        setParameter(processor, "stretch_window", 90.0f);
        setParameter(processor, "mix", 100.0f);

        float peak = 0.0f;
        for (int block = 0; block < 40; ++block)
        {
            auto buffer = makeStereoSine(blockSize, sampleRate, 220.0);
            process(processor, buffer);
            peak = juce::jmax(peak, maxAbs(buffer));
            runner.expect(isFiniteBuffer(buffer), "time stretch buffer stays finite while ratio changes");
        }

        setParameter(processor, "stretch_ratio", 72.0f);
        auto buffer = makeStereoSine(blockSize, sampleRate, 220.0);
        process(processor, buffer);
        runner.expect(processor.getLatencySamples() > 0, "time stretch reports non-zero latency");
        runner.expect(peak > 0.0001f && maxAbs(buffer) > 0.0001f, "time stretch remains audible");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "engine", 2.0f);
        setParameter(processor, "variant", 1.0f);
        setParameter(processor, "tune_amount", 100.0f);
        setParameter(processor, "tune_speed", 100.0f);
        setParameter(processor, "tune_scale", 1.0f);
        setParameter(processor, "tune_key", 0.0f);
        setParameter(processor, "mix", 100.0f);

        float diff = 0.0f;
        for (int block = 0; block < 20; ++block)
        {
            auto input = makeStereoSine(blockSize, sampleRate, 452.0);
            auto dryCopy = input;
            process(processor, input);
            diff += differenceEnergy(input, dryCopy);
        }

        const auto snapshot = processor.getPitchTimeSnapshot();
        runner.expect(snapshot.flagA, "autotune locks a stable monophonic sine");
        runner.expect(snapshot.confidence > 0.25f, "autotune reports meaningful pitch confidence");
        runner.expect(diff > 0.001f, "autotune changes the signal under confident detection");

        setParameter(processor, "engine", 2.0f);
        float noiseConfidence = 1.0f;
        bool noiseTrackingActive = true;
        for (int block = 0; block < 8; ++block)
        {
            auto noise = makeStereoNoise(blockSize, 0.14f);
            process(processor, noise);
            const auto noiseSnapshot = processor.getPitchTimeSnapshot();
            noiseConfidence = noiseSnapshot.confidence;
            noiseTrackingActive = noiseSnapshot.flagA;
            runner.expect(isFiniteBuffer(noise), "autotune stays finite on noisy input");
        }
        runner.expect(!noiseTrackingActive || noiseConfidence < 0.2f, "autotune confidence drops or tracking disengages on noisy input");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "engine", 3.0f);
        setParameter(processor, "fmt_shift", 5.0f);
        setParameter(processor, "fmt_focus", 70.0f);
        setParameter(processor, "mix", 100.0f);

        auto source = makeStereoHarmonic(blockSize * 3, sampleRate, 180.0);
        auto dryCopy = source;
        process(processor, source);
        runner.expect(isFiniteBuffer(source), "formant shift output stays finite");
        runner.expect(differenceEnergy(source, dryCopy) > 0.0005f, "formant shift modifies spectral envelope audibly");
        runner.expect(processor.getPitchTimeSnapshot().flagB, "formant shift reports active spectral warp");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "engine", 4.0f);
        setParameter(processor, "sync", 1.0f);
        setParameter(processor, "vib_rate", 0.1f);
        setParameter(processor, "vib_depth", 48.0f);
        setParameter(processor, "mix", 100.0f);

        auto buffer = makeStereoSine(blockSize * 2, sampleRate, 330.0);
        auto dryCopy = buffer;
        process(processor, buffer);
        const auto snapshot = processor.getPitchTimeSnapshot();

        runner.expect(isFiniteBuffer(buffer), "vibrato output stays finite");
        runner.expect(differenceEnergy(buffer, dryCopy) > 0.0005f, "vibrato produces an audible modulation");
        runner.expect(snapshot.flagA, "vibrato snapshot records sync mode");
        runner.expect(snapshot.primary > 0.5f, "vibrato sync resolves to a musical rate instead of raw 0.1 Hz");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "engine", 0.0f);
        setParameter(processor, "mix", 0.0f);
        setParameter(processor, "output", -6.0f);
        for (int block = 0; block < 6; ++block)
        {
            auto warmup = makeSilence(blockSize);
            process(processor, warmup);
        }
        auto dry = makeStereoSine(blockSize, sampleRate, 220.0);
        auto expected = dry;
        expected.applyGain(pitchtime::dbToGain(-6.0f));
        process(processor, dry);
        runner.expect(differenceEnergy(dry, expected) < 0.01f, "mix=0 keeps the dry path with output trim");

        setParameter(processor, "mono", 1.0f);
        auto stereo = makeStereoSine(blockSize, sampleRate, 220.0);
        process(processor, stereo);
        runner.expect(std::abs(stereo.getSample(0, 40) - stereo.getSample(1, 40)) < 0.0001f, "mono fold keeps both channels aligned");

        setParameter(processor, "bypass", 1.0f);
        auto bypassed = makeStereoSine(blockSize, sampleRate, 220.0);
        auto bypassExpected = bypassed;
        for (int sample = 0; sample < bypassExpected.getNumSamples(); ++sample)
        {
            const float monoSample = 0.5f * (bypassExpected.getSample(0, sample) + bypassExpected.getSample(1, sample));
            bypassExpected.setSample(0, sample, monoSample);
            bypassExpected.setSample(1, sample, monoSample);
        }
        bypassExpected.applyGain(pitchtime::dbToGain(-6.0f));
        process(processor, bypassed);
        runner.expect(differenceEnergy(bypassed, bypassExpected) < 0.01f, "bypass applies mono fold and output trim consistently");
    }

    {
        MusiquePitchShiftProcessor processor;
        prepare(processor, 2, 2, sampleRate, blockSize);
        setParameter(processor, "engine", 4.0f);
        setParameter(processor, "variant", 2.0f);
        setParameter(processor, "vib_depth", 65.0f);
        const auto stateA = processor.getAPVTS().copyState();

        setParameter(processor, "engine", 1.0f);
        setParameter(processor, "variant", 1.0f);
        setParameter(processor, "stretch_ratio", 150.0f);
        const auto stateB = processor.getAPVTS().copyState();

        processor.getAPVTS().replaceState(stateA);
        processor.postExternalStateChange();
        runner.expect((int) std::round(getParameterValue(processor, "engine")) == 4, "state recall restores slot A engine");
        runner.expect((int) std::round(getParameterValue(processor, "variant")) == 2, "state recall restores slot A variant");

        processor.getAPVTS().replaceState(stateB);
        processor.postExternalStateChange();
        runner.expect((int) std::round(getParameterValue(processor, "engine")) == 1, "state recall restores slot B engine");
    }

    std::cout << "Checks: " << runner.checks << ", failures: " << runner.failures << '\n';
    return runner.failures == 0 ? 0 : 1;
}

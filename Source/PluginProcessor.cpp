#include "PluginProcessor.h"
#include "PluginEditor.h"
namespace
{
    static float dbToGain(float dB) { return std::pow(10.0f, dB / 20.0f); }
}

MusiquePitchShiftProcessor::MusiquePitchShiftProcessor():AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),parameters(*this,nullptr,"MusiquePitch",createParameterLayout()){}
juce::AudioProcessorValueTreeState::ParameterLayout MusiquePitchShiftProcessor::createParameterLayout(){ std::vector<std::unique_ptr<juce::RangedAudioParameter>> p; p.push_back(std::make_unique<juce::AudioParameterFloat>("pitch","Pitch",-12.0f,12.0f,0.0f)); p.push_back(std::make_unique<juce::AudioParameterFloat>("fine","Fine",-100.0f,100.0f,0.0f)); p.push_back(std::make_unique<juce::AudioParameterFloat>("formant","Formant",0.0f,1.0f,0.5f)); p.push_back(std::make_unique<juce::AudioParameterFloat>("voice","Voice",1.0f,4.0f,1.0f)); p.push_back(std::make_unique<juce::AudioParameterFloat>("spread","Spread",juce::NormalisableRange<float>(0.0f,100.0f,0.1f,0.7f),55.0f)); p.push_back(std::make_unique<juce::AudioParameterFloat>("detune","Detune",juce::NormalisableRange<float>(0.0f,18.0f,0.05f,0.65f),7.0f)); p.push_back(std::make_unique<juce::AudioParameterFloat>("octave","Octave",-2.0f,2.0f,0.0f)); p.push_back(std::make_unique<juce::AudioParameterFloat>("mix","Mix",0.0f,100.0f,100.0f)); p.push_back(std::make_unique<juce::AudioParameterFloat>("output","Output",-24.0f,12.0f,0.0f)); p.push_back(std::make_unique<juce::AudioParameterBool>("stereo_stack","Stereo Stack",true)); p.push_back(std::make_unique<juce::AudioParameterBool>("bypass","Bypass",false)); p.push_back(std::make_unique<juce::AudioParameterBool>("mono","Mono",false)); return {p.begin(),p.end()}; }
void MusiquePitchShiftProcessor::prepareToPlay(double sr, int)
{
    sampleRate = sr;
    grainSizeSamples = juce::jlimit(1024, 4096, (int) std::round(sr * 0.04));
    if ((grainSizeSamples & 1) != 0)
        ++grainSizeSamples;

    baseDelaySamples = juce::jmax(64, grainSizeSamples / 8);
    latencySamples = baseDelaySamples + grainSizeSamples / 2;
    ringSize = juce::jmax((int) std::round(sr * 0.35), latencySamples + grainSizeSamples * 2 + 2048);

    ring[0].assign((size_t) ringSize, 0.0f);
    ring[1].assign((size_t) ringSize, 0.0f);
    writePos = 0;
    grainPhases.fill(0.0f);
    currentRatio = 1.0f;
    currentMix = parameters.getRawParameterValue("mix")->load() / 100.0f;
    currentOutputGain = dbToGain(parameters.getRawParameterValue("output")->load());
    currentFormant = parameters.getRawParameterValue("formant")->load();
    currentSpread = parameters.getRawParameterValue("spread")->load() / 100.0f;
    currentDetune = parameters.getRawParameterValue("detune")->load();
    formantLowState = { 0.0f, 0.0f };
    setLatencySamples(latencySamples);
}

void MusiquePitchShiftProcessor::releaseResources()
{
    ring[0].clear();
    ring[1].clear();
    writePos = 0;
    ringSize = 0;
    grainPhases.fill(0.0f);
    formantLowState = { 0.0f, 0.0f };
    setLatencySamples(0);
}

bool MusiquePitchShiftProcessor::isBusesLayoutSupported(const BusesLayout& l) const{ return l.getMainInputChannelSet()==juce::AudioChannelSet::stereo()&&l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo(); }

double MusiquePitchShiftProcessor::getTailLengthSeconds() const
{
    return sampleRate > 0.0 ? (double) latencySamples / sampleRate : 0.0;
}

float MusiquePitchShiftProcessor::readSampleWithDelay(int channel, float delaySamples) const noexcept
{
    if (ringSize <= 1)
        return 0.0f;

    float readPos = (float) writePos - juce::jlimit(1.0f, (float) (ringSize - 2), delaySamples);
    while (readPos < 0.0f)
        readPos += (float) ringSize;

    const int index0 = ((int) std::floor(readPos)) % ringSize;
    const int index1 = (index0 + 1) % ringSize;
    const float frac = readPos - (float) index0;
    return juce::jmap(frac, ring[(size_t) channel][(size_t) index0], ring[(size_t) channel][(size_t) index1]);
}

float MusiquePitchShiftProcessor::grainEnvelope(float phase) noexcept
{
    return 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * phase);
}

void MusiquePitchShiftProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    visualState.captureInput(b);
    const int numSamples = b.getNumSamples();
    if (numSamples <= 0 || ringSize <= 1)
        return;

    const bool mono = parameters.getRawParameterValue("mono")->load() > 0.5f;
    const bool stereoStack = parameters.getRawParameterValue("stereo_stack")->load() > 0.5f;
    const bool bypass = parameters.getRawParameterValue("bypass")->load() > 0.5f;
    const float output = parameters.getRawParameterValue("output")->load();

    if (mono)
        for (int i = 0; i < numSamples; ++i)
        {
            float m = 0.5f * (b.getSample(0, i) + b.getSample(1, i));
            b.setSample(0, i, m);
            b.setSample(1, i, m);
        }

    if (bypass)
    {
        b.applyGain(dbToGain(output));
        visualState.captureOutput(b);
        return;
    }

    const float semi = parameters.getRawParameterValue("pitch")->load()
        + parameters.getRawParameterValue("octave")->load() * 12.0f
        + parameters.getRawParameterValue("fine")->load() * 0.01f;
    const int numVoices = juce::jlimit(1, maxVoices, (int) std::round(parameters.getRawParameterValue("voice")->load()));
    const float targetRatio = std::pow(2.0f, semi / 12.0f);
    const float targetMix = parameters.getRawParameterValue("mix")->load() / 100.0f;
    const float targetOutputGain = dbToGain(output);
    const float targetFormant = parameters.getRawParameterValue("formant")->load();
    const float targetSpread = parameters.getRawParameterValue("spread")->load() / 100.0f;
    const float targetDetune = parameters.getRawParameterValue("detune")->load();
    const float ratioStep = (targetRatio - currentRatio) / (float) numSamples;
    const float mixStep = (targetMix - currentMix) / (float) numSamples;
    const float outputStep = (targetOutputGain - currentOutputGain) / (float) numSamples;
    const float formantStep = (targetFormant - currentFormant) / (float) numSamples;
    const float spreadStep = (targetSpread - currentSpread) / (float) numSamples;
    const float detuneStep = (targetDetune - currentDetune) / (float) numSamples;

    for (int i = 0; i < numSamples; ++i)
    {
        currentRatio += ratioStep;
        currentMix += mixStep;
        currentOutputGain += outputStep;
        currentFormant += formantStep;
        currentSpread += spreadStep;
        currentDetune += detuneStep;

        const float ratio = juce::jlimit(0.25f, 4.0f, currentRatio);
        const float mix = juce::jlimit(0.0f, 1.0f, currentMix);
        const float out = currentOutputGain;
        const float formant = juce::jlimit(0.0f, 1.0f, currentFormant);
        const float spread = juce::jlimit(0.0f, 1.0f, currentSpread);
        const float detune = juce::jlimit(0.0f, 18.0f, currentDetune);
        const float perceptualSpread = 0.82f * std::pow(spread, 1.6f);
        const float perceptualDetune = 18.0f * std::pow(detune / 18.0f, 1.35f);
        const float inputLeft = b.getSample(0, i);
        const float inputRight = b.getSample(1, i);
        ring[0][(size_t) writePos] = inputLeft;
        ring[1][(size_t) writePos] = inputRight;

        const float dryLeft = readSampleWithDelay(0, (float) latencySamples);
        const float dryRight = readSampleWithDelay(1, (float) latencySamples);
        float wetLeft = 0.0f;
        float wetRight = 0.0f;

        const float detuneSpreadCents = numVoices > 1 ? perceptualDetune : 0.0f;
        const float wetNormalisation = 1.0f / std::sqrt((float) numVoices);

        for (int voiceIndex = 0; voiceIndex < numVoices; ++voiceIndex)
        {
            const float voiceNorm = numVoices == 1
                ? 0.0f
                : juce::jmap((float) voiceIndex, 0.0f, (float) (numVoices - 1), -1.0f, 1.0f);
            const float detuneCents = voiceNorm * detuneSpreadCents;
            const float voiceRatio = juce::jlimit(0.25f, 4.0f, ratio * std::pow(2.0f, detuneCents / 1200.0f));
            const float voiceRatioDelta = voiceRatio - 1.0f;
            const float phaseAdvance = std::abs(voiceRatioDelta) > 1.0e-4f
                ? juce::jlimit(1.0f / (float) grainSizeSamples, 0.25f, std::abs(voiceRatioDelta) / (float) grainSizeSamples)
                : (numVoices > 1 ? 1.0f / (float) grainSizeSamples : 0.0f);

            const float phaseA = grainPhases[(size_t) voiceIndex];
            const float phaseB = std::fmod(phaseA + 0.5f, 1.0f);

            const float delayA = voiceRatioDelta >= 0.0f
                ? (float) baseDelaySamples + (1.0f - phaseA) * (float) grainSizeSamples
                : (float) baseDelaySamples + phaseA * (float) grainSizeSamples;
            const float delayB = voiceRatioDelta >= 0.0f
                ? (float) baseDelaySamples + (1.0f - phaseB) * (float) grainSizeSamples
                : (float) baseDelaySamples + phaseB * (float) grainSizeSamples;

            const float envA = grainEnvelope(phaseA);
            const float envB = grainEnvelope(phaseB);
            const float envNorm = juce::jmax(1.0e-5f, envA + envB);

            float voiceLeft = dryLeft;
            float voiceRight = dryRight;

            if (std::abs(voiceRatioDelta) > 1.0e-4f || numVoices > 1)
            {
                const float grainALeft = readSampleWithDelay(0, delayA);
                const float grainARight = readSampleWithDelay(1, delayA);
                const float grainBLeft = readSampleWithDelay(0, delayB);
                const float grainBRight = readSampleWithDelay(1, delayB);
                voiceLeft = (grainALeft * envA + grainBLeft * envB) / envNorm;
                voiceRight = (grainARight * envA + grainBRight * envB) / envNorm;
            }

            const float pan = (!stereoStack || mono) ? 0.0f : voiceNorm * perceptualSpread;
            const float panLeft = std::sqrt(0.5f * (1.0f - pan));
            const float panRight = std::sqrt(0.5f * (1.0f + pan));
            wetLeft += voiceLeft * panLeft;
            wetRight += voiceRight * panRight;

            grainPhases[(size_t) voiceIndex] += phaseAdvance;
            if (grainPhases[(size_t) voiceIndex] >= 1.0f)
                grainPhases[(size_t) voiceIndex] -= 1.0f;
        }

        if (numVoices == 1)
        {
            wetLeft = wetLeft * juce::MathConstants<float>::sqrt2;
            wetRight = wetRight * juce::MathConstants<float>::sqrt2;
        }
        else
        {
            wetLeft *= wetNormalisation;
            wetRight *= wetNormalisation;
        }

        const float formantTilt = (formant - 0.5f) * 2.0f;
        const float safeSampleRate = juce::jmax(1.0f, (float) sampleRate);
        const float formantCutoff = juce::jlimit(120.0f, safeSampleRate * 0.45f, 850.0f + std::abs(formantTilt) * 2550.0f);
        const float formantCoeff = std::exp(-juce::MathConstants<float>::twoPi * formantCutoff / safeSampleRate);
        formantLowState[0] = formantLowState[0] * formantCoeff + wetLeft * (1.0f - formantCoeff);
        formantLowState[1] = formantLowState[1] * formantCoeff + wetRight * (1.0f - formantCoeff);
        const float wetLowLeft = formantLowState[0];
        const float wetLowRight = formantLowState[1];
        const float wetHighLeft = wetLeft - wetLowLeft;
        const float wetHighRight = wetRight - wetLowRight;
        const float lowGain = formantTilt >= 0.0f ? 1.0f - 0.45f * formantTilt : 1.0f + 0.65f * -formantTilt;
        const float highGain = formantTilt >= 0.0f ? 1.0f + 1.1f * formantTilt : 1.0f - 0.5f * -formantTilt;
        wetLeft = wetLowLeft * lowGain + wetHighLeft * highGain;
        wetRight = wetLowRight * lowGain + wetHighRight * highGain;

        b.setSample(0, i, (dryLeft * (1.0f - mix) + wetLeft * mix) * out);
        b.setSample(1, i, (dryRight * (1.0f - mix) + wetRight * mix) * out);

        writePos = (writePos + 1) % ringSize;
    }

    currentRatio = targetRatio;
    currentMix = targetMix;
    currentOutputGain = targetOutputGain;
    currentFormant = targetFormant;
    currentSpread = targetSpread;
    currentDetune = targetDetune;
    visualState.captureOutput(b);
}
void MusiquePitchShiftProcessor::getStateInformation(juce::MemoryBlock& d){ auto s=parameters.copyState(); std::unique_ptr<juce::XmlElement> x(s.createXml()); copyXmlToBinary(*x,d);} void MusiquePitchShiftProcessor::setStateInformation(const void* data,int size){ std::unique_ptr<juce::XmlElement> x(getXmlFromBinary(data,size)); if(x&&x->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*x)); }
juce::AudioProcessorEditor* MusiquePitchShiftProcessor::createEditor(){ return new MusiquePitchShiftEditor(*this);} juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new MusiquePitchShiftProcessor(); }

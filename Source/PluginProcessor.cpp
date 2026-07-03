#include "PluginProcessor.h"
#if ! MUSIQUE_PITCHTIME_DSP_TESTS
#include "PluginEditor.h"
#endif

namespace
{
float getRawValue(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float fallback = 0.0f)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return raw->load();
    return fallback;
}

int getChoiceValue(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int fallback = 0)
{
    return (int) std::round(getRawValue(apvts, id, (float) fallback));
}
}

MusiquePitchShiftProcessor::MusiquePitchShiftProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "MusiquePitch", createParameterLayout())
{
    mixSmoothed.reset(preparedSampleRate, 0.03);
    outputSmoothed.reset(preparedSampleRate, 0.03);
    clearSnapshot();
    postExternalStateChange();
}

MusiquePitchShiftProcessor::~MusiquePitchShiftProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout MusiquePitchShiftProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parametersOut;

    parametersOut.push_back(std::make_unique<juce::AudioParameterChoice>(
        "engine", "Engine", juce::StringArray { "Pitch Shift", "Time Stretch", "AutoTune", "Formant Shift", "Vibrato" }, 0));
    parametersOut.push_back(std::make_unique<juce::AudioParameterChoice>(
        "variant", "Variant", juce::StringArray { "A", "B", "C" }, 0));
    parametersOut.push_back(std::make_unique<juce::AudioParameterBool>("sync", "Sync", false));

    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("pitch", "Pitch", -12.0f, 12.0f, 0.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("fine", "Fine", -100.0f, 100.0f, 0.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("formant", "Formant", 0.0f, 1.0f, 0.5f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("voice", "Voice", 1.0f, 4.0f, 1.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>(
        "spread", "Spread", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f, 0.7f), 55.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>(
        "detune", "Detune", juce::NormalisableRange<float>(0.0f, 18.0f, 0.05f, 0.65f), 7.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("octave", "Octave", -2.0f, 2.0f, 0.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 100.0f, 100.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("output", "Output", -24.0f, 12.0f, 0.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterBool>("stereo_stack", "Stereo Stack", true));
    parametersOut.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    parametersOut.push_back(std::make_unique<juce::AudioParameterBool>("mono", "Mono", false));

    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("stretch_ratio", "Stretch Ratio", 50.0f, 200.0f, 100.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("stretch_window", "Stretch Window", 15.0f, 1000.0f, 220.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("stretch_grain", "Stretch Grain", 0.0f, 100.0f, 50.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("stretch_transient", "Stretch Transient", 0.0f, 100.0f, 55.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("stretch_tone", "Stretch Tone", 0.0f, 100.0f, 50.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("stretch_spread", "Stretch Spread", 0.0f, 100.0f, 40.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("stretch_smooth", "Stretch Smooth", 0.0f, 100.0f, 60.0f));

    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("tune_amount", "Tune Amount", 0.0f, 100.0f, 80.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("tune_speed", "Tune Speed", 0.0f, 100.0f, 55.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("tune_humanize", "Tune Humanize", 0.0f, 100.0f, 25.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterChoice>(
        "tune_key", "Tune Key", juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    parametersOut.push_back(std::make_unique<juce::AudioParameterChoice>(
        "tune_scale", "Tune Scale", juce::StringArray { "Chromatic", "Major", "Minor", "Pentatonic" }, 1));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("tune_formant", "Tune Formant", 0.0f, 100.0f, 55.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterChoice>(
        "tune_range", "Tune Range", juce::StringArray { "Low", "Mid", "High", "Full" }, 3));

    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("fmt_shift", "Formant Shift", -12.0f, 12.0f, 0.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("fmt_fine", "Formant Fine", -100.0f, 100.0f, 0.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("fmt_focus", "Formant Focus", 0.0f, 100.0f, 55.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("fmt_brightness", "Formant Brightness", 0.0f, 100.0f, 50.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("fmt_body", "Formant Body", 0.0f, 100.0f, 50.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("fmt_unvoiced", "Formant Unvoiced", 0.0f, 100.0f, 50.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("fmt_spread", "Formant Spread", 0.0f, 100.0f, 20.0f));

    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("vib_rate", "Vibrato Rate", 0.1f, 12.0f, 3.2f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("vib_depth", "Vibrato Depth", 0.0f, 100.0f, 35.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("vib_spread", "Vibrato Spread", 0.0f, 100.0f, 40.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterChoice>(
        "vib_shape", "Vibrato Shape", juce::StringArray { "Sine", "Triangle", "Tape" }, 0));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("vib_tone", "Vibrato Tone", 0.0f, 100.0f, 50.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("vib_rise", "Vibrato Rise", 0.0f, 100.0f, 20.0f));
    parametersOut.push_back(std::make_unique<juce::AudioParameterFloat>("vib_detune", "Vibrato Detune", 0.0f, 100.0f, 20.0f));

    return { parametersOut.begin(), parametersOut.end() };
}

const std::array<MusiquePitchShiftProcessor::SyncDivision, 9>& MusiquePitchShiftProcessor::getSyncDivisions() noexcept
{
    return pitchtime::getSyncDivisions();
}

juce::StringArray MusiquePitchShiftProcessor::getAllParameterIds()
{
    return {
        "engine", "variant", "sync",
        "pitch", "fine", "formant", "voice", "spread", "detune", "octave",
        "mix", "output", "stereo_stack", "bypass", "mono",
        "stretch_ratio", "stretch_window", "stretch_grain", "stretch_transient", "stretch_tone", "stretch_spread", "stretch_smooth",
        "tune_amount", "tune_speed", "tune_humanize", "tune_key", "tune_scale", "tune_formant", "tune_range",
        "fmt_shift", "fmt_fine", "fmt_focus", "fmt_brightness", "fmt_body", "fmt_unvoiced", "fmt_spread",
        "vib_rate", "vib_depth", "vib_spread", "vib_shape", "vib_tone", "vib_rise", "vib_detune"
    };
}

void MusiquePitchShiftProcessor::normalisePresetObject(juce::var& preset)
{
    auto* object = preset.getDynamicObject();
    if (object == nullptr)
        return;

    auto ensure = [&](const char* key, const juce::var& value)
    {
        if (!object->hasProperty(key))
            object->setProperty(key, value);
    };

    if (!object->hasProperty("engine"))
        object->setProperty("engine", 0);
    if (!object->hasProperty("sync"))
        object->setProperty("sync", false);

    const float legacyPitch = (float) (object->hasProperty("pitch") ? object->getProperty("pitch") : juce::var(0.0f));
    const float legacyOctave = (float) (object->hasProperty("octave") ? object->getProperty("octave") : juce::var(0.0f));
    const float legacyVoice = (float) (object->hasProperty("voice") ? object->getProperty("voice") : juce::var(1.0f));
    const float legacyDetune = (float) (object->hasProperty("detune") ? object->getProperty("detune") : juce::var(0.0f));
    const bool legacyStereoStack = (bool) (object->hasProperty("stereo_stack") ? object->getProperty("stereo_stack") : juce::var(true));

    if (!object->hasProperty("variant"))
    {
        int variant = 0;
        if (std::abs(legacyOctave) >= 1.0f || std::abs(legacyPitch) >= 5.0f)
            variant = 2;
        else if (legacyVoice > 1.0f || legacyStereoStack || legacyDetune > 1.0f)
            variant = 1;
        object->setProperty("variant", variant);
    }

    ensure("fine", 0.0f);
    ensure("formant", 0.5f);
    ensure("voice", 1.0f);
    ensure("spread", 55.0f);
    ensure("detune", 7.0f);
    ensure("octave", 0.0f);
    ensure("mix", 100.0f);
    ensure("output", 0.0f);
    ensure("stereo_stack", true);
    ensure("bypass", false);
    ensure("mono", false);

    ensure("stretch_ratio", 100.0f);
    ensure("stretch_window", 220.0f);
    ensure("stretch_grain", 50.0f);
    ensure("stretch_transient", 55.0f);
    ensure("stretch_tone", 50.0f);
    ensure("stretch_spread", 40.0f);
    ensure("stretch_smooth", 60.0f);

    ensure("tune_amount", 80.0f);
    ensure("tune_speed", 55.0f);
    ensure("tune_humanize", 25.0f);
    ensure("tune_key", 0);
    ensure("tune_scale", 1);
    ensure("tune_formant", 55.0f);
    ensure("tune_range", 3);

    ensure("fmt_shift", 0.0f);
    ensure("fmt_fine", 0.0f);
    ensure("fmt_focus", 55.0f);
    ensure("fmt_brightness", 50.0f);
    ensure("fmt_body", 50.0f);
    ensure("fmt_unvoiced", 50.0f);
    ensure("fmt_spread", 20.0f);

    ensure("vib_rate", 3.2f);
    ensure("vib_depth", 35.0f);
    ensure("vib_spread", 40.0f);
    ensure("vib_shape", 0);
    ensure("vib_tone", 50.0f);
    ensure("vib_rise", 20.0f);
    ensure("vib_detune", 20.0f);
}

void MusiquePitchShiftProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = sampleRate;
    mixSmoothed.reset(sampleRate, 0.025);
    outputSmoothed.reset(sampleRate, 0.025);
    mixSmoothed.setCurrentAndTargetValue(getRawValue(parameters, "mix", 100.0f) / 100.0f);
    outputSmoothed.setCurrentAndTargetValue(pitchtime::dbToGain(getRawValue(parameters, "output", 0.0f)));

    wetBuffer.setSize(juce::jmax(1, getTotalNumOutputChannels()), juce::jmax(1, samplesPerBlock), false, false, true);

    pitchShiftEngineState.prepare(sampleRate);
    timeStretchEngineState.prepare(sampleRate);
    autoTuneEngineState.prepare(sampleRate);
    formantShiftEngineState.prepare(sampleRate);
    vibratoEngineState.prepare(sampleRate);

    postExternalStateChange();
}

void MusiquePitchShiftProcessor::releaseResources()
{
    wetBuffer.setSize(0, 0);
    resetAllEngines();
    lastLatencySamples = 0;
    setLatencySamples(0);
}

bool MusiquePitchShiftProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    const bool mono = input == juce::AudioChannelSet::mono() && output == juce::AudioChannelSet::mono();
    const bool stereo = input == juce::AudioChannelSet::stereo() && output == juce::AudioChannelSet::stereo();
    return mono || stereo;
}

void MusiquePitchShiftProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    visualState.captureInput(buffer);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const bool monoInput = getRawValue(parameters, "mono") > 0.5f;
    if (monoInput && numChannels > 1)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float monoSample = 0.5f * (buffer.getSample(0, sample) + buffer.getSample(1, sample));
            buffer.setSample(0, sample, monoSample);
            buffer.setSample(1, sample, monoSample);
        }
    }

    const float outputGain = pitchtime::dbToGain(getRawValue(parameters, "output", 0.0f));
    if (getRawValue(parameters, "bypass") > 0.5f)
    {
        clearSnapshot();
        buffer.applyGain(outputGain);
        visualState.captureOutput(buffer);
        return;
    }

    wetBuffer.setSize(numChannels, numSamples, false, false, true);
    wetBuffer.makeCopyOf(buffer, true);

    const double bpm = resolveHostBpm(*this);
    const auto params = buildParameterSnapshot();
    updateLatencyForParams(params, bpm);

    if (params.engine != lastEngineIndex)
    {
        resetAllEngines();
        lastEngineIndex = params.engine;
    }

    pitchtime::PitchTimeEngineSnapshot engineSnapshot;
    switch (params.engine)
    {
        case pitchShift:
            pitchShiftEngineState.process(wetBuffer, params, engineSnapshot);
            break;
        case timeStretch:
            timeStretchEngineState.process(wetBuffer, params, bpm, engineSnapshot);
            break;
        case autoTune:
            autoTuneEngineState.process(wetBuffer, params, engineSnapshot);
            break;
        case formantShift:
            formantShiftEngineState.process(wetBuffer, params, engineSnapshot);
            break;
        case vibrato:
            vibratoEngineState.process(wetBuffer, params, bpm, engineSnapshot);
            break;
        default:
            break;
    }

    mixSmoothed.setTargetValue(params.mix / 100.0f);
    outputSmoothed.setTargetValue(pitchtime::dbToGain(params.outputDb));
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mix = juce::jlimit(0.0f, 1.0f, mixSmoothed.getNextValue());
        const float gain = outputSmoothed.getNextValue();
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const float dry = buffer.getSample(channel, sample);
            const float wet = wetBuffer.getSample(channel, sample);
            buffer.setSample(channel, sample, (dry * (1.0f - mix) + wet * mix) * gain);
        }
    }

    storeSnapshot(engineSnapshot, (float) lastLatencySamples * 1000.0f / (float) juce::jmax(1.0, preparedSampleRate));
    visualState.captureOutput(buffer);
}

juce::AudioProcessorEditor* MusiquePitchShiftProcessor::createEditor()
{
#if MUSIQUE_PITCHTIME_DSP_TESTS
    return nullptr;
#else
    return new MusiquePitchShiftEditor(*this);
#endif
}

double MusiquePitchShiftProcessor::getTailLengthSeconds() const
{
    const auto params = buildParameterSnapshot();
    const double bpm = resolveHostBpm(const_cast<MusiquePitchShiftProcessor&>(*this));
    switch (params.engine)
    {
        case pitchShift: return pitchShiftEngineState.getTailSeconds();
        case timeStretch: return timeStretchEngineState.getTailSeconds(params, bpm);
        case autoTune: return autoTuneEngineState.getTailSeconds();
        case formantShift: return formantShiftEngineState.getTailSeconds();
        case vibrato: return vibratoEngineState.getTailSeconds(params, bpm);
        default: break;
    }
    return preparedSampleRate > 0.0 ? (double) lastLatencySamples / preparedSampleRate : 0.0;
}

void MusiquePitchShiftProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    normaliseStateTree(state);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void MusiquePitchShiftProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    normaliseStateTree(state);
    parameters.replaceState(state);
    postExternalStateChange();
}

PitchTimeSnapshot MusiquePitchShiftProcessor::getPitchTimeSnapshot() const noexcept
{
    PitchTimeSnapshot snapshot;
    for (int index = 0; index < (int) snapshot.bins.size(); ++index)
        snapshot.bins[(size_t) index] = visualBins[(size_t) index].load(std::memory_order_relaxed);

    snapshot.primary = visualPrimary.load(std::memory_order_relaxed);
    snapshot.secondary = visualSecondary.load(std::memory_order_relaxed);
    snapshot.tertiary = visualTertiary.load(std::memory_order_relaxed);
    snapshot.quaternary = visualQuaternary.load(std::memory_order_relaxed);
    snapshot.detectedMidi = visualDetectedMidi.load(std::memory_order_relaxed);
    snapshot.targetMidi = visualTargetMidi.load(std::memory_order_relaxed);
    snapshot.confidence = visualConfidence.load(std::memory_order_relaxed);
    snapshot.latencyMs = visualLatencyMs.load(std::memory_order_relaxed);
    snapshot.flagA = visualFlagA.load(std::memory_order_relaxed);
    snapshot.flagB = visualFlagB.load(std::memory_order_relaxed);
    return snapshot;
}

juce::String MusiquePitchShiftProcessor::getSyncLabelForNormalised(float normalised) const
{
    const auto& divisions = getSyncDivisions();
    const float clamped = juce::jlimit(0.0f, 0.999f, normalised);
    const int index = juce::jlimit(0, (int) divisions.size() - 1,
        (int) std::floor(clamped * (float) divisions.size()));
    return divisions[(size_t) index].label;
}

void MusiquePitchShiftProcessor::postExternalStateChange()
{
    resetAllEngines();
    lastEngineIndex = -1;
    updateLatencyForParams(buildParameterSnapshot(), resolveHostBpm(*this));
}

void MusiquePitchShiftProcessor::ensureStateParamValue(juce::ValueTree& state, const char* paramId, const juce::var& value)
{
    constexpr auto paramType = "PARAM";
    constexpr auto idProperty = "id";
    constexpr auto valueProperty = "value";

    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        auto child = state.getChild(childIndex);
        if (child.hasType(paramType) && child.getProperty(idProperty).toString() == paramId)
        {
            if (!child.hasProperty(valueProperty))
                child.setProperty(valueProperty, value, nullptr);
            return;
        }
    }

    juce::ValueTree child(paramType);
    child.setProperty(idProperty, paramId, nullptr);
    child.setProperty(valueProperty, value, nullptr);
    state.appendChild(child, nullptr);
}

juce::var MusiquePitchShiftProcessor::readStateParamValue(const juce::ValueTree& state, const char* paramId, const juce::var& fallback)
{
    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        auto child = state.getChild(childIndex);
        if (child.hasType("PARAM") && child.getProperty("id").toString() == paramId)
            return child.getProperty("value", fallback);
    }
    return fallback;
}

void MusiquePitchShiftProcessor::normaliseStateTree(juce::ValueTree& state)
{
    ensureStateParamValue(state, "engine", 0);
    ensureStateParamValue(state, "sync", false);
    ensureStateParamValue(state, "pitch", 0.0f);
    ensureStateParamValue(state, "fine", 0.0f);
    ensureStateParamValue(state, "formant", 0.5f);
    ensureStateParamValue(state, "voice", 1.0f);
    ensureStateParamValue(state, "spread", 55.0f);
    ensureStateParamValue(state, "detune", 7.0f);
    ensureStateParamValue(state, "octave", 0.0f);
    ensureStateParamValue(state, "mix", 100.0f);
    ensureStateParamValue(state, "output", 0.0f);
    ensureStateParamValue(state, "stereo_stack", true);
    ensureStateParamValue(state, "bypass", false);
    ensureStateParamValue(state, "mono", false);

    const float pitch = (float) readStateParamValue(state, "pitch", 0.0f);
    const float octave = (float) readStateParamValue(state, "octave", 0.0f);
    const float voice = (float) readStateParamValue(state, "voice", 1.0f);
    const float detune = (float) readStateParamValue(state, "detune", 0.0f);
    const bool stereoStack = (bool) readStateParamValue(state, "stereo_stack", true);
    if ((int) readStateParamValue(state, "engine", 0) == pitchShift)
    {
        int derivedVariant = 0;
        if (std::abs(octave) >= 1.0f || std::abs(pitch) >= 5.0f)
            derivedVariant = 2;
        else if (voice > 1.0f || stereoStack || detune > 1.0f)
            derivedVariant = 1;
        ensureStateParamValue(state, "variant", derivedVariant);
    }
    else
    {
        ensureStateParamValue(state, "variant", 0);
    }

    ensureStateParamValue(state, "stretch_ratio", 100.0f);
    ensureStateParamValue(state, "stretch_window", 220.0f);
    ensureStateParamValue(state, "stretch_grain", 50.0f);
    ensureStateParamValue(state, "stretch_transient", 55.0f);
    ensureStateParamValue(state, "stretch_tone", 50.0f);
    ensureStateParamValue(state, "stretch_spread", 40.0f);
    ensureStateParamValue(state, "stretch_smooth", 60.0f);

    ensureStateParamValue(state, "tune_amount", 80.0f);
    ensureStateParamValue(state, "tune_speed", 55.0f);
    ensureStateParamValue(state, "tune_humanize", 25.0f);
    ensureStateParamValue(state, "tune_key", 0);
    ensureStateParamValue(state, "tune_scale", 1);
    ensureStateParamValue(state, "tune_formant", 55.0f);
    ensureStateParamValue(state, "tune_range", 3);

    ensureStateParamValue(state, "fmt_shift", 0.0f);
    ensureStateParamValue(state, "fmt_fine", 0.0f);
    ensureStateParamValue(state, "fmt_focus", 55.0f);
    ensureStateParamValue(state, "fmt_brightness", 50.0f);
    ensureStateParamValue(state, "fmt_body", 50.0f);
    ensureStateParamValue(state, "fmt_unvoiced", 50.0f);
    ensureStateParamValue(state, "fmt_spread", 20.0f);

    ensureStateParamValue(state, "vib_rate", 3.2f);
    ensureStateParamValue(state, "vib_depth", 35.0f);
    ensureStateParamValue(state, "vib_spread", 40.0f);
    ensureStateParamValue(state, "vib_shape", 0);
    ensureStateParamValue(state, "vib_tone", 50.0f);
    ensureStateParamValue(state, "vib_rise", 20.0f);
    ensureStateParamValue(state, "vib_detune", 20.0f);
}

double MusiquePitchShiftProcessor::resolveHostBpm(juce::AudioProcessor& processor)
{
    if (auto* playHead = processor.getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                return *bpm;
    return 120.0;
}

void MusiquePitchShiftProcessor::resetAllEngines()
{
    pitchShiftEngineState.reset();
    timeStretchEngineState.reset();
    autoTuneEngineState.reset();
    formantShiftEngineState.reset();
    vibratoEngineState.reset();
    clearSnapshot();
}

void MusiquePitchShiftProcessor::clearSnapshot() noexcept
{
    pitchtime::PitchTimeEngineSnapshot snapshot;
    storeSnapshot(snapshot, 0.0f);
}

void MusiquePitchShiftProcessor::storeSnapshot(const pitchtime::PitchTimeEngineSnapshot& snapshot, float latencyMs) noexcept
{
    for (int index = 0; index < (int) snapshot.bins.size(); ++index)
        visualBins[(size_t) index].store(snapshot.bins[(size_t) index], std::memory_order_relaxed);

    visualPrimary.store(snapshot.primary, std::memory_order_relaxed);
    visualSecondary.store(snapshot.secondary, std::memory_order_relaxed);
    visualTertiary.store(snapshot.tertiary, std::memory_order_relaxed);
    visualQuaternary.store(snapshot.quaternary, std::memory_order_relaxed);
    visualDetectedMidi.store(snapshot.detectedMidi, std::memory_order_relaxed);
    visualTargetMidi.store(snapshot.targetMidi, std::memory_order_relaxed);
    visualConfidence.store(snapshot.confidence, std::memory_order_relaxed);
    visualLatencyMs.store(latencyMs, std::memory_order_relaxed);
    visualFlagA.store(snapshot.flagA, std::memory_order_relaxed);
    visualFlagB.store(snapshot.flagB, std::memory_order_relaxed);
}

pitchtime::PitchTimeParams MusiquePitchShiftProcessor::buildParameterSnapshot() const
{
    pitchtime::PitchTimeParams snapshot;
    snapshot.engine = juce::jlimit(0, numEngines - 1, getChoiceValue(parameters, "engine", 0));
    snapshot.variant = juce::jlimit(0, 2, getChoiceValue(parameters, "variant", 0));
    snapshot.sync = getRawValue(parameters, "sync") > 0.5f;
    snapshot.bypass = getRawValue(parameters, "bypass") > 0.5f;
    snapshot.mono = getRawValue(parameters, "mono") > 0.5f;
    snapshot.stereoStack = getRawValue(parameters, "stereo_stack") > 0.5f;
    snapshot.mix = getRawValue(parameters, "mix", 100.0f);
    snapshot.outputDb = getRawValue(parameters, "output", 0.0f);

    snapshot.pitch = getRawValue(parameters, "pitch", 0.0f);
    snapshot.fine = getRawValue(parameters, "fine", 0.0f);
    snapshot.formant = getRawValue(parameters, "formant", 0.5f);
    snapshot.voice = getRawValue(parameters, "voice", 1.0f);
    snapshot.voiceCount = pitchtime::quantiseVoiceCount(snapshot.voice);
    snapshot.spread = getRawValue(parameters, "spread", 55.0f);
    snapshot.detune = getRawValue(parameters, "detune", 7.0f);
    snapshot.octave = getRawValue(parameters, "octave", 0.0f);
    snapshot.snappedOctave = pitchtime::snapOctave(snapshot.octave);

    snapshot.stretchRatio = getRawValue(parameters, "stretch_ratio", 100.0f);
    snapshot.stretchWindow = getRawValue(parameters, "stretch_window", 220.0f);
    snapshot.stretchGrain = getRawValue(parameters, "stretch_grain", 50.0f);
    snapshot.stretchTransient = getRawValue(parameters, "stretch_transient", 55.0f);
    snapshot.stretchTone = getRawValue(parameters, "stretch_tone", 50.0f);
    snapshot.stretchSpread = getRawValue(parameters, "stretch_spread", 40.0f);
    snapshot.stretchSmooth = getRawValue(parameters, "stretch_smooth", 60.0f);

    snapshot.tuneAmount = getRawValue(parameters, "tune_amount", 80.0f);
    snapshot.tuneSpeed = getRawValue(parameters, "tune_speed", 55.0f);
    snapshot.tuneHumanize = getRawValue(parameters, "tune_humanize", 25.0f);
    snapshot.tuneKey = juce::jlimit(0, 11, getChoiceValue(parameters, "tune_key", 0));
    snapshot.tuneScale = juce::jlimit(0, 3, getChoiceValue(parameters, "tune_scale", 1));
    snapshot.tuneFormant = getRawValue(parameters, "tune_formant", 55.0f);
    snapshot.tuneRange = juce::jlimit(0, 3, getChoiceValue(parameters, "tune_range", 3));

    snapshot.fmtShift = getRawValue(parameters, "fmt_shift", 0.0f);
    snapshot.fmtFine = getRawValue(parameters, "fmt_fine", 0.0f);
    snapshot.fmtFocus = getRawValue(parameters, "fmt_focus", 55.0f);
    snapshot.fmtBrightness = getRawValue(parameters, "fmt_brightness", 50.0f);
    snapshot.fmtBody = getRawValue(parameters, "fmt_body", 50.0f);
    snapshot.fmtUnvoiced = getRawValue(parameters, "fmt_unvoiced", 50.0f);
    snapshot.fmtSpread = getRawValue(parameters, "fmt_spread", 20.0f);

    snapshot.vibRate = getRawValue(parameters, "vib_rate", 3.2f);
    snapshot.vibDepth = getRawValue(parameters, "vib_depth", 35.0f);
    snapshot.vibSpread = getRawValue(parameters, "vib_spread", 40.0f);
    snapshot.vibShape = juce::jlimit(0, 2, getChoiceValue(parameters, "vib_shape", 0));
    snapshot.vibTone = getRawValue(parameters, "vib_tone", 50.0f);
    snapshot.vibRise = getRawValue(parameters, "vib_rise", 20.0f);
    snapshot.vibDetune = getRawValue(parameters, "vib_detune", 20.0f);
    return snapshot;
}

void MusiquePitchShiftProcessor::updateLatencyForParams(const pitchtime::PitchTimeParams& params, double bpm)
{
    int desiredLatency = 0;
    switch (params.engine)
    {
        case pitchShift:
            desiredLatency = pitchShiftEngineState.getLatencySamples();
            break;
        case timeStretch:
            desiredLatency = timeStretchEngineState.computeLatencySamples(params, bpm);
            break;
        case autoTune:
            desiredLatency = autoTuneEngineState.getLatencySamples();
            break;
        case formantShift:
            desiredLatency = formantShiftEngineState.getLatencySamples();
            break;
        case vibrato:
            desiredLatency = vibratoEngineState.computeLatencySamples(params, bpm);
            break;
        default:
            break;
    }

    if (desiredLatency != lastLatencySamples)
    {
        setLatencySamples(desiredLatency);
        lastLatencySamples = desiredLatency;
    }
}

#if ! MUSIQUE_PITCHTIME_DSP_TESTS
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusiquePitchShiftProcessor();
}
#endif

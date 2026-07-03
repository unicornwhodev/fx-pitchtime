#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

namespace
{
const std::array<MusiquePitchShiftEditor::EngineUiConfig, MusiquePitchShiftProcessor::numEngines> kEngineConfigs {{
    {
        "PITCH SHIFT",
        { "pitch", "fine", "formant", "voice", "spread", "detune", "octave" },
        { "PITCH", "FINE", "COLOR", "VOICE", "SPREAD", "DETUNE", "OCTAVE" },
        juce::StringArray { "Solo", "Stack", "Harmony" },
        nullptr,
        0.0f,
        1.0f
    },
    {
        "TIME STRETCH",
        { "stretch_ratio", "stretch_window", "stretch_grain", "stretch_transient", "stretch_tone", "stretch_spread", "stretch_smooth" },
        { "RATIO", "WINDOW", "GRAIN", "TRANSIENT", "TONE", "SPREAD", "SMOOTH" },
        juce::StringArray { "Transparent", "Rhythmic", "Grain" },
        "stretch_window",
        15.0f,
        1000.0f
    },
    {
        "AUTOTUNE",
        { "tune_amount", "tune_speed", "tune_humanize", "tune_key", "tune_scale", "tune_formant", "tune_range" },
        { "AMOUNT", "SPEED", "HUMANIZE", "KEY", "SCALE", "FORMANT", "RANGE" },
        juce::StringArray { "Natural", "Pop", "Hard" },
        nullptr,
        0.0f,
        1.0f
    },
    {
        "FORMANT SHIFT",
        { "fmt_shift", "fmt_fine", "fmt_focus", "fmt_brightness", "fmt_body", "fmt_unvoiced", "fmt_spread" },
        { "SHIFT", "FINE", "FOCUS", "BRIGHT", "BODY", "UNVOICED", "SPREAD" },
        juce::StringArray { "Natural", "Gender", "Robot" },
        nullptr,
        0.0f,
        1.0f
    },
    {
        "VIBRATO",
        { "vib_rate", "vib_depth", "vib_spread", "vib_shape", "vib_tone", "vib_rise", "vib_detune" },
        { "RATE", "DEPTH", "SPREAD", "SHAPE", "TONE", "RISE", "DETUNE" },
        juce::StringArray { "Classic", "Tape", "Choir" },
        "vib_rate",
        0.1f,
        12.0f
    }
}};

float paramValue(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float fallback = 0.0f)
{
    if (auto* raw = apvts.getRawParameterValue(id))
        return raw->load();
    return fallback;
}

int choiceValue(const juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int fallback = 0)
{
    return (int) std::round(paramValue(apvts, id, (float) fallback));
}

void setParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void setupKnob(juce::Slider& slider, juce::Label& label, const char* text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 16);
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions {}.withHeight(fx::font::label).withStyle("Bold")));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, fx::col::textMuted);
}

juce::String noteNameFromIndex(int index)
{
    static constexpr const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[juce::jlimit(0, 11, index)];
}

juce::String scaleNameFromIndex(int index)
{
    static constexpr const char* names[] { "Chrom", "Major", "Minor", "Penta" };
    return names[juce::jlimit(0, 3, index)];
}

juce::String rangeNameFromIndex(int index)
{
    static constexpr const char* names[] { "Low", "Mid", "High", "Full" };
    return names[juce::jlimit(0, 3, index)];
}

juce::String shapeNameFromIndex(int index)
{
    static constexpr const char* names[] { "Sine", "Tri", "Tape" };
    return names[juce::jlimit(0, 2, index)];
}
}

MusiquePitchShiftEditor::MusiquePitchShiftEditor(MusiquePitchShiftProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);
    setSize(fx::dim::appW, fx::dim::appH);

    titleLabel.setText("PITCH/TIME", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions {}.withHeight(fx::font::header).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, fx::col::textPrimary);
    addAndMakeVisible(titleLabel);

    pluginIcon = juce::ImageCache::getFromMemory(BinaryData::icon_small_png, BinaryData::icon_small_pngSize);
    logoImg = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);

    auto setupButton = [&](juce::TextButton& button, bool toggle = false)
    {
        button.setColour(juce::TextButton::buttonColourId, fx::col::surfSecondary);
        button.setColour(juce::TextButton::textColourOffId, fx::col::textPrimary);
        if (toggle)
            button.setClickingTogglesState(true);
        addAndMakeVisible(button);
    };

    setupButton(bypassBtn, true);
    setupButton(monoBtn, true);
    setupButton(syncBtn, true);
    setupButton(modeBtn);
    setupButton(statusBtn);
    setupButton(prevBtn);
    setupButton(nextBtn);
    setupButton(saveBtn);
    setupButton(abBtn);

    modeBtn.setInterceptsMouseClicks(false, false);
    statusBtn.setInterceptsMouseClicks(false, false);
    modeBtn.setTooltip("Live engine mode and contextual status");
    statusBtn.setTooltip("Live engine measurements and migration-safe status");

    addAndMakeVisible(presetBox);
    addAndMakeVisible(engineBox);
    addAndMakeVisible(variantBox);
    engineBox.addItemList(juce::StringArray { "Pitch Shift", "Time Stretch", "AutoTune", "Formant Shift", "Vibrato" }, 1);

    for (int index = 0; index < 8; ++index)
    {
        setupKnob(knobs[index], knobLabels[index], "");
        addAndMakeVisible(knobs[index]);
        addAndMakeVisible(knobLabels[index]);
    }
    knobLabels[7].setText("MIX", juce::dontSendNotification);

    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);
    outputSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(outputSlider);

    activeLED.setAccent(fx::accent::pitch);
    addAndMakeVisible(activeLED);

    versionLabel.setText("Musique PitchTime v1.1", juce::dontSendNotification);
    versionLabel.setFont(juce::Font(juce::FontOptions {}.withHeight(fx::font::footer)));
    versionLabel.setColour(juce::Label::textColourId, fx::col::textMuted);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel);

    mixAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "mix", knobs[7]);
    outAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "output", outputSlider);
    engineAtt = std::make_unique<ComboAttach>(proc.getAPVTS(), "engine", engineBox);
    bypassAtt = std::make_unique<ButtonAttach>(proc.getAPVTS(), "bypass", bypassBtn);
    monoAtt = std::make_unique<ButtonAttach>(proc.getAPVTS(), "mono", monoBtn);
    syncAtt = std::make_unique<ButtonAttach>(proc.getAPVTS(), "sync", syncBtn);

    loadPresets();
    if (presets != nullptr && !presets->isEmpty())
    {
        presetBox.setSelectedItemIndex(0, juce::dontSendNotification);
        auto preset = presets->getReference(0);
        MusiquePitchShiftProcessor::normalisePresetObject(preset);
        fx::preset::applyToAPVTS(proc.getAPVTS(), preset);
        proc.postExternalStateChange();
    }

    abStateA = proc.getAPVTS().copyState();
    abStateB = abStateA.createCopy();
    showingA = true;
    abBtn.setButtonText("A");

    presetBox.onChange = [this]
    {
        const int presetIndex = presetBox.getSelectedItemIndex();
        if (presets == nullptr || presetIndex < 0 || presetIndex >= presets->size())
            return;

        auto preset = presets->getReference(presetIndex);
        MusiquePitchShiftProcessor::normalisePresetObject(preset);
        fx::preset::applyToAPVTS(proc.getAPVTS(), preset);
        proc.postExternalStateChange();
        abStateA = proc.getAPVTS().copyState();
        abStateB = abStateA.createCopy();
        showingA = true;
        abBtn.setButtonText("A");
        rebuildEngineUi(true);
    };

    prevBtn.onClick = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index > 0)
            presetBox.setSelectedItemIndex(index - 1);
    };

    nextBtn.onClick = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index < presetBox.getNumItems() - 1)
            presetBox.setSelectedItemIndex(index + 1);
    };

    saveBtn.onClick = [this]
    {
        const auto name = juce::String("User_") + juce::Time::getCurrentTime().formatted("%H%M%S");
        if (fx::preset::saveUserPreset("fx-pitchtime", name, MusiquePitchShiftProcessor::getAllParameterIds(), proc.getAPVTS()))
        {
            loadPresets();
            if (presetBox.getNumItems() > 0)
                presetBox.setSelectedItemIndex(presetBox.getNumItems() - 1);
        }
    };

    abBtn.onClick = [this]
    {
        storeCurrentABSlot();
        recallABSlot(!showingA);
    };

    engineBox.onChange = [this] { rebuildEngineUi(true); };
    variantBox.onChange = [this]
    {
        const int selection = variantBox.getSelectedItemIndex();
        if (selection >= 0)
            applyVariantSelection(getCurrentEngineIndex(), selection);
    };

    knobs[7].textFromValueFunction = [](double value)
    {
        return juce::String((int) std::round(value)) + "%";
    };

    rebuildEngineUi(true);
    startTimerHz(fx::anim::fftRefreshHz);
}

MusiquePitchShiftEditor::~MusiquePitchShiftEditor()
{
    setLookAndFeel(nullptr);
}

void MusiquePitchShiftEditor::loadPresets()
{
    presets = std::make_shared<juce::Array<juce::var>>(fx::preset::loadAllPresets("fx-pitchtime"));
    for (auto& preset : *presets)
        MusiquePitchShiftProcessor::normalisePresetObject(preset);
    refreshPresetBox();
}

void MusiquePitchShiftEditor::refreshPresetBox()
{
    presetBox.clear(juce::dontSendNotification);
    if (presets == nullptr || presets->isEmpty())
    {
        presetBox.addItem("Init", 1);
        presetBox.setSelectedId(1, juce::dontSendNotification);
        return;
    }

    int itemId = 1;
    for (auto& preset : *presets)
        if (auto* object = preset.getDynamicObject())
            presetBox.addItem(object->getProperty("name").toString(), itemId++);
}

int MusiquePitchShiftEditor::getCurrentEngineIndex() const
{
    return juce::jlimit(0, MusiquePitchShiftProcessor::numEngines - 1, choiceValue(proc.getAPVTS(), "engine"));
}

int MusiquePitchShiftEditor::getCurrentVariantIndex() const
{
    return juce::jlimit(0, 2, choiceValue(proc.getAPVTS(), "variant"));
}

void MusiquePitchShiftEditor::rebuildEngineUi(bool force)
{
    const int engineIndex = getCurrentEngineIndex();
    if (!force && engineIndex == displayedEngine)
        return;

    displayedEngine = engineIndex;
    const auto& config = kEngineConfigs[(size_t) engineIndex];
    for (int index = 0; index < 7; ++index)
        knobLabels[index].setText(config.labels[(size_t) index], juce::dontSendNotification);
    knobLabels[7].setText("MIX", juce::dontSendNotification);

    bindEngineKnobs(engineIndex);
    rebuildVariantItems(engineIndex);
    updateKnobTextFunctions(engineIndex);
    updateHeaderButtons();
    repaint(0, fx::dim::headerH + fx::dim::presetBarH, getWidth(), fx::dim::visualH);
}

void MusiquePitchShiftEditor::rebuildVariantItems(int engineIndex)
{
    variantBox.clear(juce::dontSendNotification);
    variantBox.addItemList(kEngineConfigs[(size_t) engineIndex].variants, 1);
    variantBox.setSelectedItemIndex(getCurrentVariantIndex(), juce::dontSendNotification);
}

void MusiquePitchShiftEditor::bindEngineKnobs(int engineIndex)
{
    const auto& config = kEngineConfigs[(size_t) engineIndex];
    for (int index = 0; index < 7; ++index)
        engineKnobAtts[(size_t) index] = std::make_unique<SliderAttach>(proc.getAPVTS(), config.paramIds[(size_t) index], knobs[index]);
}

void MusiquePitchShiftEditor::applyVariantSelection(int, int variantIndex)
{
    setParameter(proc.getAPVTS(), "variant", (float) juce::jlimit(0, 2, variantIndex));
}

void MusiquePitchShiftEditor::updateKnobTextFunctions(int engineIndex)
{
    auto makePercent = [](double value) { return juce::String((int) std::round(value)) + "%"; };
    const auto& config = kEngineConfigs[(size_t) engineIndex];

    for (int index = 0; index < 7; ++index)
    {
        const juce::String paramId = config.paramIds[(size_t) index];
        if (paramId == "pitch" || paramId == "fmt_shift")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return juce::String(value >= 0.0 ? "+" : "") + juce::String(value, 1);
            };
        }
        else if (paramId == "fine" || paramId == "fmt_fine")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return juce::String((int) std::round(value)) + " ct";
            };
        }
        else if (paramId == "formant")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                if (value < 0.4) return juce::String("Dark");
                if (value > 0.6) return juce::String("Bright");
                return juce::String("Neutral");
            };
        }
        else if (paramId == "voice")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return juce::String((int) std::round(value));
            };
        }
        else if (paramId == "octave")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                const int octave = juce::jlimit(-2, 2, (int) std::round(value));
                return juce::String(octave >= 0 ? "+" : "") + juce::String(octave);
            };
        }
        else if (paramId == "spread" || paramId == "stretch_spread" || paramId == "stretch_grain"
                 || paramId == "stretch_transient" || paramId == "stretch_tone" || paramId == "stretch_smooth"
                 || paramId == "tune_amount" || paramId == "tune_speed" || paramId == "tune_humanize"
                 || paramId == "tune_formant" || paramId == "fmt_focus" || paramId == "fmt_brightness"
                 || paramId == "fmt_body" || paramId == "fmt_unvoiced" || paramId == "fmt_spread"
                 || paramId == "vib_depth" || paramId == "vib_spread" || paramId == "vib_tone"
                 || paramId == "vib_rise" || paramId == "vib_detune")
        {
            knobs[index].textFromValueFunction = makePercent;
        }
        else if (paramId == "detune")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return juce::String(value, 1) + " ct";
            };
        }
        else if (paramId == "stretch_ratio")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return juce::String((int) std::round(value)) + "%";
            };
        }
        else if (paramId == "stretch_window")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return juce::String((int) std::round(value)) + " ms";
            };
        }
        else if (paramId == "tune_key")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return noteNameFromIndex((int) std::round(value));
            };
        }
        else if (paramId == "tune_scale")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return scaleNameFromIndex((int) std::round(value));
            };
        }
        else if (paramId == "tune_range")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return rangeNameFromIndex((int) std::round(value));
            };
        }
        else if (paramId == "vib_rate")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return juce::String(value, 2) + " Hz";
            };
        }
        else if (paramId == "vib_shape")
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return shapeNameFromIndex((int) std::round(value));
            };
        }
        else
        {
            knobs[index].textFromValueFunction = [](double value)
            {
                return juce::String(value, 1);
            };
        }
    }
}

void MusiquePitchShiftEditor::updateHeaderButtons()
{
    const auto snapshot = proc.getPitchTimeSnapshot();
    const auto& apvts = proc.getAPVTS();
    const int engineIndex = getCurrentEngineIndex();
    const auto& config = kEngineConfigs[(size_t) engineIndex];
    const bool mono = paramValue(apvts, "mono") > 0.5f;
    const bool sync = paramValue(apvts, "sync") > 0.5f;

    monoBtn.setButtonText(mono ? "MONO IN" : "STEREO IN");
    monoBtn.setColour(juce::TextButton::buttonColourId, mono ? fx::accent::pitch.withAlpha(0.2f) : fx::col::surfSecondary);
    monoBtn.setColour(juce::TextButton::textColourOffId, mono ? fx::accent::pitch.brighter(0.18f) : fx::col::textPrimary);

    if (config.syncParamId != nullptr)
    {
        syncBtn.setEnabled(true);
        const float raw = paramValue(apvts, config.syncParamId, config.syncMin);
        const float normalised = config.syncMax > config.syncMin
            ? juce::jlimit(0.0f, 1.0f, (raw - config.syncMin) / (config.syncMax - config.syncMin))
            : 0.0f;
        syncBtn.setButtonText(sync ? proc.getSyncLabelForNormalised(normalised) : "FREE");
    }
    else
    {
        syncBtn.setButtonText("N/A");
        syncBtn.setEnabled(false);
    }
    syncBtn.setColour(juce::TextButton::buttonColourId,
        sync && config.syncParamId != nullptr ? fx::accent::pitch.withAlpha(0.22f) : fx::col::surfSecondary);
    syncBtn.setColour(juce::TextButton::textColourOffId,
        sync && config.syncParamId != nullptr ? fx::accent::pitch.brighter(0.16f) : fx::col::textPrimary);

    juce::String modeText = config.variants[getCurrentVariantIndex()].toUpperCase();
    juce::String statusText;
    switch (engineIndex)
    {
        case MusiquePitchShiftProcessor::pitchShift:
            statusText = juce::String(snapshot.primary >= 0.0f ? "+" : "") + juce::String(snapshot.primary, 1)
                       + " ST  /  " + juce::String((int) std::round(snapshot.secondary)) + "V";
            break;
        case MusiquePitchShiftProcessor::timeStretch:
            statusText = juce::String((int) std::round(snapshot.primary * 100.0f)) + "%  /  "
                       + juce::String(snapshot.secondary, 0) + " ms";
            break;
        case MusiquePitchShiftProcessor::autoTune:
            modeText = snapshot.flagA ? pitchtime::noteNameFromMidi(snapshot.detectedMidi).toUpperCase() : "LISTEN";
            statusText = snapshot.flagA
                ? pitchtime::noteNameFromMidi(snapshot.targetMidi).toUpperCase() + "  /  " + juce::String((int) std::round(snapshot.confidence * 100.0f)) + "%"
                : "LOW CONF";
            break;
        case MusiquePitchShiftProcessor::formantShift:
            statusText = juce::String(snapshot.primary >= 0.0f ? "+" : "") + juce::String(snapshot.primary, 1) + " ST";
            break;
        case MusiquePitchShiftProcessor::vibrato:
            modeText = shapeNameFromIndex(choiceValue(apvts, "vib_shape"));
            statusText = juce::String(snapshot.primary, 2) + " Hz  /  " + juce::String((int) std::round(snapshot.secondary)) + "%";
            break;
        default:
            break;
    }

    modeBtn.setButtonText(modeText);
    statusBtn.setButtonText(statusText);
    modeBtn.setColour(juce::TextButton::buttonColourId, fx::accent::pitch.withAlpha(0.16f));
    modeBtn.setColour(juce::TextButton::textColourOffId, fx::col::textPrimary);
    statusBtn.setColour(juce::TextButton::buttonColourId,
        (snapshot.flagA || snapshot.flagB) ? fx::accent::pitch.withAlpha(0.22f) : fx::col::surfSecondary);
    statusBtn.setColour(juce::TextButton::textColourOffId,
        (snapshot.flagA || snapshot.flagB) ? fx::accent::pitch.brighter(0.15f) : fx::col::textPrimary);
}

void MusiquePitchShiftEditor::storeCurrentABSlot()
{
    const auto currentState = proc.getAPVTS().copyState();
    if (!abStateA.isValid())
    {
        abStateA = currentState;
        abStateB = currentState.createCopy();
        showingA = true;
        return;
    }

    if (showingA)
        abStateA = currentState;
    else
        abStateB = currentState;
}

void MusiquePitchShiftEditor::recallABSlot(bool showA)
{
    if (!abStateA.isValid())
        return;

    proc.getAPVTS().replaceState(showA ? abStateA : abStateB);
    proc.postExternalStateChange();
    showingA = showA;
    abBtn.setButtonText(showingA ? "A" : "B");
    rebuildEngineUi(true);
}

void MusiquePitchShiftEditor::timerCallback()
{
    rebuildEngineUi();

    const auto inputLevels = proc.getVisualState().getInputLevels();
    const auto outputLevels = proc.getVisualState().getOutputLevels();
    const auto snapshot = proc.getPitchTimeSnapshot();
    inMeter.setLevel(inputLevels.left, inputLevels.right);
    outMeter.setLevel(outputLevels.left, outputLevels.right);
    activeLED.setOn(juce::jmax(outputLevels.left, outputLevels.right) > 0.02f || snapshot.flagA || snapshot.confidence > 0.25f);

    animPhase += 0.012f + snapshot.primary * 0.00018f;
    if (animPhase > juce::MathConstants<float>::twoPi * 100.0f)
        animPhase -= juce::MathConstants<float>::twoPi * 100.0f;

    updateHeaderButtons();
    repaint(0, fx::dim::headerH + fx::dim::presetBarH, getWidth(), fx::dim::visualH);
}

void MusiquePitchShiftEditor::paintVisualization(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto snapshot = proc.getPitchTimeSnapshot();
    const auto& apvts = proc.getAPVTS();
    const int engineIndex = getCurrentEngineIndex();
    const auto& config = kEngineConfigs[(size_t) engineIndex];
    const bool sync = paramValue(apvts, "sync") > 0.5f;
    const bool mono = paramValue(apvts, "mono") > 0.5f;

    auto drawBadge = [&](juce::Rectangle<float> rect, const juce::String& text, juce::Colour colour)
    {
        g.setColour(colour.withAlpha(0.16f));
        g.fillRoundedRectangle(rect, 8.0f);
        g.setColour(colour.withAlpha(0.55f));
        g.drawRoundedRectangle(rect, 8.0f, 1.0f);
        g.setColour(fx::col::textPrimary);
        g.setFont(juce::Font(juce::FontOptions {}.withHeight(10.0f).withStyle("Bold")));
        g.drawText(text, rect.toNearestInt(), juce::Justification::centred);
    };

    const float left = (float) area.getX();
    const float top = (float) area.getY();
    const float width = (float) area.getWidth();
    const float height = (float) area.getHeight();

    drawBadge({ left + 16.0f, top + 14.0f, 120.0f, 22.0f }, config.title, fx::accent::pitch);
    drawBadge({ left + width - 326.0f, top + 14.0f, 106.0f, 22.0f }, mono ? "INPUT MONO" : "INPUT STEREO", fx::col::textSecondary);
    drawBadge({ left + width - 212.0f, top + 14.0f, 116.0f, 22.0f }, config.variants[getCurrentVariantIndex()].toUpperCase(), fx::accent::pitch);
    drawBadge({ left + width - 88.0f, top + 14.0f, 66.0f, 22.0f }, sync ? "SYNC" : "FREE", fx::col::textSecondary);

    if (engineIndex == MusiquePitchShiftProcessor::pitchShift)
    {
        const float totalShift = snapshot.primary;
        const float shiftNorm = juce::jlimit(0.0f, 1.0f, (totalShift + 12.0f) / 24.0f);
        const float gridLeft = left + 30.0f;
        const float gridRight = left + width - 30.0f;
        const float gridTop = top + 78.0f;
        const float gridBottom = top + height - 30.0f;

        g.setFont(juce::Font(juce::FontOptions {}.withHeight(9.0f)));
        for (int semitone = -12; semitone <= 12; semitone += 3)
        {
            const float lineNorm = ((float) semitone + 12.0f) / 24.0f;
            const float y = gridBottom - lineNorm * (gridBottom - gridTop);
            g.setColour(semitone == 0 ? fx::col::gridMajor : fx::col::gridMinor);
            g.drawHorizontalLine((int) y, gridLeft, gridRight);
            g.setColour(fx::col::textMuted);
            g.drawText(juce::String(semitone > 0 ? "+" : "") + juce::String(semitone),
                       (int) left + 4, (int) y - 6, 24, 12, juce::Justification::centredRight);
        }

        const float centerY = gridBottom - shiftNorm * (gridBottom - gridTop);
        juce::Path line;
        for (int pixel = 0; pixel <= (int) (gridRight - gridLeft); ++pixel)
        {
            const float x = gridLeft + (float) pixel;
            const float wobble = std::sin(animPhase * 2.0f + (float) pixel * 0.035f) * (6.0f + snapshot.secondary * 1.4f);
            const float y = centerY + wobble;
            if (pixel == 0)
                line.startNewSubPath(x, y);
            else
                line.lineTo(x, y);
        }
        g.setColour(fx::accent::pitch.withAlpha(0.82f));
        g.strokePath(line, juce::PathStrokeType(2.2f));

        const int voices = juce::jlimit(1, 4, (int) std::round(snapshot.secondary));
        for (int voiceIndex = 0; voiceIndex < voices; ++voiceIndex)
        {
            const float norm = voices == 1 ? 0.0f : juce::jmap((float) voiceIndex, 0.0f, (float) (voices - 1), -1.0f, 1.0f);
            const float x = left + width * 0.68f + norm * (20.0f + snapshot.tertiary * 0.55f);
            const float y = centerY + norm * 22.0f;
            g.setColour(fx::accent::pitch.withAlpha(voiceIndex == 0 ? 0.82f : 0.36f));
            g.fillEllipse(x - 8.0f, y - 8.0f, 16.0f, 16.0f);
        }

        g.setColour(fx::accent::pitch);
        g.setFont(juce::Font(juce::FontOptions {}.withHeight(34.0f).withStyle("Bold")));
        g.drawText(juce::String(totalShift >= 0.0f ? "+" : "") + juce::String(totalShift, 1) + " st",
                   (int) (left + width * 0.34f), (int) (top + 34.0f), 240, 42, juce::Justification::centredLeft);
        g.setColour(fx::col::textMuted);
        g.setFont(juce::Font(juce::FontOptions {}.withHeight(12.0f)));
        g.drawText(juce::String(voices) + (voices == 1 ? " voice" : " voices") + "  /  detune "
                   + juce::String(snapshot.quaternary, 1) + " ct",
                   (int) (left + width * 0.34f), (int) (top + 76.0f), 220, 20, juce::Justification::centredLeft);
        return;
    }

    if (engineIndex == MusiquePitchShiftProcessor::timeStretch)
    {
        const float ratio = snapshot.primary;
        const float ratioNorm = juce::jlimit(0.0f, 1.0f, (ratio - 0.5f) / 1.5f);
        const float laneTop = top + 88.0f;
        const float laneBottom = top + height - 42.0f;
        const float laneLeft = left + 32.0f;
        const float laneWidth = width - 64.0f;
        const int grainCount = 7;
        const float grainWidth = laneWidth / (float) grainCount;

        for (int grain = 0; grain < grainCount; ++grain)
        {
            const float shift = std::sin(animPhase + grain * 0.55f) * snapshot.bins[2] * 16.0f;
            const float heightScale = 0.35f + snapshot.bins[6] * 0.45f;
            juce::Rectangle<float> rect(laneLeft + grainWidth * grain + 5.0f + shift,
                                        laneTop + (1.0f - heightScale) * 74.0f,
                                        grainWidth - 14.0f,
                                        (laneBottom - laneTop) * heightScale + 56.0f);
            g.setColour((grain % 2 == 0 ? fx::accent::pitch : fx::col::textSecondary).withAlpha(0.22f + 0.08f * snapshot.bins[3]));
            g.fillRoundedRectangle(rect, 8.0f);
            g.setColour(fx::accent::pitch.withAlpha(0.48f));
            g.drawRoundedRectangle(rect, 8.0f, 1.0f);
        }

        const float playheadX = laneLeft + ratioNorm * laneWidth;
        g.setColour(fx::accent::pitch);
        g.drawLine(playheadX, laneTop - 12.0f, playheadX, laneBottom + 10.0f, 2.2f);
        g.setColour(fx::col::textPrimary);
        g.setFont(juce::Font(juce::FontOptions {}.withHeight(16.0f).withStyle("Bold")));
        g.drawText(juce::String((int) std::round(ratio * 100.0f)) + "% ratio  /  " + juce::String(snapshot.secondary, 0) + " ms window",
                   (int) left + 40, (int) top + 112, (int) width - 80, 24, juce::Justification::centred);
        return;
    }

    if (engineIndex == MusiquePitchShiftProcessor::autoTune)
    {
        const float rulerLeft = left + 40.0f;
        const float rulerRight = left + width - 40.0f;
        const float rulerY = top + height * 0.58f;
        const float minMidi = 48.0f;
        const float maxMidi = 84.0f;
        g.setColour(fx::col::gridMinor);
        g.drawLine(rulerLeft, rulerY, rulerRight, rulerY, 1.2f);

        for (int note = (int) minMidi; note <= (int) maxMidi; note += 3)
        {
            const float norm = (note - minMidi) / (maxMidi - minMidi);
            const float x = rulerLeft + norm * (rulerRight - rulerLeft);
            g.drawVerticalLine((int) x, rulerY - 26.0f, rulerY + 26.0f);
            g.setColour(fx::col::textMuted);
            g.drawText(noteNameFromIndex(note % 12), (int) x - 12, (int) rulerY + 30, 24, 12, juce::Justification::centred);
            g.setColour(fx::col::gridMinor);
        }

        if (snapshot.flagA)
        {
            const float detNorm = juce::jlimit(0.0f, 1.0f, (snapshot.detectedMidi - minMidi) / (maxMidi - minMidi));
            const float targetNorm = juce::jlimit(0.0f, 1.0f, (snapshot.targetMidi - minMidi) / (maxMidi - minMidi));
            const float detX = rulerLeft + detNorm * (rulerRight - rulerLeft);
            const float targetX = rulerLeft + targetNorm * (rulerRight - rulerLeft);
            g.setColour(fx::col::textSecondary);
            g.drawLine(detX, rulerY - 54.0f, detX, rulerY + 8.0f, 1.4f);
            g.setColour(fx::accent::pitch);
            g.drawLine(targetX, rulerY - 72.0f, targetX, rulerY + 8.0f, 2.6f);
            g.drawArrow(juce::Line<float>(detX, rulerY - 20.0f, targetX, rulerY - 20.0f), 3.0f, 8.0f, 8.0f);
            g.fillEllipse(targetX - 8.0f, rulerY - 82.0f, 16.0f, 16.0f);
        }

        g.setColour(snapshot.flagA ? fx::col::textPrimary : fx::col::textMuted);
        g.setFont(juce::Font(juce::FontOptions {}.withHeight(18.0f).withStyle("Bold")));
        g.drawText(snapshot.flagA ? pitchtime::noteNameFromMidi(snapshot.detectedMidi) + " -> " + pitchtime::noteNameFromMidi(snapshot.targetMidi)
                                  : "Waiting for stable monophonic input",
                   (int) left + 40, (int) top + 110, (int) width - 80, 28, juce::Justification::centred);
        g.setColour(fx::accent::pitch.withAlpha(0.24f));
        g.fillRoundedRectangle({ left + 180.0f, top + 160.0f, width - 360.0f, 16.0f }, 8.0f);
        g.setColour(fx::accent::pitch.withAlpha(0.7f));
        g.fillRoundedRectangle({ left + 180.0f, top + 160.0f, (width - 360.0f) * snapshot.confidence, 16.0f }, 8.0f);
        return;
    }

    if (engineIndex == MusiquePitchShiftProcessor::formantShift)
    {
        const float graphLeft = left + 38.0f;
        const float graphTop = top + 86.0f;
        const float graphWidth = width - 76.0f;
        const float graphHeight = height - 138.0f;

        juce::Path sourcePath;
        juce::Path shiftedPath;
        for (int index = 0; index < 4; ++index)
        {
            const float x = graphLeft + graphWidth * ((float) index / 3.0f);
            const float srcY = graphTop + graphHeight * (1.0f - snapshot.bins[(size_t) index]);
            const float dstY = graphTop + graphHeight * (1.0f - snapshot.bins[(size_t) (index + 4)]);
            if (index == 0)
            {
                sourcePath.startNewSubPath(x, srcY);
                shiftedPath.startNewSubPath(x, dstY);
            }
            else
            {
                sourcePath.lineTo(x, srcY);
                shiftedPath.lineTo(x, dstY);
            }
        }

        g.setColour(fx::col::textSecondary.withAlpha(0.72f));
        g.strokePath(sourcePath, juce::PathStrokeType(2.0f));
        g.setColour(fx::accent::pitch.withAlpha(0.84f));
        g.strokePath(shiftedPath, juce::PathStrokeType(2.6f));
        g.setColour(fx::col::textMuted);
        g.drawText("Source envelope", (int) graphLeft, (int) graphTop - 20, 140, 16, juce::Justification::left);
        g.drawText("Warped envelope", (int) graphLeft + 160, (int) graphTop - 20, 150, 16, juce::Justification::left);
        return;
    }

    const float graphLeft = left + 32.0f;
    const float graphWidth = width - 64.0f;
    const float centerY = top + height * 0.55f;
    juce::Path leftPath;
    juce::Path rightPath;
    for (int pixel = 0; pixel <= (int) graphWidth; ++pixel)
    {
        const float t = (float) pixel / graphWidth;
        const float x = graphLeft + (float) pixel;
        const float yA = centerY + std::sin(animPhase * 1.9f + t * juce::MathConstants<float>::twoPi * (1.0f + snapshot.primary * 0.3f))
                                 * (24.0f + snapshot.secondary * 0.4f);
        const float yB = centerY + std::sin(animPhase * 1.9f + t * juce::MathConstants<float>::twoPi * (1.0f + snapshot.primary * 0.3f)
                                 + snapshot.bins[2] * 1.8f) * (18.0f + snapshot.secondary * 0.3f);
        if (pixel == 0)
        {
            leftPath.startNewSubPath(x, yA);
            rightPath.startNewSubPath(x, yB);
        }
        else
        {
            leftPath.lineTo(x, yA);
            rightPath.lineTo(x, yB);
        }
    }
    g.setColour(fx::accent::pitch.withAlpha(0.84f));
    g.strokePath(leftPath, juce::PathStrokeType(2.0f));
    g.setColour(fx::accent::pitch.withAlpha(0.34f));
    g.strokePath(rightPath, juce::PathStrokeType(1.6f));
    g.setColour(fx::col::textMuted);
    g.drawText("Stereo LFO modulation with Hermite delay readout and rise shaping.", area.removeFromBottom(22), juce::Justification::centred);
}

void MusiquePitchShiftEditor::paint(juce::Graphics& g)
{
    g.fillAll(fx::col::bg);
    fx::paint::header(g, getWidth(), fx::accent::pitch);
    if (pluginIcon.isValid())
        g.drawImage(pluginIcon, juce::Rectangle<float>(12.0f, 10.0f, 40.0f, 40.0f), juce::RectanglePlacement::centred);
    fx::paint::presetBar(g, getWidth());
    fx::paint::graphArea(g, getWidth());
    fx::paint::graphGrid(g, getWidth(), 30, 30);
    paintVisualization(g, juce::Rectangle<int>(0, fx::dim::headerH + fx::dim::presetBarH, getWidth(), fx::dim::visualH));
    fx::paint::controls(g, getWidth(), 8);
    fx::paint::footer(g, getWidth());
    if (logoImg.isValid())
    {
        const int footerY = fx::dim::appH - fx::dim::footerH;
        g.drawImage(logoImg, juce::Rectangle<float>((float) getWidth() - 52.0f, (float) footerY + 4.0f, 32.0f, 32.0f),
                    juce::RectanglePlacement::centred);
    }
    fx::paint::footerLabel(g, "OUT", 92, 180);
    fx::paint::outline(g, getLocalBounds());
}

void MusiquePitchShiftEditor::resized()
{
    titleLabel.setBounds(56, 10, 180, 40);
    bypassBtn.setBounds(getWidth() - 464, 16, 64, fx::dim::btnH);
    monoBtn.setBounds(getWidth() - 392, 16, 96, fx::dim::btnH);
    syncBtn.setBounds(getWidth() - 288, 16, 72, fx::dim::btnH);
    modeBtn.setBounds(getWidth() - 208, 16, 78, fx::dim::btnH);
    statusBtn.setBounds(getWidth() - 122, 16, 98, fx::dim::btnH);

    const int presetY = fx::dim::headerH + 11;
    prevBtn.setBounds(126, presetY, 30, fx::dim::btnH);
    presetBox.setBounds(160, presetY, 210, fx::dim::btnH);
    nextBtn.setBounds(374, presetY, 30, fx::dim::btnH);
    engineBox.setBounds(426, presetY, 150, fx::dim::btnH);
    variantBox.setBounds(586, presetY, 146, fx::dim::btnH);
    saveBtn.setBounds(748, presetY, 56, fx::dim::btnH);
    abBtn.setBounds(812, presetY, 48, fx::dim::btnH);

    const int controlsTop = fx::dim::headerH + fx::dim::presetBarH + fx::dim::visualH;
    const int knobWidth = getWidth() / 8;
    const int knobY = controlsTop + 14;
    for (int index = 0; index < 8; ++index)
    {
        const int x = index * knobWidth;
        knobs[index].setBounds(x + (knobWidth - 92) / 2, knobY, 92, 90);
        knobLabels[index].setBounds(x + (knobWidth - 120) / 2, knobY + 92, 120, 16);
    }

    const int footerY = fx::dim::appH - fx::dim::footerH;
    inMeter.setBounds(16, footerY + 6, 20, fx::dim::footerH - 12);
    outMeter.setBounds(42, footerY + 6, 20, fx::dim::footerH - 12);
    outputSlider.setBounds(92, footerY + 8, 180, 24);
    activeLED.setBounds(300, footerY + 14, 12, 12);
    versionLabel.setBounds(getWidth() - 232, footerY + 8, 170, 24);
}

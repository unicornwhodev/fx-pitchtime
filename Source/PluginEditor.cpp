#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

MusiquePitchShiftEditor::MusiquePitchShiftEditor(MusiquePitchShiftProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setLookAndFeel(&lnf);
    setSize(fx::dim::appW, fx::dim::appH);

    // Header
    titleLabel.setText("PITCH SHIFT", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::header).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, fx::col::textPrimary);
    addAndMakeVisible(titleLabel);

    pluginIcon = juce::ImageCache::getFromMemory(BinaryData::icon_small_png, BinaryData::icon_small_pngSize);
    logoImg = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);

    auto setupHdrBtn = [&](juce::TextButton& b, bool toggle = false) {
        b.setColour(juce::TextButton::buttonColourId, fx::col::surfSecondary);
        b.setColour(juce::TextButton::textColourOffId, fx::col::textPrimary);
        if (toggle) b.setClickingTogglesState(true);
        addAndMakeVisible(b);
    };
    setupHdrBtn(bypassBtn, true);
    setupHdrBtn(inputMonoBtn, true);
    setupHdrBtn(formantBtn, true);
    setupHdrBtn(stackModeBtn, true);
    setupHdrBtn(settingsBtn);
    fx::ui::markUnsupportedControl(settingsBtn);
    formantBtn.setClickingTogglesState(false);
    formantBtn.setTooltip("Shows the current formant colour state driven by the FORMANT knob");
    formantBtn.onClick = [] {};
    stackModeBtn.setTooltip("Toggle between centred mono stack voices and stereo spread stack voices");

    // Preset bar
    setupHdrBtn(prevBtn); setupHdrBtn(nextBtn); setupHdrBtn(saveBtn); setupHdrBtn(abBtn);
    addAndMakeVisible(presetBox);

    presets = std::make_shared<juce::Array<juce::var>>(fx::preset::loadAllPresets("fx-pitchtime"));
    if (presets->isEmpty()) { presetBox.addItem("Init", 1); presetBox.setSelectedId(1); }
    else
    {
        int id = 1;
        for (auto& pv : *presets)
            if (auto* o = pv.getDynamicObject())
                presetBox.addItem(o->getProperty("name").toString(), id++);
        presetBox.setSelectedItemIndex(0, juce::dontSendNotification);
        fx::preset::applyToAPVTS(proc.getAPVTS(), presets->getReference(0));
    }
    presetBox.onChange = [this] {
        int i = presetBox.getSelectedItemIndex();
        if (i >= 0 && i < presets->size()) fx::preset::applyToAPVTS(proc.getAPVTS(), presets->getReference(i));
    };
    prevBtn.onClick = [this] { int i = presetBox.getSelectedItemIndex(); if (i > 0) presetBox.setSelectedItemIndex(i - 1); };
    nextBtn.onClick = [this] { int i = presetBox.getSelectedItemIndex(); if (i < presetBox.getNumItems() - 1) presetBox.setSelectedItemIndex(i + 1); };
    saveBtn.onClick = [this] {
        auto name = juce::String("User_") + juce::Time::getCurrentTime().formatted("%H%M%S");
        juce::StringArray ids {"pitch","fine","formant","voice","spread","detune","octave","mix","output","stereo_stack","bypass","mono"};
        if (fx::preset::saveUserPreset("fx-pitchtime", name, ids, proc.getAPVTS()))
        {
            *presets = fx::preset::loadAllPresets("fx-pitchtime");
            presetBox.clear();
            int id = 1;
            for (auto& pv : *presets)
                if (auto* o = pv.getDynamicObject()) presetBox.addItem(o->getProperty("name").toString(), id++);
            presetBox.setSelectedItemIndex(presetBox.getNumItems() - 1);
        }
    };

    // Knobs
    const char* labels[8] = {"PITCH", "FINE", "FORMANT", "VOICE", "SPREAD", "DETUNE", "OCTAVE", "MIX"};
    for (int i = 0; i < 8; ++i)
    {
        knobs[i].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 16);
        addAndMakeVisible(knobs[i]);
        knobLabels[i].setText(labels[i], juce::dontSendNotification);
        knobLabels[i].setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::label).withStyle("Bold")));
        knobLabels[i].setJustificationType(juce::Justification::centred);
        knobLabels[i].setColour(juce::Label::textColourId, fx::col::textMuted);
        addAndMakeVisible(knobLabels[i]);
    }

    // Footer
    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);
    outputSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible(outputSlider);
    activeLED.setAccent(fx::accent::pitch);
    addAndMakeVisible(activeLED);
    versionLabel.setText("Musique Pitch Shift v1.0", juce::dontSendNotification);
    versionLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(fx::font::footer)));
    versionLabel.setColour(juce::Label::textColourId, fx::col::textMuted);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(versionLabel);

    // Attachments
    pitchAtt   = std::make_unique<SliderAttach>(proc.getAPVTS(), "pitch",   knobs[0]);
    fineAtt    = std::make_unique<SliderAttach>(proc.getAPVTS(), "fine",    knobs[1]);
    formantAtt = std::make_unique<SliderAttach>(proc.getAPVTS(), "formant", knobs[2]);
    voiceAtt   = std::make_unique<SliderAttach>(proc.getAPVTS(), "voice",   knobs[3]);
    spreadAtt  = std::make_unique<SliderAttach>(proc.getAPVTS(), "spread",  knobs[4]);
    detuneAtt  = std::make_unique<SliderAttach>(proc.getAPVTS(), "detune",  knobs[5]);
    octaveAtt  = std::make_unique<SliderAttach>(proc.getAPVTS(), "octave",  knobs[6]);
    mixAtt     = std::make_unique<SliderAttach>(proc.getAPVTS(), "mix",     knobs[7]);
    outAtt     = std::make_unique<SliderAttach>(proc.getAPVTS(), "output",  outputSlider);
    bypassAtt  = std::make_unique<ButtonAttach>(proc.getAPVTS(), "bypass",  bypassBtn);
    inputMonoAtt = std::make_unique<ButtonAttach>(proc.getAPVTS(), "mono", inputMonoBtn);
    stackModeAtt = std::make_unique<ButtonAttach>(proc.getAPVTS(), "stereo_stack", stackModeBtn);

    startTimerHz(fx::anim::fftRefreshHz);
}

MusiquePitchShiftEditor::~MusiquePitchShiftEditor() { setLookAndFeel(nullptr); }

void MusiquePitchShiftEditor::timerCallback()
{
    const auto inputLevels = proc.getVisualState().getInputLevels();
    const auto outputLevels = proc.getVisualState().getOutputLevels();
    inMeter.setLevel(inputLevels.left, inputLevels.right);
    outMeter.setLevel(outputLevels.left, outputLevels.right);

    phase += 0.04f;
    if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;

    float pitch = 0.0f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("pitch")) pitch = p->load();
    float octave = 0.0f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("octave")) octave = p->load();
    float formant = 0.5f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("formant")) formant = p->load();
    int voices = 1;
    if (auto* p = proc.getAPVTS().getRawParameterValue("voice")) voices = juce::jlimit(1, 4, (int) std::round(p->load()));
    float spread = 0.0f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("spread")) spread = p->load();
    const bool stereoStack = proc.getAPVTS().getRawParameterValue("stereo_stack")->load() > 0.5f;
    const bool inputMono = proc.getAPVTS().getRawParameterValue("mono")->load() > 0.5f;
    const bool isBypassed = proc.getAPVTS().getRawParameterValue("bypass")->load() > 0.5f;

    const float totalPitch = pitch + octave * 12.0f;
    activeLED.setOn(!isBypassed && juce::jmax(outputLevels.left, outputLevels.right) > 0.02f);
    inputMonoBtn.setButtonText(inputMono ? "IN MONO" : "IN STEREO");
    stackModeBtn.setButtonText(stereoStack ? "STACK STEREO" : "STACK MONO");
    inputMonoBtn.setColour(juce::TextButton::buttonColourId,
        inputMono ? fx::accent::pitch.withAlpha(0.18f) : fx::col::surfSecondary);
    inputMonoBtn.setColour(juce::TextButton::textColourOffId,
        inputMono ? fx::accent::pitch.brighter(0.2f) : fx::col::textPrimary);
    stackModeBtn.setColour(juce::TextButton::buttonColourId,
        stereoStack ? fx::accent::pitch.withAlpha(0.18f) : fx::col::surfSecondary);
    stackModeBtn.setColour(juce::TextButton::textColourOffId,
        stereoStack ? fx::accent::pitch.brighter(0.2f) : fx::col::textPrimary);

    const bool formantBright = formant > 0.54f;
    const bool formantDark = formant < 0.46f;
    formantBtn.setButtonText(formantBright ? "FORM BRIGHT" : (formantDark ? "FORM DARK" : "FORM NEUTRAL"));
    formantBtn.setColour(juce::TextButton::buttonColourId,
        formantBright || formantDark ? fx::accent::pitch.withAlpha(0.18f) : fx::col::surfSecondary);
    formantBtn.setColour(juce::TextButton::textColourOffId,
        formantBright || formantDark ? fx::accent::pitch.brighter(0.2f) : fx::col::textPrimary);

    repaint(0, fx::dim::headerH + fx::dim::presetBarH, getWidth(), fx::dim::visualH);
}

void MusiquePitchShiftEditor::paintVisualization(juce::Graphics& g, juce::Rectangle<int> area)
{
    float pitchSemi = 0.0f, fineCents = 0.0f, octave = 0.0f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("pitch")) pitchSemi = p->load();
    if (auto* p = proc.getAPVTS().getRawParameterValue("fine")) fineCents = p->load();
    if (auto* p = proc.getAPVTS().getRawParameterValue("octave")) octave = p->load();

    float voice = 1.0f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("voice")) voice = p->load();
    float formant = 0.5f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("formant")) formant = p->load();
    float spread = 0.0f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("spread")) spread = p->load();
    float detune = 0.0f;
    if (auto* p = proc.getAPVTS().getRawParameterValue("detune")) detune = p->load();
    const bool stereoStack = proc.getAPVTS().getRawParameterValue("stereo_stack")->load() > 0.5f;
    const bool inputMono = proc.getAPVTS().getRawParameterValue("mono")->load() > 0.5f;

    const float w = (float)area.getWidth();
    const float h = (float)area.getHeight();
    const float cx = (float)area.getX();
    const float cy = (float)area.getY();
    const float pad = 30.0f;

    float totalShift = pitchSemi + fineCents / 100.0f + octave * 12.0f;

    // === Semitone grid ===
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f)));
    for (int st = -12; st <= 12; st += 3)
    {
        float norm = ((float)st + 12.0f) / 24.0f;
        float yPos = cy + h - pad - norm * (h - 2.0f * pad);
        g.setColour(st == 0 ? fx::col::gridMajor : fx::col::gridMinor);
        g.drawHorizontalLine((int)yPos, cx + pad, cx + w - pad);
        g.setColour(fx::col::textMuted);
        g.drawText(juce::String(st > 0 ? "+" : "") + juce::String(st) + "st",
                   (int)(cx + 2), (int)(yPos - 5), 34, 10, juce::Justification::centredRight);
    }

    // Time axis (horizontal)
    for (int t = 0; t <= 4; ++t)
    {
        float xPos = cx + pad + (float)t * (w - 2.0f * pad) / 4.0f;
        g.setColour(fx::col::gridMinor);
        g.drawVerticalLine((int)xPos, cy + pad, cy + h - pad);
    }

    // Mode badges
    auto drawModeBadge = [&](juce::Rectangle<float> rect, const juce::String& text, juce::Colour fill)
    {
        g.setColour(fill.withAlpha(0.18f));
        g.fillRoundedRectangle(rect, 7.0f);
        g.setColour(fill.withAlpha(0.55f));
        g.drawRoundedRectangle(rect, 7.0f, 1.0f);
        g.setColour(fx::col::textPrimary);
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")));
        g.drawText(text, rect.toNearestInt(), juce::Justification::centred);
    };

    drawModeBadge({ cx + pad, cy + 12.0f, 108.0f, 22.0f }, inputMono ? "INPUT MONO" : "INPUT STEREO", fx::accent::pitch);
    drawModeBadge({ cx + pad + 116.0f, cy + 12.0f, 116.0f, 22.0f }, stereoStack ? "STACK STEREO" : "STACK MONO", fx::col::textSecondary);

    // === Pitch shift line (animated) ===
    float shiftNorm = (totalShift + 12.0f) / 24.0f;
    shiftNorm = juce::jlimit(0.0f, 1.0f, shiftNorm);
    float shiftY = cy + h - pad - shiftNorm * (h - 2.0f * pad);

    // Shift band (showing voice spread)
    float voiceSpread = (voice - 1.0f) * 15.0f; // pixels per voice
    g.setColour(fx::accent::pitch.withAlpha(0.1f));
    g.fillRect(cx + pad, shiftY - voiceSpread, w - 2.0f * pad, voiceSpread * 2.0f + 2.0f);

    // Formant tilt shading
    const float formantTilt = (formant - 0.5f) * 2.0f;
    const float shadeHeight = juce::jmap(std::abs(formantTilt), 0.0f, 1.0f, 0.0f, 46.0f);
    if (shadeHeight > 1.0f)
    {
        const auto shade = juce::Rectangle<float>(cx + pad, shiftY - shadeHeight * 0.5f, w - 2.0f * pad, shadeHeight);
        g.setColour((formantTilt >= 0.0f ? fx::col::textSecondary : fx::col::textMuted).withAlpha(0.12f));
        g.fillRoundedRectangle(shade, 8.0f);
    }

    // Animated pitch shift curve
    juce::Path pitchPath;
    for (int i = 0; i <= (int)(w - 2.0f * pad); ++i)
    {
        float t = (float)i / (w - 2.0f * pad);
        float xPos = cx + pad + (float)i;

        // Animated micro-modulation
        float mod = std::sin(t * 12.0f + phase * 2.0f) * 2.0f;
        float vibrato = std::sin(phase * 3.0f + t * 4.0f) * 1.5f;
        float yPos = shiftY + mod + vibrato;

        if (i == 0) pitchPath.startNewSubPath(xPos, yPos);
        else pitchPath.lineTo(xPos, yPos);
    }

    g.setColour(fx::accent::pitch.withAlpha(0.85f));
    g.strokePath(pitchPath, juce::PathStrokeType(2.5f));

    // Voice lines (additional shift curves for multi-voice)
    int numVoices = (int)voice;
    for (int v = 1; v < numVoices; ++v)
    {
        juce::Path vPath;
        float vOffset = (float)v * 8.0f;
        for (int i = 0; i <= (int)(w - 2.0f * pad); ++i)
        {
            float t = (float)i / (w - 2.0f * pad);
            float xPos = cx + pad + (float)i;
            float mod = std::sin(t * 12.0f + phase * 2.0f + (float)v * 1.5f) * 2.5f;
            float yPos = shiftY + mod + vOffset * ((v % 2 == 0) ? 1.0f : -1.0f);

            if (i == 0) vPath.startNewSubPath(xPos, yPos);
            else vPath.lineTo(xPos, yPos);
        }
        g.setColour(fx::accent::pitch.withAlpha(0.3f));
        g.strokePath(vPath, juce::PathStrokeType(1.5f));
    }

    // === Large pitch display (center) ===
    juce::String noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int noteIdx = ((int)std::round(totalShift) % 12 + 12) % 12;
    int octaveDisplay = 4 + (int)std::floor((totalShift + 9.0f) / 12.0f);

    // Semitone display
    g.setColour(fx::accent::pitch);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(36.0f).withStyle("Bold")));
    juce::String shiftText = (totalShift >= 0 ? "+" : "") + juce::String(totalShift, 1) + " st";
    g.drawText(shiftText, (int)(cx + w * 0.35f), (int)(cy + 20), 300, 40, juce::Justification::centredLeft);

    // Note name
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(18.0f)));
    g.setColour(fx::col::textSecondary);
    g.drawText(noteNames[noteIdx] + juce::String(octaveDisplay),
               (int)(cx + w * 0.35f), (int)(cy + 58), 80, 24, juce::Justification::centredLeft);

    // Cents display
    float cents = (totalShift - std::round(totalShift)) * 100.0f;
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(14.0f)));
    g.setColour(fx::col::textMuted);
    g.drawText(juce::String((int)cents) + " cents",
               (int)(cx + w * 0.35f + 80), (int)(cy + 60), 80, 20, juce::Justification::centredLeft);

    const juce::String stackText = juce::String((int) std::round(voice)) + ((int) std::round(voice) == 1 ? " voice stack" : " voices stack") + (stereoStack ? " stereo" : " mono");
    const juce::String formantText = std::abs(formantTilt) < 0.04f
        ? "formant colour neutral"
        : (formantTilt > 0.0f ? "formant colour bright" : "formant colour dark");
    const juce::String spreadText = "stack width " + juce::String((int) std::round(spread)) + "%  detune " + juce::String(detune, 1) + " ct";
    const juce::String inputText = inputMono ? "input stage summed before pitch processing" : "input stage keeps original stereo image";
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f).withStyle("Bold")));
    g.setColour(fx::col::textPrimary.withAlpha(0.9f));
    g.drawText(stackText, (int)(cx + w * 0.35f), (int)(cy + 88), 120, 18, juce::Justification::centredLeft);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
    g.setColour(fx::col::textMuted);
    g.drawText(formantText, (int)(cx + w * 0.35f), (int)(cy + 106), 140, 18, juce::Justification::centredLeft);
    g.drawText(spreadText, (int)(cx + w * 0.35f), (int)(cy + 122), 180, 18, juce::Justification::centredLeft);
    g.drawText(inputText, (int)(cx + w * 0.35f), (int)(cy + 138), 180, 18, juce::Justification::centredLeft);

    // Zero line indicator
    float zeroY = cy + h - pad - 0.5f * (h - 2.0f * pad);
    g.setColour(fx::col::textMuted.withAlpha(0.4f));
    g.drawText("0", (int)(cx + w - 28), (int)(zeroY - 5), 20, 10, juce::Justification::centred);

    // Animated glow dot at current shift position
    float dotX = cx + w * 0.5f + std::sin(phase) * 40.0f;
    float pulse = 0.6f + 0.4f * std::sin(phase * 2.0f);
    g.setColour(fx::accent::pitch.withAlpha(0.2f * pulse));
    g.fillEllipse(dotX - 16.0f, shiftY - 16.0f, 32.0f, 32.0f);
    g.setColour(fx::accent::pitch.withAlpha(0.7f));
    g.fillEllipse(dotX - 5.0f, shiftY - 5.0f, 10.0f, 10.0f);
}

void MusiquePitchShiftEditor::paint(juce::Graphics& g)
{
    g.fillAll(fx::col::bg);
    fx::paint::header(g, getWidth(), fx::accent::pitch);
    if (pluginIcon.isValid())
        g.drawImage(pluginIcon, juce::Rectangle<float>(12, 10, 40, 40), juce::RectanglePlacement::centred);
    fx::paint::presetBar(g, getWidth());
    fx::paint::graphArea(g, getWidth());
    fx::paint::graphGrid(g, getWidth());
    paintVisualization(g, juce::Rectangle<int>(0, fx::dim::headerH + fx::dim::presetBarH, getWidth(), fx::dim::visualH));
    fx::paint::controls(g, getWidth(), 8);
    fx::paint::footer(g, getWidth());
    if (logoImg.isValid())
    {
        const int fy = fx::dim::appH - fx::dim::footerH;
        g.drawImage(logoImg, juce::Rectangle<float>((float)getWidth() - 52.0f, (float)fy + 4.0f, 32.0f, 32.0f), juce::RectanglePlacement::centred);
    }
    fx::paint::footerLabel(g, "OUT", 80, 180);
    fx::paint::outline(g, getLocalBounds());
}

void MusiquePitchShiftEditor::resized()
{
    // Header
    titleLabel.setBounds(56, 10, 160, 40);
    bypassBtn.setBounds(getWidth() - 478, 16, 64, fx::dim::btnH);
    inputMonoBtn.setBounds(getWidth() - 408, 16, 112, fx::dim::btnH);
    formantBtn.setBounds(getWidth() - 290, 16, 100, fx::dim::btnH);
    stackModeBtn.setBounds(getWidth() - 184, 16, 112, fx::dim::btnH);
    settingsBtn.setBounds(getWidth() - 64, 16, 42, fx::dim::btnH);

    // Preset bar
    const int py = fx::dim::headerH + 11;
    prevBtn.setBounds(260, py, 30, fx::dim::btnH);
    presetBox.setBounds(294, py, 250, fx::dim::btnH);
    nextBtn.setBounds(548, py, 30, fx::dim::btnH);
    saveBtn.setBounds(590, py, 56, fx::dim::btnH);
    abBtn.setBounds(652, py, 48, fx::dim::btnH);

    // Knobs
    const int ctrlTop = fx::dim::headerH + fx::dim::presetBarH + fx::dim::visualH;
    const int numKnobs = 8;
    const int kW = getWidth() / numKnobs;
    const int kY = ctrlTop + 14;
    for (int i = 0; i < numKnobs; ++i)
    {
        int x = i * kW;
        knobs[i].setBounds(x + (kW - 92) / 2, kY, 92, 90);
        knobLabels[i].setBounds(x + (kW - 120) / 2, kY + 92, 120, 16);
    }

    // Footer
    const int fy = fx::dim::appH - fx::dim::footerH;
    inMeter.setBounds(16, fy + 6, 20, fx::dim::footerH - 12);
    outMeter.setBounds(42, fy + 6, 20, fx::dim::footerH - 12);
    outputSlider.setBounds(80, fy + 8, 180, 24);
    activeLED.setBounds(280, fy + 14, 12, 12);
    versionLabel.setBounds(getWidth() - 220, fy + 8, 160, 24);
}

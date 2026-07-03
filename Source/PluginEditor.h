#pragma once

#include <JuceHeader.h>
#include <array>
#include "PluginProcessor.h"
#include "FXTokens.h"
#include "FXLookAndFeel.h"
#include "FXComponents.h"

class MusiquePitchShiftEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    struct EngineUiConfig
    {
        const char* title;
        std::array<const char*, 7> paramIds;
        std::array<const char*, 7> labels;
        juce::StringArray variants;
        const char* syncParamId;
        float syncMin;
        float syncMax;
    };

    explicit MusiquePitchShiftEditor(MusiquePitchShiftProcessor&);
    ~MusiquePitchShiftEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttach = APVTS::SliderAttachment;
    using ComboAttach = APVTS::ComboBoxAttachment;
    using ButtonAttach = APVTS::ButtonAttachment;

    void timerCallback() override;
    void paintVisualization(juce::Graphics&, juce::Rectangle<int>);
    void loadPresets();
    void refreshPresetBox();
    void rebuildEngineUi(bool force = false);
    void rebuildVariantItems(int engineIndex);
    void bindEngineKnobs(int engineIndex);
    void applyVariantSelection(int engineIndex, int variantIndex);
    void updateHeaderButtons();
    void updateKnobTextFunctions(int engineIndex);
    int getCurrentEngineIndex() const;
    int getCurrentVariantIndex() const;
    void storeCurrentABSlot();
    void recallABSlot(bool showA);

    MusiquePitchShiftProcessor& proc;
    fx::FXLookAndFeel lnf { fx::accent::pitch };

    juce::Label titleLabel;
    juce::Image pluginIcon, logoImg;
    juce::TextButton bypassBtn { "Bypass" };
    juce::TextButton monoBtn { "STEREO IN" };
    juce::TextButton syncBtn { "SYNC" };
    juce::TextButton modeBtn { "MODE" };
    juce::TextButton statusBtn { "STATUS" };

    juce::TextButton prevBtn { "<" };
    juce::TextButton nextBtn { ">" };
    juce::TextButton saveBtn { "Save" };
    juce::TextButton abBtn { "A/B" };

    juce::ComboBox presetBox;
    juce::ComboBox engineBox;
    juce::ComboBox variantBox;

    juce::Slider knobs[8];
    juce::Label knobLabels[8];

    fx::MeterComponent inMeter, outMeter;
    juce::Slider outputSlider;
    fx::LEDComponent activeLED;
    juce::Label versionLabel;

    std::array<std::unique_ptr<SliderAttach>, 7> engineKnobAtts;
    std::unique_ptr<SliderAttach> mixAtt, outAtt;
    std::unique_ptr<ComboAttach> engineAtt;
    std::unique_ptr<ButtonAttach> bypassAtt, monoAtt, syncAtt;

    std::shared_ptr<juce::Array<juce::var>> presets;
    int displayedEngine = -1;
    float animPhase = 0.0f;
    bool showingA = true;
    juce::ValueTree abStateA, abStateB;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusiquePitchShiftEditor)
};

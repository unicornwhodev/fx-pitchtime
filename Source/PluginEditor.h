#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "FXTokens.h"
#include "FXLookAndFeel.h"
#include "FXComponents.h"

class MusiquePitchShiftEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MusiquePitchShiftEditor(MusiquePitchShiftProcessor&);
    ~MusiquePitchShiftEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    using SliderAttach = APVTS::SliderAttachment;
    using ButtonAttach = APVTS::ButtonAttachment;

    void timerCallback() override;
    void paintVisualization(juce::Graphics&, juce::Rectangle<int> area);

    MusiquePitchShiftProcessor& proc;
    fx::FXLookAndFeel lnf { fx::accent::pitch };

    // Header
    juce::Label titleLabel;
    juce::Image pluginIcon, logoImg;
    juce::TextButton bypassBtn{"Bypass"}, inputMonoBtn{"INPUT STEREO"}, formantBtn{"Formant"}, stackModeBtn{"STEREO STACK"}, settingsBtn{juce::CharPointer_UTF8("\xe2\x9a\x99")};

    // Preset bar
    juce::TextButton prevBtn{"<"}, nextBtn{">"}, saveBtn{"Save"}, abBtn{"A/B"};
    juce::ComboBox presetBox;

    // 8 knobs
    juce::Slider knobs[8];
    juce::Label knobLabels[8];

    // Footer
    fx::MeterComponent inMeter, outMeter;
    juce::Slider outputSlider;
    juce::Label versionLabel;
    fx::LEDComponent activeLED;

    // Visualization
    float phase = 0.0f;

    // Attachments
    std::unique_ptr<SliderAttach> pitchAtt, fineAtt, formantAtt, voiceAtt, spreadAtt, detuneAtt, octaveAtt, mixAtt, outAtt;
    std::unique_ptr<ButtonAttach> bypassAtt, inputMonoAtt, stackModeAtt;

    std::shared_ptr<juce::Array<juce::var>> presets;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusiquePitchShiftEditor)
};

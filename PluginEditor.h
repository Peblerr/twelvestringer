#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

//==============================================================================
class TwelveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TwelveLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

//==============================================================================
class TwelveStringerEditor : public juce::AudioProcessorEditor,
                             private juce::Timer
{
public:
    explicit TwelveStringerEditor (TwelveStringerProcessor&);
    ~TwelveStringerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title, caption;
        juce::Colour accent;
        std::vector<Knob*> knobs;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void setupKnob (Knob&, const char* paramID, const juce::String& name, juce::Colour accent);
    void paintStrings (juce::Graphics&, juce::Rectangle<float>) const;

    TwelveStringerProcessor& processor;
    TwelveLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    Knob octave, focus, sparkle, unison, detune, drift, strum, width, dry, output;
    std::array<Section, 3> sections;

    juce::ComboBox presetBox;
    juce::ToggleButton liveButton { "LIVE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> liveAttachment;

    juce::Rectangle<int> stringsArea, statusArea;
    juce::String statusText;
    float phase = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwelveStringerEditor)
};

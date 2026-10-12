#pragma once

#include "PluginProcessor.h"

//==============================================================================
class RblLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RblLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
};

//==============================================================================
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (RollingBassLabProcessor& p) : proc (p) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    int stepAt (juce::Point<float> pos) const;
    void showStepMenu (int step);

    RollingBassLabProcessor& proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGrid)
};

//==============================================================================
class MidiDragBox : public juce::Component
{
public:
    explicit MidiDragBox (RollingBassLabProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; repaint(); }

private:
    RollingBassLabProcessor& proc;
    bool dragging = false;
};

//==============================================================================
class RollingBassLabEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RollingBassLabEditor (RollingBassLabProcessor&);
    ~RollingBassLabEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void setupCombo (juce::ComboBox&, const juce::StringArray& items,
                     std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&, const juce::String& paramId);
    void placeKnob (Knob&, juce::Rectangle<int> area);
    void saveMidiFile();

    RollingBassLabProcessor& proc;
    RblLookAndFeel lnf;

    // Header
    juce::ComboBox presetBox;
    juce::ToggleButton quietButton { "QUIET CHECK" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> quietAtt;

    // Pattern
    juce::ComboBox styleBox, keyBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAtt, keyAtt;
    juce::Label styleLabel, keyLabel;
    Knob octave, density, swing, gate, previewBpm;
    juce::TextButton generateButton { "GENERATE" };
    juce::ToggleButton seqButton { "PATTERN ON" }, previewButton { "PREVIEW" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> seqAtt, previewAtt;
    StepGrid grid;
    MidiDragBox dragBox;
    juce::TextButton saveMidiButton { "SAVE .MID" };
    std::unique_ptr<juce::FileChooser> chooser;

    // Synth
    juce::ComboBox waveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAtt;
    juce::Label waveLabel;
    Knob sub, cutoff, reso, envAmt, decay, sustain, drive, glide;

    // Duck + output
    Knob duckDepth, duckRelease, gain;

    float meterLevel = 0.0f;

    juce::Rectangle<int> patternPanel, synthPanel, duckPanel, outPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollingBassLabEditor)
};

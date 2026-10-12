#include "PluginEditor.h"

namespace Col
{
    const juce::Colour bg     { 0xff141518 };
    const juce::Colour panel  { 0xff1e2024 };
    const juce::Colour edge   { 0xff2d3036 };
    const juce::Colour amber  { 0xffffb03a };
    const juce::Colour teal   { 0xff3ccfbf };
    const juce::Colour text   { 0xffd2cdc0 };
    const juce::Colour dim    { 0xff6a6e76 };
    const juce::Colour stepOff{ 0xff26292e };
}

//==============================================================================
RblLookAndFeel::RblLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Col::text);
    setColour (juce::ComboBox::backgroundColourId, Col::stepOff);
    setColour (juce::ComboBox::textColourId, Col::text);
    setColour (juce::ComboBox::outlineColourId, Col::edge);
    setColour (juce::ComboBox::arrowColourId, Col::amber);
    setColour (juce::PopupMenu::backgroundColourId, Col::panel);
    setColour (juce::PopupMenu::textColourId, Col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::amber.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, Col::amber);
    setColour (juce::TextButton::textColourOffId, Col::text);
    setColour (juce::TextButton::textColourOnId, Col::bg);
}

void RblLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto r = bounds.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float radius = size * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Track
    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (Col::edge);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, startAngle, angle, true);
    g.setColour (Col::amber);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Body
    auto body = r.reduced (radius * 0.28f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d44), body.getX(), body.getY(),
                                             juce::Colour (0xff1a1c20), body.getRight(), body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (body, 1.0f);

    // Pointer
    const float len = body.getWidth() * 0.5f;
    juce::Line<float> pointer (c, c.getPointOnCircumference (len * 0.85f, angle));
    g.setColour (Col::amber);
    g.drawLine (pointer, 2.2f);
}

void RblLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                           bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    auto fill = b.getToggleState() ? Col::amber : Col::stepOff;
    if (down) fill = Col::amber.darker (0.3f);
    else if (highlighted) fill = fill.brighter (0.12f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Col::amber.withAlpha (0.7f));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);
}

void RblLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Col::teal.withAlpha (0.18f) : Col::stepOff);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? Col::teal : (highlighted ? Col::dim.brighter() : Col::dim));
    g.drawRoundedRectangle (r, 4.0f, 1.2f);

    auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.32f);
    g.setColour (on ? Col::teal : Col::edge);
    g.fillEllipse (led);

    g.setColour (on ? Col::teal : Col::text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centredLeft);
}

void RblLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (Col::stepOff);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Col::edge);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Col::amber);
    g.fillPath (arrow);
}

//==============================================================================
// Step grid
namespace
{
    constexpr int minSemis = -12, maxSemis = 24;

    juce::String semisLabel (int root, int semis)
    {
        return juce::MidiMessage::getMidiNoteName (root + semis, true, true, 3);
    }
}

int StepGrid::stepAt (juce::Point<float> pos) const
{
    const float w = (float) getWidth() / (float) RollingBassLabProcessor::numSteps;
    return juce::jlimit (0, RollingBassLabProcessor::numSteps - 1, (int) (pos.x / w));
}

void StepGrid::paint (juce::Graphics& g)
{
    const int n = RollingBassLabProcessor::numSteps;
    const float w = (float) getWidth() / (float) n;
    const float h = (float) getHeight();
    const int playing = proc.getPlayingStep();
    const int root = proc.getRootMidiNote();

    for (int i = 0; i < n; ++i)
    {
        auto cell = juce::Rectangle<float> (i * w, 0.0f, w, h).reduced (2.0f);
        const auto s = proc.getStep (i);

        g.setColour ((i / 4) % 2 == 0 ? Col::stepOff : Col::stepOff.brighter (0.05f));
        g.fillRoundedRectangle (cell, 3.0f);

        if (i % 4 == 0)
        {
            g.setColour (Col::dim.withAlpha (0.5f));
            g.fillRect (cell.getX(), cell.getBottom() - 3.0f, cell.getWidth(), 3.0f); // kick marker
        }

        if (s.on)
        {
            const float norm = (float) (s.semis - minSemis) / (float) (maxSemis - minSemis);
            auto noteArea = cell.reduced (3.0f, 16.0f);
            const float barY = noteArea.getBottom() - norm * noteArea.getHeight();
            auto bar = juce::Rectangle<float> (cell.getX() + 3.0f, barY - 6.0f, cell.getWidth() - 6.0f, 12.0f);

            g.setColour (Col::amber.withAlpha (0.18f));
            g.fillRoundedRectangle (cell.withTop (barY), 3.0f);
            g.setColour (s.accent ? Col::amber : Col::amber.withAlpha (0.7f));
            g.fillRoundedRectangle (bar, 3.0f);

            if (s.accent)
            {
                g.setColour (Col::text);
                g.drawRoundedRectangle (bar, 3.0f, 1.2f);
            }

            if (s.slide)
            {
                g.setColour (Col::teal);
                g.fillRect (bar.getRight() - 1.0f, barY - 1.5f, w * 0.6f, 3.0f);
            }

            g.setColour (Col::text);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (semisLabel (root, s.semis), cell.removeFromTop (14.0f), juce::Justification::centred);
        }

        if (i == playing)
        {
            g.setColour (Col::teal);
            g.drawRoundedRectangle (juce::Rectangle<float> (i * w, 0.0f, w, h).reduced (2.0f), 3.0f, 2.0f);
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int i = stepAt (e.position);
    auto s = proc.getStep (i);

    if (e.mods.isPopupMenu())
    {
        showStepMenu (i);
        return;
    }

    if (e.mods.isShiftDown())
        s.accent = ! s.accent;
    else if (e.mods.isAltDown())
        s.slide = ! s.slide;
    else
        s.on = ! s.on;

    proc.setStep (i, s);
    repaint();
}

void StepGrid::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int i = stepAt (e.position);
    auto s = proc.getStep (i);
    if (! s.on)
        return;
    s.semis = juce::jlimit (minSemis, maxSemis, s.semis + (wheel.deltaY > 0 ? 1 : -1));
    proc.setStep (i, s);
    repaint();
}

void StepGrid::showStepMenu (int step)
{
    const auto s = proc.getStep (step);
    const int root = proc.getRootMidiNote();

    juce::PopupMenu notes;
    const int choices[] = { 24, 19, 17, 15, 14, 12, 10, 7, 5, 3, 2, 0, -2, -5, -7, -12 };
    for (int c : choices)
        notes.addItem (1000 + c + 100, semisLabel (root, c) + "  (" + (c >= 0 ? "+" : "") + juce::String (c) + ")",
                       true, s.on && s.semis == c);

    juce::PopupMenu m;
    m.addSectionHeader ("Step " + juce::String (step + 1));
    m.addItem (1, "Step on", true, s.on);
    m.addItem (2, "Accent", true, s.accent);
    m.addItem (3, "Slide into next", true, s.slide);
    m.addSubMenu ("Note", notes);

    juce::Component::SafePointer<StepGrid> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe, step] (int result)
                     {
                         if (safe == nullptr || result == 0)
                             return;
                         auto st = safe->proc.getStep (step);
                         if (result == 1) st.on = ! st.on;
                         else if (result == 2) st.accent = ! st.accent;
                         else if (result == 3) st.slide = ! st.slide;
                         else if (result >= 1000)
                         {
                             st.semis = result - 1100;
                             st.on = true;
                         }
                         safe->proc.setStep (step, st);
                         safe->repaint();
                     });
}

//==============================================================================
void MidiDragBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (dragging ? Col::teal.withAlpha (0.25f) : Col::teal.withAlpha (0.1f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Col::teal);
    g.drawRoundedRectangle (r, 4.0f, 1.2f);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("DRAG MIDI TO ABLETON", r, juce::Justification::centred);
}

void MidiDragBox::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6)
        return;

    dragging = true;
    repaint();

    const auto file = proc.writePatternMidiFile();
    if (file.existsAsFile())
    {
        juce::Component::SafePointer<MidiDragBox> safe (this);
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                    [safe]
                                                                    {
                                                                        if (safe != nullptr)
                                                                        {
                                                                            safe->dragging = false;
                                                                            safe->repaint();
                                                                        }
                                                                    });
    }
}

//==============================================================================
RollingBassLabEditor::RollingBassLabEditor (RollingBassLabProcessor& p)
    : AudioProcessorEditor (&p), proc (p), grid (p), dragBox (p)
{
    setLookAndFeel (&lnf);

    // Header
    presetBox.addItemList (RollingBassLabProcessor::getPresetNames(), 1);
    presetBox.setTextWhenNothingSelected ("Presets...");
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0)
        {
            proc.applyPreset (idx);
            grid.repaint();
        }
    };
    addAndMakeVisible (presetBox);

    addAndMakeVisible (quietButton);
    quietButton.setTooltip ("Drops the output 18 dB. If the groove still hits this quiet, it's working.");
    quietAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "quiet", quietButton);

    // Pattern
    setupCombo (styleBox, RollingBassLabProcessor::getStyleNames(), styleAtt, "style");
    setupCombo (keyBox, RollingBassLabProcessor::getKeyNames(), keyAtt, "key");
    keyBox.onChange = [this] { grid.repaint(); };
    for (auto* l : { &styleLabel, &keyLabel, &waveLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, Col::dim);
        addAndMakeVisible (*l);
    }
    styleLabel.setText ("STYLE", juce::dontSendNotification);
    keyLabel.setText ("KEY", juce::dontSendNotification);
    waveLabel.setText ("WAVE", juce::dontSendNotification);

    setupKnob (octave, "octave", "OCTAVE");
    octave.slider.onValueChange = [this] { grid.repaint(); };
    setupKnob (density, "density", "DENSITY");
    setupKnob (swing, "swing", "SWING");
    setupKnob (gate, "gate", "GATE");
    setupKnob (previewBpm, "previewBpm", "PREV BPM");

    generateButton.onClick = [this]
    {
        proc.generatePattern();
        grid.repaint();
    };
    addAndMakeVisible (generateButton);

    addAndMakeVisible (seqButton);
    addAndMakeVisible (previewButton);
    seqButton.setTooltip ("On: plays the step pattern in time with your DAW. Off: play the bass from MIDI notes.");
    previewButton.setTooltip ("Runs the pattern on its own clock when your DAW is stopped.");
    seqAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "seqOn", seqButton);
    previewAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "preview", previewButton);

    addAndMakeVisible (grid);
    addAndMakeVisible (dragBox);
    saveMidiButton.onClick = [this] { saveMidiFile(); };
    addAndMakeVisible (saveMidiButton);

    // Synth
    setupCombo (waveBox, RollingBassLabProcessor::getWaveNames(), waveAtt, "wave");
    setupKnob (sub, "sub", "SUB");
    setupKnob (cutoff, "cutoff", "CUTOFF");
    setupKnob (reso, "reso", "RESO");
    setupKnob (envAmt, "envAmt", "ENV AMT");
    setupKnob (decay, "decay", "DECAY");
    setupKnob (sustain, "sustain", "SUSTAIN");
    setupKnob (drive, "drive", "DRIVE");
    setupKnob (glide, "glide", "GLIDE");

    // Duck + output
    setupKnob (duckDepth, "duckDepth", "DEPTH");
    setupKnob (duckRelease, "duckRelease", "RELEASE");
    setupKnob (gain, "gain", "OUTPUT");

    setSize (960, 620);
    startTimerHz (30);
}

RollingBassLabEditor::~RollingBassLabEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RollingBassLabEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Col::text);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    k.label.setColour (juce::Label::textColourId, Col::dim);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);

    if (paramId == "density" || paramId == "swing" || paramId == "sub" || paramId == "reso" || paramId == "envAmt"
        || paramId == "sustain" || paramId == "drive" || paramId == "duckDepth" || paramId == "gate")
    {
        k.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    }
    else if (paramId == "cutoff")
    {
        k.slider.textFromValueFunction = [] (double v) { return v >= 1000.0 ? juce::String (v / 1000.0, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; };
    }
    else if (paramId == "decay" || paramId == "glide" || paramId == "duckRelease")
    {
        k.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };
    }
    else if (paramId == "gain")
    {
        k.slider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    }
    else if (paramId == "previewBpm")
    {
        k.slider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)); };
    }
    k.slider.updateText();
}

void RollingBassLabEditor::setupCombo (juce::ComboBox& box, const juce::StringArray& items,
                                       std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att,
                                       const juce::String& paramId)
{
    box.addItemList (items, 1);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, paramId, box);
}

void RollingBassLabEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void RollingBassLabEditor::saveMidiFile()
{
    const auto temp = proc.writePatternMidiFile();
    chooser = std::make_unique<juce::FileChooser> ("Save bass pattern as MIDI",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                       .getChildFile (temp.getFileName()),
                                                   "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [temp] (const juce::FileChooser& fc)
                          {
                              auto dest = fc.getResult();
                              if (dest == juce::File())
                                  return;
                              if (! dest.hasFileExtension ("mid"))
                                  dest = dest.withFileExtension ("mid");
                              temp.copyFileTo (dest);
                          });
}

//==============================================================================
void RollingBassLabEditor::paint (juce::Graphics& g)
{
    g.fillAll (Col::bg);

    // Header
    g.setColour (Col::amber);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("ROLLING BASS LAB", 20, 12, 320, 30, juce::Justification::centredLeft);
    g.setColour (Col::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("TECH HOUSE BASS GENERATOR", 22, 38, 320, 14, juce::Justification::centredLeft);

    auto drawPanel = [&g] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (Col::panel);
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (Col::edge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Col::teal);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (title, r.getX() + 12, r.getY() + 6, 300, 14, juce::Justification::centredLeft);
    };

    drawPanel (patternPanel, "PATTERN");
    drawPanel (synthPanel, "BASS SYNTH");
    drawPanel (duckPanel, "KICK DUCK");
    drawPanel (outPanel, "OUTPUT");

    // Hint under the grid
    g.setColour (Col::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Click = step on/off   Shift-click = accent   Alt-click = slide   Wheel = pitch   Right-click = menu",
                patternPanel.getX() + 14, grid.getBottom() + 8, 600, 24, juce::Justification::centredLeft);

    // Meter
    auto meter = juce::Rectangle<int> (outPanel.getRight() - 30, outPanel.getY() + 30, 12, outPanel.getHeight() - 44);
    g.setColour (Col::stepOff);
    g.fillRoundedRectangle (meter.toFloat(), 3.0f);
    const float db = juce::Decibels::gainToDecibels (meterLevel, -60.0f);
    const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto fill = meter.toFloat().withTop (meter.getBottom() - norm * meter.getHeight());
    g.setColour (db > -1.0f ? juce::Colours::red : (db > -12.0f ? Col::amber : Col::teal));
    g.fillRoundedRectangle (fill, 3.0f);
}

void RollingBassLabEditor::resized()
{
    auto area = getLocalBounds().reduced (14);

    // Header
    auto header = area.removeFromTop (48);
    quietButton.setBounds (header.removeFromRight (150).reduced (0, 10));
    header.removeFromRight (10);
    presetBox.setBounds (header.removeFromRight (200).reduced (0, 10));
    area.removeFromTop (6);

    // Pattern panel
    patternPanel = area.removeFromTop (330);
    {
        auto p = patternPanel.reduced (12);
        p.removeFromTop (18);

        auto row = p.removeFromTop (96);
        auto comboCol = [&] (juce::ComboBox& box, juce::Label& label, int width)
        {
            auto c = row.removeFromLeft (width);
            label.setBounds (c.removeFromTop (16));
            box.setBounds (c.removeFromTop (28).reduced (2, 0));
            row.removeFromLeft (8);
        };
        comboCol (styleBox, styleLabel, 140);
        comboCol (keyBox, keyLabel, 70);

        for (auto* k : { &octave, &density, &swing, &gate })
        {
            placeKnob (*k, row.removeFromLeft (78));
            row.removeFromLeft (2);
        }

        row.removeFromLeft (10);
        auto buttons = row.removeFromLeft (150);
        generateButton.setBounds (buttons.removeFromTop (40).reduced (0, 2));
        buttons.removeFromTop (6);
        seqButton.setBounds (buttons.removeFromTop (28));

        row.removeFromLeft (10);
        auto previewCol = row.removeFromLeft (120);
        previewButton.setBounds (previewCol.removeFromTop (28));
        placeKnob (previewBpm, previewCol.withTrimmedTop (4));

        p.removeFromTop (8);
        auto bottom = p.removeFromBottom (34);
        saveMidiButton.setBounds (bottom.removeFromRight (110));
        bottom.removeFromRight (8);
        dragBox.setBounds (bottom.removeFromRight (200));

        grid.setBounds (p.withTrimmedBottom (30));
    }

    area.removeFromTop (10);

    // Bottom row
    outPanel = area.removeFromRight (150);
    area.removeFromRight (10);
    duckPanel = area.removeFromRight (190);
    area.removeFromRight (10);
    synthPanel = area;

    {
        auto s = synthPanel.reduced (12);
        s.removeFromTop (18);
        auto top = s.removeFromTop (s.getHeight() / 2);
        auto waveCol = top.removeFromLeft (110);
        waveLabel.setBounds (waveCol.removeFromTop (16));
        waveBox.setBounds (waveCol.removeFromTop (28).reduced (2, 0));
        const int kw = top.getWidth() / 4;
        for (auto* k : { &sub, &cutoff, &reso, &envAmt })
            placeKnob (*k, top.removeFromLeft (kw));

        s.removeFromLeft (110);
        const int kw2 = s.getWidth() / 4;
        for (auto* k : { &decay, &sustain, &drive, &glide })
            placeKnob (*k, s.removeFromLeft (kw2));
    }

    {
        auto d = duckPanel.reduced (12);
        d.removeFromTop (18);
        placeKnob (duckDepth, d.removeFromTop (d.getHeight() / 2));
        placeKnob (duckRelease, d);
    }

    {
        auto o = outPanel.reduced (12);
        o.removeFromTop (18);
        o.removeFromRight (26);
        placeKnob (gain, o.removeFromTop (juce::jmin (110, o.getHeight())));
    }
}

void RollingBassLabEditor::timerCallback()
{
    meterLevel = juce::jmax (proc.getOutputLevel(), meterLevel * 0.8f);
    grid.repaint();
    repaint (outPanel);
}

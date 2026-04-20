#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginEditor::PluginEditor(PluginProcessor& processor)
    // AudioProcessorEditor's constructor needs a pointer to the processor;
    // passing &processor registers this window with JUCE's component hierarchy.
    : AudioProcessorEditor(&processor), processor_(processor),
      // SampleTabPanel is constructed here (before the body of the constructor),
      // so it receives a valid processor reference from the start.
      sampleTabPanel_(processor),
      freezeBufferDisplay_(processor),
      teensyPanel_(processor.getAPVTS(), processor.getTeensyMenu())
{
    // Set the fixed window size (expanded to fit the Teensy UI emulation panel above).
    setSize(750, 800);

    // --- Title label ---
    addAndMakeVisible(titleLabel_);
    titleLabel_.setText("SAMPLER WITH FREEZE", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(20.0f));
    titleLabel_.setJustificationType(juce::Justification::centred);

    // --- Sample tab panel (left column) ---
    // Contains all per-pad UI: waveform, markers, load button, obey-note-off toggle.
    addAndMakeVisible(sampleTabPanel_);

    // --- Teensy UI emulation panel (above main section) ---
    addAndMakeVisible(teensyPanel_);

    // --- Freeze buffer waveform display ---
    addAndMakeVisible(freezeBufferDisplay_);

    // --- Freeze toggle button ---
    // ButtonAttachment wires the ToggleButton to the "freeze" AudioParameterBool.
    // When the user clicks, the attachment calls parameter->setValue(); when the
    // DAW sets the parameter via automation, it updates the button state.
    addAndMakeVisible(freezeButton_);
    freezeButton_.setButtonText("FREEZE");
    freezeAttachment_ = std::make_unique<ButtonAttachment>(
        processor_.getAPVTS(), "freeze", freezeButton_);

    // --- Stutter rate knob ---
    // Rotary slider with 7 discrete snap positions (0–6) mapping to kStutterValues[].
    addAndMakeVisible(stutterLabel_);
    stutterLabel_.setText("Stutter", juce::dontSendNotification);
    stutterLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(stutterKnob_);
    stutterKnob_.setSliderStyle(juce::Slider::Rotary);
    stutterKnob_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    stutterKnob_.textFromValueFunction = [](double v) -> juce::String {
        static const char* labels[] = { "1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
        return labels[juce::jlimit(0, 6, (int)v)];
    };
    stutterAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "stutterRate", stutterKnob_);

    // --- Speed knob ---
    addAndMakeVisible(speedLabel_);
    speedLabel_.setText("Speed", juce::dontSendNotification);
    speedLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(speedSlider_);
    speedSlider_.setSliderStyle(juce::Slider::Rotary);
    speedSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    speedAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "speed", speedSlider_);

    // --- Loop length knob ---
    addAndMakeVisible(loopLengthLabel_);
    loopLengthLabel_.setText("Loop Len", juce::dontSendNotification);
    loopLengthLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(loopLengthSlider_);
    loopLengthSlider_.setSliderStyle(juce::Slider::Rotary);
    loopLengthSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    loopLengthAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "loopLength", loopLengthSlider_);

    // --- Loop position knob ---
    addAndMakeVisible(loopPositionLabel_);
    loopPositionLabel_.setText("Loop Pos", juce::dontSendNotification);
    loopPositionLabel_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(loopPositionSlider_);
    loopPositionSlider_.setSliderStyle(juce::Slider::Rotary);
    loopPositionSlider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    loopPositionAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "loopPosition", loopPositionSlider_);

    // --- Parallel mode toggle ---
    // Switches the Mixer between Sequential (input+sampler -> freeze) and
    // Parallel (sampler only -> freeze; input bypasses) routing.
    addAndMakeVisible(parallelModeButton_);
    parallelModeButton_.setButtonText("Parallel Mode");
    parallelAttachment_ = std::make_unique<ButtonAttachment>(
        processor_.getAPVTS(), "parallelMode", parallelModeButton_);

    // --- Input level slider ---
    addAndMakeVisible(inputLevelLabel_);
    inputLevelLabel_.setText("Input Level", juce::dontSendNotification);
    addAndMakeVisible(inputLevelSlider_);
    inputLevelSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    inputLevelSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    inputLevelAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "inputLevel", inputLevelSlider_);

    // --- Sampler level slider ---
    addAndMakeVisible(samplerLevelLabel_);
    samplerLevelLabel_.setText("Sampler Level", juce::dontSendNotification);
    addAndMakeVisible(samplerLevelSlider_);
    samplerLevelSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    samplerLevelSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    samplerLevelAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "samplerLevel", samplerLevelSlider_);

    // --- Pad buttons ---
    // Each pad triggers on mouse-down and stops on mouse-up (if obeyNoteOff).
    // Also switches the sample tab panel to the corresponding tab.
    const char* padLabels[4] = { "PAD 1", "PAD 2", "PAD 3", "PAD 4" };
    for (int i = 0; i < 4; ++i)
    {
        addAndMakeVisible(padButtons_[i]);
        padButtons_[i].setButtonText(padLabels[i]);
        padButtons_[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a5a));

        // Trigger on press (not release) and switch tab.
        // Stop on release if obeyNoteOff is active for this pad.
        padButtons_[i].onStateChange = [this, i]
        {
            if (padButtons_[i].isDown())
            {
                processor_.triggerPad(i);
                sampleTabPanel_.selectTab(i);
            }
            else
            {
                processor_.stopPad(i);
            }
        };
    }
}

PluginEditor::~PluginEditor()
{
    // Attachments are destroyed first (unique_ptr LIFO order in declarations).
    // They deregister as parameter listeners in their destructors.
    // The Slider/Button members are destroyed after, which is the correct order.
}

void PluginEditor::paint(juce::Graphics& g)
{
    // Fill the whole window with a dark background colour.
    g.fillAll(juce::Colour(0xff2a2a2a));

    int titleH   = 40;   // Height of the title bar.
    int teensyH  = 130;  // Height of the Teensy UI emulation panel.
    int padsH    = 50;   // Height of the pad buttons section at the very bottom.
    int mixerH   = 90;   // Height of the mixer strip.
    int mainH    = getHeight() - titleH - teensyH - mixerH - padsH;  // Main middle section.
    int mainTop  = titleH + teensyH;  // Y where the main section starts.
    int leftW    = 400;  // Width of the SampleTabPanel column.

    // --- Section divider lines ---
    g.setColour(juce::Colour(0xff444444));
    g.drawLine(0, (float)titleH, (float)getWidth(), (float)titleH);                        // Below title.
    g.drawLine(0, (float)mainTop, (float)getWidth(), (float)mainTop);                      // Below teensy panel.
    g.drawLine((float)leftW, (float)mainTop, (float)leftW, (float)(mainTop + mainH));      // Between panels.
    g.drawLine(0, (float)(mainTop + mainH), (float)getWidth(), (float)(mainTop + mainH));  // Above mixer.
    g.drawLine(0, (float)(mainTop + mainH + mixerH), (float)getWidth(), (float)(mainTop + mainH + mixerH)); // Above pads.

    // --- Section header labels ---
    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText("FREEZE EFFECT", leftW + 10, mainTop + 4, 200, 16, juce::Justification::centredLeft);
    g.drawText("MIXER",         10,          mainTop + mainH + 4, 200, 16, juce::Justification::centredLeft);
    g.drawText("PADS",          10,          mainTop + mainH + mixerH + 4, 200, 16, juce::Justification::centredLeft);
}

void PluginEditor::resized()
{
    // All measurements match the layout diagram in the class comment.
    int titleH   = 40;
    int teensyH  = 130;
    int padsH    = 50;
    int mixerH   = 90;
    int mainH    = getHeight() - titleH - teensyH - mixerH - padsH;
    int mainTop  = titleH + teensyH;
    int leftW    = 400;
    int pad      = 10;
    int rowH     = 28;

    // --- Title ---
    titleLabel_.setBounds(0, 0, getWidth(), titleH);

    // --- Teensy UI emulation panel (between title and main section) ---
    teensyPanel_.setBounds(0, titleH, getWidth(), teensyH);

    // --- Sample tab panel (left column, full height of main section) ---
    sampleTabPanel_.setBounds(0, mainTop, leftW, mainH);

    // --- Freeze controls (right column) ---
    int fx     = leftW + pad;
    int fw     = getWidth() - leftW - pad * 2;
    int fy     = mainTop + 24;

    freezeButton_.setBounds(fx, fy, fw, rowH);
    fy += rowH + 8;

    // --- 2×2 knob grid: Stutter | Speed  (top row)
    //                       LoopLen | LoopPos (bottom row) ---
    // Each cell: centred label (labelH) above a square rotary knob (knobSz).
    // cellW = half the column width minus the inter-column gap.
    int gap    = 8;
    int labelH = 16;
    int knobSz = 80;   // Rotary bounds are square; text box is inside this height.
    int cellW  = (fw - gap) / 2;
    int col1X  = fx + cellW + gap;

    // Row 0 ---------------------------------------------------------------
    stutterLabel_.setBounds    (fx,    fy,          cellW, labelH);
    stutterKnob_.setBounds     (fx,    fy + labelH, cellW, knobSz);
    speedLabel_.setBounds      (col1X, fy,          cellW, labelH);
    speedSlider_.setBounds     (col1X, fy + labelH, cellW, knobSz);
    fy += labelH + knobSz + gap;

    // Row 1 ---------------------------------------------------------------
    loopLengthLabel_.setBounds (fx,    fy,          cellW, labelH);
    loopLengthSlider_.setBounds(fx,    fy + labelH, cellW, knobSz);
    loopPositionLabel_.setBounds (col1X, fy,          cellW, labelH);
    loopPositionSlider_.setBounds(col1X, fy + labelH, cellW, knobSz);
    fy += labelH + knobSz + gap;

    // --- Freeze buffer waveform display ---
    // Occupies the remaining space in the right column below the loop sliders.
    int displayH = (mainTop + mainH) - fy - pad;
    if (displayH > 0)
        freezeBufferDisplay_.setBounds(fx, fy, fw, displayH);

    // --- Mixer strip (bottom) ---
    // The mixer is positioned below the main section.
    int my    = mainTop + mainH + 22;  // A bit below the "MIXER" header text.
    int halfW = (getWidth() - pad * 3) / 2;  // Each mixer control pair gets half the width.

    parallelModeButton_.setBounds(pad, my, 150, rowH);
    my += rowH + 4;

    // Two level sliders side by side.
    inputLevelLabel_.setBounds(pad, my, 80, rowH);
    inputLevelSlider_.setBounds(pad + 80, my, halfW - 80, rowH);
    samplerLevelLabel_.setBounds(pad * 2 + halfW, my, 95, rowH);
    samplerLevelSlider_.setBounds(pad * 2 + halfW + 95, my, halfW - 95, rowH);

    // --- Pad buttons (bottom row) ---
    int padY    = mainTop + mainH + mixerH + 18;  // Below the "PADS" header text.
    int padBtnH = padsH - 24;                     // Button height (leave room for header).
    int totalPadW = getWidth() - pad * 2;         // Available width for all 4 pads.
    int padGap  = 8;                               // Gap between buttons.
    int padBtnW = (totalPadW - padGap * 3) / 4;   // Equal width per button.

    for (int i = 0; i < 4; ++i)
    {
        int px = pad + i * (padBtnW + padGap);
        padButtons_[i].setBounds(px, padY, padBtnW, padBtnH);
    }
}

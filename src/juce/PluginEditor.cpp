#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginEditor::PluginEditor(PluginProcessor& processor)
    // AudioProcessorEditor's constructor needs a pointer to the processor;
    // passing &processor registers this window with JUCE's component hierarchy.
    : AudioProcessorEditor(&processor), processor_(processor),
      // SampleTabPanel is constructed here (before the body of the constructor),
      // so it receives a valid processor reference from the start.
      sampleTabPanel_(processor)
{
    // Set the fixed window size. Users can't resize this window.
    setSize(750, 500);

    // --- Title label ---
    addAndMakeVisible(titleLabel_);
    titleLabel_.setText("SAMPLER WITH FREEZE", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(20.0f));
    titleLabel_.setJustificationType(juce::Justification::centred);

    // --- Sample tab panel (left column) ---
    // Contains all per-pad UI: waveform, markers, load button, obey-note-off toggle.
    addAndMakeVisible(sampleTabPanel_);

    // --- Freeze toggle button ---
    // ButtonAttachment wires the ToggleButton to the "freeze" AudioParameterBool.
    // When the user clicks, the attachment calls parameter->setValue(); when the
    // DAW sets the parameter via automation, it updates the button state.
    addAndMakeVisible(freezeButton_);
    freezeButton_.setButtonText("FREEZE");
    freezeAttachment_ = std::make_unique<ButtonAttachment>(
        processor_.getAPVTS(), "freeze", freezeButton_);

    // --- Stutter rate combo box ---
    // ComboBoxAttachment maps item indices to the "stutterRate" AudioParameterChoice.
    // The item list here must match the StringArray in createParameterLayout().
    addAndMakeVisible(stutterLabel_);
    stutterLabel_.setText("Stutter", juce::dontSendNotification);
    addAndMakeVisible(stutterRateBox_);
    stutterRateBox_.addItemList(
        {"1 beat", "1/2 beat", "1/4 beat", "1/8 beat", "1/16 beat", "1/32 beat", "1/64 beat"}, 1);
    stutterAttachment_ = std::make_unique<ComboBoxAttachment>(
        processor_.getAPVTS(), "stutterRate", stutterRateBox_);

    // --- Speed slider ---
    // SliderAttachment maps the slider position to the "speed" AudioParameterFloat.
    // The NormalisableRange in createParameterLayout defines how the slider value
    // maps to a physical BPM multiplier (0.25x to 4x with skew toward 1x).
    addAndMakeVisible(speedLabel_);
    speedLabel_.setText("Speed", juce::dontSendNotification);
    addAndMakeVisible(speedSlider_);
    speedSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    speedSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    speedAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "speed", speedSlider_);

    // --- Dry/wet slider ---
    addAndMakeVisible(dryWetLabel_);
    dryWetLabel_.setText("Dry/Wet", juce::dontSendNotification);
    addAndMakeVisible(dryWetSlider_);
    dryWetSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    dryWetSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    dryWetAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "dryWet", dryWetSlider_);

    // --- Loop start slider ---
    // Controls where in the freeze buffer the loop region begins.
    addAndMakeVisible(loopStartLabel_);
    loopStartLabel_.setText("Loop Start", juce::dontSendNotification);
    addAndMakeVisible(loopStartSlider_);
    loopStartSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    loopStartSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    loopStartAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "loopStart", loopStartSlider_);

    // --- Loop end slider ---
    addAndMakeVisible(loopEndLabel_);
    loopEndLabel_.setText("Loop End", juce::dontSendNotification);
    addAndMakeVisible(loopEndSlider_);
    loopEndSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    loopEndSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    loopEndAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "loopEnd", loopEndSlider_);

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

    int titleH = 40;   // Height of the title bar.
    int mixerH = 90;   // Height of the mixer strip at the bottom.
    int mainH  = getHeight() - titleH - mixerH;  // Height of the main middle section.
    int leftW  = 400;  // Width of the SampleTabPanel column.

    // --- Section divider lines ---
    g.setColour(juce::Colour(0xff444444));
    g.drawLine(0,      (float)titleH,          (float)getWidth(), (float)titleH);           // Below title.
    g.drawLine((float)leftW, (float)titleH,    (float)leftW,      (float)(titleH + mainH)); // Between panels.
    g.drawLine(0,      (float)(titleH + mainH),(float)getWidth(), (float)(titleH + mainH)); // Above mixer.

    // --- Section header labels (painted directly, not as JUCE Label components) ---
    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText("FREEZE EFFECT", leftW + 10, titleH + 4, 200, 16, juce::Justification::centredLeft);
    g.drawText("MIXER",         10,          titleH + mainH + 4, 200, 16, juce::Justification::centredLeft);
}

void PluginEditor::resized()
{
    // All measurements match the layout diagram in the class comment.
    int titleH = 40;
    int mixerH = 90;
    int mainH  = getHeight() - titleH - mixerH;
    int leftW  = 400;
    int pad    = 10;
    int rowH   = 28;

    // --- Title ---
    titleLabel_.setBounds(0, 0, getWidth(), titleH);

    // --- Sample tab panel (left column, full height of main section) ---
    sampleTabPanel_.setBounds(0, titleH, leftW, mainH);

    // --- Freeze controls (right column) ---
    // fx/fy: origin of the right-column content
    // fx+labelW: where the controls start (after the label)
    // sw: control width (right column minus label minus margins)
    int fx     = leftW + pad;
    int fw     = getWidth() - leftW - pad * 2;  // Total right column width.
    int labelW = 75;
    int sx     = fx + labelW;  // X where the slider/combo starts.
    int sw     = fw - labelW;  // Width of the slider/combo.
    int fy     = titleH + 24;  // A bit below the "FREEZE EFFECT" header text.

    freezeButton_.setBounds(fx, fy, fw, rowH);
    fy += rowH + 8;

    stutterLabel_.setBounds(fx, fy, labelW, rowH);
    stutterRateBox_.setBounds(sx, fy, sw, rowH);
    fy += rowH + 4;

    speedLabel_.setBounds(fx, fy, labelW, rowH);
    speedSlider_.setBounds(sx, fy, sw, rowH);
    fy += rowH + 4;

    dryWetLabel_.setBounds(fx, fy, labelW, rowH);
    dryWetSlider_.setBounds(sx, fy, sw, rowH);
    fy += rowH + 4;

    loopStartLabel_.setBounds(fx, fy, labelW, rowH);
    loopStartSlider_.setBounds(sx, fy, sw, rowH);
    fy += rowH + 4;

    loopEndLabel_.setBounds(fx, fy, labelW, rowH);
    loopEndSlider_.setBounds(sx, fy, sw, rowH);

    // --- Mixer strip (bottom) ---
    // The mixer is positioned below the main section.
    int my    = titleH + mainH + 22;  // A bit below the "MIXER" header text.
    int halfW = (getWidth() - pad * 3) / 2;  // Each mixer control pair gets half the width.

    parallelModeButton_.setBounds(pad, my, 150, rowH);
    my += rowH + 4;

    // Two level sliders side by side.
    inputLevelLabel_.setBounds(pad, my, 80, rowH);
    inputLevelSlider_.setBounds(pad + 80, my, halfW - 80, rowH);
    samplerLevelLabel_.setBounds(pad * 2 + halfW, my, 95, rowH);
    samplerLevelSlider_.setBounds(pad * 2 + halfW + 95, my, halfW - 95, rowH);
}

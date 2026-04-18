#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginEditor::PluginEditor(PluginProcessor& processor)
    : AudioProcessorEditor(&processor), processor_(processor),
      sampleTabPanel_(processor)
{
    setSize(750, 500);

    // ---- Title ----
    addAndMakeVisible(titleLabel_);
    titleLabel_.setText("SAMPLER WITH FREEZE", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(20.0f));
    titleLabel_.setJustificationType(juce::Justification::centred);

    // ---- Sample tab panel (left area) ----
    addAndMakeVisible(sampleTabPanel_);

    // ---- Freeze toggle ----
    addAndMakeVisible(freezeButton_);
    freezeButton_.setButtonText("FREEZE");
    freezeAttachment_ = std::make_unique<ButtonAttachment>(
        processor_.getAPVTS(), "freeze", freezeButton_);

    // ---- Stutter rate ----
    addAndMakeVisible(stutterLabel_);
    stutterLabel_.setText("Stutter", juce::dontSendNotification);
    addAndMakeVisible(stutterRateBox_);
    stutterRateBox_.addItemList(
        {"1 beat", "1/2 beat", "1/4 beat", "1/8 beat", "1/16 beat"}, 1);
    stutterAttachment_ = std::make_unique<ComboBoxAttachment>(
        processor_.getAPVTS(), "stutterRate", stutterRateBox_);

    // ---- Speed slider ----
    addAndMakeVisible(speedLabel_);
    speedLabel_.setText("Speed", juce::dontSendNotification);
    addAndMakeVisible(speedSlider_);
    speedSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    speedSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    speedAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "speed", speedSlider_);

    // ---- Dry/Wet slider ----
    addAndMakeVisible(dryWetLabel_);
    dryWetLabel_.setText("Dry/Wet", juce::dontSendNotification);
    addAndMakeVisible(dryWetSlider_);
    dryWetSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    dryWetSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    dryWetAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "dryWet", dryWetSlider_);

    // ---- Loop Start slider ----
    addAndMakeVisible(loopStartLabel_);
    loopStartLabel_.setText("Loop Start", juce::dontSendNotification);
    addAndMakeVisible(loopStartSlider_);
    loopStartSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    loopStartSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    loopStartAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "loopStart", loopStartSlider_);

    // ---- Loop End slider ----
    addAndMakeVisible(loopEndLabel_);
    loopEndLabel_.setText("Loop End", juce::dontSendNotification);
    addAndMakeVisible(loopEndSlider_);
    loopEndSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    loopEndSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    loopEndAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "loopEnd", loopEndSlider_);

    // ---- Parallel mode toggle ----
    addAndMakeVisible(parallelModeButton_);
    parallelModeButton_.setButtonText("Parallel Mode");
    parallelAttachment_ = std::make_unique<ButtonAttachment>(
        processor_.getAPVTS(), "parallelMode", parallelModeButton_);

    // ---- Input level slider ----
    addAndMakeVisible(inputLevelLabel_);
    inputLevelLabel_.setText("Input Level", juce::dontSendNotification);
    addAndMakeVisible(inputLevelSlider_);
    inputLevelSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    inputLevelSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    inputLevelAttachment_ = std::make_unique<SliderAttachment>(
        processor_.getAPVTS(), "inputLevel", inputLevelSlider_);

    // ---- Sampler level slider ----
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
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    int titleH = 40;
    int mixerH = 90;
    int mainH = getHeight() - titleH - mixerH;
    int leftW = 400;

    // Section divider lines
    g.setColour(juce::Colour(0xff444444));
    g.drawLine(0, (float)titleH, (float)getWidth(), (float)titleH);
    g.drawLine((float)leftW, (float)titleH, (float)leftW, (float)(titleH + mainH));
    g.drawLine(0, (float)(titleH + mainH), (float)getWidth(), (float)(titleH + mainH));

    // Section headers
    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText("FREEZE EFFECT", leftW + 10, titleH + 4, 200, 16, juce::Justification::centredLeft);
    g.drawText("MIXER", 10, titleH + mainH + 4, 200, 16, juce::Justification::centredLeft);
}

void PluginEditor::resized()
{
    int titleH = 40;
    int mixerH = 90;
    int mainH = getHeight() - titleH - mixerH;
    int leftW = 400;
    int pad = 10;
    int rowH = 28;

    // Title
    titleLabel_.setBounds(0, 0, getWidth(), titleH);

    // Sample tab panel (left area, full height of main section)
    sampleTabPanel_.setBounds(0, titleH, leftW, mainH);

    // Freeze controls (right column)
    int fx = leftW + pad;
    int fw = getWidth() - leftW - pad * 2;
    int labelW = 75;
    int sx = fx + labelW;
    int sw = fw - labelW;
    int fy = titleH + 24;

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

    // Mixer controls (bottom strip)
    int my = titleH + mainH + 22;
    int halfW = (getWidth() - pad * 3) / 2;

    parallelModeButton_.setBounds(pad, my, 150, rowH);
    my += rowH + 4;

    inputLevelLabel_.setBounds(pad, my, 80, rowH);
    inputLevelSlider_.setBounds(pad + 80, my, halfW - 80, rowH);
    samplerLevelLabel_.setBounds(pad * 2 + halfW, my, 95, rowH);
    samplerLevelSlider_.setBounds(pad * 2 + halfW + 95, my, halfW - 95, rowH);
}

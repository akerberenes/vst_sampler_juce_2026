#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginEditor::PluginEditor(PluginProcessor& processor)
    : AudioProcessorEditor(&processor), processor_(processor)
{
    setSize(400, 300);
    
    addAndMakeVisible(titleLabel_);
    titleLabel_.setText("Sampler with Freeze", juce::dontSendNotification);
    titleLabel_.setJustificationType(juce::Justification::centredTop);
    
    addAndMakeVisible(loadSample1Button_);
    loadSample1Button_.setButtonText("Load Sample 1");
    
    addAndMakeVisible(loadSample2Button_);
    loadSample2Button_.setButtonText("Load Sample 2");
    
    addAndMakeVisible(loadSample3Button_);
    loadSample3Button_.setButtonText("Load Sample 3");
    
    addAndMakeVisible(loadSample4Button_);
    loadSample4Button_.setButtonText("Load Sample 4");
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    
    titleLabel_.setBounds(area.removeFromTop(40).reduced(10));
    
    auto buttonArea = area.removeFromTop(100).reduced(10);
    loadSample1Button_.setBounds(buttonArea.removeFromTop(20));
    loadSample2Button_.setBounds(buttonArea.removeFromTop(20));
    loadSample3Button_.setBounds(buttonArea.removeFromTop(20));
    loadSample4Button_.setBounds(buttonArea.removeFromTop(20));
}

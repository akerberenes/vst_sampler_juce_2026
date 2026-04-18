#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class PluginProcessor;

/**
 * PluginEditor
 * 
 * Minimal UI for the sampler plugin (MVP).
 * Shows sample info and basic transport controls.
 */
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    PluginProcessor& processor_;
    
    juce::Label titleLabel_;
    juce::TextButton loadSample1Button_;
    juce::TextButton loadSample2Button_;
    juce::TextButton loadSample3Button_;
    juce::TextButton loadSample4Button_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

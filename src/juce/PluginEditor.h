#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SampleTabPanel.h"

class PluginProcessor;

/**
 * PluginEditor
 * 
 * Full UI for the sampler plugin.
 * Left: tabbed per-sample editor with waveform and draggable start/end markers.
 * Right: freeze effect controls.
 * Bottom: mixer strip.
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

    // Title
    juce::Label titleLabel_;

    // Tabbed sample editor (replaces old load buttons + labels)
    SampleTabPanel sampleTabPanel_;

    // Freeze controls
    juce::ToggleButton freezeButton_;
    juce::ComboBox stutterRateBox_;
    juce::Slider speedSlider_;
    juce::Slider dryWetSlider_;
    juce::Slider loopStartSlider_;
    juce::Slider loopEndSlider_;
    juce::Label stutterLabel_, speedLabel_, dryWetLabel_;
    juce::Label loopStartLabel_, loopEndLabel_;

    // Mixer controls
    juce::ToggleButton parallelModeButton_;
    juce::Slider inputLevelSlider_;
    juce::Slider samplerLevelSlider_;
    juce::Label inputLevelLabel_, samplerLevelLabel_;

    // Parameter attachments (auto-sync UI <-> DSP parameters)
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttachment> freezeAttachment_;
    std::unique_ptr<ComboBoxAttachment> stutterAttachment_;
    std::unique_ptr<SliderAttachment> speedAttachment_;
    std::unique_ptr<SliderAttachment> dryWetAttachment_;
    std::unique_ptr<SliderAttachment> loopStartAttachment_;
    std::unique_ptr<SliderAttachment> loopEndAttachment_;
    std::unique_ptr<ButtonAttachment> parallelAttachment_;
    std::unique_ptr<SliderAttachment> inputLevelAttachment_;
    std::unique_ptr<SliderAttachment> samplerLevelAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

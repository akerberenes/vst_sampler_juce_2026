#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WaveformDisplay.h"

class PluginProcessor;

/**
 * SampleTabPanel
 *
 * Tabbed panel with one tab per sample (Sample 1–4).
 * Each tab shows: waveform display with start/end markers,
 * "Obey Note Off" toggle, MIDI note label, load button.
 */
class SampleTabPanel : public juce::Component
{
public:
    explicit SampleTabPanel(PluginProcessor& processor);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Call after loading a sample to refresh that tab's waveform
    void sampleLoaded(int index);

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    PluginProcessor& processor_;
    int selectedTab_ = 0;

    // Tab buttons
    juce::TextButton tabButtons_[4];

    // Per-tab content (all 4 exist, only selectedTab_ is visible)
    struct TabContent
    {
        WaveformDisplay waveform;
        juce::ToggleButton obeyNoteOffButton;
        juce::TextButton loadButton;
        juce::Label midiLabel;
        juce::Label nameLabel;
        std::unique_ptr<ButtonAttachment> obeyAttachment;
    };
    TabContent tabs_[4];

    // File chooser (must persist during async operation)
    std::unique_ptr<juce::FileChooser> fileChooser_;

    void selectTab(int index);
    void loadButtonClicked(int index);
    void updateTabAppearance();
};

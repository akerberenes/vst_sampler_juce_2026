#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SampleTabPanel.h"
#include "FreezeBufferDisplay.h"
#include "TeensyEmulationPanel.h"

class PluginProcessor;

/**
 * PluginEditor
 *
 * The top-level GUI window for the plugin. JUCE instantiates this via
 * PluginProcessor::createEditor() and destroys it when the user closes
 * the plugin window.
 *
 * --- Layout (750 x 550) ---
 *
 *  +-------------------------------------------+
 *  |        SAMPLER WITH FREEZE  (title)        |  <- 40px
 *  +-----------------------+-------------------+
 *  |                       |  FREEZE EFFECT    |
 *  |  SampleTabPanel       |  [FREEZE]         |  <- 370px
 *  |  (tabs + waveforms)   |  Stutter  [....]  |
 *  |  400px wide           |  Speed    [....]  |
 *  |                       |  Loop Len  [....]  |
 *  |                       |  Loop Pos  [....]  |
 *  +-----------------------+-------------------+
 *  |  MIXER                                    |  <- 90px
 *  |  [Parallel Mode]                          |
 *  |  Input Level  [slider]  Sampler Level [...]|
 *  +-------------------------------------------+
 *  |  PADS   [1] [2] [3] [4]                   |  <- 50px
 *  +-------------------------------------------+
 *
 * --- Parameter attachments ---
 * Each UI control is connected to its APVTS parameter via an Attachment object.
 * The attachment registers as a parameter listener and automatically:
 *   - Updates the UI when the parameter changes (e.g. DAW automation).
 *   - Updates the parameter when the user moves the control.
 * This two-way sync is why we use attachments instead of manual getValue/setValue calls.
 *
 * Attachment types used:
 *   SliderAttachment   -- connects a Slider to a float/int parameter.
 *   ButtonAttachment   -- connects a ToggleButton to a bool parameter.
 *   ComboBoxAttachment -- connects a ComboBox to a choice parameter.
 *
 * The per-sample "obeyNoteOff" buttons and waveform markers live in SampleTabPanel,
 * which manages its own per-tab ButtonAttachments internally.
 */
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor& processor);
    ~PluginEditor() override;

    // --- JUCE Component overrides ---

    // Draws the background, section divider lines, and section header text.
    void paint(juce::Graphics&) override;

    // Positions all child components according to the fixed layout.
    void resized() override;

private:
    // Reference to the processor (needed for getAPVTS()).
    PluginProcessor& processor_;

    // --- Title bar ---
    juce::Label titleLabel_;

    // --- Left section: per-sample tab editor ---
    // Contains 4 tabs (one per pad) with waveform display, load button,
    // MIDI label, name label, and obey-note-off toggle.
    SampleTabPanel sampleTabPanel_;

    // --- Right section: freeze controls ---
    juce::ToggleButton  freezeButton_;
    juce::Slider        stutterKnob_;  // Rotary, 7 discrete snap positions (1–1/64 beat).
    juce::Slider        speedSlider_;
    juce::Slider        loopLengthSlider_;
    juce::Slider        loopPositionSlider_;
    juce::Label         stutterLabel_, speedLabel_;
    juce::Label         loopLengthLabel_, loopPositionLabel_;
    FreezeBufferDisplay freezeBufferDisplay_;

    // --- Bottom section: mixer strip ---
    juce::ToggleButton parallelModeButton_;
    juce::Slider       inputLevelSlider_;
    juce::Slider       samplerLevelSlider_;
    juce::Label        inputLevelLabel_, samplerLevelLabel_;

    // --- Pad buttons ---
    juce::TextButton padButtons_[4];

    // --- Teensy UI emulation panel ---
    TeensyEmulationPanel teensyPanel_;

    // --- Parameter attachments ---
    // Declared as unique_ptr so they are destroyed before the Slider/Button members
    // (attachments deregister as listeners in their destructor; the control must
    // still exist at that point).
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ButtonAttachment>   freezeAttachment_;
    std::unique_ptr<SliderAttachment>   stutterAttachment_;
    std::unique_ptr<SliderAttachment>   speedAttachment_;
    std::unique_ptr<SliderAttachment>   loopLengthAttachment_;
    std::unique_ptr<SliderAttachment>   loopPositionAttachment_;
    std::unique_ptr<ButtonAttachment>   parallelAttachment_;
    std::unique_ptr<SliderAttachment>   inputLevelAttachment_;
    std::unique_ptr<SliderAttachment>   samplerLevelAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

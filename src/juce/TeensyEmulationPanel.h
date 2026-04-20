#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "TeensyMenu.h"

/**
 * TeensyEmulationPanel
 *
 * JUCE component that visually emulates the Teensy hardware UI:
 * - A 2×16 character LCD display (4 zones, monospace green-on-black).
 * - 3 rotary knobs: Page, Param, Value.
 * - 1 push button: Action.
 *
 * The knobs are connected to APVTS parameters for DAW automation.
 * On each timer tick, the display polls the TeensyMenu for updated text.
 */
class TeensyEmulationPanel : public juce::Component, private juce::Timer
{
public:
    TeensyEmulationPanel(juce::AudioProcessorValueTreeState& apvts, TeensyMenu& menu);
    ~TeensyEmulationPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    TeensyMenu& menu_;

    // LCD display text (cached from TeensyMenu).
    std::string lcdRow0_;
    std::string lcdRow1_;
    // Cached state for conditional rendering.
    TeensyMenu::Page cachedPage_ = TeensyMenu::Page::Sample1;
    int cachedPresetFunction_ = 0;  // 0 = save, 1 = reload

    // --- Controls ---
    juce::Slider pageKnob_;
    juce::Slider paramKnob_;
    juce::Slider valueKnob_;
    juce::TextButton actionButton_;

    juce::Label pageLabel_;
    juce::Label paramLabel_;
    juce::Label valueLabel_;

    // --- APVTS attachments ---
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> pageAttachment_;
    std::unique_ptr<SliderAttachment> paramAttachment_;
    std::unique_ptr<SliderAttachment> valueAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TeensyEmulationPanel)
};

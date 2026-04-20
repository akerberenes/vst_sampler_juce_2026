#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "WaveformDisplay.h"

class PluginProcessor;

/**
 * SampleTabPanel
 *
 * A 4-tab panel — one tab per sample pad.
 *
 * --- Layout ---
 * Tab buttons run across the top. Clicking a button calls selectTab(),
 * which shows the selected tab's content and hides the other three.
 * This saves screen space: all 4 pads share the same pixel region.
 *
 * Each tab contains:
 *   - A WaveformDisplay with draggable start/end markers.
 *   - A "Load Audio File..." button to open a file chooser.
 *   - A MIDI note label (C1/C2/C3/C4) so the user knows which key triggers the pad.
 *   - A sample name label showing the loaded filename.
 *   - An "Obey Note Off" toggle (ButtonAttachment → APVTS).
 *
 * --- APVTS integration ---
 * Each tab's obeyNoteOff button is connected to the APVTS via a ButtonAttachment.
 * Waveform marker drags fire the `onRegionChanged` callback, which calls
 * `parameter->setValueNotifyingHost()` to update the APVTS atomically.
 * processBlock reads these values each block via getRawParameterValue.
 *
 * --- File loading ---
 * loadButtonClicked uses a JUCE FileChooser. The chooser must remain alive
 * for the duration of the async operation, so fileChooser_ is a member
 * (not a local variable, which would be destroyed immediately after launch).
 *
 * --- addChildComponent vs addAndMakeVisible ---
 * All per-tab components are added with addChildComponent (hidden by default).
 * selectTab() makes exactly one tab's components visible at a time.
 * Tab header buttons use addAndMakeVisible because they're always shown.
 */
class SampleTabPanel : public juce::Component
{
public:
    explicit SampleTabPanel(PluginProcessor& processor);

    // --- JUCE Component overrides ---

    // Draws the panel background and a border around the content area.
    void paint(juce::Graphics& g) override;

    // Positions tab buttons and the currently visible tab's content.
    void resized() override;

    // --- Public interface ---

    // Called by PluginEditor after processor_.loadSample() returns.
    // Updates the waveform display, resets markers, and pushes 0/1 to APVTS.
    void sampleLoaded(int index);

    // Switch to the tab for pad `index` (0-3). Public so PluginEditor can
    // drive tab selection from the pad buttons.
    void selectTab(int index);

private:
    // Type aliases for verbose APVTS attachment types.
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    // Reference to the processor for getAPVTS(), loadSample(), getSampleWaveform().
    PluginProcessor& processor_;

    // Index of the currently visible tab (0-3).
    int selectedTab_ = 0;

    // The four tab header buttons (always visible).
    juce::TextButton tabButtons_[4];

    // Per-tab content bundle.
    // All 4 structs exist in memory; only one is visible at a time.
    struct TabContent
    {
        WaveformDisplay waveform;               // Draws the waveform + drag markers.
        juce::ToggleButton obeyNoteOffButton;   // Toggle: stop on MIDI note-off?
        juce::TextButton   loadButton;          // Opens the file chooser.
        juce::Label        midiLabel;           // Shows "MIDI: C1" etc.
        juce::Label        nameLabel;           // Shows the loaded filename.
        // Volume slider: controls the per-pad output gain sent to the DSP layer.
        // Range 0.0 (silence) → 1.0 (unity) → 2.0 (+6 dB boost).
        juce::Label  volumeLabel;
        juce::Slider volumeSlider;
        // Loop position / length sliders (replace draggable waveform markers).
        // loopPosSlider: start of the active region (0.0 = beginning).
        // loopLenSlider: length of the active region (1.0 = full sample).
        juce::Label  loopPosLabel;
        juce::Slider loopPosSlider;
        juce::Label  loopLenLabel;
        juce::Slider loopLenSlider;
        std::unique_ptr<ButtonAttachment> obeyAttachment;    // Connects button to APVTS.
        std::unique_ptr<SliderAttachment> gainAttachment;    // Connects slider to APVTS sampleGain.
        std::unique_ptr<SliderAttachment> loopPosAttachment; // Connects to APVTS sampleLoopPosN.
        std::unique_ptr<SliderAttachment> loopLenAttachment; // Connects to APVTS sampleLoopLenN.
    };
    TabContent tabs_[4];

    // FileChooser must persist for the lifetime of the async callback.
    // Declared as a member so it stays alive after loadButtonClicked() returns.
    std::unique_ptr<juce::FileChooser> fileChooser_;

    // Opens an async file chooser and loads the selected file onto pad `index`.
    void loadButtonClicked(int index);

    // Sets the visual "selected" / "unselected" colours on all tab buttons.
    void updateTabAppearance();
};

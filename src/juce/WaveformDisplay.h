#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/**
 * WaveformDisplay
 *
 * A JUCE Component that draws an audio waveform and lets the user drag
 * start/end region markers.
 *
 * --- How it draws the waveform ---
 * Audio files can contain millions of samples, but the component is only
 * a few hundred pixels wide. Drawing every sample individually would be
 * slow and produce the same visual as one bar per pixel anyway.
 * Instead, rebuildPeaks() downsamples to ~400 peak values: it splits the
 * sample data into 400 equal-sized chunks and stores the max absolute
 * amplitude per chunk. paint() draws one vertical bar per peak.
 *
 * --- Start and end markers ---
 * Two vertical lines divide the waveform into three visual zones:
 *   - Before start marker: dimmed (will not play)
 *   - Between markers:     bright cyan (active playback region)
 *   - After end marker:    dimmed (will not play)
 *
 * The start marker has a small handle at the top (green); the end marker
 * has a handle at the bottom (red) to help distinguish them.
 *
 * --- Mouse interaction ---
 * mouseDown identifies which marker (if any) is within kGrabRadius pixels
 * of the click. mouseDrag updates that marker, enforcing a 1% minimum gap.
 * mouseUp clears the drag state.
 *
 * --- Callback ---
 * onRegionChanged fires every drag update. SampleTabPanel sets this to
 * push the new fractions into the APVTS, keeping DSP and display in sync.
 */
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();

    // --- Data ---

    // Load a new set of samples. Triggers rebuildPeaks() and repaints.
    void setWaveform(const std::vector<float>& samples);

    // Programmatically move the start/end markers (e.g. when restoring state).
    // Both are clamped to [0.0, 1.0]. Triggers a repaint.
    void setStartFraction(float f);
    void setEndFraction(float f);

    float getStartFraction() const { return startFraction_; }
    float getEndFraction()   const { return endFraction_; }

    // --- Callback ---

    // Called after every drag update with the new (start, end) fractions.
    // Set this in SampleTabPanel to push values to the APVTS.
    std::function<void(float start, float end)> onRegionChanged;

    // --- JUCE Component overrides ---

    void paint(juce::Graphics& g) override;           // Draws waveform + markers.
    void mouseDown(const juce::MouseEvent& e) override; // Records which marker was grabbed.
    void mouseDrag(const juce::MouseEvent& e) override; // Moves grabbed marker + fires callback.
    void mouseUp(const juce::MouseEvent& e) override;   // Releases drag target.

private:
    // Downsampled peak data (one float per display column).
    std::vector<float> waveformPeaks_;

    // Marker positions as fractions of component width (0.0-1.0).
    float startFraction_ = 0.0f;
    float endFraction_   = 1.0f;

    // Which marker is currently being dragged.
    enum class DragTarget { None, Start, End };
    DragTarget dragTarget_ = DragTarget::None;

    // Click must be within this many pixels of a marker to grab it.
    static constexpr int kGrabRadius = 6;

    // Downsample `samples` to ~400 peaks and store in waveformPeaks_.
    void rebuildPeaks(const std::vector<float>& samples);

    // Convert fraction [0,1] to pixel x.
    float fractionToX(float f) const;

    // Convert pixel x to fraction [0,1] (clamped).
    float xToFraction(float x) const;
};

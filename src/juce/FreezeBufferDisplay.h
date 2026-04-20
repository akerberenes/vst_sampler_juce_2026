#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class PluginProcessor;

/**
 * FreezeBufferDisplay
 *
 * A read-only waveform visualisation of the freeze ring buffer.
 * Automatically polls the buffer ~20 times per second via a juce::Timer and
 * redraws whenever new data arrives.
 *
 * The loop region (derived from loopLength + loopPosition) is shown as:
 *   - a bright green vertical bar at the loop start
 *   - a red vertical bar at the loop end
 *
 * This mirrors the visual language used by WaveformDisplay for per-sample loops.
 * The markers are READ-ONLY; the user moves them via the loopLength / loopPosition
 * sliders, not by dragging.
 */
class FreezeBufferDisplay : public juce::Component,
                            public juce::Timer
{
public:
    explicit FreezeBufferDisplay(PluginProcessor& proc);
    ~FreezeBufferDisplay() override;

    // Call from PluginEditor whenever the loop parameters change so the markers
    // update even if the waveform itself hasn't changed.
    void setLoopParams(float loopLength, float loopPosition);

    // --- juce::Component ---
    void paint(juce::Graphics& g) override;

    // --- juce::Timer ---
    void timerCallback() override;

private:
    PluginProcessor& processor_;

    // Downsampled peak pairs: alternating min/max per display column.
    std::vector<float> peaks_;       // size = 2 * numColumns
    int numColumns_ = 0;             // set in paint() from component width

    // Loop region fractions in [0, 1].
    // startFraction_ = clamp(loopPosition_, 0, 1 - loopLength_)
    // endFraction_   = startFraction_ + loopLength_
    float loopLength_   = 1.0f;
    float loopPosition_ = 0.0f;

    // Scratch buffer to avoid per-callback heap allocation.
    std::vector<float> snapBuffer_;

    // Re-compute peaks_ from snapBuffer_ for the current component width.
    void rebuildPeaks();

    // Map a [0,1] fraction to a pixel x-coordinate.
    float fractionToX(float f) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreezeBufferDisplay)
};

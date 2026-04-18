#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/**
 * WaveformDisplay
 *
 * Shows an audio waveform with draggable start/end markers.
 * The region between start and end is highlighted; outside is dimmed.
 * Markers are vertical lines that can be dragged horizontally.
 */
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();

    void setWaveform(const std::vector<float>& samples);
    void setStartFraction(float f);
    void setEndFraction(float f);
    float getStartFraction() const { return startFraction_; }
    float getEndFraction() const { return endFraction_; }

    // Callback when user drags a marker
    std::function<void(float start, float end)> onRegionChanged;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    std::vector<float> waveformPeaks_;  // Downsampled peaks for drawing
    float startFraction_ = 0.0f;
    float endFraction_ = 1.0f;

    enum class DragTarget { None, Start, End };
    DragTarget dragTarget_ = DragTarget::None;

    static constexpr int kGrabRadius = 6;

    void rebuildPeaks(const std::vector<float>& samples);
    float fractionToX(float f) const;
    float xToFraction(float x) const;
};

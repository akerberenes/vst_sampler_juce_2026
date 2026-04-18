#include "WaveformDisplay.h"
#include <algorithm>
#include <cmath>

WaveformDisplay::WaveformDisplay()
{
    // No setup needed; JUCE Component default constructor handles everything.
}

void WaveformDisplay::setWaveform(const std::vector<float>& samples)
{
    // Downsample to display peaks and request a repaint.
    rebuildPeaks(samples);
    repaint();
}

void WaveformDisplay::setStartFraction(float f)
{
    startFraction_ = std::clamp(f, 0.0f, 1.0f);
    repaint();
}

void WaveformDisplay::setEndFraction(float f)
{
    endFraction_ = std::clamp(f, 0.0f, 1.0f);
    repaint();
}

void WaveformDisplay::rebuildPeaks(const std::vector<float>& samples)
{
    // --- Peak downsampling ---
    // Target: ~400 bars (one per column of a typical ~400px-wide component).
    // Algorithm: split the full sample data into `targetPeaks` equal-sized chunks;
    // for each chunk, store the maximum absolute amplitude (the "peak").
    // This is the standard approach used in audio editors (Audacity, DAW clip views, etc.)
    constexpr int targetPeaks = 400;
    waveformPeaks_.clear();

    if (samples.empty())
        return;

    // samplesPerPeak = how many source samples map to one display bar.
    // std::max guards against a zero value for very short samples.
    int samplesPerPeak = std::max(1, (int)samples.size() / targetPeaks);
    waveformPeaks_.reserve(targetPeaks + 1);

    for (size_t i = 0; i < samples.size(); i += samplesPerPeak)
    {
        float peak = 0.0f;
        int end = std::min((int)(i + samplesPerPeak), (int)samples.size());
        // Find the max absolute value in this chunk.
        for (int j = (int)i; j < end; ++j)
            peak = std::max(peak, std::abs(samples[j]));
        waveformPeaks_.push_back(peak);
    }
}

float WaveformDisplay::fractionToX(float f) const
{
    // Map fraction [0,1] to pixel x [0, component width].
    return f * (float)getWidth();
}

float WaveformDisplay::xToFraction(float x) const
{
    // Map pixel x to fraction [0,1]; clamp so dragging past the edges is safe.
    return std::clamp(x / (float)getWidth(), 0.0f, 1.0f);
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float w    = bounds.getWidth();
    float h    = bounds.getHeight();
    float midY = h * 0.5f;   // Vertical centre (zero-crossing line).

    // --- Background ---
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(bounds);

    // Placeholder when no sample is loaded yet.
    if (waveformPeaks_.empty())
    {
        g.setColour(juce::Colours::grey);
        g.drawText("No sample loaded", bounds, juce::Justification::centred);
        return;
    }

    float startX = fractionToX(startFraction_);
    float endX   = fractionToX(endFraction_);

    // --- Dim regions outside the active playback window ---
    // A semi-transparent black overlay darkens bars outside the selection.
    g.setColour(juce::Colour(0x40000000));
    g.fillRect(0.0f, 0.0f, startX, h);       // Before start marker.
    g.fillRect(endX,  0.0f, w - endX, h);    // After end marker.

    // --- Draw waveform bars ---
    int   numPeaks  = (int)waveformPeaks_.size();
    float peakWidth = w / (float)numPeaks;   // Width of one bar in pixels.

    for (int i = 0; i < numPeaks; ++i)
    {
        float x     = (float)i * peakWidth;
        // Scale peak [0,1] to half the component height, with a 10% top margin.
        float peakH = waveformPeaks_[i] * midY * 0.9f;

        // Bright cyan inside the selection; dark teal outside.
        bool inRegion = (x >= startX && x <= endX);
        g.setColour(inRegion ? juce::Colour(0xff4fc3f7) : juce::Colour(0xff2a5060));

        // Draw bar symmetrically around the centre line (top half + bottom half).
        g.fillRect(x, midY - peakH, peakWidth * 0.8f, peakH * 2.0f);
    }

    // --- Centre axis (zero-crossing line) ---
    g.setColour(juce::Colour(0x40ffffff));   // 25% white.
    g.drawHorizontalLine((int)midY, 0.0f, w);

    // --- Start marker (green) ---
    // Vertical line + small handle at the top for easy grabbing.
    g.setColour(juce::Colours::lime);
    g.drawVerticalLine((int)startX, 0.0f, h);
    g.fillRect(startX - 3.0f, 0.0f, 6.0f, 10.0f);

    // --- End marker (red) ---
    // Vertical line + small handle at the bottom (bottom placement distinguishes it from start).
    g.setColour(juce::Colours::red);
    g.drawVerticalLine((int)endX, 0.0f, h);
    g.fillRect(endX - 3.0f, h - 10.0f, 6.0f, 10.0f);
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
    float mx     = (float)e.x;
    float startX = fractionToX(startFraction_);
    float endX   = fractionToX(endFraction_);

    // Determine which marker (if any) was clicked.
    // Start marker takes priority if both are within grab radius (e.g. at same position).
    if (std::abs(mx - startX) < kGrabRadius)
        dragTarget_ = DragTarget::Start;
    else if (std::abs(mx - endX) < kGrabRadius)
        dragTarget_ = DragTarget::End;
    else
        dragTarget_ = DragTarget::None;  // Click in empty space: no drag.
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragTarget_ == DragTarget::None)
        return;

    float f = xToFraction((float)e.x);

    if (dragTarget_ == DragTarget::Start)
    {
        // Keep start at least 1% below end to avoid a zero-length region.
        startFraction_ = std::max(0.0f, std::min(f, endFraction_ - 0.01f));
    }
    else
    {
        // Keep end at least 1% above start.
        endFraction_ = std::min(1.0f, std::max(f, startFraction_ + 0.01f));
    }

    repaint();

    // Fire the callback so SampleTabPanel can push the values to APVTS.
    if (onRegionChanged)
        onRegionChanged(startFraction_, endFraction_);
}

void WaveformDisplay::mouseUp(const juce::MouseEvent&)
{
    // Release the drag target; the marker stays at its last position.
    dragTarget_ = DragTarget::None;
}

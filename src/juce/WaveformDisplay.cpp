#include "WaveformDisplay.h"
#include <algorithm>
#include <cmath>

WaveformDisplay::WaveformDisplay()
{
}

void WaveformDisplay::setWaveform(const std::vector<float>& samples)
{
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
    // Downsample to ~400 peaks for efficient drawing
    constexpr int targetPeaks = 400;
    waveformPeaks_.clear();

    if (samples.empty())
        return;

    int samplesPerPeak = std::max(1, (int)samples.size() / targetPeaks);
    waveformPeaks_.reserve(targetPeaks + 1);

    for (size_t i = 0; i < samples.size(); i += samplesPerPeak)
    {
        float peak = 0.0f;
        int end = std::min((int)(i + samplesPerPeak), (int)samples.size());
        for (int j = (int)i; j < end; ++j)
            peak = std::max(peak, std::abs(samples[j]));
        waveformPeaks_.push_back(peak);
    }
}

float WaveformDisplay::fractionToX(float f) const
{
    return f * (float)getWidth();
}

float WaveformDisplay::xToFraction(float x) const
{
    return std::clamp(x / (float)getWidth(), 0.0f, 1.0f);
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float w = bounds.getWidth();
    float h = bounds.getHeight();
    float midY = h * 0.5f;

    // Background
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(bounds);

    if (waveformPeaks_.empty())
    {
        g.setColour(juce::Colours::grey);
        g.drawText("No sample loaded", bounds, juce::Justification::centred);
        return;
    }

    float startX = fractionToX(startFraction_);
    float endX = fractionToX(endFraction_);

    // Draw dimmed regions (outside selection)
    g.setColour(juce::Colour(0x40000000));
    g.fillRect(0.0f, 0.0f, startX, h);
    g.fillRect(endX, 0.0f, w - endX, h);

    // Draw waveform
    int numPeaks = (int)waveformPeaks_.size();
    float peakWidth = w / (float)numPeaks;

    for (int i = 0; i < numPeaks; ++i)
    {
        float x = (float)i * peakWidth;
        float peakH = waveformPeaks_[i] * midY * 0.9f;

        // Color: bright in selection, dim outside
        bool inRegion = (x >= startX && x <= endX);
        g.setColour(inRegion ? juce::Colour(0xff4fc3f7) : juce::Colour(0xff2a5060));

        g.fillRect(x, midY - peakH, peakWidth * 0.8f, peakH * 2.0f);
    }

    // Draw center line
    g.setColour(juce::Colour(0x40ffffff));
    g.drawHorizontalLine((int)midY, 0.0f, w);

    // Draw start marker (green line)
    g.setColour(juce::Colours::lime);
    g.drawVerticalLine((int)startX, 0.0f, h);
    g.fillRect(startX - 3.0f, 0.0f, 6.0f, 10.0f);  // Handle at top

    // Draw end marker (red line)
    g.setColour(juce::Colours::red);
    g.drawVerticalLine((int)endX, 0.0f, h);
    g.fillRect(endX - 3.0f, h - 10.0f, 6.0f, 10.0f);  // Handle at bottom
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
    float mx = (float)e.x;
    float startX = fractionToX(startFraction_);
    float endX = fractionToX(endFraction_);

    // Check which marker is closer
    if (std::abs(mx - startX) < kGrabRadius)
        dragTarget_ = DragTarget::Start;
    else if (std::abs(mx - endX) < kGrabRadius)
        dragTarget_ = DragTarget::End;
    else
        dragTarget_ = DragTarget::None;
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragTarget_ == DragTarget::None)
        return;

    float f = xToFraction((float)e.x);

    if (dragTarget_ == DragTarget::Start)
    {
        startFraction_ = std::min(f, endFraction_ - 0.01f);
        startFraction_ = std::max(startFraction_, 0.0f);
    }
    else
    {
        endFraction_ = std::max(f, startFraction_ + 0.01f);
        endFraction_ = std::min(endFraction_, 1.0f);
    }

    repaint();

    if (onRegionChanged)
        onRegionChanged(startFraction_, endFraction_);
}

void WaveformDisplay::mouseUp(const juce::MouseEvent&)
{
    dragTarget_ = DragTarget::None;
}

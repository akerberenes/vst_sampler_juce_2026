#include "FreezeBufferDisplay.h"
#include "PluginProcessor.h"
#include <algorithm>
#include <cmath>

FreezeBufferDisplay::FreezeBufferDisplay(PluginProcessor& proc)
    : processor_(proc)
{
    startTimerHz(20);   // ~20 fps — responsive without thrashing the UI thread.
}

FreezeBufferDisplay::~FreezeBufferDisplay()
{
    stopTimer();
}

void FreezeBufferDisplay::setLoopParams(float loopLength, float loopPosition)
{
    loopLength_   = loopLength;
    loopPosition_ = loopPosition;
    repaint();
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------

void FreezeBufferDisplay::timerCallback()
{
    // Pull a fresh snapshot of the ring buffer from the processor.
    processor_.getFreezeBufferSnapshot(snapBuffer_);

    // Also read current loop parameters directly from the APVTS so the marker
    // positions stay in sync without needing explicit calls from the editor.
    auto& apvts = processor_.getAPVTS();
    loopLength_   = *apvts.getRawParameterValue("loopLength");
    loopPosition_ = *apvts.getRawParameterValue("loopPosition");

    // Recompute peaks for the current component width.
    rebuildPeaks();

    repaint();
}

// ---------------------------------------------------------------------------
// Peak building
// ---------------------------------------------------------------------------

void FreezeBufferDisplay::rebuildPeaks()
{
    numColumns_ = std::max(1, getWidth());
    peaks_.clear();

    if (snapBuffer_.empty())
        return;

    // For each display column, store the max absolute amplitude of the
    // corresponding audio chunk.  This is the same peak-downsampling strategy
    // used by WaveformDisplay.
    peaks_.resize(numColumns_, 0.0f);
    int totalSamples = (int)snapBuffer_.size();

    for (int col = 0; col < numColumns_; ++col)
    {
        int sampleStart = (int)((float)col       / (float)numColumns_ * (float)totalSamples);
        int sampleEnd   = (int)((float)(col + 1) / (float)numColumns_ * (float)totalSamples);
        sampleEnd = std::min(sampleEnd, totalSamples);

        float peak = 0.0f;
        for (int s = sampleStart; s < sampleEnd; ++s)
            peak = std::max(peak, std::abs(snapBuffer_[s]));
        peaks_[col] = peak;
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float FreezeBufferDisplay::fractionToX(float f) const
{
    return f * (float)getWidth();
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------

void FreezeBufferDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float w    = bounds.getWidth();
    float h    = bounds.getHeight();
    float midY = h * 0.5f;

    // --- Background ---
    g.setColour(juce::Colour(0xff1a1a1a));
    g.fillRect(bounds);

    // Thin border to make the component visually distinct.
    g.setColour(juce::Colour(0xff404040));
    g.drawRect(bounds, 1.0f);

    // --- Empty state ---
    if (peaks_.empty() || numColumns_ == 0)
    {
        g.setColour(juce::Colours::dimgrey);
        g.drawText("Freeze buffer empty", bounds, juce::Justification::centred, false);
        return;
    }

    // Compute the actual start/end fractions from the loopLength / loopPosition.
    // This mirrors the clamping in FreezeEffect::applyLoopBounds().
    float startFrac = std::clamp(loopPosition_, 0.0f, 1.0f - loopLength_);
    float endFrac   = startFrac + loopLength_;

    float startX = fractionToX(startFrac);
    float endX   = fractionToX(endFrac);

    // --- Dim regions outside the loop window ---
    g.setColour(juce::Colour(0x50000000));
    g.fillRect(0.0f,  0.0f, startX,     h);
    g.fillRect(endX,  0.0f, w - endX,   h);

    // --- Waveform bars ---
    float barW = w / (float)numColumns_;
    for (int col = 0; col < numColumns_; ++col)
    {
        float x     = (float)col * barW;
        float peakH = peaks_[col] * midY * 0.9f;

        bool inRegion = (x >= startX && x < endX);
        g.setColour(inRegion ? juce::Colour(0xff4fc3f7)    // bright cyan inside loop
                             : juce::Colour(0xff2a5060));  // dark teal outside loop

        g.fillRect(x, midY - peakH, barW * 0.8f, peakH * 2.0f);
    }

    // --- Centre axis ---
    g.setColour(juce::Colour(0x40ffffff));
    g.drawHorizontalLine((int)midY, 0.0f, w);

    // --- Loop start marker (green) ---
    g.setColour(juce::Colours::lime);
    g.drawVerticalLine((int)startX, 0.0f, h);
    g.fillRect(startX - 3.0f, 0.0f, 6.0f, 10.0f);   // small top handle

    // --- Loop end marker (red) ---
    g.setColour(juce::Colours::red);
    g.drawVerticalLine((int)endX, 0.0f, h);
    g.fillRect(endX - 3.0f, h - 10.0f, 6.0f, 10.0f);  // small bottom handle
}

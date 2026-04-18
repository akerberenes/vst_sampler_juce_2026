#pragma once

#include "Sampler.h"
#include <array>

/**
 * SamplerBank
 * 
 * Manages 4 independent Sampler instances and mixes their output.
 * 
 * No JUCE dependencies - pure C++
 */
class SamplerBank
{
public:
    static constexpr int NUM_SAMPLES = 4;
    
    SamplerBank();
    ~SamplerBank() = default;
    
    // Configuration
    void setSampleRate(int sr);
    void prepare(int sampleRate, int maxBlockSize);
    
    // Sample access (0-3)
    Sampler& getSample(int index);
    const Sampler& getSample(int index) const;
    
    // Trigger sample playback
    // noteIndex: 0-3 for the 4 samples
    // beatDuration: how many beats to play
    // tempo: BPM
    void triggerSample(int noteIndex, double beatDuration, double tempoInBPM);
    
    // Stop all samples
    void stopAll();
    
    // Main processing - sums all 4 samplers
    void processBlock(float* outAudio, int numSamples, double tempoInBPM, float mixLevel = 1.0f);
    
    // Sample loading
    void loadSampleData(int noteIndex, const float* audioData, int lengthInSamples);
    void clearSampleData(int noteIndex);
    
    // Loop configuration per sample
    void setSampleLoopMode(int noteIndex, bool enable);
    void setSampleStartFraction(int noteIndex, float fraction);
    void setSampleEndFraction(int noteIndex, float fraction);
    
private:
    std::array<Sampler, NUM_SAMPLES> samplers_;
    int sampleRate_ = 48000;
};

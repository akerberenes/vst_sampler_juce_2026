#pragma once

#include <vector>
#include <atomic>
#include <cstring>

/**
 * Sampler
 * 
 * Plays back a single loaded audio sample with tempo-synced loop support.
 * One-shot playback only (no polyphony per sample).
 * 
 * No JUCE dependencies - pure C++
 */
class Sampler
{
public:
    Sampler();
    ~Sampler() = default;

    // Audio data
    void setSampleData(const float* audioData, int lengthInSamples, int channels = 1);
    void clearSampleData();
    bool hasSampleData() const { return !sampleData_.empty(); }
    
    // Playback configuration
    void setSampleRate(int sr) { sampleRate_ = sr; }
    int getSampleRate() const { return sampleRate_; }
    
    // Sample playback parameters
    void setLoopMode(bool enable) { loopMode_.store(enable); }
    bool getLoopMode() const { return loopMode_.load(); }
    
    // Set fractional loop (e.g., 0.5 = half of sample, 1.0 = full sample)
    void setLoopFraction(float fraction);
    float getLoopFraction() const { return loopFraction_; }
    
    // Trigger playback
    // beatDuration: how many beats to play (e.g., 4 beats = 1 bar at 4/4)
    // tempo: BPM for calculating actual playback time
    void trigger(double beatDuration, double tempoInBPM);
    
    // Main processing
    void processBlock(float* outAudio, int numSamples, double tempoInBPM);
    
    // State queries
    bool isPlaying() const { return isPlaying_.load(); }
    float getPlaybackPosition() const;  // 0.0 to 1.0
    
    // Stop playback
    void stop();
    
    // Sample info
    int getSampleLengthInSamples() const { return sampleData_.size(); }
    float getSampleDurationInSeconds() const;
    
private:
    std::vector<float> sampleData_;
    int sampleRate_ = 48000;
    int channels_ = 1;
    
    std::atomic<bool> isPlaying_{false};
    std::atomic<double> playheadPosition_{0.0};
    std::atomic<int> samplesTillStop_{0};
    
    bool loopMode_ = false;
    float loopFraction_ = 1.0f;  // Full sample by default
    
    // Calculate loop end in samples based on fraction
    int getLoopEndInSamples() const;
    
    // Linear interpolation for smooth playback
    float interpolateSample(double position) const;
};

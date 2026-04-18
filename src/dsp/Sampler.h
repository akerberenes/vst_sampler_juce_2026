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
    void setLoopMode(bool enable) { loopMode_ = enable; }
    bool getLoopMode() const { return loopMode_; }
    
    // Set playback region as fractions of the full sample (0.0 to 1.0)
    void setStartFraction(float fraction);
    void setEndFraction(float fraction);
    float getStartFraction() const { return startFraction_; }
    float getEndFraction() const { return endFraction_; }
    
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
    float startFraction_ = 0.0f;
    float endFraction_ = 1.0f;
    
    // Calculate playback region in samples
    int getStartInSamples() const;
    int getEndInSamples() const;
    
    // Linear interpolation for smooth playback
    float interpolateSample(double position) const;
};

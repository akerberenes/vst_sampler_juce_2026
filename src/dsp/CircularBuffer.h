#pragma once

#include <vector>
#include <atomic>
#include <cstring>

/**
 * CircularBuffer
 * 
 * A thread-safe circular buffer for audio processing with freeze effect support.
 * Supports variable playback speed and loop boundary manipulation.
 * 
 * No JUCE dependencies - pure C++
 */
class CircularBuffer
{
public:
    explicit CircularBuffer(int maxSizeInSamples = 0);
    ~CircularBuffer() = default;

    // Configuration
    void allocate(int sizeInSamples);
    void clear();
    
    // Recording (write pointer automatic increment)
    void pushSample(float sample);
    void pushBlock(const float* audio, int numSamples);
    
    // Playback (read pointer controlled by caller)
    float pullSample();
    void pullBlock(float* outAudio, int numSamples);
    
    // Freeze state management
    void freeze();
    void unfreeze();
    bool isFrozen() const;
    
    // Loop boundary control (for echo freeze stutter/extend)
    // Set loop region as fraction of buffer (0.0 to 1.0)
    void setLoopStart(float fraction);
    void setLoopEnd(float fraction);
    float getLoopStart() const { return loopStartFraction_; }
    float getLoopEnd() const { return loopEndFraction_; }
    
    // Playback speed control (1.0 = normal, 0.5 = half speed, 2.0 = double)
    void setPlaybackSpeed(float speed);
    float getPlaybackSpeed() const { return playbackSpeed_.load(); }
    
    // Read position control
    void setReadPosition(float fraction);  // 0.0 to 1.0 within loop
    float getReadPosition() const;
    
    // Stutter/rhythmic playback support
    void jumpReadPointerByFraction(float fraction);  // Jump within loop
    
    // Buffer info
    int getSize() const { return buffer_.size(); }
    
    // Get buffer for direct access (use carefully)
    const float* getBuffer() const { return buffer_.data(); }
    
private:
    std::vector<float> buffer_;
    std::atomic<int> writePos_{0};
    std::atomic<double> readPos_{0.0};
    std::atomic<bool> frozen_{false};
    std::atomic<float> playbackSpeed_{1.0f};
    
    float loopStartFraction_ = 0.0f;
    float loopEndFraction_ = 1.0f;
    
    // Helper: get actual read position within loop boundaries
    double getLoopAdjustedReadPos() const;
};

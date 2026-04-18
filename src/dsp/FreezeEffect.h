#pragma once

#include "CircularBuffer.h"
#include <atomic>

/**
 * FreezeEffect
 * 
 * Master echo-freeze effect that:
 * - Captures output to a circular buffer (input + samplers in sequential mode,
 *   samplers only in parallel mode)
 * - Can freeze the buffer to loop indefinitely
 * - Supports stutter (quantized playback jumps)
 * - Supports speed manipulation (pitch shift frozen audio)
 * - Supports loop boundary control (extend/shorten frozen section)
 * 
 * No JUCE dependencies - pure C++
 */
class FreezeEffect
{
public:
    enum class State : uint8_t
    {
        Recording = 0,      // Continuously capturing audio
        Frozen = 1          // Buffer is frozen, looping
    };
    
    explicit FreezeEffect(int maxBufferSizeInSamples = 0);
    ~FreezeEffect() = default;
    
    // Configuration
    void prepare(int sampleRate, int maxBlockSize);
    void setBufferSizeInSamples(int size);
    int getBufferSizeInSamples() const { return freezeBuffer_.getSize(); }
    
    // Freeze state control
    void setFrozen(bool b);
    bool isFrozen() const { return freezeBuffer_.isFrozen(); }
    
    // Main processing
    // Input: audio to capture/freeze
    // Output: frozen playback (with stutter/speed applied)
    void processBlock(const float* inputAudio, float* outputAudio, int numSamples, 
                      double tempoInBPM);
    
    // Stutter configuration
    // stutterFraction: 1/4, 1/8, 1/16 of a beat (quantized to tempo)
    void setStutterFraction(double beatFraction) { stutterFraction_ = beatFraction; }
    double getStutterFraction() const { return stutterFraction_; }
    
    // Playback speed (pitch shift)
    // 1.0 = normal, 0.5 = half speed/pitch, 2.0 = double
    void setPlaybackSpeed(float speed) { freezeBuffer_.setPlaybackSpeed(speed); }
    float getPlaybackSpeed() const { return freezeBuffer_.getPlaybackSpeed(); }
    
    // Loop boundary control (fraction of buffer)
    void setLoopStart(float fraction) { freezeBuffer_.setLoopStart(fraction); }
    void setLoopEnd(float fraction) { freezeBuffer_.setLoopEnd(fraction); }
    float getLoopStart() const { return freezeBuffer_.getLoopStart(); }
    float getLoopEnd() const { return freezeBuffer_.getLoopEnd(); }
    
    // Freeze parameters
    void setDryWetMix(float wet);  // 0.0 = dry (no freeze), 1.0 = full freeze
    float getDryWetMix() const { return dryWetMix_.load(); }
    
    // State queries
    State getState() const { return static_cast<State>(state_.load()); }
    float getBufferFillPercentage() const;  // Debug: how full is the freeze buffer?
    
private:
    CircularBuffer freezeBuffer_;
    int sampleRate_ = 48000;
    
    std::atomic<uint8_t> state_{static_cast<uint8_t>(State::Recording)};
    std::atomic<float> dryWetMix_{1.0f};
    
    double stutterFraction_ = 0.125;  // 1/8 beat default
    double stutterAccumulator_ = 0.0;
    
    // Helper: execute stutter jump logic per block
    void updateStutterPlayback(double tempoInBPM, int blockSize);
};

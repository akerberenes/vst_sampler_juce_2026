#pragma once

#include <atomic>

/**
 * Mixer
 * 
 * Routes audio based on parallel/sequential mode:
 * - Parallel: input isolated, sampler plays independently
 * - Sequential: input + sampler mixed together
 * 
 * Used by FreezeEffect to determine what gets captured.
 * 
 * No JUCE dependencies - pure C++
 */
class Mixer
{
public:
    enum class Mode : uint8_t
    {
        Sequential = 0,  // Input + sampler mixed
        Parallel = 1     // Input isolated, sampler independent
    };
    
    Mixer();
    ~Mixer() = default;
    
    // Mode control
    void setMode(Mode m) { mode_.store(static_cast<uint8_t>(m)); }
    Mode getMode() const { return static_cast<Mode>(mode_.load()); }
    
    bool isParallelMode() const { return getMode() == Mode::Parallel; }
    bool isSequentialMode() const { return getMode() == Mode::Sequential; }
    
    // Input/output blending
    // In sequential mode: blend input + sampler output
    // In parallel: this is ignored (input stays separate)
    void setInputLevel(float level);  // 0.0 to 1.0
    void setSamplerLevel(float level);  // 0.0 to 1.0
    float getInputLevel() const { return inputLevel_.load(); }
    float getSamplerLevel() const { return samplerLevel_.load(); }
    
    // Main mixing operation
    // For sequential mode: actually mixes input + samplerAudio into output
    // For parallel mode: output = samplerAudio, input is bypassed
    void processBlock(const float* inputAudio, const float* samplerAudio, 
                      float* outputAudio, int numSamples);
    
private:
    std::atomic<uint8_t> mode_{static_cast<uint8_t>(Mode::Sequential)};
    std::atomic<float> inputLevel_{1.0f};
    std::atomic<float> samplerLevel_{1.0f};
};

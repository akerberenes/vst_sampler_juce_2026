#include "Mixer.h"
#include <algorithm>
#include <cstring>

Mixer::Mixer()
    : inputLevel_(1.0f), samplerLevel_(1.0f)
{
}

void Mixer::setInputLevel(float level)
{
    inputLevel_.store(std::clamp(level, 0.0f, 1.0f));
}

void Mixer::setSamplerLevel(float level)
{
    samplerLevel_.store(std::clamp(level, 0.0f, 1.0f));
}

void Mixer::processBlock(const float* inputAudio, const float* samplerAudio,
                        float* outputAudio, int numSamples)
{
    if (!outputAudio || numSamples <= 0)
        return;
    
    Mode currentMode = getMode();
    
    if (currentMode == Mode::Parallel)
    {
        // Parallel: only sampler output goes to freeze buffer
        // Input is isolated (mixed back in by PluginProcessor)
        float samplerLevel = samplerLevel_.load();
        if (samplerAudio)
        {
            for (int i = 0; i < numSamples; ++i)
                outputAudio[i] = samplerAudio[i] * samplerLevel;
        }
        else
            std::fill(outputAudio, outputAudio + numSamples, 0.0f);
    }
    else  // Sequential
    {
        // Sequential: mix input + sampler
        float inputLevel = inputLevel_.load();
        float samplerLevel = samplerLevel_.load();
        
        for (int i = 0; i < numSamples; ++i)
        {
            float input = inputAudio ? inputAudio[i] * inputLevel : 0.0f;
            float sampler = samplerAudio ? samplerAudio[i] * samplerLevel : 0.0f;
            outputAudio[i] = input + sampler;
        }
    }
}

#include "Mixer.h"
#include <algorithm>
#include <cstring>

Mixer::Mixer()
    : inputLevel_(1.0f), samplerLevel_(1.0f)
{
    // Both levels default to 1.0 (unity gain, no attenuation).
}

void Mixer::setInputLevel(float level)
{
    // Clamp to [0, 1] so the level can't boost beyond unity or go negative.
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

    // Read the current mode once (one atomic load per block, not per sample).
    Mode currentMode = getMode();

    if (currentMode == Mode::Parallel)
    {
        // Parallel: the freeze effect only sees the sampler output.
        // The input audio is NOT included here; PluginProcessor adds it back
        // to the final output AFTER the freeze step completes.
        float sl = samplerLevel_.load();
        if (samplerAudio)
        {
            for (int i = 0; i < numSamples; ++i)
                outputAudio[i] = samplerAudio[i] * sl;
        }
        else
        {
            // No sampler audio (no samples loaded/playing): output silence.
            std::fill(outputAudio, outputAudio + numSamples, 0.0f);
        }
    }
    else  // Sequential
    {
        // Sequential: blend input + sampler before the freeze effect captures them.
        // Both are attenuated by their respective level knobs.
        float il = inputLevel_.load();
        float sl = samplerLevel_.load();

        for (int i = 0; i < numSamples; ++i)
        {
            // If a pointer is null, treat that source as silence (0.0f).
            float in  = inputAudio   ? inputAudio[i]   * il : 0.0f;
            float smp = samplerAudio ? samplerAudio[i] * sl : 0.0f;
            outputAudio[i] = in + smp;
        }
    }
}

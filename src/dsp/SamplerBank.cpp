#include "SamplerBank.h"

SamplerBank::SamplerBank()
{
}

void SamplerBank::setSampleRate(int sr)
{
    sampleRate_ = sr;
    for (auto& sampler : samplers_)
        sampler.setSampleRate(sr);
}

void SamplerBank::prepare(int sampleRate, int maxBlockSize)
{
    setSampleRate(sampleRate);
}

Sampler& SamplerBank::getSample(int index)
{
    if (index < 0 || index >= NUM_SAMPLES)
        index = 0;
    return samplers_[index];
}

const Sampler& SamplerBank::getSample(int index) const
{
    if (index < 0 || index >= NUM_SAMPLES)
        index = 0;
    return samplers_[index];
}

void SamplerBank::triggerSample(int noteIndex, double beatDuration, double tempoInBPM)
{
    if (noteIndex >= 0 && noteIndex < NUM_SAMPLES)
        samplers_[noteIndex].trigger(beatDuration, tempoInBPM);
}

void SamplerBank::stopAll()
{
    for (auto& sampler : samplers_)
        sampler.stop();
}

void SamplerBank::processBlock(float* outAudio, int numSamples, double tempoInBPM, float mixLevel)
{
    if (!outAudio)
        return;
    
    // Clear output
    std::fill(outAudio, outAudio + numSamples, 0.0f);
    
    // Mix all 4 samplers
    float tempBuffer[4096];  // Temp buffer for each sampler
    for (auto& sampler : samplers_)
    {
        if (sampler.hasSampleData() && sampler.isPlaying())
        {
            sampler.processBlock(tempBuffer, numSamples, tempoInBPM);
            for (int i = 0; i < numSamples; ++i)
                outAudio[i] += tempBuffer[i] * mixLevel * 0.25f;  // 0.25 to prevent clipping
        }
    }
}

void SamplerBank::loadSampleData(int noteIndex, const float* audioData, int lengthInSamples)
{
    if (noteIndex >= 0 && noteIndex < NUM_SAMPLES)
        samplers_[noteIndex].setSampleData(audioData, lengthInSamples);
}

void SamplerBank::clearSampleData(int noteIndex)
{
    if (noteIndex >= 0 && noteIndex < NUM_SAMPLES)
        samplers_[noteIndex].clearSampleData();
}

void SamplerBank::setSampleLoopMode(int noteIndex, bool enable)
{
    if (noteIndex >= 0 && noteIndex < NUM_SAMPLES)
        samplers_[noteIndex].setLoopMode(enable);
}

void SamplerBank::setSampleLoopFraction(int noteIndex, float fraction)
{
    if (noteIndex >= 0 && noteIndex < NUM_SAMPLES)
        samplers_[noteIndex].setLoopFraction(fraction);
}

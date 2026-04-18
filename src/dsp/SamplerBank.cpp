#include "SamplerBank.h"

SamplerBank::SamplerBank()
{
    // samplers_ is a std::array — its 4 Sampler elements are default-constructed here.
}

void SamplerBank::setSampleRate(int sr)
{
    sampleRate_ = sr;
    // Propagate to every sampler so their beat-to-sample calculations stay accurate.
    for (auto& sampler : samplers_)
        sampler.setSampleRate(sr);
}

void SamplerBank::prepare(int sampleRate, int maxBlockSize)
{
    // maxBlockSize is accepted for API symmetry with JUCE's prepareToPlay,
    // but SamplerBank doesn't pre-allocate per-block buffers — it uses a
    // stack-allocated temp buffer in processBlock instead.
    setSampleRate(sampleRate);
}

Sampler& SamplerBank::getSample(int index)
{
    // Clamp out-of-range index to 0 rather than crashing. The caller
    // (e.g. handleMidiNoteOff) is expected to pass a valid index,
    // but we guard here for robustness.
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
    // Silently ignore out-of-range indices (MIDI notes that don't map to a pad).
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
    if (!outAudio || numSamples > 4096)
        return;

    // Start with silence.
    std::fill(outAudio, outAudio + numSamples, 0.0f);

    // Temporary buffer for one sampler's output per iteration.
    // Stack-allocated (4096 floats = 16 KB) — safe for audio thread use.
    float tempBuffer[4096];

    for (auto& sampler : samplers_)
    {
        // Skip samplers that have no sample loaded or are not playing —
        // calling processBlock on them would just return silence, but
        // skipping them saves the function-call overhead.
        if (sampler.hasSampleData() && sampler.isPlaying())
        {
            sampler.processBlock(tempBuffer, numSamples, tempoInBPM);

            // Accumulate into outAudio with per-pad gain of 0.25.
            // 4 pads × 0.25 = 1.0 maximum total gain when all pads play at once.
            for (int i = 0; i < numSamples; ++i)
                outAudio[i] += tempBuffer[i] * mixLevel * 0.25f;
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

void SamplerBank::setSampleStartFraction(int noteIndex, float fraction)
{
    if (noteIndex >= 0 && noteIndex < NUM_SAMPLES)
        samplers_[noteIndex].setStartFraction(fraction);
}

void SamplerBank::setSampleEndFraction(int noteIndex, float fraction)
{
    if (noteIndex >= 0 && noteIndex < NUM_SAMPLES)
        samplers_[noteIndex].setEndFraction(fraction);
}

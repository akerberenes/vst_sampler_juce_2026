#include "Sampler.h"
#include <algorithm>
#include <cmath>

Sampler::Sampler()
{
}

void Sampler::setSampleData(const float* audioData, int lengthInSamples, int channels)
{
    if (!audioData || lengthInSamples <= 0)
        return;
    
    sampleData_.assign(audioData, audioData + (lengthInSamples * channels));
    channels_ = channels;
}

void Sampler::clearSampleData()
{
    sampleData_.clear();
    isPlaying_.store(false);
}

void Sampler::setStartFraction(float fraction)
{
    startFraction_ = std::clamp(fraction, 0.0f, 1.0f);
}

void Sampler::setEndFraction(float fraction)
{
    endFraction_ = std::clamp(fraction, 0.0f, 1.0f);
}

void Sampler::trigger(double beatDuration, double tempoInBPM)
{
    if (!hasSampleData())
        return;
    
    // Calculate duration in samples
    double durationSeconds = (beatDuration * 60.0) / tempoInBPM;
    int durationSamples = static_cast<int>(durationSeconds * sampleRate_);
    
    playheadPosition_.store(static_cast<double>(getStartInSamples()));
    samplesTillStop_.store(durationSamples);
    isPlaying_.store(true);
}

void Sampler::processBlock(float* outAudio, int numSamples, double tempoInBPM)
{
    if (!outAudio || !hasSampleData())
    {
        std::fill(outAudio, outAudio + numSamples, 0.0f);
        return;
    }
    
    if (!isPlaying_.load())
    {
        std::fill(outAudio, outAudio + numSamples, 0.0f);
        return;
    }
    
    // Load atomics once before the loop (only audio thread runs this)
    double pos = playheadPosition_.load();
    int remaining = samplesTillStop_.load();
    int regionStart = getStartInSamples();
    int regionEnd = getEndInSamples();
    if (regionEnd <= regionStart)
        regionEnd = regionStart + 1;
    
    for (int i = 0; i < numSamples; ++i)
    {
        if (remaining <= 0)
        {
            isPlaying_.store(false);
            playheadPosition_.store(pos);
            samplesTillStop_.store(0);
            std::fill(outAudio + i, outAudio + numSamples, 0.0f);
            return;
        }
        
        outAudio[i] = interpolateSample(pos);
        
        pos += 1.0;
        if (pos >= regionEnd)
        {
            if (loopMode_)
                pos = static_cast<double>(regionStart);
            else
            {
                isPlaying_.store(false);
                playheadPosition_.store(pos);
                samplesTillStop_.store(0);
                std::fill(outAudio + i + 1, outAudio + numSamples, 0.0f);
                return;
            }
        }
        
        --remaining;
    }
    
    // Store atomics once after the loop
    playheadPosition_.store(pos);
    samplesTillStop_.store(remaining);
}

void Sampler::stop()
{
    isPlaying_.store(false);
    samplesTillStop_.store(0);
}

float Sampler::getPlaybackPosition() const
{
    int start = getStartInSamples();
    int end = getEndInSamples();
    int regionLen = end - start;
    if (regionLen <= 0)
        return 0.0f;
    
    return static_cast<float>((playheadPosition_.load() - start) / regionLen);
}

float Sampler::getSampleDurationInSeconds() const
{
    if (channels_ <= 0 || sampleRate_ <= 0)
        return 0.0f;
    
    return static_cast<float>(sampleData_.size()) / (channels_ * sampleRate_);
}

int Sampler::getStartInSamples() const
{
    if (channels_ <= 0)
        return 0;
    
    int totalSamples = sampleData_.size() / channels_;
    return static_cast<int>(totalSamples * startFraction_);
}

int Sampler::getEndInSamples() const
{
    if (channels_ <= 0)
        return 0;
    
    int totalSamples = sampleData_.size() / channels_;
    return static_cast<int>(totalSamples * endFraction_);
}

float Sampler::interpolateSample(double position) const
{
    if (sampleData_.empty() || channels_ <= 0)
        return 0.0f;
    
    int totalSamples = sampleData_.size() / channels_;
    if (totalSamples <= 0)
        return 0.0f;
    
    int idx = static_cast<int>(position);
    double frac = position - static_cast<double>(idx);
    
    if (idx < 0 || idx >= totalSamples)
        return 0.0f;
    
    float sample1 = sampleData_[idx * channels_];
    float sample2 = (idx + 1 < totalSamples) ? sampleData_[(idx + 1) * channels_] : 0.0f;
    
    return sample1 + static_cast<float>(frac) * (sample2 - sample1);
}

#pragma once

#include "Effect.h"
#include <cmath>
#include <algorithm>

/**
 * SimpleFilter — one-pole low-pass filter.
 *
 * Parameter (0.0–1.0) controls the cutoff frequency.
 * At 0.0: very dark (almost muted). At 1.0: fully open (bypass).
 * Uses a simple one-pole IIR filter for Teensy portability.
 */
class SimpleFilter : public Effect
{
public:
    float processSample(float input) override
    {
        if (cutoff_ >= 1.0f)
            return input;
        // Map 0.0–1.0 to filter coefficient (0.01–1.0).
        // Higher cutoff_ = less filtering.
        float coeff = 0.01f + cutoff_ * 0.99f;
        // One-pole low-pass: y[n] = coeff * x[n] + (1-coeff) * y[n-1]
        lastOutput_ = coeff * input + (1.0f - coeff) * lastOutput_;
        return lastOutput_;
    }

    const char* getName() const override { return "Filter"; }

    float getParamValue() const override { return cutoff_; }
    void setParamValue(float normalised) override
    {
        cutoff_ = std::clamp(normalised, 0.0f, 1.0f);
    }

private:
    float cutoff_ = 1.0f;
    float lastOutput_ = 0.0f;
};

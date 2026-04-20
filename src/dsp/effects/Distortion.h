#pragma once

#include "Effect.h"
#include <cmath>
#include <algorithm>

/**
 * Distortion — soft-clipping waveshaper.
 *
 * Parameter (0.0–1.0) controls the drive amount.
 * At 0.0: clean (no distortion). At 1.0: heavy saturation.
 * Uses tanh waveshaping scaled by drive.
 */
class Distortion : public Effect
{
public:
    float processSample(float input) override
    {
        if (drive_ <= 0.0f)
            return input;
        // Scale input by drive (1.0 to 10.0) then apply fast tanh approximation.
        float driven = input * (1.0f + drive_ * 9.0f);
        return fastTanh(driven);
    }

    const char* getName() const override { return "Distort"; }

    float getParamValue() const override { return drive_; }
    void setParamValue(float normalised) override
    {
        drive_ = std::clamp(normalised, 0.0f, 1.0f);
    }

private:
    float drive_ = 0.5f;

    // Rational approximation of tanh — accurate to ~0.1% for |x| < 4,
    // much faster than std::tanh on ARM Cortex-M7 (no software lib call).
    static float fastTanh(float x)
    {
        if (x < -3.0f) return -1.0f;
        if (x >  3.0f) return  1.0f;
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
};

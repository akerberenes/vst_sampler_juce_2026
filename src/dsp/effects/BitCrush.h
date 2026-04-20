#pragma once

#include "Effect.h"
#include <cmath>
#include <algorithm>

/**
 * BitCrush — sample quantisation / bit reduction effect.
 *
 * Parameter (0.0–1.0) controls the crush intensity.
 * At 0.0: full resolution (no effect). At 1.0: very coarse quantisation (~2 bits).
 * The signal is quantised to fewer amplitude steps, producing a lo-fi digital sound.
 */
class BitCrush : public Effect
{
public:
    float processSample(float input) override
    {
        if (crush_ <= 0.0f)
            return input;
        // Map 0.0–1.0 to a number of quantisation levels: 256 down to 4.
        float levels = 256.0f - crush_ * 252.0f;
        if (levels < 2.0f) levels = 2.0f;
        // Quantise: round to nearest level.
        float half = levels * 0.5f;
        return std::round(input * half) / half;
    }

    const char* getName() const override { return "BitCrush"; }

    float getParamValue() const override { return crush_; }
    void setParamValue(float normalised) override
    {
        crush_ = std::clamp(normalised, 0.0f, 1.0f);
    }

private:
    float crush_ = 0.5f;
};

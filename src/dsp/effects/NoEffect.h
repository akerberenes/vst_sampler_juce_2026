#pragma once

#include "Effect.h"

/**
 * NoEffect — bypass / passthrough.
 * Always index 0 in the EffectLibrary.
 */
class NoEffect : public Effect
{
public:
    float processSample(float input) override { return input; }
    const char* getName() const override { return "None"; }
    float getParamValue() const override { return 0.0f; }
    void setParamValue(float) override {}
};

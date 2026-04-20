#pragma once

#include <string>

/**
 * Effect (abstract base)
 *
 * Base class for per-sample audio effects. Each Sampler can have one Effect
 * applied to its output. Effects are designed to be simple, single-parameter
 * processors that can run on both the VST (JUCE) and Teensy platforms.
 *
 * --- Contract ---
 * - processSample(float) : apply the effect to one audio sample and return the result.
 * - getName()            : return a short display name (max 8 chars for LCD).
 * - getParamValue()      : return the current parameter as a normalised float (0.0–1.0).
 * - setParamValue(float) : set the parameter from a normalised float (0.0–1.0).
 *
 * No JUCE dependencies — pure C++17.
 */
class Effect
{
public:
    virtual ~Effect() = default;

    virtual float processSample(float input) = 0;

    virtual const char* getName() const = 0;

    virtual float getParamValue() const = 0;
    virtual void setParamValue(float normalised) = 0;
};

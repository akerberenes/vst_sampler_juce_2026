#pragma once

#include "Effect.h"
#include "NoEffect.h"
#include "Distortion.h"
#include "BitCrush.h"
#include "SimpleFilter.h"
#include <memory>

/**
 * EffectLibrary — static registry of all available per-sample effects.
 *
 * Provides factory methods to create effect instances by index, and queries
 * for the total number of effects and their names. Index 0 is always NoEffect.
 *
 * To add a new effect:
 *   1. Create a header-only class inheriting Effect (in src/dsp/effects/).
 *   2. #include it above.
 *   3. Add a case to createEffect() and getEffectName().
 *   4. Increment NUM_EFFECTS.
 *
 * No JUCE dependencies — pure C++17.
 */
class EffectLibrary
{
public:
    static constexpr int NUM_EFFECTS = 4;

    static std::unique_ptr<Effect> createEffect(int index)
    {
        switch (index)
        {
            case 0:  return std::make_unique<NoEffect>();
            case 1:  return std::make_unique<Distortion>();
            case 2:  return std::make_unique<BitCrush>();
            case 3:  return std::make_unique<SimpleFilter>();
            default: return std::make_unique<NoEffect>();
        }
    }

    static const char* getEffectName(int index)
    {
        switch (index)
        {
            case 0:  return "None";
            case 1:  return "Distort";
            case 2:  return "BitCrush";
            case 3:  return "Filter";
            default: return "None";
        }
    }

    static int getEffectCount() { return NUM_EFFECTS; }
};

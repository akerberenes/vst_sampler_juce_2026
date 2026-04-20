#include <catch2/catch.hpp>
#include "Mixer.h"
#include <cmath>

using namespace Catch;

TEST_CASE("Mixer mode initialization", "[Mixer]")
{
    Mixer mixer;
    
    SECTION("Default mode is sequential")
    {
        REQUIRE(mixer.isSequentialMode() == true);
        REQUIRE(mixer.isParallelMode() == false);
    }
}

TEST_CASE("Mixer mode switching", "[Mixer]")
{
    Mixer mixer;
    
    SECTION("Switch to parallel mode")
    {
        mixer.setMode(Mixer::Mode::Parallel);
        REQUIRE(mixer.isParallelMode() == true);
        REQUIRE(mixer.isSequentialMode() == false);
    }
    
    SECTION("Switch back to sequential mode")
    {
        mixer.setMode(Mixer::Mode::Parallel);
        mixer.setMode(Mixer::Mode::Sequential);
        REQUIRE(mixer.isSequentialMode() == true);
        REQUIRE(mixer.isParallelMode() == false);
    }
    
    SECTION("Get mode returns correct value")
    {
        mixer.setMode(Mixer::Mode::Sequential);
        REQUIRE(mixer.getMode() == Mixer::Mode::Sequential);
        
        mixer.setMode(Mixer::Mode::Parallel);
        REQUIRE(mixer.getMode() == Mixer::Mode::Parallel);
    }
}

TEST_CASE("Mixer level control", "[Mixer]")
{
    Mixer mixer;
    
    SECTION("Default levels are 1.0")
    {
        REQUIRE(mixer.getInputLevel() == Approx(1.0f));
        REQUIRE(mixer.getSamplerLevel() == Approx(1.0f));
    }
    
    SECTION("Set input level")
    {
        mixer.setInputLevel(0.5f);
        REQUIRE(mixer.getInputLevel() == Approx(0.5f));
        
        mixer.setInputLevel(0.0f);
        REQUIRE(mixer.getInputLevel() == Approx(0.0f));
        
        mixer.setInputLevel(1.0f);
        REQUIRE(mixer.getInputLevel() == Approx(1.0f));
    }
    
    SECTION("Set sampler level")
    {
        mixer.setSamplerLevel(0.7f);
        REQUIRE(mixer.getSamplerLevel() == Approx(0.7f));
        
        mixer.setSamplerLevel(0.0f);
        REQUIRE(mixer.getSamplerLevel() == Approx(0.0f));
        
        mixer.setSamplerLevel(1.0f);
        REQUIRE(mixer.getSamplerLevel() == Approx(1.0f));
    }
}

TEST_CASE("Mixer sequential mode processing", "[Mixer]")
{
    Mixer mixer;
    mixer.setMode(Mixer::Mode::Sequential);
    mixer.setInputLevel(0.5f);
    mixer.setSamplerLevel(0.5f);
    
    SECTION("Mix input and sampler in sequential mode")
    {
        float input[4] = {0.2f, 0.4f, 0.6f, 0.8f};
        float sampler[4] = {0.1f, 0.1f, 0.1f, 0.1f};
        float output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        
        mixer.processBlock(input, sampler, output, 4);
        
        // In sequential mode: output = input * inputLevel + sampler * samplerLevel
        // output[0] = 0.2 * 0.5 + 0.1 * 0.5 = 0.1 + 0.05 = 0.15
        REQUIRE(output[0] == Approx(0.15f));
        REQUIRE(output[1] == Approx(0.25f));
        REQUIRE(output[2] == Approx(0.35f));
        REQUIRE(output[3] == Approx(0.45f));
    }
}

TEST_CASE("Mixer parallel mode processing", "[Mixer]")
{
    Mixer mixer;
    mixer.setMode(Mixer::Mode::Parallel);
    mixer.setInputLevel(0.5f);
    mixer.setSamplerLevel(0.5f);
    
    SECTION("Output only sampler in parallel mode")
    {
        float input[4] = {0.9f, 0.9f, 0.9f, 0.9f};
        float sampler[4] = {0.2f, 0.4f, 0.6f, 0.8f};
        float output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        
        mixer.processBlock(input, sampler, output, 4);
        
        // In parallel mode: output = sampler (input is isolated/bypassed)
        // But sampler level is still applied
        // output = sampler * samplerLevel
        REQUIRE(output[0] == Approx(0.1f));  // 0.2 * 0.5
        REQUIRE(output[1] == Approx(0.2f));  // 0.4 * 0.5
        REQUIRE(output[2] == Approx(0.3f));  // 0.6 * 0.5
        REQUIRE(output[3] == Approx(0.4f));  // 0.8 * 0.5
    }
}

TEST_CASE("Mixer zero levels", "[Mixer]")
{
    Mixer mixer;
    mixer.setMode(Mixer::Mode::Sequential);
    mixer.setInputLevel(0.0f);
    mixer.setSamplerLevel(0.0f);
    
    SECTION("Zero levels produce silence")
    {
        float input[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        float sampler[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        float output[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        
        mixer.processBlock(input, sampler, output, 4);
        
        for (int i = 0; i < 4; ++i)
        {
            REQUIRE(output[i] == Approx(0.0f));
        }
    }
}

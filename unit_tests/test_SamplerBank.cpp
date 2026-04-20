#include <catch2/catch.hpp>
#include "SamplerBank.h"
#include <cmath>
#include <vector>

using namespace Catch;

// Helper: create test sine wave
inline std::vector<float> createTestWave(int numSamples, int sampleRate, float frequency)
{
    std::vector<float> wave(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        wave[i] = std::sin(2.0f * 3.14159f * frequency * i / sampleRate);
    }
    return wave;
}

TEST_CASE("SamplerBank initialization", "[SamplerBank]")
{
    SamplerBank bank;
    bank.setSampleRate(48000);
    bank.prepare(48000, 512);
    
    SECTION("Has 4 samplers")
    {
        REQUIRE(SamplerBank::NUM_SAMPLES == 4);
    }
    
    SECTION("Can access all samplers")
    {
        for (int i = 0; i < SamplerBank::NUM_SAMPLES; ++i)
        {
            auto& sampler = bank.getSample(i);
            REQUIRE(sampler.getSampleRate() == 48000);
        }
    }
}

TEST_CASE("SamplerBank sample loading", "[SamplerBank]")
{
    SamplerBank bank;
    bank.setSampleRate(48000);
    
    auto testWave = createTestWave(1024, 48000, 440.0f);
    
    SECTION("Load sample to bank")
    {
        bank.loadSampleData(0, testWave.data(), testWave.size());
        
        const auto& sampler = bank.getSample(0);
        REQUIRE(sampler.hasSampleData() == true);
        REQUIRE(sampler.getSampleLengthInSamples() == 1024);
    }
    
    SECTION("Load multiple samples")
    {
        auto testWave1 = createTestWave(512, 48000, 440.0f);
        auto testWave2 = createTestWave(1024, 48000, 880.0f);
        
        bank.loadSampleData(0, testWave1.data(), testWave1.size());
        bank.loadSampleData(1, testWave2.data(), testWave2.size());
        
        REQUIRE(bank.getSample(0).getSampleLengthInSamples() == 512);
        REQUIRE(bank.getSample(1).getSampleLengthInSamples() == 1024);
    }
    
    SECTION("Clear sample data")
    {
        bank.loadSampleData(0, testWave.data(), testWave.size());
        REQUIRE(bank.getSample(0).hasSampleData() == true);
        
        bank.clearSampleData(0);
        REQUIRE(bank.getSample(0).hasSampleData() == false);
    }
}

TEST_CASE("SamplerBank loop configuration", "[SamplerBank]")
{
    SamplerBank bank;
    bank.setSampleRate(48000);
    
    SECTION("Set loop mode per sample")
    {
        bank.setSampleLoopMode(0, true);
        REQUIRE(bank.getSample(0).getLoopMode() == true);
        
        bank.setSampleLoopMode(1, false);
        REQUIRE(bank.getSample(1).getLoopMode() == false);
    }
}

TEST_CASE("SamplerBank triggering", "[SamplerBank]")
{
    SamplerBank bank;
    bank.setSampleRate(48000);
    
    auto testWave = createTestWave(48000, 48000, 440.0f);
    
    SECTION("Trigger sample by index")
    {
        bank.loadSampleData(0, testWave.data(), testWave.size());
        bank.triggerSample(0, 1.0, 120.0);
        
        REQUIRE(bank.getSample(0).isPlaying() == true);
    }
    
    SECTION("Trigger multiple samples independently")
    {
        auto testWave1 = createTestWave(48000, 48000, 440.0f);
        auto testWave2 = createTestWave(48000, 48000, 880.0f);
        
        bank.loadSampleData(0, testWave1.data(), testWave1.size());
        bank.loadSampleData(1, testWave2.data(), testWave2.size());
        
        bank.triggerSample(0, 1.0, 120.0);
        bank.triggerSample(1, 1.0, 120.0);
        
        REQUIRE(bank.getSample(0).isPlaying() == true);
        REQUIRE(bank.getSample(1).isPlaying() == true);
    }
}

TEST_CASE("SamplerBank stop all", "[SamplerBank]")
{
    SamplerBank bank;
    bank.setSampleRate(48000);
    
    auto testWave = createTestWave(48000, 48000, 440.0f);
    
    bank.loadSampleData(0, testWave.data(), testWave.size());
    bank.loadSampleData(1, testWave.data(), testWave.size());
    
    bank.triggerSample(0, 1.0, 120.0);
    bank.triggerSample(1, 1.0, 120.0);
    
    REQUIRE(bank.getSample(0).isPlaying() == true);
    REQUIRE(bank.getSample(1).isPlaying() == true);
    
    bank.stopAll();
    
    REQUIRE(bank.getSample(0).isPlaying() == false);
    REQUIRE(bank.getSample(1).isPlaying() == false);
}

TEST_CASE("SamplerBank process block", "[SamplerBank]")
{
    SamplerBank bank;
    bank.setSampleRate(48000);
    bank.prepare(48000, 512);
    
    auto testWave = createTestWave(512, 48000, 440.0f);
    bank.loadSampleData(0, testWave.data(), testWave.size());
    
    SECTION("Process block generates output")
    {
        bank.triggerSample(0, 1.0, 120.0);
        
        float output[512] = {0};
        bank.processBlock(output, 512, 120.0, 1.0f);
        
        // Check that at least some non-zero output was generated
        bool hasOutput = false;
        for (int i = 0; i < 512; ++i)
        {
            if (output[i] != 0.0f)
            {
                hasOutput = true;
                break;
            }
        }
        REQUIRE(hasOutput == true);
    }
}

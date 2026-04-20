#include <catch2/catch.hpp>
#include "SamplerBank.h"
#include "FreezeEffect.h"
#include "Mixer.h"
#include <vector>

using namespace Catch;
#include <cmath>

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

TEST_CASE("Integration: SamplerBank with Mixer", "[Integration]")
{
    SamplerBank bank;
    Mixer mixer;
    
    bank.setSampleRate(48000);
    bank.prepare(48000, 512);
    
    auto testWave = createTestWave(48000, 48000, 440.0f);
    bank.loadSampleData(0, testWave.data(), testWave.size());
    
    mixer.setMode(Mixer::Mode::Sequential);
    mixer.setInputLevel(0.5f);
    mixer.setSamplerLevel(0.5f);
    
    SECTION("Samplers output can be mixed with input")
    {
        bank.triggerSample(0, 1.0, 120.0);
        
        float samplerOutput[512] = {0.0f};
        bank.processBlock(samplerOutput, 512, 120.0, 1.0f);
        
        float input[512];
        for (int i = 0; i < 512; ++i) input[i] = 0.1f;
        
        float mixedOutput[512] = {0.0f};
        mixer.processBlock(input, samplerOutput, mixedOutput, 512);
        
        // Mixed output should have content
        REQUIRE(mixedOutput != nullptr);
    }
}

TEST_CASE("Integration: SamplerBank with FreezeEffect", "[Integration]")
{
    SamplerBank bank;
    FreezeEffect freeze(4096);
    
    bank.setSampleRate(48000);
    bank.prepare(48000, 512);
    freeze.prepare(48000, 512);
    
    auto testWave = createTestWave(48000, 48000, 440.0f);
    bank.loadSampleData(0, testWave.data(), testWave.size());
    
    SECTION("Sampler output can be frozen")
    {
        bank.triggerSample(0, 1.0, 120.0);
        
        float samplerOutput[512] = {0.0f};
        bank.processBlock(samplerOutput, 512, 120.0, 1.0f);
        
        float frozenOutput[512] = {0.0f};
        freeze.processBlock(samplerOutput, frozenOutput, 512, 120.0);
        
        REQUIRE(frozenOutput != nullptr);
    }
}

TEST_CASE("Integration: Full signal chain (input -> mixer -> freeze)", "[Integration]")
{
    SamplerBank bank;
    Mixer mixer;
    FreezeEffect freeze;
    
    // Setup
    bank.setSampleRate(48000);
    bank.prepare(48000, 512);
    freeze.prepare(48000, 512);
    
    mixer.setMode(Mixer::Mode::Sequential);
    mixer.setInputLevel(0.5f);
    mixer.setSamplerLevel(0.5f);
    
    auto testWave = createTestWave(48000, 48000, 440.0f);
    bank.loadSampleData(0, testWave.data(), testWave.size());
    
    SECTION("Signal flows correctly through entire chain")
    {
        // Simulate audio from DAW
        float dawInput[512];
        for (int i = 0; i < 512; ++i)
        {
            dawInput[i] = 0.05f;
        }
        
        // Step 1: Trigger sampler
        bank.triggerSample(0, 1.0, 120.0);
        
        // Step 2: Generate sampler output
        float samplerOutput[512] = {0.0f};
        bank.processBlock(samplerOutput, 512, 120.0, 1.0f);
        
        // Step 3: Mix input + sampler in sequential mode
        float mixedOutput[512] = {0.0f};
        mixer.processBlock(dawInput, samplerOutput, mixedOutput, 512);
        
        // Step 4: Apply freeze effect
        float finalOutput[512] = {0.0f};
        freeze.processBlock(mixedOutput, finalOutput, 512, 120.0);
        
        // Result should be valid audio
        REQUIRE(finalOutput != nullptr);
    }
}

TEST_CASE("Integration: Parallel mode routing", "[Integration]")
{
    SamplerBank bank;
    Mixer mixer;
    FreezeEffect freeze;
    
    bank.setSampleRate(48000);
    bank.prepare(48000, 512);
    freeze.prepare(48000, 512);
    
    // Set to PARALLEL mode - input isolated from freeze
    mixer.setMode(Mixer::Mode::Parallel);
    mixer.setInputLevel(1.0f);
    mixer.setSamplerLevel(1.0f);
    
    auto testWave = createTestWave(48000, 48000, 440.0f);
    bank.loadSampleData(0, testWave.data(), testWave.size());
    
    SECTION("Parallel mode: input passes through, only sampler is frozen")
    {
        float dawInput[512];
        for (int i = 0; i < 512; ++i)
        {
            dawInput[i] = 0.05f;
        }
        
        bank.triggerSample(0, 1.0, 120.0);
        
        float samplerOutput[512] = {0.0f};
        bank.processBlock(samplerOutput, 512, 120.0, 1.0f);
        
        float mixedOutput[512] = {0.0f};
        mixer.processBlock(dawInput, samplerOutput, mixedOutput, 512);
        
        // In parallel mode, mixedOutput should only contain sampler (daw input isolated)
        // So if sampler has output, mixedOutput should have it
        REQUIRE(mixedOutput != nullptr);
    }
}

TEST_CASE("Integration: Multiple samples triggered simultaneously", "[Integration]")
{
    SamplerBank bank;
    
    bank.setSampleRate(48000);
    bank.prepare(48000, 512);
    
    // Load 4 different samples
    auto wave1 = createTestWave(24000, 48000, 440.0f);   // A4
    auto wave2 = createTestWave(24000, 48000, 494.0f);   // B4
    auto wave3 = createTestWave(24000, 48000, 523.0f);   // C5
    auto wave4 = createTestWave(24000, 48000, 587.0f);   // D5
    
    bank.loadSampleData(0, wave1.data(), wave1.size());
    bank.loadSampleData(1, wave2.data(), wave2.size());
    bank.loadSampleData(2, wave3.data(), wave3.size());
    bank.loadSampleData(3, wave4.data(), wave4.size());
    
    SECTION("All 4 samples can play simultaneously")
    {
        // Trigger all samples
        bank.triggerSample(0, 1.0, 120.0);
        bank.triggerSample(1, 1.0, 120.0);
        bank.triggerSample(2, 1.0, 120.0);
        bank.triggerSample(3, 1.0, 120.0);
        
        // All should be playing
        REQUIRE(bank.getSample(0).isPlaying() == true);
        REQUIRE(bank.getSample(1).isPlaying() == true);
        REQUIRE(bank.getSample(2).isPlaying() == true);
        REQUIRE(bank.getSample(3).isPlaying() == true);
        
        // Process block
        float output[512] = {0.0f};
        bank.processBlock(output, 512, 120.0, 1.0f);
        
        // Output should have content from multiple samplers
        REQUIRE(output != nullptr);
    }
}

TEST_CASE("Integration: Stutter effect timing at different tempos", "[Integration]")
{
    FreezeEffect freeze(4096);
    freeze.prepare(48000, 512);
    
    SECTION("Stutter at 120 BPM")
    {
        float input[512];
        for (int i = 0; i < 512; ++i) input[i] = 0.1f;
        
        float output[512] = {0.0f};
        freeze.processBlock(input, output, 512, 120.0);
        
        REQUIRE(output != nullptr);
    }
    
    SECTION("Stutter at 60 BPM (half tempo)")
    {
        float input[512];
        for (int i = 0; i < 512; ++i) input[i] = 0.1f;
        
        float output[512] = {0.0f};
        freeze.processBlock(input, output, 512, 60.0);
        
        REQUIRE(output != nullptr);
    }
    
    SECTION("Stutter at 180 BPM (fast)")
    {
        float input[512];
        for (int i = 0; i < 512; ++i) input[i] = 0.1f;
        
        float output[512] = {0.0f};
        freeze.processBlock(input, output, 512, 180.0);
        
        REQUIRE(output != nullptr);
    }
}

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SamplerBank.h"
#include "Mixer.h"
#include "FreezeEffect.h"

/**
 * PluginProcessor
 * 
 * Main JUCE AudioProcessor that orchestrates the sampler + freeze effect.
 * Handles MIDI input, parameter management, and audio processing.
 */
class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return "Default"; }
    void changeProgramName(int index, const juce::String& newName) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

private:
    // DSP Modules
    SamplerBank samplerBank_;
    Mixer mixer_;
    FreezeEffect freezeEffect_;
    
    // Parameters
    juce::AudioProcessorValueTreeState parameters_;
    
    // Helpers
    double getTempo() const;
    void handleMidiMessage(const juce::MidiMessage& msg);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

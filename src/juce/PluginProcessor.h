#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
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

    // Public API for the editor
    juce::AudioProcessorValueTreeState& getAPVTS() { return parameters_; }
    void loadSample(int index, const juce::File& file);
    juce::String getSampleName(int index) const;
    const std::vector<float>& getSampleWaveform(int index) const;

private:
    // DSP Modules
    SamplerBank samplerBank_;
    Mixer mixer_;
    FreezeEffect freezeEffect_;
    
    // Parameters
    juce::AudioProcessorValueTreeState parameters_;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Audio file loading
    juce::AudioFormatManager formatManager_;
    juce::String sampleNames_[4];
    std::vector<float> sampleWaveforms_[4];  // Copy for UI waveform display

    // MIDI note-to-sample mapping: C1(24), C2(36), C3(48), C4(60)
    static constexpr int kMidiNotes[4] = { 24, 36, 48, 60 };
    static constexpr double kStutterValues[5] = { 1.0, 0.5, 0.25, 0.125, 0.0625 };
    static int midiNoteToSampleIndex(int noteNumber);

    // Helpers
    double getTempo() const;
    void handleMidiNoteOn(const juce::MidiMessage& msg);
    void handleMidiNoteOff(const juce::MidiMessage& msg);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

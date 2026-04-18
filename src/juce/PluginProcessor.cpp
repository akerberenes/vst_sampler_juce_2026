#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, juce::Identifier("SamplerParams"),
                  juce::AudioProcessorValueTreeState::ParameterLayout())
{
}

PluginProcessor::~PluginProcessor()
{
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    samplerBank_.prepare(static_cast<int>(sampleRate), samplesPerBlock);
    freezeEffect_.prepare(static_cast<int>(sampleRate), samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Handle MIDI
    for (auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            handleMidiMessage(msg);
    }

    // Get input audio
    float* inputAudio = buffer.getWritePointer(0);
    
    // Process samplers
    float samplerOutput[4096];
    std::fill(samplerOutput, samplerOutput + buffer.getNumSamples(), 0.0f);
    samplerBank_.processBlock(samplerOutput, buffer.getNumSamples(), getTempo());
    
    // Mix input + samplers
    float mixedOutput[4096];
    mixer_.processBlock(inputAudio, samplerOutput, mixedOutput, buffer.getNumSamples());
    
    // Process freeze effect
    freezeEffect_.processBlock(mixedOutput, buffer.getWritePointer(0), 
                                buffer.getNumSamples(), getTempo());
    
    // Copy to stereo output
    if (totalNumOutputChannels > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor(*this);
}

const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
}

double PluginProcessor::getTempo() const
{
    if (auto playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                return bpm.value();
        }
    }
    return 120.0;
}

void PluginProcessor::handleMidiMessage(const juce::MidiMessage& msg)
{
    int noteNumber = msg.getNoteNumber();
    int sampleIndex = (noteNumber - 60) % 4;  // Map to samples 0-3
    
    if (sampleIndex >= 0 && sampleIndex < 4)
    {
        // Trigger with 1 bar duration (4 beats)
        samplerBank_.triggerSample(sampleIndex, 4.0, getTempo());
    }
}

// Audio plugin instantiation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

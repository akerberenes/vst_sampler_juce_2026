#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, juce::Identifier("SamplerParams"),
                  createParameterLayout())
{
    formatManager_.registerBasicFormats();
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
            handleMidiNoteOn(msg);
        else if (msg.isNoteOff())
            handleMidiNoteOff(msg);
    }

    int numSamples = buffer.getNumSamples();
    if (numSamples > 4096)
        return;

    // Read parameters and apply to DSP modules
    bool frozen = *parameters_.getRawParameterValue("freeze") > 0.5f;
    int stutterIdx = (int)*parameters_.getRawParameterValue("stutterRate");
    float speed = *parameters_.getRawParameterValue("speed");
    float dryWet = *parameters_.getRawParameterValue("dryWet");
    float loopStart = *parameters_.getRawParameterValue("loopStart");
    float loopEnd = *parameters_.getRawParameterValue("loopEnd");
    bool parallel = *parameters_.getRawParameterValue("parallelMode") > 0.5f;
    float inputLevel = *parameters_.getRawParameterValue("inputLevel");
    float samplerLevel = *parameters_.getRawParameterValue("samplerLevel");

    freezeEffect_.setFrozen(frozen);
    freezeEffect_.setStutterFraction(kStutterValues[stutterIdx]);
    freezeEffect_.setPlaybackSpeed(speed);
    freezeEffect_.setDryWetMix(dryWet);
    freezeEffect_.setLoopStart(loopStart);
    freezeEffect_.setLoopEnd(loopEnd);
    mixer_.setMode(parallel ? Mixer::Mode::Parallel : Mixer::Mode::Sequential);
    mixer_.setInputLevel(inputLevel);
    mixer_.setSamplerLevel(samplerLevel);

    // Apply per-sample start/end fractions
    for (int i = 0; i < 4; ++i)
    {
        auto si = juce::String(i);
        float ss = *parameters_.getRawParameterValue("sampleStart" + si);
        float se = *parameters_.getRawParameterValue("sampleEnd" + si);
        samplerBank_.setSampleStartFraction(i, ss);
        samplerBank_.setSampleEndFraction(i, se);
    }

    // Get input audio — copy to temp buffer because we write back to the same DAW buffer
    float inputCopy[4096];
    std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + numSamples, inputCopy);
    
    // Generate sampler output
    float samplerOutput[4096];
    std::fill(samplerOutput, samplerOutput + numSamples, 0.0f);
    samplerBank_.processBlock(samplerOutput, numSamples, getTempo());
    
    // Mix according to mode (sequential: input+sampler, parallel: sampler only)
    float mixedOutput[4096];
    mixer_.processBlock(inputCopy, samplerOutput, mixedOutput, numSamples);
    
    // Apply freeze effect
    float freezeOutput[4096];
    freezeEffect_.processBlock(mixedOutput, freezeOutput, numSamples, getTempo());
    
    // Write final output to DAW buffer
    float* outputChannel = buffer.getWritePointer(0);
    if (parallel)
    {
        // Parallel mode: frozen sampler + dry input
        for (int i = 0; i < numSamples; ++i)
            outputChannel[i] = freezeOutput[i] + inputCopy[i] * inputLevel;
    }
    else
    {
        // Sequential mode: freeze output is the final mix
        std::copy(freezeOutput, freezeOutput + numSamples, outputChannel);
    }
    
    // Copy mono to stereo
    if (totalNumOutputChannels > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
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
                return *bpm;
        }
    }
    return 120.0;
}

int PluginProcessor::midiNoteToSampleIndex(int noteNumber)
{
    for (int i = 0; i < 4; ++i)
        if (noteNumber == kMidiNotes[i])
            return i;
    return -1;
}

void PluginProcessor::handleMidiNoteOn(const juce::MidiMessage& msg)
{
    int idx = midiNoteToSampleIndex(msg.getNoteNumber());
    if (idx >= 0)
        samplerBank_.triggerSample(idx, 4.0, getTempo());
}

void PluginProcessor::handleMidiNoteOff(const juce::MidiMessage& msg)
{
    int idx = midiNoteToSampleIndex(msg.getNoteNumber());
    if (idx < 0)
        return;

    juce::String paramId = "obeyNoteOff" + juce::String(idx);
    bool obey = *parameters_.getRawParameterValue(paramId) > 0.5f;
    if (obey)
        samplerBank_.getSample(idx).stop();
}

void PluginProcessor::loadSample(int index, const juce::File& file)
{
    if (index < 0 || index >= 4)
        return;

    auto* reader = formatManager_.createReaderFor(file);
    if (!reader)
        return;

    int numSamples = (int)reader->lengthInSamples;
    juce::AudioSampleBuffer tempBuffer((int)reader->numChannels, numSamples);
    reader->read(&tempBuffer, 0, numSamples, 0, true, true);
    delete reader;

    // Pass first channel (mono) to DSP layer
    samplerBank_.loadSampleData(index, tempBuffer.getReadPointer(0), numSamples);
    sampleNames_[index] = file.getFileNameWithoutExtension();

    // Store waveform copy for UI display
    const float* ch0 = tempBuffer.getReadPointer(0);
    sampleWaveforms_[index].assign(ch0, ch0 + numSamples);
}

juce::String PluginProcessor::getSampleName(int index) const
{
    if (index < 0 || index >= 4)
        return {};
    return sampleNames_[index];
}

const std::vector<float>& PluginProcessor::getSampleWaveform(int index) const
{
    static const std::vector<float> empty;
    if (index < 0 || index >= 4)
        return empty;
    return sampleWaveforms_[index];
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Freeze controls
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"freeze", 1}, "Freeze", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"stutterRate", 1}, "Stutter Rate",
        juce::StringArray{"1 beat", "1/2 beat", "1/4 beat", "1/8 beat", "1/16 beat"}, 3));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"speed", 1}, "Speed",
        juce::NormalisableRange<float>(0.25f, 4.0f, 0.01f, 0.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dryWet", 1}, "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"loopStart", 1}, "Loop Start",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"loopEnd", 1}, "Loop End",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    // Per-sample controls (obeyNoteOff, sampleStart, sampleEnd for each of 4 samples)
    for (int i = 0; i < 4; ++i)
    {
        auto si = juce::String(i);
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"obeyNoteOff" + si, 1},
            "Obey Note Off " + juce::String(i + 1), false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"sampleStart" + si, 1},
            "Sample Start " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"sampleEnd" + si, 1},
            "Sample End " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    }

    // Mixer controls
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"parallelMode", 1}, "Parallel Mode", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"inputLevel", 1}, "Input Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"samplerLevel", 1}, "Sampler Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    return { params.begin(), params.end() };
}

// Audio plugin instantiation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

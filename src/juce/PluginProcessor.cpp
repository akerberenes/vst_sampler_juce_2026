#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    // Tell JUCE what I/O buses this plugin uses.
    // "withInput" + "withOutput" declare one stereo input and one stereo output;
    // the `true` argument marks them as active by default.
    : AudioProcessor(BusesProperties()
        .withInput("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      // Initialise the APVTS (parameter store).
      // Arguments: the AudioProcessor owning it, an undo manager (nullptr = none),
      // a state identifier string, and the parameter layout produced by our factory.
      parameters_(*this, nullptr, juce::Identifier("SamplerParams"),
                  createParameterLayout())
{
    // Register all built-in JUCE decoders (WAV, AIFF, MP3, FLAC, etc.).
    // Without this call, createReaderFor() would always return nullptr.
    formatManager_.registerBasicFormats();

    // Connect the TeensyMenu to the SamplerBank so effect assignments reach the samplers.
    teensyMenu_.setSamplerBank(&samplerBank_);

    // Cache per-sample APVTS parameter ID strings (avoids rebuilding them every block).
    for (int i = 0; i < 4; ++i)
    {
        auto si = juce::String(i);
        sampleParamIds_[i].loopPos = "sampleLoopPos" + si;
        sampleParamIds_[i].loopLen = "sampleLoopLen" + si;
        sampleParamIds_[i].gain    = "sampleGain" + si;
    }

    // --- Preset file directory ---
    // Presets are stored as XML in <appdata>/SamplerWithFreeze/Presets/
    presetDir_ = juce::File::getSpecialLocation(
                     juce::File::userApplicationDataDirectory)
                     .getChildFile("SamplerWithFreeze")
                     .getChildFile("Presets");
    presetDir_.createDirectory();

    // --- Preset save/reload/load callbacks ---
    teensyMenu_.onSave = [this]
    {
        savePresetToFile(teensyMenu_.getPresetName());
    };

    teensyMenu_.onReload = [this]
    {
        loadPresetFromFile(teensyMenu_.getPresetName());
    };

    teensyMenu_.onLoadPreset = [this](int presetIndex)
    {
        juce::String name = "Preset" + juce::String(presetIndex + 1);
        loadPresetFromFile(name.toStdString());
    };

    // Mark the TeensyMenu dirty whenever any APVTS parameter changes.
    parameters_.state.addListener(this);

    // Create default presets if they don't exist yet.
    // Preset1 = initial state snapshot; Preset2 = a dummy alternative.
    juce::File preset1 = presetDir_.getChildFile("Preset1.xml");
    if (!preset1.existsAsFile())
        savePresetToFile("Preset1");

    juce::File preset2 = presetDir_.getChildFile("Preset2.xml");
    if (!preset2.existsAsFile())
        savePresetToFile("Preset2");
}

PluginProcessor::~PluginProcessor()
{
    parameters_.state.removeListener(this);
}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    samplerBank_.prepare(static_cast<int>(sampleRate), samplesPerBlock);
    freezeEffect_.prepare(static_cast<int>(sampleRate), samplesPerBlock);

    // Pre-allocate audio processing buffers (avoids 16+ KB on the stack each block).
    inputCopy_.resize(samplesPerBlock);
    samplerOutput_.resize(samplesPerBlock);
    mixedOutput_.resize(samplesPerBlock);
    freezeOutput_.resize(samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    // Called when the DAW stops audio. Our DSP modules hold their data until
    // the next prepareToPlay, so nothing needs to be freed here.
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Accept mono or stereo I/O, but require the input and output channel count
    // to match (we don't upmix or downmix internally).
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // --- Zero unused output channels ---
    // If the DAW provides more output channels than inputs, clear the extras
    // so we don't output stale data from previous blocks.
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // --- Handle MIDI ---
    // Iterate every MIDI message in this block's MidiBuffer.
    // MidiMessage::isNoteOn/Off lets us distinguish note events from CCs etc.
    for (auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            handleMidiNoteOn(msg);
        else if (msg.isNoteOff())
            handleMidiNoteOff(msg);
    }

    int numSamples = buffer.getNumSamples();
    // Guard: buffer must fit our pre-allocated arrays.
    if (numSamples > static_cast<int>(inputCopy_.size()))
        return;

    // --- Read APVTS parameters ---
    // getRawParameterValue returns an atomic<float>* that the APVTS owns.
    // Dereferencing it is thread-safe (no lock needed) because it's updated atomically.
    bool  frozen      = *parameters_.getRawParameterValue("freeze")      > 0.5f;
    int   stutterIdx  = (int)*parameters_.getRawParameterValue("stutterRate");
    float speed       = *parameters_.getRawParameterValue("speed");
    float loopLength  = *parameters_.getRawParameterValue("loopLength");
    float loopPos     = *parameters_.getRawParameterValue("loopPosition");
    bool  parallel    = *parameters_.getRawParameterValue("parallelMode")  > 0.5f;
    float inputLevel  = *parameters_.getRawParameterValue("inputLevel");
    float samplerLevel= *parameters_.getRawParameterValue("samplerLevel");

    // Push parameters into DSP modules (one set per block).
    freezeEffect_.setFrozen(frozen);
    freezeEffect_.setStutterFraction(kStutterValues[stutterIdx]);  // Index into lookup table.
    freezeEffect_.setPlaybackSpeed(speed);
    freezeEffect_.setLoopLength(loopLength);
    freezeEffect_.setLoopPosition(loopPos);
    mixer_.setMode(parallel ? Mixer::Mode::Parallel : Mixer::Mode::Sequential);
    mixer_.setInputLevel(inputLevel);
    mixer_.setSamplerLevel(samplerLevel);

    // --- Per-sample start/end region ---
    // Each pad has independent start/end markers set by the waveform display.
    // Read them from APVTS and push to SamplerBank each block.
    for (int i = 0; i < 4; ++i)
    {
        float loopPos = *parameters_.getRawParameterValue(sampleParamIds_[i].loopPos);
        float loopLen = *parameters_.getRawParameterValue(sampleParamIds_[i].loopLen);
        float ss = loopPos;
        float se = std::min(1.0f, loopPos + loopLen);
        float sg = *parameters_.getRawParameterValue(sampleParamIds_[i].gain);
        samplerBank_.setSampleStartFraction(i, ss);
        samplerBank_.setSampleEndFraction(i, se);
        samplerBank_.setSampleGain(i, sg);
    }

    // --- Copy input BEFORE writing output ---
    std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + numSamples, inputCopy_.data());

    // --- Generate sampler audio ---
    std::fill(samplerOutput_.begin(), samplerOutput_.begin() + numSamples, 0.0f);
    samplerBank_.processBlock(samplerOutput_.data(), numSamples, getTempo());

    // --- Route through Mixer ---
    mixer_.processBlock(inputCopy_.data(), samplerOutput_.data(), mixedOutput_.data(), numSamples);

    // --- Apply freeze effect ---
    freezeEffect_.processBlock(mixedOutput_.data(), freezeOutput_.data(), numSamples, getTempo());

    // --- Write final output to DAW buffer ---
    float* outputChannel = buffer.getWritePointer(0);
    if (parallel)
    {
        for (int i = 0; i < numSamples; ++i)
            outputChannel[i] = freezeOutput_[i] + inputCopy_[i] * inputLevel;
    }
    else
    {
        std::copy(freezeOutput_.data(), freezeOutput_.data() + numSamples, outputChannel);
    }

    // --- Copy mono result to second channel for stereo output ---
    // We process in mono internally; duplicate channel 0 to channel 1 so DAWs
    // that expect stereo get valid audio on both channels.
    if (totalNumOutputChannels > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    // Return a new editor window. JUCE takes ownership and deletes it when the
    // GUI is closed. The editor is passed `*this` so it can call getAPVTS() etc.
    return new PluginEditor(*this);
}

const juce::String PluginProcessor::getName() const
{
    // JucePlugin_Name is a macro defined by JUCE based on the PLUGIN_NAME
    // CMakeLists property ("Sampler With Freeze").
    return JucePlugin_Name;
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Serialise APVTS parameters into XML.
    auto state = parameters_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Append sample file paths as child elements so they survive save/restore.
    for (int i = 0; i < 4; ++i)
    {
        if (samplePaths_[i].isNotEmpty())
        {
            auto* sampleEl = xml->createNewChildElement("SamplePath");
            sampleEl->setAttribute("index", i);
            sampleEl->setAttribute("path", samplePaths_[i]);
        }
    }

    // Append TeensyMenu per-sample effect state.
    for (int i = 0; i < 4; ++i)
    {
        auto* effectEl = xml->createNewChildElement("SampleEffect");
        effectEl->setAttribute("index", i);
        effectEl->setAttribute("effectIndex", teensyMenu_.getSelectedEffectIndex(i));
        effectEl->setAttribute("paramValue", static_cast<double>(teensyMenu_.getEffectParamValue(i)));
    }

    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    // Restore APVTS parameters.
    if (xmlState->hasTagName(parameters_.state.getType()))
        parameters_.replaceState(juce::ValueTree::fromXml(*xmlState));

    // Reload sample files from stored paths.
    for (auto* child : xmlState->getChildWithTagNameIterator("SamplePath"))
    {
        int index = child->getIntAttribute("index", -1);
        juce::String path = child->getStringAttribute("path");
        if (index >= 0 && index < 4 && path.isNotEmpty())
        {
            juce::File file(path);
            if (file.existsAsFile())
                loadSample(index, file);
        }
    }

    // Restore TeensyMenu per-sample effect state.
    for (auto* child : xmlState->getChildWithTagNameIterator("SampleEffect"))
    {
        int index = child->getIntAttribute("index", -1);
        int effectIndex = child->getIntAttribute("effectIndex", 0);
        float paramValue = static_cast<float>(child->getDoubleAttribute("paramValue", 0.5));
        if (index >= 0 && index < 4)
            teensyMenu_.setEffectForSample(index, effectIndex, paramValue);
    }
}

double PluginProcessor::getTempo() const
{
    // Ask the DAW's playhead for the current BPM.
    // getPlayHead() may return nullptr if no playhead is available (e.g. offline rendering).
    // getPosition() may also return nullopt during initialisation.
    // If either is unavailable, fall back to 120 BPM.
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
    // Linear search through the 4-element kMidiNotes array.
    // Returns the matching index (0-3) or -1 if not found.
    for (int i = 0; i < 4; ++i)
        if (noteNumber == kMidiNotes[i])
            return i;
    return -1;
}

void PluginProcessor::handleMidiNoteOn(const juce::MidiMessage& msg)
{
    // Map MIDI note number to a pad index (0-3).
    int idx = midiNoteToSampleIndex(msg.getNoteNumber());
    if (idx >= 0)
        // Trigger 4 beats of playback. The sampler stops itself after 4 beats
        // (unless loop mode is enabled, in which case it wraps indefinitely
        //  until the countdown expires or stop() is called).
        samplerBank_.triggerSample(idx, 4.0, getTempo());
}

void PluginProcessor::handleMidiNoteOff(const juce::MidiMessage& msg)
{
    int idx = midiNoteToSampleIndex(msg.getNoteNumber());
    if (idx < 0)
        return;

    // Each pad has its own "obeyNoteOff" boolean parameter.
    // If true: stop playback when the MIDI note is released (keyboard-style behaviour).
    // If false: the sample plays its full beat-duration regardless of note release.
    juce::String paramId = "obeyNoteOff" + juce::String(idx);
    bool obey = *parameters_.getRawParameterValue(paramId) > 0.5f;
    if (obey)
        samplerBank_.getSample(idx).stop();
}

void PluginProcessor::triggerPad(int index)
{
    if (index >= 0 && index < 4)
        samplerBank_.triggerSample(index, 4.0, getTempo());
}

void PluginProcessor::stopPad(int index)
{
    if (index < 0 || index >= 4)
        return;
    juce::String paramId = "obeyNoteOff" + juce::String(index);
    bool obey = *parameters_.getRawParameterValue(paramId) > 0.5f;
    if (obey)
        samplerBank_.getSample(index).stop();
}

void PluginProcessor::loadSample(int index, const juce::File& file)
{
    if (index < 0 || index >= 4)
        return;

    // createReaderFor detects the file format from the extension and creates
    // the appropriate AudioFormatReader (e.g. WavAudioFormatReader).
    // Returns nullptr if the format is unsupported or the file doesn't exist.
    auto* reader = formatManager_.createReaderFor(file);
    if (!reader)
        return;

    // Decode the entire file into a temporary JUCE AudioSampleBuffer.
    // The buffer has one row per channel and `numSamples` columns (frames).
    int numSamples = (int)reader->lengthInSamples;
    juce::AudioSampleBuffer tempBuffer((int)reader->numChannels, numSamples);
    reader->read(&tempBuffer, 0, numSamples, 0, true, true);
    delete reader;  // Must be deleted manually (raw pointer returned by JUCE).

    // Pass channel 0 (mono) to the DSP layer.
    // Multi-channel files are silently downmixed to mono here.
    samplerBank_.loadSampleData(index, tempBuffer.getReadPointer(0), numSamples);
    sampleNames_[index] = file.getFileNameWithoutExtension();
    samplePaths_[index] = file.getFullPathName();

    // Copy channel 0 into our own vector for the WaveformDisplay UI component.
    // The UI reads this on the main thread; processBlock never reads sampleWaveforms_.
    const float* ch0 = tempBuffer.getReadPointer(0);
    sampleWaveforms_[index].assign(ch0, ch0 + numSamples);

    teensyMenu_.setDirty(true);
}

juce::String PluginProcessor::getSampleName(int index) const
{
    if (index < 0 || index >= 4)
        return {};
    return sampleNames_[index];
}

const std::vector<float>& PluginProcessor::getSampleWaveform(int index) const
{
    // Return a reference to the stored waveform (read-only for the caller).
    // The static empty vector is returned for out-of-range indices so the
    // caller always gets a valid reference (never a dangling ref).
    static const std::vector<float> empty;
    if (index < 0 || index >= 4)
        return empty;
    return sampleWaveforms_[index];
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    // Build the list of all parameters that the APVTS will manage.
    // Each parameter has:
    //   - A ParameterID: a string ID used everywhere to look it up, plus a version number.
    //   - A display name shown in the DAW's automation list.
    //   - A range/type and a default value.
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // --- Freeze controls ---

    // Toggle: is the buffer frozen right now?
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"freeze", 1}, "Freeze", false));

    // Stutter rate: index into kStutterValues[]. Presented as a choice (drop-down).
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{"stutterRate", 1}, "Stutter Rate",
        juce::StringArray{"1 beat", "1/2 beat", "1/4 beat", "1/8 beat", "1/16 beat", "1/32 beat", "1/64 beat"}, 3));
    // Default index 3 = "1/8 beat" (kStutterValues[3] = 0.125).

    // Playback speed: 0.25x to 4x with a skew factor of 0.5 (more resolution near 1x).
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"speed", 1}, "Speed",
        juce::NormalisableRange<float>(0.25f, 4.0f, 0.01f, 0.5f), 1.0f));

    // Loop length: fraction of the freeze buffer used as the loop window (0.0–1.0).
    // 1.0 = full buffer (default); reducing this shrinks the loop region.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"loopLength", 1}, "Loop Length",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    // Loop position: shifts the start (and end) of the window within the buffer (0.0–1.0).
    // Clamped so the window never exceeds the buffer end given the current loop length.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"loopPosition", 1}, "Loop Position",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // --- Per-sample controls (4 pads × 3 parameters each) ---
    for (int i = 0; i < 4; ++i)
    {
        auto si = juce::String(i);

        // Stop playback when the MIDI note is released (toggle per pad).
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{"obeyNoteOff" + si, 1},
            "Obey Note Off " + juce::String(i + 1), false));

        // Per-pad loop window: position and length (fraction 0.0-1.0).
        // loopPosition: start of the active window within the loaded sample.
        // loopLength:   size of the active window (1.0 = full sample, default).
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"sampleLoopPos" + si, 1},
            "Loop Position " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"sampleLoopLen" + si, 1},
            "Loop Length " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

        // Per-pad output volume. 0.0 = silence, 1.0 = unity gain, 2.0 = +6 dB.
        // The slider range matches Sampler::setGain()'s accepted range.
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{"sampleGain" + si, 1},
            "Sample Gain " + juce::String(i + 1),
            juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f));
    }

    // --- Mixer controls ---

    // Switch between sequential (input+sampler mixed) and parallel routing.
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"parallelMode", 1}, "Parallel Mode", false));

    // Gain knobs for the input and sampler buses.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"inputLevel", 1}, "Input Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"samplerLevel", 1}, "Sampler Level",
        juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    // --- Teensy UI emulation knobs ---
    // These 3 parameters drive the TeensyMenu state machine.

    // Page knob: 0.0–1.0 mapped to 5 pages (Sample1–4, Preset).
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"teensyPage", 1}, "Teensy Page",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Param knob: 0.0–1.0 mapped to effect selection or save/reload.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"teensyParam", 1}, "Teensy Param",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Value knob: 0.0–1.0 sets the selected effect's parameter.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"teensyValue", 1}, "Teensy Value",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // ParameterLayout's constructor accepts a begin/end iterator pair.
    return { params.begin(), params.end() };
}

// ---------------------------------------------------------------------------
// File-based preset save / load
// ---------------------------------------------------------------------------
void PluginProcessor::savePresetToFile(const std::string& presetName)
{
    // Build XML containing APVTS state, sample paths, and per-sample effects.
    auto state = parameters_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    for (int i = 0; i < 4; ++i)
    {
        if (samplePaths_[i].isNotEmpty())
        {
            auto* el = xml->createNewChildElement("SamplePath");
            el->setAttribute("index", i);
            el->setAttribute("path", samplePaths_[i]);
        }
    }

    for (int i = 0; i < 4; ++i)
    {
        auto* el = xml->createNewChildElement("SampleEffect");
        el->setAttribute("index", i);
        el->setAttribute("effectIndex", teensyMenu_.getSelectedEffectIndex(i));
        el->setAttribute("paramValue",
                         static_cast<double>(teensyMenu_.getEffectParamValue(i)));
    }

    juce::File file = presetDir_.getChildFile(juce::String(presetName) + ".xml");
    xml->writeTo(file);
}

void PluginProcessor::loadPresetFromFile(const std::string& presetName)
{
    juce::File file = presetDir_.getChildFile(juce::String(presetName) + ".xml");
    if (!file.existsAsFile())
    {
        // File doesn't exist yet — still update the name so the UI reflects it.
        teensyMenu_.setPresetName(presetName);
        teensyMenu_.setDirty(false);
        return;
    }

    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr)
        return;

    // Restore APVTS.
    if (xml->hasTagName(parameters_.state.getType()))
        parameters_.replaceState(juce::ValueTree::fromXml(*xml));

    // Restore sample files.
    for (auto* child : xml->getChildWithTagNameIterator("SamplePath"))
    {
        int index = child->getIntAttribute("index", -1);
        juce::String path = child->getStringAttribute("path");
        if (index >= 0 && index < 4 && path.isNotEmpty())
        {
            juce::File f(path);
            if (f.existsAsFile())
                loadSample(index, f);
        }
    }

    // Restore per-sample effects.
    for (auto* child : xml->getChildWithTagNameIterator("SampleEffect"))
    {
        int index = child->getIntAttribute("index", -1);
        int effectIndex = child->getIntAttribute("effectIndex", 0);
        float paramValue = static_cast<float>(
            child->getDoubleAttribute("paramValue", 0.5));
        if (index >= 0 && index < 4)
            teensyMenu_.setEffectForSample(index, effectIndex, paramValue);
    }

    // Update the menu's preset name and clear dirty flag.
    teensyMenu_.setPresetName(presetName);
    teensyMenu_.setDirty(false);
}

// --- JUCE plugin entry point ---
// The DAW calls createPluginFilter() to instantiate the plugin.
// It must be a free function with C linkage (JUCE_CALLTYPE = __stdcall on Windows).
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}

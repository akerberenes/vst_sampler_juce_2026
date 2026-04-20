#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "SamplerBank.h"
#include "Mixer.h"
#include "FreezeEffect.h"

/**
 * PluginProcessor
 *
 * The central hub of the plugin. JUCE calls this class's methods to drive the
 * entire audio pipeline, respond to MIDI, and save/restore plugin state.
 *
 * --- JUCE AudioProcessor contract ---
 * Every JUCE plugin must subclass AudioProcessor and override a set of
 * methods. The DAW calls them in a defined order:
 *
 *   prepareToPlay(sampleRate, blockSize)  -- called once before audio starts;
 *                                            allocate resources here.
 *   processBlock(buffer, midiMessages)    -- called every audio cycle;
 *                                            read/write audio here.
 *   releaseResources()                    -- called when audio stops;
 *                                            free resources here.
 *
 * --- Signal flow (per audio block) ---
 *
 *   MIDI in         → handleMidiNoteOn/Off → SamplerBank::trigger / stop
 *
 *   DAW audio in    ─┬──────────────────────────────┬─ (parallel mode)
 *   SamplerBank     ─┴─ Mixer::processBlock ── FreezeEffect ──┴─ DAW out
 *
 * --- AudioProcessorValueTreeState (APVTS) ---
 * All plugin parameters (freeze, speed, etc.) live in `parameters_`.
 * The APVTS connects UI sliders/buttons (attachments in the editor) to the
 * underlying float/bool values, and handles automation and state save/load.
 *
 * getRawParameterValue(id) returns an atomic<float>* that processBlock reads
 * without locking -- this is safe because APVTS atomics are lock-free.
 *
 * --- Thread safety summary ---
 * - processBlock runs on the real-time audio thread (no allocation, no locking).
 * - loadSample, getSampleWaveform run on the main thread.
 * - APVTS parameters are updated by the UI thread and read by the audio thread
 *   via atomic<float>* -- this is safe by design.
 */
class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    // --- JUCE AudioProcessor overrides ---

    // Called by the DAW before playback starts. Initialise all DSP here.
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    // Called by the DAW when audio stops. Free any resources allocated in prepareToPlay.
    void releaseResources() override;

    // Returns true if the given channel layout is supported.
    // We accept mono or stereo I/O but require input and output to match.
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // Called every audio block. This is the real-time audio engine:
    //   - reads MIDI messages
    //   - reads APVTS parameters
    //   - generates sampler audio
    //   - routes through the Mixer
    //   - applies the FreezeEffect
    //   - writes the result back into `buffer`
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Returns a pointer to a new PluginEditor (the GUI window).
    // JUCE manages the lifecycle of the editor.
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // --- Plugin metadata ---

    // Plugin name as defined in CMakeLists.txt (PLUGIN_NAME property).
    const juce::String getName() const override;

    bool acceptsMidi()  const override { return true; }   // We need MIDI to trigger pads.
    bool producesMidi() const override { return false; }  // We don't generate MIDI events.
    bool isMidiEffect() const override { return false; }  // We produce audio, not just MIDI.
    double getTailLengthSeconds() const override { return 0.0; }  // No reverb tail.

    // --- Programme / preset API (minimal) ---
    // JUCE requires these to be implemented; we use a single "Default" programme.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return "Default"; }
    void changeProgramName(int index, const juce::String& newName) override {}

    // --- State persistence ---
    // JUCE calls these to save/restore plugin state (e.g. when saving a DAW project).
    // getStateInformation serialises the APVTS; setStateInformation restores it.
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Public API for the editor ---

    // Provides the editor read/write access to all plugin parameters.
    juce::AudioProcessorValueTreeState& getAPVTS() { return parameters_; }

    // Load an audio file from disk onto pad slot `index` (0-3).
    // Reads with JUCE's AudioFormatManager, passes channel 0 to the DSP layer,
    // and stores a waveform copy for the UI.
    void loadSample(int index, const juce::File& file);

    // Returns the filename (without extension) of the loaded sample on pad `index`.
    juce::String getSampleName(int index) const;

    // Returns a reference to the per-channel float data copy used to draw the waveform.
    // Read-only; the vector is updated by loadSample() on the main thread.
    const std::vector<float>& getSampleWaveform(int index) const;

    // Copies the current freeze buffer content into `out` for the waveform display.
    // Best-effort snapshot (no lock); tearing is acceptable for visualisation.
    void getFreezeBufferSnapshot(std::vector<float>& out) const
    {
        freezeEffect_.copyBufferSnapshot(out);
    }

    // Trigger sample playback on pad `index` (0-3) from the UI.
    // Same behaviour as a MIDI note-on: plays 4 beats at the current tempo.
    void triggerPad(int index);

    // Stop sample playback on pad `index` (0-3) if its "obeyNoteOff" param is true.
    // Same behaviour as a MIDI note-off.
    void stopPad(int index);

private:
    // --- DSP pipeline ---

    // 4-pad sample playback engine.
    SamplerBank samplerBank_;

    // Routes audio between sampler and freeze based on mode (sequential/parallel).
    Mixer mixer_;

    // Echo-freeze effect (ring buffer + stutter + speed + dry/wet).
    FreezeEffect freezeEffect_;

    // --- Parameter management ---

    // APVTS: owns all plugin parameters and connects them to UI attachments.
    // Constructed with a ParameterLayout produced by createParameterLayout().
    juce::AudioProcessorValueTreeState parameters_;

    // Factory function that declares every parameter ID, range, and default value.
    // Called once from the PluginProcessor constructor via APVTS initialisation.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // --- Audio file support ---

    // JUCE's AudioFormatManager knows how to read WAV, AIFF, MP3, FLAC, etc.
    // It creates an AudioFormatReader for a given file extension.
    juce::AudioFormatManager formatManager_;

    // Display name for each loaded sample (filename without extension).
    juce::String sampleNames_[4];

    // Full file paths of loaded samples (used for state persistence).
    juce::String samplePaths_[4];

    // Per-pad waveform data (copy of channel 0) for the WaveformDisplay UI component.
    // Populated on load; read by the UI on the main thread.
    std::vector<float> sampleWaveforms_[4];

    // --- MIDI mapping constants ---

    // The MIDI note numbers that map to pads 0-3.
    // C1=24, C2=36, C3=48, C4=60  (standard GM drum map layout).
    static constexpr int kMidiNotes[4] = { 24, 36, 48, 60 };

    // Stutter rate values indexed by the "stutterRate" AudioParameterChoice.
    // Each value is a fraction of one beat (1.0=whole beat, 0.015625=1/64 beat).
    // The array now has 7 entries matching the 7-item StringArray in createParameterLayout().
    static constexpr double kStutterValues[7] = { 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.015625 };

    // Convert a MIDI note number to a pad index (0-3), or -1 if not mapped.
    static int midiNoteToSampleIndex(int noteNumber);

    // --- Helpers ---

    // Read current project BPM from the DAW playhead. Returns 120.0 if unavailable.
    double getTempo() const;

    // Called per MIDI note-on event. Looks up the pad index and calls triggerSample.
    void handleMidiNoteOn(const juce::MidiMessage& msg);

    // Called per MIDI note-off event. Stops the pad if its "obeyNoteOff" param is true.
    void handleMidiNoteOff(const juce::MidiMessage& msg);

    // JUCE macro: disables copy construction and signals JUCE's leak detector.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

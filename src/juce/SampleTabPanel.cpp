#include "SampleTabPanel.h"
#include "PluginProcessor.h"

// MIDI note name displayed on each tab's label.
// Indices correspond to pads 0-3 and kMidiNotes[] {24,36,48,60} in PluginProcessor.
static const char* kNoteNames[] = { "C1", "C2", "C3", "C4" };

SampleTabPanel::SampleTabPanel(PluginProcessor& processor)
    : processor_(processor)
{
    for (int i = 0; i < 4; ++i)
    {
        // --- Tab header buttons (always visible) ---
        // setClickingTogglesState(false): clicking doesn't toggle the button state;
        // we manage the selected appearance manually in updateTabAppearance().
        addAndMakeVisible(tabButtons_[i]);
        tabButtons_[i].setButtonText("Sample " + juce::String(i + 1));
        tabButtons_[i].setClickingTogglesState(false);
        tabButtons_[i].onClick = [this, i] { selectTab(i); };

        // --- Waveform display ---
        // addChildComponent: adds the component but leaves it hidden.
        // Only the selected tab's components will be made visible by selectTab().
        addChildComponent(tabs_[i].waveform);

        // When the user drags a marker, push the new fraction directly to the APVTS
        // by calling setValueNotifyingHost(), which:
        //   1. Updates the atomic<float> that processBlock reads.
        //   2. Notifies the DAW automation system about the change.
        //   3. Triggers any other listeners attached to that parameter.
        tabs_[i].waveform.onRegionChanged = [this, i](float start, float end)
        {
            auto& apvts = processor_.getAPVTS();
            auto si = juce::String(i);
            if (auto* p = apvts.getParameter("sampleStart" + si))
                p->setValueNotifyingHost(p->convertTo0to1(start));
            if (auto* p = apvts.getParameter("sampleEnd" + si))
                p->setValueNotifyingHost(p->convertTo0to1(end));
        };

        // --- Obey Note Off toggle ---
        // The ButtonAttachment automatically syncs the toggle state with the APVTS
        // parameter "obeyNoteOff0"-"obeyNoteOff3". No manual getValue/setValue needed.
        addChildComponent(tabs_[i].obeyNoteOffButton);
        tabs_[i].obeyNoteOffButton.setButtonText("Obey Note Off");
        tabs_[i].obeyAttachment = std::make_unique<ButtonAttachment>(
            processor_.getAPVTS(), "obeyNoteOff" + juce::String(i),
            tabs_[i].obeyNoteOffButton);

        // --- Load button ---
        addChildComponent(tabs_[i].loadButton);
        tabs_[i].loadButton.setButtonText("Load Audio File...");
        tabs_[i].loadButton.onClick = [this, i] { loadButtonClicked(i); };

        // --- MIDI note label ---
        addChildComponent(tabs_[i].midiLabel);
        tabs_[i].midiLabel.setText(juce::String("MIDI: ") + kNoteNames[i],
                                    juce::dontSendNotification);
        tabs_[i].midiLabel.setFont(juce::Font(12.0f));
        tabs_[i].midiLabel.setColour(juce::Label::textColourId, juce::Colours::orange);

        // --- Sample name label ---
        addChildComponent(tabs_[i].nameLabel);
        auto name = processor_.getSampleName(i);
        tabs_[i].nameLabel.setText(name.isEmpty() ? "(no sample)" : name,
                                    juce::dontSendNotification);
        tabs_[i].nameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

        // --- Restore waveform if a sample is already loaded ---
        // This handles the case where the editor is reopened after loading samples
        // (e.g. the user closes and reopens the plugin window).
        const auto& wf = processor_.getSampleWaveform(i);
        if (!wf.empty())
        {
            tabs_[i].waveform.setWaveform(wf);
            // Restore marker positions from current APVTS values.
            auto si = juce::String(i);
            float ss = *processor_.getAPVTS().getRawParameterValue("sampleStart" + si);
            float se = *processor_.getAPVTS().getRawParameterValue("sampleEnd" + si);
            tabs_[i].waveform.setStartFraction(ss);
            tabs_[i].waveform.setEndFraction(se);
        }
    }

    // Show the first tab by default.
    selectTab(0);
}

void SampleTabPanel::selectTab(int index)
{
    selectedTab_ = index;

    // Show only the selected tab's content; hide all others.
    for (int i = 0; i < 4; ++i)
    {
        bool visible = (i == index);
        tabs_[i].waveform.setVisible(visible);
        tabs_[i].obeyNoteOffButton.setVisible(visible);
        tabs_[i].loadButton.setVisible(visible);
        tabs_[i].midiLabel.setVisible(visible);
        tabs_[i].nameLabel.setVisible(visible);
    }

    updateTabAppearance();  // Highlight the active tab button.
    resized();              // Re-layout to position the newly-visible components.
    repaint();
}

void SampleTabPanel::updateTabAppearance()
{
    // Colour the selected tab's button brighter to show it's active.
    for (int i = 0; i < 4; ++i)
    {
        if (i == selectedTab_)
        {
            tabButtons_[i].setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff3a3a3a));
            tabButtons_[i].setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
            tabButtons_[i].setColour(juce::TextButton::textColourOffId,  juce::Colours::white);
        }
        else
        {
            tabButtons_[i].setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff222222));
            tabButtons_[i].setColour(juce::TextButton::textColourOnId,   juce::Colours::grey);
            tabButtons_[i].setColour(juce::TextButton::textColourOffId,  juce::Colours::grey);
        }
    }
}

void SampleTabPanel::paint(juce::Graphics& g)
{
    // Fill the panel background.
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(getLocalBounds());

    // Draw a border around the content area (below the tab strip).
    g.setColour(juce::Colour(0xff444444));
    int tabH = 28;
    g.drawRect(0, tabH, getWidth(), getHeight() - tabH);
}

void SampleTabPanel::resized()
{
    int tabH = 28;   // Height of the tab button strip.
    int pad  = 8;
    int tabW = getWidth() / 4;

    // Position the 4 tab buttons across the top.
    for (int i = 0; i < 4; ++i)
        tabButtons_[i].setBounds(i * tabW, 0, tabW, tabH);

    // Position the selected tab's content components below the tab strip.
    int cy = tabH + pad;          // Y start of content area.
    int cw = getWidth() - pad * 2; // Width of content area.
    int ch = getHeight() - tabH - pad;

    int i = selectedTab_;
    int rowH = 24;

    // Row 1: MIDI label | sample name | load button
    tabs_[i].midiLabel.setBounds(pad, cy, 60, rowH);
    tabs_[i].nameLabel.setBounds(pad + 65, cy, cw - 200, rowH);
    tabs_[i].loadButton.setBounds(pad + cw - 130, cy, 130, rowH);
    cy += rowH + pad;

    // Row 2: Waveform display (takes the bulk of the available height)
    int wfH = ch - rowH * 2 - pad * 4;
    tabs_[i].waveform.setBounds(pad, cy, cw, wfH);
    cy += wfH + pad;

    // Row 3: Obey Note Off toggle
    tabs_[i].obeyNoteOffButton.setBounds(pad, cy, 160, rowH);
}

void SampleTabPanel::sampleLoaded(int index)
{
    if (index < 0 || index >= 4)
        return;

    // Update the waveform display with the newly loaded sample data.
    const auto& wf = processor_.getSampleWaveform(index);
    tabs_[index].waveform.setWaveform(wf);

    // Update the name label.
    auto name = processor_.getSampleName(index);
    tabs_[index].nameLabel.setText(name.isEmpty() ? "(no sample)" : name,
                                    juce::dontSendNotification);

    // Reset markers to full range (0.0 - 1.0) for the new sample.
    tabs_[index].waveform.setStartFraction(0.0f);
    tabs_[index].waveform.setEndFraction(1.0f);

    // Push the reset values to the APVTS so processBlock picks them up immediately.
    // convertTo0to1(1.0) returns 1.0 (normalised range for a 0-1 parameter).
    auto si = juce::String(index);
    auto& apvts = processor_.getAPVTS();
    if (auto* p = apvts.getParameter("sampleStart" + si))
        p->setValueNotifyingHost(0.0f);
    if (auto* p = apvts.getParameter("sampleEnd" + si))
        p->setValueNotifyingHost(1.0f);
}

void SampleTabPanel::loadButtonClicked(int index)
{
    // Build a file chooser that filters to common audio formats.
    // The title tells the user which pad slot they're loading.
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Select audio file for Sample " + juce::String(index + 1),
        juce::File{},
        "*.wav;*.mp3;*.flac;*.aif;*.aiff");

    // launchAsync opens the OS file picker without blocking the message thread.
    // The lambda is called when the user confirms or cancels.
    // `fileChooser_` must stay alive until the lambda fires, hence the member variable.
    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, index](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                // Decode and load the audio file into the DSP layer.
                processor_.loadSample(index, file);
                // Update the UI to show the new waveform and reset markers.
                sampleLoaded(index);
            }
        });
}

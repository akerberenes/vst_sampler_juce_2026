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

        // The waveform display shows the active region visually.
        // The region is now driven by the loop position / length sliders below,
        // so the drag-marker callback is intentionally left unset.

        // --- Volume slider ---
        // This slider maps directly to the "sampleGainN" APVTS parameter.
        // SliderAttachment syncs it automatically: moving the slider updates the
        // parameter; DAW automation updates the slider. We use addChildComponent
        // so it is hidden until this tab is selected (same pattern as above).
        addChildComponent(tabs_[i].volumeLabel);
        tabs_[i].volumeLabel.setText("Volume", juce::dontSendNotification);
        tabs_[i].volumeLabel.setFont(juce::Font(12.0f));
        tabs_[i].volumeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        tabs_[i].volumeLabel.setJustificationType(juce::Justification::centred);
        addChildComponent(tabs_[i].volumeSlider);
        tabs_[i].volumeSlider.setSliderStyle(juce::Slider::Rotary);
        tabs_[i].volumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 18);
        tabs_[i].gainAttachment = std::make_unique<SliderAttachment>(
            processor_.getAPVTS(), "sampleGain" + juce::String(i),
            tabs_[i].volumeSlider);

        // --- Obey Note Off toggle ---
        // The ButtonAttachment automatically syncs the toggle state with the APVTS
        // parameter "obeyNoteOff0"-"obeyNoteOff3". No manual getValue/setValue needed.
        addChildComponent(tabs_[i].obeyNoteOffButton);
        tabs_[i].obeyNoteOffButton.setButtonText("Obey Note Off");
        tabs_[i].obeyAttachment = std::make_unique<ButtonAttachment>(
            processor_.getAPVTS(), "obeyNoteOff" + juce::String(i),
            tabs_[i].obeyNoteOffButton);

        // --- Loop Position / Length sliders ---
        // These replace the draggable waveform markers as the primary way to set
        // the active playback region. Both are horizontal sliders (0.0–1.0).
        // loopPos = where the window starts; loopLen = how long it is.
        addChildComponent(tabs_[i].loopPosLabel);
        tabs_[i].loopPosLabel.setText("Loop Pos", juce::dontSendNotification);
        tabs_[i].loopPosLabel.setFont(juce::Font(12.0f));
        tabs_[i].loopPosLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        tabs_[i].loopPosLabel.setJustificationType(juce::Justification::centred);
        addChildComponent(tabs_[i].loopPosSlider);
        tabs_[i].loopPosSlider.setSliderStyle(juce::Slider::Rotary);
        tabs_[i].loopPosSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 18);
        tabs_[i].loopPosAttachment = std::make_unique<SliderAttachment>(
            processor_.getAPVTS(), "sampleLoopPos" + juce::String(i),
            tabs_[i].loopPosSlider);

        addChildComponent(tabs_[i].loopLenLabel);
        tabs_[i].loopLenLabel.setText("Loop Len", juce::dontSendNotification);
        tabs_[i].loopLenLabel.setFont(juce::Font(12.0f));
        tabs_[i].loopLenLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        tabs_[i].loopLenLabel.setJustificationType(juce::Justification::centred);
        addChildComponent(tabs_[i].loopLenSlider);
        tabs_[i].loopLenSlider.setSliderStyle(juce::Slider::Rotary);
        tabs_[i].loopLenSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 18);
        tabs_[i].loopLenAttachment = std::make_unique<SliderAttachment>(
            processor_.getAPVTS(), "sampleLoopLen" + juce::String(i),
            tabs_[i].loopLenSlider);

        // When either slider changes, update the waveform display markers so the
        // active region stays visually in sync with the knob values.
        // SliderAttachment fires onValueChange for both user drags and DAW automation.
        auto updateWaveformRegion = [this, i]()
        {
            float pos = (float)tabs_[i].loopPosSlider.getValue();
            float len = (float)tabs_[i].loopLenSlider.getValue();
            tabs_[i].waveform.setStartFraction(pos);
            tabs_[i].waveform.setEndFraction(juce::jmin(1.0f, pos + len));
        };
        tabs_[i].loopPosSlider.onValueChange = updateWaveformRegion;
        tabs_[i].loopLenSlider.onValueChange = updateWaveformRegion;

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
            // Restore marker positions from current APVTS loop pos/len values.
            auto si = juce::String(i);
            float pos = *processor_.getAPVTS().getRawParameterValue("sampleLoopPos" + si);
            float len = *processor_.getAPVTS().getRawParameterValue("sampleLoopLen" + si);
            tabs_[i].waveform.setStartFraction(pos);
            tabs_[i].waveform.setEndFraction(juce::jmin(1.0f, pos + len));
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
        tabs_[i].volumeLabel.setVisible(visible);
        tabs_[i].volumeSlider.setVisible(visible);
        tabs_[i].loopPosLabel.setVisible(visible);
        tabs_[i].loopPosSlider.setVisible(visible);
        tabs_[i].loopLenLabel.setVisible(visible);
        tabs_[i].loopLenSlider.setVisible(visible);
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

    // Row 2: Waveform display (takes the bulk of the available height).
    // Below it: one row of 3 knobs (loop pos, loop len, volume) + obey-note-off.
    int labelH = 14;
    int knobSz = 70;  // Square bounds for each rotary (arc + text box inside).
    int knobRowH = labelH + knobSz;
    int wfH = ch - knobRowH - rowH * 2 - pad * 4;
    tabs_[i].waveform.setBounds(pad, cy, cw, wfH);
    cy += wfH + pad;

    // Row 3: three rotary knobs side by side — Loop Pos | Loop Len | Volume
    int knobGap = 8;
    int cellW   = (cw - knobGap * 2) / 3;
    int col1X   = pad + cellW + knobGap;
    int col2X   = pad + (cellW + knobGap) * 2;

    tabs_[i].loopPosLabel.setBounds (pad,   cy,          cellW, labelH);
    tabs_[i].loopPosSlider.setBounds(pad,   cy + labelH, cellW, knobSz);
    tabs_[i].loopLenLabel.setBounds (col1X, cy,          cellW, labelH);
    tabs_[i].loopLenSlider.setBounds(col1X, cy + labelH, cellW, knobSz);
    tabs_[i].volumeLabel.setBounds  (col2X, cy,          cellW, labelH);
    tabs_[i].volumeSlider.setBounds (col2X, cy + labelH, cellW, knobSz);
    cy += knobRowH + pad;

    // Row 4: Obey Note Off toggle
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

    // Reset loop markers: full range (position=0, length=1) for the new sample.
    tabs_[index].waveform.setStartFraction(0.0f);
    tabs_[index].waveform.setEndFraction(1.0f);

    // Push the reset values to the APVTS so processBlock picks them up immediately.
    auto si = juce::String(index);
    auto& apvts = processor_.getAPVTS();
    if (auto* p = apvts.getParameter("sampleLoopPos" + si))
        p->setValueNotifyingHost(0.0f);
    if (auto* p = apvts.getParameter("sampleLoopLen" + si))
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

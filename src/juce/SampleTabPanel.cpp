#include "SampleTabPanel.h"
#include "PluginProcessor.h"

static const char* kNoteNames[] = { "C1", "C2", "C3", "C4" };

SampleTabPanel::SampleTabPanel(PluginProcessor& processor)
    : processor_(processor)
{
    for (int i = 0; i < 4; ++i)
    {
        // Tab header buttons
        addAndMakeVisible(tabButtons_[i]);
        tabButtons_[i].setButtonText("Sample " + juce::String(i + 1));
        tabButtons_[i].setClickingTogglesState(false);
        tabButtons_[i].onClick = [this, i] { selectTab(i); };

        // Waveform display
        addChildComponent(tabs_[i].waveform);
        tabs_[i].waveform.onRegionChanged = [this, i](float start, float end)
        {
            // Push marker drag to parameter system
            auto& apvts = processor_.getAPVTS();
            auto si = juce::String(i);
            if (auto* p = apvts.getParameter("sampleStart" + si))
                p->setValueNotifyingHost(p->convertTo0to1(start));
            if (auto* p = apvts.getParameter("sampleEnd" + si))
                p->setValueNotifyingHost(p->convertTo0to1(end));
        };

        // Obey Note Off toggle
        addChildComponent(tabs_[i].obeyNoteOffButton);
        tabs_[i].obeyNoteOffButton.setButtonText("Obey Note Off");
        tabs_[i].obeyAttachment = std::make_unique<ButtonAttachment>(
            processor_.getAPVTS(), "obeyNoteOff" + juce::String(i),
            tabs_[i].obeyNoteOffButton);

        // Load button
        addChildComponent(tabs_[i].loadButton);
        tabs_[i].loadButton.setButtonText("Load Audio File...");
        tabs_[i].loadButton.onClick = [this, i] { loadButtonClicked(i); };

        // MIDI note label
        addChildComponent(tabs_[i].midiLabel);
        tabs_[i].midiLabel.setText(juce::String("MIDI: ") + kNoteNames[i],
                                    juce::dontSendNotification);
        tabs_[i].midiLabel.setFont(juce::Font(12.0f));
        tabs_[i].midiLabel.setColour(juce::Label::textColourId, juce::Colours::orange);

        // Sample name label
        addChildComponent(tabs_[i].nameLabel);
        auto name = processor_.getSampleName(i);
        tabs_[i].nameLabel.setText(name.isEmpty() ? "(no sample)" : name,
                                    juce::dontSendNotification);
        tabs_[i].nameLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

        // Load existing waveform if sample already loaded
        const auto& wf = processor_.getSampleWaveform(i);
        if (!wf.empty())
        {
            tabs_[i].waveform.setWaveform(wf);
            // Sync markers from parameter values
            auto si = juce::String(i);
            float ss = *processor_.getAPVTS().getRawParameterValue("sampleStart" + si);
            float se = *processor_.getAPVTS().getRawParameterValue("sampleEnd" + si);
            tabs_[i].waveform.setStartFraction(ss);
            tabs_[i].waveform.setEndFraction(se);
        }
    }

    selectTab(0);
}

void SampleTabPanel::selectTab(int index)
{
    selectedTab_ = index;

    for (int i = 0; i < 4; ++i)
    {
        bool visible = (i == index);
        tabs_[i].waveform.setVisible(visible);
        tabs_[i].obeyNoteOffButton.setVisible(visible);
        tabs_[i].loadButton.setVisible(visible);
        tabs_[i].midiLabel.setVisible(visible);
        tabs_[i].nameLabel.setVisible(visible);
    }

    updateTabAppearance();
    resized();
    repaint();
}

void SampleTabPanel::updateTabAppearance()
{
    for (int i = 0; i < 4; ++i)
    {
        if (i == selectedTab_)
        {
            tabButtons_[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a3a3a));
            tabButtons_[i].setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            tabButtons_[i].setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        }
        else
        {
            tabButtons_[i].setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222222));
            tabButtons_[i].setColour(juce::TextButton::textColourOnId, juce::Colours::grey);
            tabButtons_[i].setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
        }
    }
}

void SampleTabPanel::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(getLocalBounds());

    // Border around content area below tabs
    g.setColour(juce::Colour(0xff444444));
    int tabH = 28;
    g.drawRect(0, tabH, getWidth(), getHeight() - tabH);
}

void SampleTabPanel::resized()
{
    int tabH = 28;
    int pad = 8;
    int tabW = getWidth() / 4;

    // Tab buttons across the top
    for (int i = 0; i < 4; ++i)
        tabButtons_[i].setBounds(i * tabW, 0, tabW, tabH);

    // Content area
    int cy = tabH + pad;
    int cw = getWidth() - pad * 2;
    int ch = getHeight() - tabH - pad;

    int i = selectedTab_;

    // Row 1: MIDI label + sample name + load button
    int rowH = 24;
    tabs_[i].midiLabel.setBounds(pad, cy, 60, rowH);
    tabs_[i].nameLabel.setBounds(pad + 65, cy, cw - 200, rowH);
    tabs_[i].loadButton.setBounds(pad + cw - 130, cy, 130, rowH);
    cy += rowH + pad;

    // Row 2: Waveform display (takes most space)
    int wfH = ch - rowH * 2 - pad * 4;
    tabs_[i].waveform.setBounds(pad, cy, cw, wfH);
    cy += wfH + pad;

    // Row 3: Obey note off
    tabs_[i].obeyNoteOffButton.setBounds(pad, cy, 160, rowH);
}

void SampleTabPanel::sampleLoaded(int index)
{
    if (index < 0 || index >= 4)
        return;

    const auto& wf = processor_.getSampleWaveform(index);
    tabs_[index].waveform.setWaveform(wf);

    auto name = processor_.getSampleName(index);
    tabs_[index].nameLabel.setText(name.isEmpty() ? "(no sample)" : name,
                                    juce::dontSendNotification);

    // Reset markers to full range when a new sample is loaded
    tabs_[index].waveform.setStartFraction(0.0f);
    tabs_[index].waveform.setEndFraction(1.0f);

    // Push reset to parameters
    auto si = juce::String(index);
    auto& apvts = processor_.getAPVTS();
    if (auto* p = apvts.getParameter("sampleStart" + si))
        p->setValueNotifyingHost(0.0f);
    if (auto* p = apvts.getParameter("sampleEnd" + si))
        p->setValueNotifyingHost(1.0f);
}

void SampleTabPanel::loadButtonClicked(int index)
{
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Select audio file for Sample " + juce::String(index + 1),
        juce::File{},
        "*.wav;*.mp3;*.flac;*.aif;*.aiff");

    fileChooser_->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, index](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                processor_.loadSample(index, file);
                sampleLoaded(index);
            }
        });
}

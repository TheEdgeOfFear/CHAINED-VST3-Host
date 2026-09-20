#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ChainedAudioEngine.h"
#include "ChainedLookAndFeel.h"
#include "PluginSlotComponent.h"
#include "PatchCableOverlay.h"
#include "WaveformLooperComponent.h"
#include <array>

class MainComponent : public juce::Component,
                      public juce::ChangeListener,
                      public juce::Timer,
                      public juce::KeyListener
{
public:
    MainComponent(ChainedAudioEngine& engineRef);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

    bool keyPressed(const juce::KeyPress& key) override;
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;

private:
    ChainedAudioEngine& engine;
    ChainedLookAndFeel lookAndFeel;

    juce::Image backgroundImage;

    // Top Bar
    juce::TextButton modeUnifiedButton;
    juce::TextButton modeDualityButton;
    juce::Slider bpmSlider;
    juce::Label bpmLabel;
    juce::TextButton tapTempoButton;
    juce::TextButton audioSettingsButton;
    juce::TextButton midiLearnButton;
    juce::TextButton savePresetButton;
    juce::TextButton loadPresetButton;

    // Master DSP Knobs & Meters
    juce::Slider inputGainSlider;
    juce::Label inputGainLabel;
    juce::Slider gateSlider;
    juce::Label gateLabel;
    juce::Slider tubeCompSlider;
    juce::Label tubeCompLabel;
    juce::Slider limiterSlider;
    juce::Label limiterLabel;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;

    // Row Footswitches & Input Selectors
    juce::ComboBox masterInputCombo;
    juce::ComboBox row1InputCombo;
    juce::ComboBox row2InputCombo;
    juce::TextButton row1Footswitch;
    juce::TextButton row2Footswitch;

    // 12 Plugin Slots (2x6 grid)
    std::array<std::unique_ptr<PluginSlotComponent>, 12> slotComponents;
    std::unique_ptr<PatchCableOverlay> cableOverlay;

    // Backing Track Player UI
    std::unique_ptr<WaveformLooperComponent> waveformLooper;
    juce::Label trackNameLabel;
    juce::TextButton loadMp3Button;
    juce::TextButton loadFolderButton;
    juce::TextButton sequentialModeButton;
    juce::TextButton prevTrackButton;
    juce::TextButton rewindButton;
    juce::TextButton playPauseButton;
    juce::TextButton stopButton;
    juce::TextButton forwardButton;
    juce::TextButton nextTrackButton;
    juce::TextButton loopToggleButton;
    juce::Slider speedSlider;
    juce::Label speedLabel;
    juce::TextButton bpmLinkButton;
    juce::Slider mp3VolumeSlider;
    juce::Label mp3VolumeLabel;

    // Audio meters cache
    float inPeakL{ 0.0f }, inPeakR{ 0.0f };
    float outPeakL{ 0.0f }, outPeakR{ 0.0f };

    void setupControls();
    void updateModeUI();
    void openAudioSettings();
    void openMidiSettings();
    void drawMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds, float valL, float valR);
    void drawFootswitch(juce::Graphics& g, const juce::Rectangle<float>& bounds, bool isEngaged, const juce::String& text, juce::Colour ledColor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

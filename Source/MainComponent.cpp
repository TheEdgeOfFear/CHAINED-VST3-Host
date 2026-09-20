#include "MainComponent.h"
#include "ChainedMidiDialogComponent.h"
#include <BinaryData.h>

MainComponent::MainComponent(ChainedAudioEngine& engineRef)
    : engine(engineRef)
{
    setLookAndFeel(&lookAndFeel);

    // Load background image from binary data
    backgroundImage = juce::ImageCache::getFromMemory(BinaryData::CHAINED_jpg, BinaryData::CHAINED_jpgSize);

    setupControls();

    setWantsKeyboardFocus(true);
    addKeyListener(this);

    engine.addChangeListener(this);
    startTimerHz(30);

    setSize(1360, 840);
}

MainComponent::~MainComponent()
{
    removeKeyListener(this);
    stopTimer();
    engine.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void MainComponent::setupControls()
{
    // Mode Buttons
    addAndMakeVisible(modeUnifiedButton);
    modeUnifiedButton.setButtonText("UNIFIED [12 CHAIN]");
    modeUnifiedButton.setClickingTogglesState(true);
    modeUnifiedButton.setRadioGroupId(101);
    modeUnifiedButton.onClick = [this] { engine.setRoutingMode(RoutingMode::Unified); };

    addAndMakeVisible(modeDualityButton);
    modeDualityButton.setButtonText("DUALITY [DUAL 6x6]");
    modeDualityButton.setClickingTogglesState(true);
    modeDualityButton.setRadioGroupId(101);
    modeDualityButton.onClick = [this] { engine.setRoutingMode(RoutingMode::Duality); };

    // BPM & Tap
    addAndMakeVisible(bpmSlider);
    bpmSlider.setSliderStyle(juce::Slider::LinearBar);
    bpmSlider.setRange(20.0, 300.0, 0.1);
    bpmSlider.setValue(engine.getMasterBpm(), juce::dontSendNotification);
    bpmSlider.setTextValueSuffix(" BPM");
    bpmSlider.onValueChange = [this] { engine.setMasterBpm(bpmSlider.getValue()); };

    addAndMakeVisible(bpmLabel);
    bpmLabel.setText("TEMPO", juce::dontSendNotification);
    bpmLabel.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    bpmLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(tapTempoButton);
    tapTempoButton.setButtonText("TAP");
    tapTempoButton.onClick = [this]
    {
        engine.tapTempo();
        bpmSlider.setValue(engine.getMasterBpm(), juce::dontSendNotification);
    };

    // Audio Settings Button
    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.setButtonText("AUDIO I/O");
    audioSettingsButton.onClick = [this] { openAudioSettings(); };

    // MIDI Learn / Mappings Button
    addAndMakeVisible(midiLearnButton);
    midiLearnButton.setButtonText("MIDI MAPPINGS");
    midiLearnButton.onClick = [this] { openMidiSettings(); };

    // Preset Buttons
    addAndMakeVisible(savePresetButton);
    savePresetButton.setButtonText("SAVE RIG");
    savePresetButton.onClick = [this]
    {
        auto fileChooser = std::make_shared<juce::FileChooser>("Save Rig Preset",
            juce::File("C:\\Coding\\Tunings VST3\\CHAINED"),
            "*.chained");

        fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, fileChooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                {
                    if (!file.hasFileExtension(".chained"))
                        file = file.withFileExtension(".chained");
                    engine.savePresetToFile(file);
                }
            });
    };

    addAndMakeVisible(loadPresetButton);
    loadPresetButton.setButtonText("LOAD RIG");
    loadPresetButton.onClick = [this]
    {
        auto fileChooser = std::make_shared<juce::FileChooser>("Load Rig Preset",
            juce::File("C:\\Coding\\Tunings VST3\\CHAINED"),
            "*.chained");

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, fileChooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    engine.loadPresetFromFile(file);
                    for (auto& s : slotComponents)
                        if (s) s->updateState();
                    updateModeUI();
                    bpmSlider.setValue(engine.getMasterBpm(), juce::dontSendNotification);
                    inputGainSlider.setValue(engine.getInputGainDecibels(), juce::dontSendNotification);
                    outputGainSlider.setValue(engine.getOutputGainDecibels(), juce::dontSendNotification);
                    gateSlider.setValue(engine.getGateThresholdDecibels(), juce::dontSendNotification);
                    tubeCompSlider.setValue(engine.getTubeCompMode(), juce::dontSendNotification);
                    limiterSlider.setValue(engine.getLimiterThresholdDecibels(), juce::dontSendNotification);
                }
            });
    };

    // Master DSP Knobs
    addAndMakeVisible(inputGainSlider);
    inputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    inputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    inputGainSlider.setRange(-24.0, 24.0, 0.1);
    inputGainSlider.setValue(engine.getInputGainDecibels(), juce::dontSendNotification);
    inputGainSlider.setTextValueSuffix(" dB");
    inputGainSlider.onValueChange = [this] { engine.setInputGainDecibels((float)inputGainSlider.getValue()); };

    addAndMakeVisible(inputGainLabel);
    inputGainLabel.setText("INPUT GAIN", juce::dontSendNotification);
    inputGainLabel.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    inputGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    inputGainLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(gateSlider);
    gateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    gateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    gateSlider.setRange(-90.0, 0.0, 0.5);
    gateSlider.setValue(engine.getGateThresholdDecibels(), juce::dontSendNotification);
    gateSlider.setTextValueSuffix(" dB");
    gateSlider.onValueChange = [this] { engine.setGateThresholdDecibels((float)gateSlider.getValue()); };

    addAndMakeVisible(gateLabel);
    gateLabel.setText("NOISE GATE", juce::dontSendNotification);
    gateLabel.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    gateLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    gateLabel.setJustificationType(juce::Justification::centred);

    // Tube Compressor Knob (OFF, 4, 8, 12, 20)
    addAndMakeVisible(tubeCompSlider);
    tubeCompSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    tubeCompSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    tubeCompSlider.setRange(0.0, 4.0, 1.0);
    tubeCompSlider.setValue(engine.getTubeCompMode(), juce::dontSendNotification);
    tubeCompSlider.textFromValueFunction = [](double val) -> juce::String
    {
        const int m = static_cast<int>(std::round(val));
        if (m == 1) return "4:1";
        if (m == 2) return "8:1";
        if (m == 3) return "12:1";
        if (m == 4) return "20:1";
        return "OFF";
    };
    tubeCompSlider.onValueChange = [this] { engine.setTubeCompMode((int)tubeCompSlider.getValue()); };

    addAndMakeVisible(tubeCompLabel);
    tubeCompLabel.setText("TUBE COMP", juce::dontSendNotification);
    tubeCompLabel.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    tubeCompLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    tubeCompLabel.setJustificationType(juce::Justification::centred);

    // Master Limiter Knob
    addAndMakeVisible(limiterSlider);
    limiterSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    limiterSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    limiterSlider.setRange(-18.0, 0.0, 0.5);
    limiterSlider.setValue(engine.getLimiterThresholdDecibels(), juce::dontSendNotification);
    limiterSlider.setTextValueSuffix(" dB");
    limiterSlider.onValueChange = [this] { engine.setLimiterThresholdDecibels((float)limiterSlider.getValue()); };

    addAndMakeVisible(limiterLabel);
    limiterLabel.setText("LIMITER", juce::dontSendNotification);
    limiterLabel.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    limiterLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    limiterLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(outputGainSlider);
    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    outputGainSlider.setRange(-24.0, 24.0, 0.1);
    outputGainSlider.setValue(engine.getOutputGainDecibels(), juce::dontSendNotification);
    outputGainSlider.setTextValueSuffix(" dB");
    outputGainSlider.onValueChange = [this] { engine.setOutputGainDecibels((float)outputGainSlider.getValue()); };

    addAndMakeVisible(outputGainLabel);
    outputGainLabel.setText("OUTPUT GAIN", juce::dontSendNotification);
    outputGainLabel.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    outputGainLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    outputGainLabel.setJustificationType(juce::Justification::centred);

    // Input Channel Selectors
    auto setupInputCombo = [](juce::ComboBox& box)
    {
        box.addItem("INPUT 1 (L)", 1);
        box.addItem("INPUT 2 (R)", 2);
        box.addItem("INPUT 1+2 (STEREO)", 3);
    };

    addAndMakeVisible(masterInputCombo);
    setupInputCombo(masterInputCombo);
    masterInputCombo.setSelectedId(1, juce::dontSendNotification);
    masterInputCombo.onChange = [this]
    {
        const int id = masterInputCombo.getSelectedId();
        engine.setMasterInputMode(id == 1 ? InputChannelMode::Input1 : (id == 2 ? InputChannelMode::Input2 : InputChannelMode::Stereo_1_2));
    };

    addAndMakeVisible(row1InputCombo);
    setupInputCombo(row1InputCombo);
    row1InputCombo.setSelectedId(1, juce::dontSendNotification);
    row1InputCombo.onChange = [this]
    {
        const int id = row1InputCombo.getSelectedId();
        engine.setRow1InputMode(id == 1 ? InputChannelMode::Input1 : (id == 2 ? InputChannelMode::Input2 : InputChannelMode::Stereo_1_2));
    };

    addAndMakeVisible(row2InputCombo);
    setupInputCombo(row2InputCombo);
    row2InputCombo.setSelectedId(2, juce::dontSendNotification);
    row2InputCombo.onChange = [this]
    {
        const int id = row2InputCombo.getSelectedId();
        engine.setRow2InputMode(id == 1 ? InputChannelMode::Input1 : (id == 2 ? InputChannelMode::Input2 : InputChannelMode::Stereo_1_2));
    };

    // Row Footswitches
    addAndMakeVisible(row1Footswitch);
    row1Footswitch.setButtonText("ROW 1");
    row1Footswitch.setClickingTogglesState(true);
    row1Footswitch.setToggleState(engine.getRow1Enabled(), juce::dontSendNotification);
    row1Footswitch.onClick = [this]
    {
        engine.setRow1Enabled(row1Footswitch.getToggleState());
        cableOverlay->repaint();
    };

    addAndMakeVisible(row2Footswitch);
    row2Footswitch.setButtonText("ROW 2");
    row2Footswitch.setClickingTogglesState(true);
    row2Footswitch.setToggleState(engine.getRow2Enabled(), juce::dontSendNotification);
    row2Footswitch.onClick = [this]
    {
        engine.setRow2Enabled(row2Footswitch.getToggleState());
        cableOverlay->repaint();
    };

    // 12 Slot Components
    for (int i = 0; i < 12; ++i)
    {
        if (auto* slot = engine.getSlot(i))
        {
            slotComponents[static_cast<size_t>(i)] = std::make_unique<PluginSlotComponent>(*slot, engine);
            addAndMakeVisible(slotComponents[static_cast<size_t>(i)].get());
        }
    }

    // Patch Cable Overlay
    cableOverlay = std::make_unique<PatchCableOverlay>(engine);
    cableOverlay->setSlotBoundsProvider([this](int slotIndex) -> juce::Rectangle<int>
    {
        if (slotIndex >= 0 && slotIndex < 12 && slotComponents[static_cast<size_t>(slotIndex)])
            return slotComponents[static_cast<size_t>(slotIndex)]->getBounds();
        return {};
    });
    addAndMakeVisible(cableOverlay.get());

    // Backing Track Player Section
    waveformLooper = std::make_unique<WaveformLooperComponent>(engine.getBackingPlayer());
    addAndMakeVisible(waveformLooper.get());

    addAndMakeVisible(trackNameLabel);
    trackNameLabel.setText("NO TRACK LOADED", juce::dontSendNotification);
    trackNameLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
    trackNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff));

    addAndMakeVisible(loadMp3Button);
    loadMp3Button.setButtonText("LOAD MP3");
    loadMp3Button.onClick = [this]
    {
        auto fileChooser = std::make_shared<juce::FileChooser>("Select Audio Track",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory),
            "*.mp3;*.wav;*.aif;*.aiff;*.flac;*.ogg");

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, fileChooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    engine.getBackingPlayer().loadFile(file);
                    trackNameLabel.setText(file.getFileName(), juce::dontSendNotification);
                    waveformLooper->repaint();
                }
            });
    };

    addAndMakeVisible(loadFolderButton);
    loadFolderButton.setButtonText("LOAD FOLDER");
    loadFolderButton.onClick = [this]
    {
        auto fileChooser = std::make_shared<juce::FileChooser>("Select Audio Folder",
            juce::File::getSpecialLocation(juce::File::userMusicDirectory));

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, fileChooser](const juce::FileChooser& fc)
            {
                auto folder = fc.getResult();
                if (folder.isDirectory())
                {
                    engine.getBackingPlayer().loadFolder(folder);
                    trackNameLabel.setText(engine.getBackingPlayer().getCurrentFileName(), juce::dontSendNotification);
                    waveformLooper->repaint();
                }
            });
    };

    addAndMakeVisible(sequentialModeButton);
    sequentialModeButton.setButtonText("AUTO-NEXT");
    sequentialModeButton.setClickingTogglesState(true);
    sequentialModeButton.setToggleState(engine.getBackingPlayer().getSequentialMode(), juce::dontSendNotification);
    sequentialModeButton.onClick = [this]
    {
        engine.getBackingPlayer().setSequentialMode(sequentialModeButton.getToggleState());
    };

    addAndMakeVisible(prevTrackButton);
    prevTrackButton.setButtonText("<<");
    prevTrackButton.onClick = [this]
    {
        engine.getBackingPlayer().previousTrack();
        trackNameLabel.setText(engine.getBackingPlayer().getCurrentFileName(), juce::dontSendNotification);
        waveformLooper->repaint();
    };

    addAndMakeVisible(rewindButton);
    rewindButton.setButtonText("< 5s");
    rewindButton.onClick = [this] { engine.getBackingPlayer().skipBackward(5.0); };

    addAndMakeVisible(playPauseButton);
    playPauseButton.setButtonText("PLAY");
    playPauseButton.onClick = [this]
    {
        engine.getBackingPlayer().togglePlayPause();
        playPauseButton.setButtonText(engine.getBackingPlayer().getIsPlaying() ? "PAUSE" : "PLAY");
    };

    addAndMakeVisible(stopButton);
    stopButton.setButtonText("STOP");
    stopButton.onClick = [this]
    {
        engine.getBackingPlayer().stop();
        playPauseButton.setButtonText("PLAY");
    };

    addAndMakeVisible(forwardButton);
    forwardButton.setButtonText("5s >");
    forwardButton.onClick = [this] { engine.getBackingPlayer().skipForward(5.0); };

    addAndMakeVisible(nextTrackButton);
    nextTrackButton.setButtonText(">>");
    nextTrackButton.onClick = [this]
    {
        engine.getBackingPlayer().nextTrack();
        trackNameLabel.setText(engine.getBackingPlayer().getCurrentFileName(), juce::dontSendNotification);
        waveformLooper->repaint();
    };

    addAndMakeVisible(loopToggleButton);
    loopToggleButton.setButtonText("LOOP");
    loopToggleButton.setClickingTogglesState(true);
    loopToggleButton.setToggleState(engine.getBackingPlayer().getLoopEnabled(), juce::dontSendNotification);
    loopToggleButton.onClick = [this]
    {
        engine.getBackingPlayer().setLoopEnabled(loopToggleButton.getToggleState());
        waveformLooper->repaint();
    };

    addAndMakeVisible(speedSlider);
    speedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    speedSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 20);
    speedSlider.setRange(0.25, 2.0, 0.01);
    speedSlider.setValue(1.0, juce::dontSendNotification);
    speedSlider.setTextValueSuffix("x");
    speedSlider.onValueChange = [this]
    {
        engine.getBackingPlayer().setSpeed((float)speedSlider.getValue());
    };

    addAndMakeVisible(speedLabel);
    speedLabel.setText("SPEED (NO PITCH SHIFT)", juce::dontSendNotification);
    speedLabel.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    speedLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));

    addAndMakeVisible(bpmLinkButton);
    bpmLinkButton.setButtonText("BPM LINK");
    bpmLinkButton.setClickingTogglesState(true);
    bpmLinkButton.onClick = [this]
    {
        const bool isLinked = bpmLinkButton.getToggleState();
        engine.getBackingPlayer().setBpmLinked(isLinked);
        if (isLinked)
        {
            const double trkBpm = engine.getBackingPlayer().getDetectedBpm();
            if (trkBpm > 20.0)
            {
                engine.setMasterBpm(trkBpm);
                bpmSlider.setValue(trkBpm, juce::dontSendNotification);
            }
        }
    };

    addAndMakeVisible(mp3VolumeSlider);
    mp3VolumeSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mp3VolumeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 45, 14);
    mp3VolumeSlider.setRange(0.0, 1.5, 0.01);
    mp3VolumeSlider.setValue(0.8, juce::dontSendNotification);
    mp3VolumeSlider.onValueChange = [this]
    {
        engine.getBackingPlayer().setVolume((float)mp3VolumeSlider.getValue());
    };

    addAndMakeVisible(mp3VolumeLabel);
    mp3VolumeLabel.setText("TRACK VOL", juce::dontSendNotification);
    mp3VolumeLabel.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    mp3VolumeLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
    mp3VolumeLabel.setJustificationType(juce::Justification::centred);

    updateModeUI();
}

void MainComponent::updateModeUI()
{
    const bool isUnified = (engine.getRoutingMode() == RoutingMode::Unified);
    modeUnifiedButton.setToggleState(isUnified, juce::dontSendNotification);
    modeDualityButton.setToggleState(!isUnified, juce::dontSendNotification);

    masterInputCombo.setVisible(isUnified);
    row1InputCombo.setVisible(!isUnified);
    row2InputCombo.setVisible(!isUnified);

    cableOverlay->repaint();
    repaint();
}

void MainComponent::openAudioSettings()
{
    auto* dialog = new juce::AudioDeviceSelectorComponent(engine.getDeviceManager(), 1, 2, 1, 2, true, true, true, false);
    dialog->setSize(520, 360);

    juce::DialogWindow::LaunchOptions opt;
    opt.content.setOwned(dialog);
    opt.dialogTitle = "CHAINED - Audio Device & Soundcard Settings";
    opt.dialogBackgroundColour = juce::Colour(0xff181a1d);
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = false;
    opt.launchAsync();
}

void MainComponent::openMidiSettings()
{
    auto* dialog = new ChainedMidiDialogComponent(engine);
    dialog->setSize(680, 520);

    juce::DialogWindow::LaunchOptions opt;
    opt.content.setOwned(dialog);
    opt.dialogTitle = "CHAINED - MIDI Controller & Footswitch Mappings";
    opt.dialogBackgroundColour = juce::Colour(0xff14171c);
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = false;
    opt.launchAsync();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    updateModeUI();
    for (auto& s : slotComponents)
        if (s) s->updateState();
    bpmSlider.setValue(engine.getMasterBpm(), juce::dontSendNotification);
    inputGainSlider.setValue(engine.getInputGainDecibels(), juce::dontSendNotification);
    outputGainSlider.setValue(engine.getOutputGainDecibels(), juce::dontSendNotification);
    gateSlider.setValue(engine.getGateThresholdDecibels(), juce::dontSendNotification);
    tubeCompSlider.setValue(engine.getTubeCompMode(), juce::dontSendNotification);
    limiterSlider.setValue(engine.getLimiterThresholdDecibels(), juce::dontSendNotification);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key.isKeyCode(juce::KeyPress::spaceKey))
    {
        engine.getBackingPlayer().togglePlayPause();
        playPauseButton.setButtonText(engine.getBackingPlayer().getIsPlaying() ? "PAUSE" : "PLAY");
        return true;
    }
    return false;
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Ignore spacebar if user is typing text into a TextEditor
    if (dynamic_cast<juce::TextEditor*>(originatingComponent) != nullptr)
        return false;

    if (key.isKeyCode(juce::KeyPress::spaceKey))
    {
        engine.getBackingPlayer().togglePlayPause();
        playPauseButton.setButtonText(engine.getBackingPlayer().getIsPlaying() ? "PAUSE" : "PLAY");
        return true;
    }
    return false;
}

void MainComponent::timerCallback()
{
    // Real-time slot state sync
    for (auto& s : slotComponents)
        if (s) s->updateState();

    // Metering update
    inPeakL = inPeakL * 0.8f + engine.getInputLevelL() * 0.2f;
    inPeakR = inPeakR * 0.8f + engine.getInputLevelR() * 0.2f;
    outPeakL = outPeakL * 0.8f + engine.getOutputLevelL() * 0.2f;
    outPeakR = outPeakR * 0.8f + engine.getOutputLevelR() * 0.2f;

    // Track status update
    if (engine.getBackingPlayer().getIsPlaying())
        playPauseButton.setButtonText("PAUSE");
    else
        playPauseButton.setButtonText("PLAY");

    if (engine.getBackingPlayer().isLoaded())
    {
        juce::String trackInfo = engine.getBackingPlayer().getCurrentFileName();
        const double b = engine.getBackingPlayer().getDetectedBpm();
        if (b > 20.0)
            trackInfo += "  [" + juce::String(b, 1) + " BPM]";
        trackNameLabel.setText(trackInfo, juce::dontSendNotification);
    }

    // Footswitches sync
    row1Footswitch.setToggleState(engine.getRow1Enabled(), juce::dontSendNotification);
    row2Footswitch.setToggleState(engine.getRow2Enabled(), juce::dontSendNotification);

    repaint();
}

void MainComponent::drawMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds, float valL, float valR)
{
    g.setColour(juce::Colour(0xff0e1012));
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(juce::Colour(0xff2d333b));
    g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

    const float mid = bounds.getY() + bounds.getHeight() * 0.5f;
    const float maxW = bounds.getWidth() - 4.0f;

    auto drawBar = [&](float y, float h, float val)
    {
        const float fillW = std::clamp(val * maxW, 0.0f, maxW);
        if (fillW > 0.5f)
        {
            juce::ColourGradient grad(juce::Colour(0xff00ff66), bounds.getX() + 2.0f, y,
                                      juce::Colour(0xffff3300), bounds.getX() + maxW, y, false);
            grad.addColour(0.7, juce::Colour(0xffffcc00));
            g.setGradientFill(grad);
            g.fillRect(bounds.getX() + 2.0f, y, fillW, h);
        }
    };

    drawBar(bounds.getY() + 2.0f, (bounds.getHeight() - 6.0f) * 0.5f, valL);
    drawBar(mid + 1.0f, (bounds.getHeight() - 6.0f) * 0.5f, valR);
}

void MainComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Draw background image scaled nicely
    if (backgroundImage.isValid())
    {
        g.drawImage(backgroundImage, bounds, juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        g.fillAll(juce::Colour(0xff121417));
    }

    // Semi-translucent dark industrial overlay for contrast
    g.setColour(juce::Colour(0xff0a0b0d).withAlpha(0.60f));
    g.fillRect(bounds);

    // Header Panel Glass Bar
    const auto topBar = juce::Rectangle<float>(0, 0, bounds.getWidth(), 58.0f);
    g.setColour(juce::Colour(0xff16191f).withAlpha(0.85f));
    g.fillRect(topBar);
    g.setColour(juce::Colour(0xff333a45));
    g.drawHorizontalLine(58, 0, bounds.getWidth());

    // Product Title & Brand
    g.setColour(juce::Colour(0xffeceff4));
    g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
    g.drawText("CHAINED", 18, 8, 140, 24, juce::Justification::left, true);

    g.setColour(juce::Colour(0xff00e5ff));
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.drawText("THE EDGE OF FEAR", 19, 32, 140, 16, juce::Justification::left, true);

    if (engine.isPluginScanRunning())
    {
        g.setColour(juce::Colour(0xffffbb00));
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
        juce::String scanStatus = "SCANNING VST3...";
        if (engine.getCurrentlyScanningPluginName().isNotEmpty())
            scanStatus += " (" + engine.getCurrentlyScanningPluginName() + ")";
        g.drawText(scanStatus, 856, 18, 300, 30, juce::Justification::left, true);
    }

    // Master Knobs Section Background (Holds 5 Knobs)
    const auto masterKnobsRect = juce::Rectangle<float>(bounds.getWidth() - 410.0f, 58.0f, 396.0f, 88.0f);
    g.setColour(juce::Colour(0xff14171c).withAlpha(0.85f));
    g.fillRoundedRectangle(masterKnobsRect, 6.0f);
    g.setColour(juce::Colour(0xff2a303a));
    g.drawRoundedRectangle(masterKnobsRect, 6.0f, 1.0f);

    // Draw Input & Output Meters cleanly under knobs
    drawMeter(g, juce::Rectangle<float>(masterKnobsRect.getX() + 8.0f, masterKnobsRect.getY() + 72.0f, 70.0f, 10.0f), inPeakL, inPeakR);
    drawMeter(g, juce::Rectangle<float>(masterKnobsRect.getRight() - 78.0f, masterKnobsRect.getY() + 72.0f, 70.0f, 10.0f), outPeakL, outPeakR);

    // Section Dividers / Grid Headers
    g.setColour(juce::Colour(0xff00d2ff).withAlpha(0.4f));
    g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    if (engine.getRoutingMode() == RoutingMode::Unified)
    {
        g.drawText("UNIFIED SIGNAL CHAIN (12 DAISY-CHAINED PEDAL SLOTS)", 20, 134, 500, 16, juce::Justification::left, true);
    }
    else
    {
        g.drawText("ROW 1 SIGNAL CHAIN (SLOTS 1-6)", 20, 134, 300, 16, juce::Justification::left, true);
        g.drawText("ROW 2 SIGNAL CHAIN (SLOTS 7-12)", 20, 326, 300, 16, juce::Justification::left, true);
    }
}

void MainComponent::resized()
{
    const auto bounds = getLocalBounds();
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();

    // Top Bar Layout
    modeUnifiedButton.setBounds(180, 14, 150, 30);
    modeDualityButton.setBounds(336, 14, 150, 30);

    bpmLabel.setBounds(500, 10, 50, 14);
    bpmSlider.setBounds(500, 26, 85, 22);
    tapTempoButton.setBounds(590, 18, 48, 30);

    audioSettingsButton.setBounds(650, 18, 90, 30);
    midiLearnButton.setBounds(746, 18, 100, 30);
    savePresetButton.setBounds(w - 180, 18, 80, 30);
    loadPresetButton.setBounds(w - 94, 18, 80, 30);

    // Master Knobs Section (5 knobs: Input Gain, Noise Gate, Tube Comp, Limiter, Output Gain)
    const int knobsX = w - 402;
    const int knobSpacing = 78;
    const int knobW = 74;

    inputGainLabel.setBounds(knobsX, 60, knobW, 14);
    inputGainSlider.setBounds(knobsX, 74, knobW, 52);

    gateLabel.setBounds(knobsX + knobSpacing, 60, knobW, 14);
    gateSlider.setBounds(knobsX + knobSpacing, 74, knobW, 52);

    tubeCompLabel.setBounds(knobsX + knobSpacing * 2, 60, knobW, 14);
    tubeCompSlider.setBounds(knobsX + knobSpacing * 2, 74, knobW, 52);

    limiterLabel.setBounds(knobsX + knobSpacing * 3, 60, knobW, 14);
    limiterSlider.setBounds(knobsX + knobSpacing * 3, 74, knobW, 52);

    outputGainLabel.setBounds(knobsX + knobSpacing * 4, 60, knobW, 14);
    outputGainSlider.setBounds(knobsX + knobSpacing * 4, 74, knobW, 52);

    // Left Footswitches & Input Selectors
    const int leftX = 20;
    masterInputCombo.setBounds(leftX, 70, 160, 26);

    row1InputCombo.setBounds(leftX, 154, 110, 24);
    row1Footswitch.setBounds(leftX, 182, 110, 140);

    row2InputCombo.setBounds(leftX, 346, 110, 24);
    row2Footswitch.setBounds(leftX, 374, 110, 140);

    // 2x6 Grid Slots Layout
    const int gridStartX = 140;
    const int slotW = (w - gridStartX - 24) / 6;
    const int slotH = 172;
    const int row1Y = 150;
    const int row2Y = 342;

    for (int col = 0; col < 6; ++col)
    {
        const int sx = gridStartX + col * slotW;
        if (slotComponents[static_cast<size_t>(col)])
            slotComponents[static_cast<size_t>(col)]->setBounds(sx + 4, row1Y, slotW - 8, slotH);

        if (slotComponents[static_cast<size_t>(col + 6)])
            slotComponents[static_cast<size_t>(col + 6)]->setBounds(sx + 4, row2Y, slotW - 8, slotH);
    }

    // Cable Overlay covers entire window
    cableOverlay->setBounds(bounds);

    // Bottom Backing Track / Looper Section
    const int bottomY = 530;
    const int bottomH = h - bottomY - 12;

    trackNameLabel.setBounds(20, bottomY, 210, 24);
    loadMp3Button.setBounds(236, bottomY, 78, 24);
    loadFolderButton.setBounds(320, bottomY, 90, 24);
    sequentialModeButton.setBounds(416, bottomY, 84, 24);

    prevTrackButton.setBounds(506, bottomY, 32, 24);
    rewindButton.setBounds(542, bottomY, 42, 24);
    playPauseButton.setBounds(588, bottomY, 65, 24);
    stopButton.setBounds(657, bottomY, 48, 24);
    forwardButton.setBounds(709, bottomY, 42, 24);
    nextTrackButton.setBounds(755, bottomY, 32, 24);

    loopToggleButton.setBounds(793, bottomY, 52, 24);

    speedLabel.setText("SPEED", juce::dontSendNotification);
    speedLabel.setBounds(852, bottomY, 48, 24);
    speedSlider.setBounds(902, bottomY, 130, 24);
    bpmLinkButton.setBounds(1038, bottomY, 75, 24);

    mp3VolumeLabel.setBounds(w - 95, bottomY - 14, 75, 14);
    mp3VolumeSlider.setBounds(w - 95, bottomY, 75, 48);

    waveformLooper->setBounds(20, bottomY + 28, w - 125, bottomH - 28);
}

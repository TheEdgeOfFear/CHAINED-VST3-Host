#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginSlot.h"
#include "ChainedPlayHead.h"
#include "BackingTrackPlayer.h"
#include "ChainedMidiManager.h"
#include <array>
#include <memory>
#include <atomic>

enum class RoutingMode
{
    Unified,   // Single 12-slot daisy chain
    Duality    // Split into 2 parallel rows of 6 slots
};

enum class InputChannelMode
{
    Input1,       // Left / Soundcard Input 1
    Input2,       // Right / Soundcard Input 2
    Stereo_1_2    // Stereo / Both Input 1 & 2
};

class ChainedAudioEngine : public juce::AudioIODeviceCallback,
                           public juce::MidiInputCallback,
                           public juce::ChangeBroadcaster
{
public:
    ChainedAudioEngine();
    ~ChainedAudioEngine() override;

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }
    juce::KnownPluginList& getKnownPluginList() { return knownPluginList; }

    void initializeAudio();
    void startPluginScan(bool forceRescan = false);
    bool isPluginScanRunning() const;
    juce::String getCurrentlyScanningPluginName() const { return currentScanningPlugin; }
    void performScan(juce::Thread* thread, bool forceRescan);

    // Slots (0..5: Row 1, 6..11: Row 2)
    PluginSlot* getSlot(int index)
    {
        if (index >= 0 && index < 12)
            return slots[static_cast<size_t>(index)].get();
        return nullptr;
    }

    // Routing Mode
    void setRoutingMode(RoutingMode mode);
    RoutingMode getRoutingMode() const { return routingMode.load(); }
    void toggleRoutingMode();

    // Input Assignments
    void setMasterInputMode(InputChannelMode mode) { masterInputMode.store(mode); }
    InputChannelMode getMasterInputMode() const { return masterInputMode.load(); }

    void setRow1InputMode(InputChannelMode mode) { row1InputMode.store(mode); }
    InputChannelMode getRow1InputMode() const { return row1InputMode.load(); }

    void setRow2InputMode(InputChannelMode mode) { row2InputMode.store(mode); }
    InputChannelMode getRow2InputMode() const { return row2InputMode.load(); }

    // Row Footswitch Enables (Clean/Engage/Bypass)
    void setRow1Enabled(bool enabled) { row1Enabled.store(enabled); }
    bool getRow1Enabled() const { return row1Enabled.load(); }
    void toggleRow1Enabled() { setRow1Enabled(!getRow1Enabled()); }

    void setRow2Enabled(bool enabled) { row2Enabled.store(enabled); }
    bool getRow2Enabled() const { return row2Enabled.load(); }
    void toggleRow2Enabled() { setRow2Enabled(!getRow2Enabled()); }

    // Gains & Gate
    void setInputGainDecibels(float db) { inputGainDb.store(db); }
    float getInputGainDecibels() const { return inputGainDb.load(); }

    void setOutputGainDecibels(float db) { outputGainDb.store(db); }
    float getOutputGainDecibels() const { return outputGainDb.load(); }

    void setGateThresholdDecibels(float db) { gateThresholdDb.store(db); }
    float getGateThresholdDecibels() const { return gateThresholdDb.load(); }

    // Tube Compressor ratios: 0=OFF, 1=4:1, 2=8:1, 3=12:1, 4=20:1
    void setTubeCompMode(int mode) { tubeCompMode.store(std::clamp(mode, 0, 4)); }
    int getTubeCompMode() const { return tubeCompMode.load(); }

    // Limiter threshold in dB: -18.0 dB to 0.0 dB
    void setLimiterThresholdDecibels(float db) { limiterThreshDb.store(std::clamp(db, -18.0f, 0.0f)); }
    float getLimiterThresholdDecibels() const { return limiterThreshDb.load(); }

    // BPM
    void setMasterBpm(double bpm);
    double getMasterBpm() const { return playHead.getBpm(); }
    void tapTempo();

    // Metering (RMS & Peak linear)
    float getInputLevelL() const { return inMeterL.load(); }
    float getInputLevelR() const { return inMeterR.load(); }
    float getOutputLevelL() const { return outMeterL.load(); }
    float getOutputLevelR() const { return outMeterR.load(); }

    bool isRowOccupied(int rowNumber) const
    {
        const int start = (rowNumber == 1) ? 0 : 6;
        const int end = start + 6;
        for (int i = start; i < end; ++i)
        {
            if (slots[static_cast<size_t>(i)] && slots[static_cast<size_t>(i)]->isOccupied())
                return true;
        }
        return false;
    }

    double getSampleRate() const { return currentSampleRate > 0.0 ? currentSampleRate : 44100.0; }
    int getBlockSize() const { return currentBlockSize > 0 ? currentBlockSize : 512; }

    // Subsystems
    BackingTrackPlayer& getBackingPlayer() { return backingPlayer; }
    ChainedMidiManager& getMidiManager() { return midiManager; }
    ChainedPlayHead& getPlayHead() { return playHead; }

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                         int numInputChannels,
                                         float* const* outputChannelData,
                                         int numOutputChannels,
                                         int numSamples,
                                         const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    // Preset management
    juce::ValueTree exportPresetToValueTree(const juce::String& presetName = "Default Rig");
    void loadPresetFromValueTree(const juce::ValueTree& tree);
    void savePresetToFile(const juce::File& file);
    void loadPresetFromFile(const juce::File& file);

private:
    juce::AudioDeviceManager deviceManager;
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;

    std::array<std::unique_ptr<PluginSlot>, 12> slots;
    ChainedPlayHead playHead;
    BackingTrackPlayer backingPlayer;
    ChainedMidiManager midiManager;

    std::atomic<RoutingMode> routingMode{ RoutingMode::Unified };
    std::atomic<InputChannelMode> masterInputMode{ InputChannelMode::Input1 };
    std::atomic<InputChannelMode> row1InputMode{ InputChannelMode::Input1 };
    std::atomic<InputChannelMode> row2InputMode{ InputChannelMode::Input2 };

    std::atomic<bool> row1Enabled{ true };
    std::atomic<bool> row2Enabled{ true };

    std::atomic<float> inputGainDb{ 0.0f };
    std::atomic<float> outputGainDb{ 0.0f };
    std::atomic<float> gateThresholdDb{ -90.0f };
    std::atomic<int> tubeCompMode{ 0 }; // 0=OFF, 1=4, 2=8, 3=12, 4=20
    std::atomic<float> limiterThreshDb{ 0.0f }; // -18 to 0 dB

    std::atomic<float> inMeterL{ 0.0f };
    std::atomic<float> inMeterR{ 0.0f };
    std::atomic<float> outMeterL{ 0.0f };
    std::atomic<float> outMeterR{ 0.0f };

    // DSP buffers
    juce::AudioBuffer<float> row1Buffer;
    juce::AudioBuffer<float> row2Buffer;
    juce::AudioBuffer<float> masterBuffer;
    juce::AudioBuffer<float> dryBypassBuffer;
    juce::MidiBuffer incomingMidiBuffer;
    juce::MidiBuffer slotMidiBuffer;
    std::mutex midiMutex;

    double currentSampleRate{ 44100.0 };
    int currentBlockSize{ 512 };

    // Tap tempo logic
    std::vector<juce::int64> tapTimes;

    // Dynamics state
    float gateGain{ 1.0f };
    float compEnvelope{ 1.0f };
    float limiterEnvelope{ 1.0f };

    void processGate(juce::AudioBuffer<float>& buffer, int numSamples);
    void processTubeCompressor(juce::AudioBuffer<float>& buffer, int numSamples);
    void processMasterLimiter(juce::AudioBuffer<float>& buffer, int numSamples);
    void routeInput(const float* const* inputData, int numInputs, juce::AudioBuffer<float>& dst, InputChannelMode mode, int numSamples);

    juce::File getPluginCacheFile() const;
    juce::File getDeadMansPedalFile() const;

    class ScannerThread : public juce::Thread
    {
    public:
        ScannerThread(ChainedAudioEngine& owner, bool force)
            : Thread("ChainedVST3Scanner"), engine(owner), forceRescan(force)
        {}

        void run() override
        {
            engine.performScan(this, forceRescan);
        }

    private:
        ChainedAudioEngine& engine;
        bool forceRescan{ false };
    };

    std::unique_ptr<ScannerThread> scannerThread;
    std::atomic<bool> isScanning{ false };
    juce::String currentScanningPlugin;
};


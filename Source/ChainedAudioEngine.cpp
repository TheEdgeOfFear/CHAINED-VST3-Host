#include "ChainedAudioEngine.h"

#if JUCE_WINDOWS
 #define NOMINMAX
 #include <windows.h>
#endif

ChainedAudioEngine::ChainedAudioEngine()
{
    formatManager.addDefaultFormats();

    for (int i = 0; i < 12; ++i)
    {
        slots[static_cast<size_t>(i)] = std::make_unique<PluginSlot>(i);
        slots[static_cast<size_t>(i)]->setPlayHead(&playHead);
    }

    backingPlayer.onBpmDetected = [this](double detectedBpm)
    {
        if (backingPlayer.getBpmLinked())
        {
            setMasterBpm(detectedBpm);
        }
        sendChangeMessage();
    };
}

ChainedAudioEngine::~ChainedAudioEngine()
{
    if (scannerThread != nullptr)
    {
        scannerThread->signalThreadShouldExit();
        scannerThread->stopThread(2000);
        scannerThread.reset();
    }

    deviceManager.removeAudioCallback(this);
    deviceManager.removeMidiInputDeviceCallback({}, this);

    for (auto& slot : slots)
    {
        if (slot)
            slot->unloadPlugin();
    }
}

juce::File ChainedAudioEngine::getPluginCacheFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("TheEdgeOfFear")
        .getChildFile("CHAINED");
    dir.createDirectory();
    return dir.getChildFile("VST3_KnownPlugins.xml");
}

juce::File ChainedAudioEngine::getDeadMansPedalFile() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("TheEdgeOfFear")
        .getChildFile("CHAINED");
    dir.createDirectory();
    return dir.getChildFile("Scanner_DeadmansPedal.txt");
}

#if JUCE_WINDOWS
static void safelyScanVst3(juce::VST3PluginFormat& format, juce::OwnedArray<juce::PluginDescription>& types, const juce::String& path)
{
    __try
    {
        format.findAllTypesForFile(types, path);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        // Trapped crash in 3rd party VST3 DLL safely
    }
}
#endif

void ChainedAudioEngine::initializeAudio()
{
    // Try ASIO first for professional low-latency & high quality sound
    juce::String audioError;
    auto& deviceTypes = deviceManager.getAvailableDeviceTypes();
    bool hasAsio = false;
    for (auto* type : deviceTypes)
    {
        if (type != nullptr && type->getTypeName().equalsIgnoreCase("ASIO"))
        {
            hasAsio = true;
            break;
        }
    }

    if (hasAsio)
    {
        deviceManager.setCurrentAudioDeviceType("ASIO", true);
        audioError = deviceManager.initialiseWithDefaultDevices(2, 2);
    }

    if (!hasAsio || audioError.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr)
    {
        deviceManager.setCurrentAudioDeviceType("Windows Audio", true);
        deviceManager.initialise(2, 2, nullptr, true);
    }

    deviceManager.addAudioCallback(this);

    // Enable all available MIDI input devices
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (const auto& dev : midiInputs)
    {
        deviceManager.setMidiInputDeviceEnabled(dev.identifier, true);
        deviceManager.addMidiInputDeviceCallback(dev.identifier, this);
    }

    // Load cached VST3 plugins for instant startup
    auto cacheFile = getPluginCacheFile();
    if (cacheFile.existsAsFile())
    {
        std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(cacheFile));
        if (xml != nullptr)
            knownPluginList.recreateFromXml(*xml);
    }

    // Start background scanner (non-blocking)
    startPluginScan(false);
}

void ChainedAudioEngine::startPluginScan(bool forceRescan)
{
    if (scannerThread != nullptr)
    {
        if (scannerThread->isThreadRunning())
            return;
        scannerThread.reset();
    }

    scannerThread = std::make_unique<ScannerThread>(*this, forceRescan);
    scannerThread->startThread(juce::Thread::Priority::normal);
}

bool ChainedAudioEngine::isPluginScanRunning() const
{
    return isScanning.load(std::memory_order_relaxed);
}

void ChainedAudioEngine::performScan(juce::Thread* thread, bool forceRescan)
{
    isScanning.store(true);

#if JUCE_WINDOWS
    const UINT oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#endif

    if (forceRescan)
    {
        knownPluginList.clear();
    }

    juce::VST3PluginFormat vst3Format;

    // Blacklist problematic plugins that show modal errors on initialization
    juce::StringArray blacklist;
    blacklist.add("gladiator.vst3");
    blacklist.add("gladiator");

    juce::Array<juce::File> searchRoots;
    const juce::File commonVst3("C:\\Program Files\\Common Files\\VST3");
    if (commonVst3.isDirectory())
        searchRoots.add(commonVst3);

    const juce::File localVst3 = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getParentDirectory()
        .getChildFile("Local/Programs/Common/VST3");
    if (localVst3.isDirectory())
        searchRoots.add(localVst3);

    juce::Array<juce::File> filesToScan;

    for (const auto& root : searchRoots)
    {
        juce::Array<juce::File> found;
        root.findChildFiles(found, juce::File::findFilesAndDirectories, true, "*.vst3");

        for (const auto& f : found)
        {
            // Skip inner files if already inside a .vst3 bundle
            const juce::String path = f.getFullPathName();
            if (path.containsIgnoreCase("Contents\\x86_64-win") || path.containsIgnoreCase("Contents/x86_64-win"))
                continue;

            bool isBlacklisted = false;
            for (const auto& b : blacklist)
            {
                if (f.getFileName().containsIgnoreCase(b) || path.containsIgnoreCase(b))
                {
                    isBlacklisted = true;
                    break;
                }
            }

            if (!isBlacklisted)
            {
                filesToScan.add(f);
            }
        }
    }

    for (const auto& f : filesToScan)
    {
        if (thread != nullptr && thread->threadShouldExit())
            break;

        currentScanningPlugin = f.getFileNameWithoutExtension();

        if (!forceRescan)
        {
            bool alreadyKnown = false;
            for (const auto& known : knownPluginList.getTypes())
            {
                if (known.fileOrIdentifier.equalsIgnoreCase(f.getFullPathName()) ||
                    known.name.equalsIgnoreCase(f.getFileNameWithoutExtension()))
                {
                    alreadyKnown = true;
                    break;
                }
            }
            if (alreadyKnown)
                continue;
        }

        juce::OwnedArray<juce::PluginDescription> typesFound;
        try
        {
#if JUCE_WINDOWS
            safelyScanVst3(vst3Format, typesFound, f.getFullPathName());
#else
            vst3Format.findAllTypesForFile(typesFound, f.getFullPathName());
#endif
            for (auto* t : typesFound)
            {
                if (t != nullptr)
                {
                    if (t->manufacturerName.isEmpty() || t->manufacturerName == "Unknown")
                    {
                        auto parent = f.getParentDirectory();
                        if (parent.getFileName() != "VST3")
                            t->manufacturerName = parent.getFileName();
                        else
                            t->manufacturerName = "Other";
                    }
                    knownPluginList.addType(*t);
                }
            }
        }
        catch (...)
        {
        }
    }

#if JUCE_WINDOWS
    SetErrorMode(oldErrorMode);
#endif

    currentScanningPlugin.clear();
    isScanning.store(false);

    // Save updated plugin list to XML cache
    auto cacheFile = getPluginCacheFile();
    std::unique_ptr<juce::XmlElement> xml(knownPluginList.createXml());
    if (xml != nullptr)
        xml->writeTo(cacheFile);

    juce::MessageManager::callAsync([this]
    {
        sendChangeMessage();
    });
}

void ChainedAudioEngine::setRoutingMode(RoutingMode mode)
{
    routingMode.store(mode);
    sendChangeMessage();
}

void ChainedAudioEngine::toggleRoutingMode()
{
    if (routingMode.load() == RoutingMode::Unified)
        setRoutingMode(RoutingMode::Duality);
    else
        setRoutingMode(RoutingMode::Unified);
}

void ChainedAudioEngine::setMasterBpm(double bpm)
{
    playHead.setBpm(bpm);
    backingPlayer.setMasterBpm(bpm);
    sendChangeMessage();
}

void ChainedAudioEngine::tapTempo()
{
    const auto now = juce::Time::currentTimeMillis();
    tapTimes.push_back(now);

    // Keep only last 4 taps within 2.5 seconds
    while (tapTimes.size() > 4 || (tapTimes.size() > 1 && now - tapTimes.front() > 2500))
    {
        tapTimes.erase(tapTimes.begin());
    }

    if (tapTimes.size() >= 2)
    {
        double avgIntervalMs = 0.0;
        for (size_t i = 1; i < tapTimes.size(); ++i)
        {
            avgIntervalMs += static_cast<double>(tapTimes[i] - tapTimes[i - 1]);
        }
        avgIntervalMs /= static_cast<double>(tapTimes.size() - 1);

        if (avgIntervalMs > 50.0)
        {
            const double calculatedBpm = 60000.0 / avgIntervalMs;
            setMasterBpm(std::clamp(calculatedBpm, 20.0, 300.0));
        }
    }
}

void ChainedAudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    currentSampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    currentBlockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;

    row1Buffer.setSize(2, currentBlockSize);
    row2Buffer.setSize(2, currentBlockSize);
    masterBuffer.setSize(2, currentBlockSize);
    dryBypassBuffer.setSize(2, currentBlockSize);

    for (auto& slot : slots)
    {
        if (slot)
            slot->prepareToPlay(currentSampleRate, currentBlockSize);
    }

    backingPlayer.prepareToPlay(currentSampleRate, currentBlockSize);
    playHead.resetPosition();
}

void ChainedAudioEngine::audioDeviceStopped()
{
    for (auto& slot : slots)
    {
        if (slot)
            slot->unloadPlugin();
    }
}

void ChainedAudioEngine::handleIncomingMidiMessage(juce::MidiInput* /*source*/, const juce::MidiMessage& message)
{
    std::lock_guard<std::mutex> lock(midiMutex);
    incomingMidiBuffer.addEvent(message, 0);
}

void ChainedAudioEngine::routeInput(const float* const* inputData, int numInputs, juce::AudioBuffer<float>& dst, InputChannelMode mode, int numSamples)
{
    dst.clear();
    if (inputData == nullptr || numInputs == 0)
        return;

    const float* inL = inputData[0];
    const float* inR = (numInputs > 1 && inputData[1] != nullptr) ? inputData[1] : inL;

    if (inL == nullptr && inR == nullptr)
        return;
    if (inL == nullptr) inL = inR;
    if (inR == nullptr) inR = inL;

    if (mode == InputChannelMode::Input1)
    {
        dst.copyFrom(0, 0, inL, numSamples);
        dst.copyFrom(1, 0, inL, numSamples); // mono duplicate
    }
    else if (mode == InputChannelMode::Input2)
    {
        dst.copyFrom(0, 0, inR, numSamples);
        dst.copyFrom(1, 0, inR, numSamples); // mono duplicate
    }
    else // Stereo_1_2
    {
        dst.copyFrom(0, 0, inL, numSamples);
        dst.copyFrom(1, 0, inR, numSamples);
    }
}

void ChainedAudioEngine::processGate(juce::AudioBuffer<float>& buffer, int numSamples)
{
    const float threshDb = gateThresholdDb.load();
    if (threshDb <= -89.0f)
        return; // Gate disabled

    const float threshLinear = juce::Decibels::decibelsToGain(threshDb);
    const float alphaAttack = 0.05f;
    const float alphaRelease = 0.002f;

    const float* l = buffer.getReadPointer(0);
    const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : l;

    for (int i = 0; i < numSamples; ++i)
    {
        const float peak = std::max(std::abs(l[i]), std::abs(r[i]));
        const float target = (peak > threshLinear) ? 1.0f : 0.0f;

        if (target > gateGain)
            gateGain += alphaAttack * (target - gateGain);
        else
            gateGain += alphaRelease * (target - gateGain);

        buffer.setSample(0, i, buffer.getSample(0, i) * gateGain);
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, i, buffer.getSample(1, i) * gateGain);
    }
}

void ChainedAudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                         int numInputChannels,
                                                         float* const* outputChannelData,
                                                         int numOutputChannels,
                                                         int numSamples,
                                                         const juce::AudioIODeviceCallbackContext& /*context*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Advance Playhead
    playHead.advanceSamples(numSamples, currentSampleRate);

    // Retrieve and process MIDI
    slotMidiBuffer.clear();
    {
        std::lock_guard<std::mutex> lock(midiMutex);
        slotMidiBuffer.addEvents(incomingMidiBuffer, 0, numSamples, 0);
        incomingMidiBuffer.clear();
    }

    midiManager.processMidi(slotMidiBuffer, [this](const juce::String& targetId, int slotIdx, int paramIdx, float val)
    {
        if (targetId == "row1_footswitch")
            toggleRow1Enabled();
        else if (targetId == "row2_footswitch")
            toggleRow2Enabled();
        else if (targetId == "mode_switch")
            toggleRoutingMode();
        else if (targetId == "master_input_gain")
            setInputGainDecibels(juce::jmap(val, -24.0f, 24.0f));
        else if (targetId == "master_output_gain")
            setOutputGainDecibels(juce::jmap(val, -24.0f, 24.0f));
        else if (targetId == "noise_gate_thresh")
            setGateThresholdDecibels(juce::jmap(val, -90.0f, 0.0f));
        else if (targetId == "tube_comp_mode")
            setTubeCompMode(static_cast<int>(std::round(juce::jmap(val, 0.0f, 4.0f))));
        else if (targetId == "limiter_thresh")
            setLimiterThresholdDecibels(juce::jmap(val, -18.0f, 0.0f));
        else if (targetId == "mp3_play_pause")
            backingPlayer.togglePlayPause();
        else if (targetId == "mp3_prev")
            backingPlayer.previousTrack();
        else if (targetId == "mp3_next")
            backingPlayer.nextTrack();
        else if (targetId == "mp3_loop_toggle")
            backingPlayer.setLoopEnabled(!backingPlayer.getLoopEnabled());
        else if (targetId.startsWith("slot_bypass:") || targetId.startsWith("slot_enable:"))
        {
            int sIdx = -1;
            if (targetId.startsWith("slot_bypass:"))
                sIdx = targetId.fromFirstOccurrenceOf("slot_bypass:", false, false).getIntValue();
            else if (targetId.startsWith("slot_enable:"))
                sIdx = targetId.fromFirstOccurrenceOf("slot_enable:", false, false).getIntValue();

            if (auto* slot = getSlot(sIdx))
            {
                if (val >= 0.5f)
                    slot->toggleBypass();
            }
        }
        else if (targetId.startsWith("vst:"))
        {
            auto parts = juce::StringArray::fromTokens(targetId, ":", "");
            if (parts.size() >= 3)
            {
                const int s = parts[1].getIntValue();
                const int p = parts[2].getIntValue();
                if (auto* slot = getSlot(s))
                {
                    if (auto* inst = slot->getInstance())
                    {
                        const auto& params = inst->getParameters();
                        if (p >= 0 && p < params.size())
                        {
                            if (auto* param = params[p])
                                param->setValue(val);
                        }
                    }
                }
            }
        }
        else if (slotIdx >= 0 && slotIdx < 12)
        {
            if (auto* slot = getSlot(slotIdx))
            {
                if (auto* inst = slot->getInstance())
                {
                    const auto& params = inst->getParameters();
                    if (paramIdx >= 0 && paramIdx < params.size())
                    {
                        if (auto* param = params[paramIdx])
                            param->setValue(val);
                    }
                }
            }
        }
    });

    const float inGain = juce::Decibels::decibelsToGain(inputGainDb.load());
    const float outGain = juce::Decibels::decibelsToGain(outputGainDb.load());

    // Input Metering calculation
    float inMaxL = 0.0f, inMaxR = 0.0f;
    if (inputChannelData != nullptr && numInputChannels > 0)
    {
        const float* l = inputChannelData[0];
        const float* r = numInputChannels > 1 ? inputChannelData[1] : l;
        for (int i = 0; i < numSamples; ++i)
        {
            inMaxL = std::max(inMaxL, std::abs(l[i]));
            inMaxR = std::max(inMaxR, std::abs(r[i]));
        }
    }
    inMeterL.store(inMaxL * inGain);
    inMeterR.store(inMaxR * inGain);

    masterBuffer.setSize(2, numSamples, false, false, true);
    masterBuffer.clear();

    const auto mode = routingMode.load();
    juce::MidiBuffer slotMidiCopy;

    if (mode == RoutingMode::Unified)
    {
        // Unified Mode: 12-Slot contiguous daisy chain
        routeInput(inputChannelData, numInputChannels, masterBuffer, masterInputMode.load(), numSamples);
        masterBuffer.applyGain(inGain);
        processGate(masterBuffer, numSamples);

        for (int i = 0; i < 12; ++i)
        {
            // Row 1 footswitch controls slots 0..5, Row 2 footswitch controls slots 6..11
            if (i < 6 && !row1Enabled.load())
                continue;
            if (i >= 6 && !row2Enabled.load())
                continue;

            if (auto* slot = slots[static_cast<size_t>(i)].get())
            {
                slotMidiCopy = slotMidiBuffer;
                slot->processBlock(masterBuffer, slotMidiCopy);
            }
        }
    }
    else
    {
        // Duality Mode: Split 2-row parallel signal chains
        row1Buffer.setSize(2, numSamples, false, false, true);
        row2Buffer.setSize(2, numSamples, false, false, true);

        // Row 1 Path (only processes if row is enabled and has plugins or active chain)
        if (row1Enabled.load() && isRowOccupied(1))
        {
            routeInput(inputChannelData, numInputChannels, row1Buffer, row1InputMode.load(), numSamples);
            row1Buffer.applyGain(inGain);
            processGate(row1Buffer, numSamples);

            for (int i = 0; i < 6; ++i)
            {
                if (auto* slot = slots[static_cast<size_t>(i)].get())
                {
                    slotMidiCopy = slotMidiBuffer;
                    slot->processBlock(row1Buffer, slotMidiCopy);
                }
            }
            masterBuffer.addFrom(0, 0, row1Buffer, 0, 0, numSamples);
            masterBuffer.addFrom(1, 0, row1Buffer, 1, 0, numSamples);
        }
        else
        {
            row1Buffer.clear();
        }

        // Row 2 Path (only processes if row is enabled and has plugins or active chain)
        if (row2Enabled.load() && isRowOccupied(2))
        {
            routeInput(inputChannelData, numInputChannels, row2Buffer, row2InputMode.load(), numSamples);
            row2Buffer.applyGain(inGain);
            processGate(row2Buffer, numSamples);

            for (int i = 6; i < 12; ++i)
            {
                if (auto* slot = slots[static_cast<size_t>(i)].get())
                {
                    slotMidiCopy = slotMidiBuffer;
                    slot->processBlock(row2Buffer, slotMidiCopy);
                }
            }
            masterBuffer.addFrom(0, 0, row2Buffer, 0, 0, numSamples);
            masterBuffer.addFrom(1, 0, row2Buffer, 1, 0, numSamples);
        }
        else
        {
            row2Buffer.clear();
        }
    }

    // Mix in Backing Track / MP3
    backingPlayer.processBlock(masterBuffer, numSamples);

    // Process Studio Tube Compressor
    processTubeCompressor(masterBuffer, numSamples);

    // Process Master Peak Limiter
    processMasterLimiter(masterBuffer, numSamples);

    // Master Output Gain
    masterBuffer.applyGain(outGain);

    // Output Metering & write to physical outputs
    float outMaxL = 0.0f, outMaxR = 0.0f;
    const float* mbL = masterBuffer.getReadPointer(0);
    const float* mbR = masterBuffer.getNumChannels() > 1 ? masterBuffer.getReadPointer(1) : mbL;

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
        {
            const float* src = (ch % 2 == 0) ? mbL : mbR;
            juce::FloatVectorOperations::copy(outputChannelData[ch], src, numSamples);
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        outMaxL = std::max(outMaxL, std::abs(mbL[i]));
        outMaxR = std::max(outMaxR, std::abs(mbR[i]));
    }
    outMeterL.store(outMaxL);
    outMeterR.store(outMaxR);
}

void ChainedAudioEngine::processTubeCompressor(juce::AudioBuffer<float>& buffer, int numSamples)
{
    const int mode = tubeCompMode.load();
    if (mode <= 0) return; // OFF

    float ratio = 4.0f;
    float threshDb = -16.0f;
    float makeupDb = 1.5f;

    if (mode == 1)      { ratio = 4.0f;  threshDb = -16.0f; makeupDb = 1.5f; } // 4:1 Smooth leveling
    else if (mode == 2) { ratio = 8.0f;  threshDb = -18.0f; makeupDb = 3.0f; } // 8:1 Classic punch
    else if (mode == 3) { ratio = 12.0f; threshDb = -20.0f; makeupDb = 4.5f; } // 12:1 Heavy rock tone
    else if (mode == 4) { ratio = 20.0f; threshDb = -24.0f; makeupDb = 6.0f; } // 20:1 Brickwall leveling

    const float threshLinear = juce::Decibels::decibelsToGain(threshDb);
    const float makeupLinear = juce::Decibels::decibelsToGain(makeupDb);
    const float alphaAttack = static_cast<float>(1.0 - std::exp(-1.0 / (0.010 * currentSampleRate)));  // 10ms attack
    const float alphaRelease = static_cast<float>(1.0 - std::exp(-1.0 / (0.120 * currentSampleRate))); // 120ms optical release

    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float peak = std::max(std::abs(l[i]), (r != nullptr) ? std::abs(r[i]) : 0.0f);

        float targetGain = 1.0f;
        if (peak > threshLinear && threshLinear > 0.0001f)
        {
            const float peakDb = juce::Decibels::gainToDecibels(peak);
            const float overDb = peakDb - threshDb;
            const float grDb = overDb * (1.0f - 1.0f / ratio);
            targetGain = juce::Decibels::decibelsToGain(-grDb);
        }

        if (targetGain < compEnvelope)
            compEnvelope += alphaAttack * (targetGain - compEnvelope);
        else
            compEnvelope += alphaRelease * (targetGain - compEnvelope);

        // Apply dynamic gain + makeup gain
        const float totalGain = compEnvelope * makeupLinear;
        l[i] *= totalGain;
        if (r != nullptr)
            r[i] *= totalGain;

        // Subtle 2nd & 3rd harmonic tube warmth
        auto tubeHarmonics = [](float x) -> float
        {
            return x - 0.03f * (x * x * (x > 0.0f ? 1.0f : -1.0f));
        };
        l[i] = tubeHarmonics(l[i]);
        if (r != nullptr)
            r[i] = tubeHarmonics(r[i]);
    }
}

void ChainedAudioEngine::processMasterLimiter(juce::AudioBuffer<float>& buffer, int numSamples)
{
    const float threshDb = limiterThreshDb.load();
    const float threshLinear = juce::Decibels::decibelsToGain(threshDb);
    const float alphaAttack = static_cast<float>(1.0 - std::exp(-1.0 / (0.001 * currentSampleRate)));  // 1ms ultra-fast attack
    const float alphaRelease = static_cast<float>(1.0 - std::exp(-1.0 / (0.040 * currentSampleRate))); // 40ms smooth release

    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float peak = std::max(std::abs(l[i]), (r != nullptr) ? std::abs(r[i]) : 0.0f);

        float targetGain = 1.0f;
        if (peak > threshLinear && threshLinear > 0.0001f)
        {
            targetGain = threshLinear / peak;
        }

        if (targetGain < limiterEnvelope)
            limiterEnvelope += alphaAttack * (targetGain - limiterEnvelope);
        else
            limiterEnvelope += alphaRelease * (targetGain - limiterEnvelope);

        l[i] *= limiterEnvelope;
        if (r != nullptr)
            r[i] *= limiterEnvelope;

        // Safety True-Peak ceiling at -0.1 dBFS (0.988) with soft analog knee
        auto softCeiling = [](float x) -> float
        {
            const float ceiling = 0.988f;
            if (x > ceiling)
                return ceiling + (1.0f - ceiling) * std::tanh((x - ceiling) / (1.0f - ceiling));
            if (x < -ceiling)
                return -ceiling + (1.0f - ceiling) * std::tanh((x + ceiling) / (1.0f - ceiling));
            return x;
        };

        l[i] = softCeiling(l[i]);
        if (r != nullptr)
            r[i] = softCeiling(r[i]);
    }
}

juce::ValueTree ChainedAudioEngine::exportPresetToValueTree(const juce::String& presetName)
{
    juce::ValueTree root("ChainedRig");
    root.setProperty("name", presetName, nullptr);
    root.setProperty("routingMode", static_cast<int>(routingMode.load()), nullptr);
    root.setProperty("masterInputMode", static_cast<int>(masterInputMode.load()), nullptr);
    root.setProperty("row1InputMode", static_cast<int>(row1InputMode.load()), nullptr);
    root.setProperty("row2InputMode", static_cast<int>(row2InputMode.load()), nullptr);
    root.setProperty("row1Enabled", row1Enabled.load(), nullptr);
    root.setProperty("row2Enabled", row2Enabled.load(), nullptr);
    root.setProperty("inputGainDb", inputGainDb.load(), nullptr);
    root.setProperty("outputGainDb", outputGainDb.load(), nullptr);
    root.setProperty("gateThresholdDb", gateThresholdDb.load(), nullptr);
    root.setProperty("tubeCompMode", tubeCompMode.load(), nullptr);
    root.setProperty("limiterThreshDb", limiterThreshDb.load(), nullptr);
    root.setProperty("bpm", playHead.getBpm(), nullptr);

    juce::ValueTree slotsTree("Slots");
    for (int i = 0; i < 12; ++i)
    {
        if (slots[static_cast<size_t>(i)])
            slotsTree.addChild(slots[static_cast<size_t>(i)]->saveToValueTree(), -1, nullptr);
    }
    root.addChild(slotsTree, -1, nullptr);

    root.addChild(midiManager.saveToValueTree(), -1, nullptr);

    return root;
}

void ChainedAudioEngine::loadPresetFromValueTree(const juce::ValueTree& tree)
{
    if (!tree.hasType("ChainedRig")) return;

    routingMode.store(static_cast<RoutingMode>(static_cast<int>(tree.getProperty("routingMode", 0))));
    masterInputMode.store(static_cast<InputChannelMode>(static_cast<int>(tree.getProperty("masterInputMode", 0))));
    row1InputMode.store(static_cast<InputChannelMode>(static_cast<int>(tree.getProperty("row1InputMode", 0))));
    row2InputMode.store(static_cast<InputChannelMode>(static_cast<int>(tree.getProperty("row2InputMode", 1))));
    row1Enabled.store(tree.getProperty("row1Enabled", true));
    row2Enabled.store(tree.getProperty("row2Enabled", true));
    inputGainDb.store(tree.getProperty("inputGainDb", 0.0f));
    outputGainDb.store(tree.getProperty("outputGainDb", 0.0f));
    gateThresholdDb.store(tree.getProperty("gateThresholdDb", -90.0f));
    tubeCompMode.store(tree.getProperty("tubeCompMode", 0));
    limiterThreshDb.store(tree.getProperty("limiterThreshDb", 0.0f));
    setMasterBpm(tree.getProperty("bpm", 120.0));

    auto slotsTree = tree.getChildWithName("Slots");
    if (slotsTree.isValid())
    {
        for (int i = 0; i < slotsTree.getNumChildren() && i < 12; ++i)
        {
            auto slotChild = slotsTree.getChild(i);
            const int idx = slotChild.getProperty("index", i);
            if (idx >= 0 && idx < 12 && slots[static_cast<size_t>(idx)])
            {
                slots[static_cast<size_t>(idx)]->restoreFromValueTree(slotChild, formatManager, currentSampleRate, currentBlockSize);
            }
        }
    }

    auto midiTree = tree.getChildWithName("MidiBindings");
    if (midiTree.isValid())
    {
        midiManager.restoreFromValueTree(midiTree);
    }

    sendChangeMessage();
}

void ChainedAudioEngine::savePresetToFile(const juce::File& file)
{
    auto tree = exportPresetToValueTree(file.getFileNameWithoutExtension());
    std::unique_ptr<juce::XmlElement> xml(tree.createXml());
    if (xml != nullptr)
    {
        xml->writeTo(file);
    }
}

void ChainedAudioEngine::loadPresetFromFile(const juce::File& file)
{
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml != nullptr)
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        loadPresetFromValueTree(tree);
    }
}

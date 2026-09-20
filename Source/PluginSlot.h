#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "ChainedPlayHead.h"
#include <atomic>
#include <memory>
#include <mutex>

class PluginWindow;

class PluginSlot
{
public:
    PluginSlot(int slotIndex = 0)
        : index(slotIndex)
    {
    }

    ~PluginSlot()
    {
        unloadPlugin();
    }

    int getIndex() const { return index; }

    bool isOccupied() const
    {
        return isLoaded.load(std::memory_order_relaxed);
    }

    bool isBypassed() const
    {
        return bypassed.load(std::memory_order_relaxed);
    }

    void setBypassed(bool bypass)
    {
        bypassed.store(bypass, std::memory_order_relaxed);
        if (pluginInstance != nullptr)
        {
            if (auto* bypassParam = pluginInstance->getBypassParameter())
                bypassParam->setValue(bypass ? 1.0f : 0.0f);
        }
    }

    void toggleBypass()
    {
        setBypassed(!isBypassed());
    }

    juce::String getPluginName() const
    {
        if (isLoaded.load() && pluginInstance != nullptr)
            return pluginInstance->getName();
        return "EMPTY";
    }

    juce::String getPluginFormat() const
    {
        if (isLoaded.load())
            return description.pluginFormatName;
        return "";
    }

    const juce::PluginDescription& getDescription() const
    {
        return description;
    }

    juce::AudioPluginInstance* getInstance()
    {
        return pluginInstance.get();
    }

    void setPlayHead(ChainedPlayHead* head)
    {
        playHead = head;
        if (pluginInstance != nullptr && playHead != nullptr)
            pluginInstance->setPlayHead(playHead);
    }

    void loadPlugin(std::unique_ptr<juce::AudioPluginInstance> newPlugin,
                    const juce::PluginDescription& desc,
                    double sampleRate,
                    int blockSize)
    {
        closeEditorWindow();

        std::lock_guard<std::mutex> lock(processMutex);
        pluginInstance = std::move(newPlugin);
        description = desc;

        if (pluginInstance != nullptr)
        {
            pluginInstance->enableAllBuses();
            pluginInstance->setRateAndBufferSizeDetails(sampleRate, blockSize);
            pluginInstance->prepareToPlay(sampleRate, blockSize);
            if (playHead != nullptr)
                pluginInstance->setPlayHead(playHead);
            isLoaded.store(true);
            bypassed.store(false);
        }
        else
        {
            isLoaded.store(false);
        }
    }

    void unloadPlugin()
    {
        closeEditorWindow();
        std::lock_guard<std::mutex> lock(processMutex);
        if (pluginInstance != nullptr)
        {
            pluginInstance->releaseResources();
            pluginInstance.reset();
        }
        isLoaded.store(false);
    }

    void prepareToPlay(double sampleRate, int blockSize)
    {
        std::lock_guard<std::mutex> lock(processMutex);
        if (pluginInstance != nullptr)
        {
            pluginInstance->enableAllBuses();
            pluginInstance->setRateAndBufferSizeDetails(sampleRate, blockSize);
            pluginInstance->prepareToPlay(sampleRate, blockSize);
            if (playHead != nullptr)
                pluginInstance->setPlayHead(playHead);
        }
        monoBuffer.setSize(2, blockSize);
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
    {
        if (!isLoaded.load(std::memory_order_relaxed) || bypassed.load(std::memory_order_relaxed))
            return;

        std::unique_lock<std::mutex> lock(processMutex, std::try_to_lock);
        if (!lock.owns_lock() || pluginInstance == nullptr)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        const int pluginInChannels = pluginInstance->getTotalNumInputChannels();
        const int pluginOutChannels = pluginInstance->getTotalNumOutputChannels();

        if (pluginInChannels <= 0 || pluginOutChannels <= 0)
            return;

        if (pluginInChannels == 1 && numChannels >= 2)
        {
            monoBuffer.setSize(1, numSamples, false, false, true);
            monoBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
            monoBuffer.addFrom(0, 0, buffer, 1, 0, numSamples);
            monoBuffer.applyGain(0.5f);

            pluginInstance->processBlock(monoBuffer, midiMessages);

            buffer.copyFrom(0, 0, monoBuffer, 0, 0, numSamples);
            buffer.copyFrom(1, 0, monoBuffer, 0, 0, numSamples);
        }
        else
        {
            pluginInstance->processBlock(buffer, midiMessages);
        }
    }

    void openEditorWindow();
    void closeEditorWindow();
    bool isWindowOpen() const;

    juce::ValueTree saveToValueTree() const
    {
        juce::ValueTree tree("Slot");
        tree.setProperty("index", index, nullptr);
        tree.setProperty("bypassed", bypassed.load(), nullptr);
        tree.setProperty("isLoaded", isLoaded.load(), nullptr);

        if (isLoaded.load() && pluginInstance != nullptr)
        {
            tree.setProperty("name", description.name, nullptr);
            tree.setProperty("fileOrIdentifier", description.fileOrIdentifier, nullptr);
            tree.setProperty("format", description.pluginFormatName, nullptr);
            tree.setProperty("category", description.category, nullptr);
            tree.setProperty("manufacturerName", description.manufacturerName, nullptr);
            tree.setProperty("uniqueId", description.uniqueId, nullptr);
            tree.setProperty("isInstrument", description.isInstrument, nullptr);
            tree.setProperty("numInputChannels", description.numInputChannels, nullptr);
            tree.setProperty("numOutputChannels", description.numOutputChannels, nullptr);

            juce::MemoryBlock stateBlock;
            pluginInstance->getStateInformation(stateBlock);
            if (stateBlock.getSize() > 0)
            {
                tree.setProperty("stateData", stateBlock.toBase64Encoding(), nullptr);
            }
        }
        return tree;
    }

    void restoreFromValueTree(const juce::ValueTree& tree,
                              juce::AudioPluginFormatManager& formatManager,
                              double sampleRate,
                              int blockSize)
    {
        if (!tree.hasType("Slot")) return;

        bypassed.store(tree.getProperty("bypassed", false));
        const bool shouldBeLoaded = tree.getProperty("isLoaded", false);

        if (shouldBeLoaded)
        {
            juce::PluginDescription desc;
            desc.name = tree.getProperty("name", "");
            desc.fileOrIdentifier = tree.getProperty("fileOrIdentifier", "");
            desc.pluginFormatName = tree.getProperty("format", "VST3");
            desc.category = tree.getProperty("category", "");
            desc.manufacturerName = tree.getProperty("manufacturerName", "");
            desc.uniqueId = tree.getProperty("uniqueId", 0);
            desc.isInstrument = tree.getProperty("isInstrument", false);
            desc.numInputChannels = tree.getProperty("numInputChannels", 2);
            desc.numOutputChannels = tree.getProperty("numOutputChannels", 2);

            const double sr = sampleRate > 0.0 ? sampleRate : 44100.0;
            const int bs = blockSize > 0 ? blockSize : 512;

            juce::String errorMsg;
            std::unique_ptr<juce::AudioPluginInstance> instance;

            // 1. Direct scan of the file to get full native VST3 PluginDescription with UID
            juce::File pluginFile(desc.fileOrIdentifier);
            if (pluginFile.exists())
            {
                juce::VST3PluginFormat vst3Format;
                juce::OwnedArray<juce::PluginDescription> types;
                vst3Format.findAllTypesForFile(types, pluginFile.getFullPathName());

                for (auto* t : types)
                {
                    if (t != nullptr)
                    {
                        if (desc.name.isEmpty() || t->name.equalsIgnoreCase(desc.name))
                        {
                            desc = *t;
                            instance = formatManager.createPluginInstance(*t, sr, bs, errorMsg);
                            if (instance != nullptr)
                                break;
                        }
                    }
                }

                if (instance == nullptr && !types.isEmpty() && types.getFirst() != nullptr)
                {
                    desc = *types.getFirst();
                    instance = formatManager.createPluginInstance(*types.getFirst(), sr, bs, errorMsg);
                }
            }

            // 2. Fallback to formatManager createPluginInstance
            if (instance == nullptr)
            {
                instance = formatManager.createPluginInstance(desc, sr, bs, errorMsg);
            }

            if (instance != nullptr)
            {
                loadPlugin(std::move(instance), desc, sr, bs);

                const juce::String base64 = tree.getProperty("stateData", "");
                if (base64.isNotEmpty())
                {
                    juce::MemoryBlock stateBlock;
                    stateBlock.fromBase64Encoding(base64);
                    if (stateBlock.getSize() > 0 && pluginInstance != nullptr)
                    {
                        pluginInstance->setStateInformation(stateBlock.getData(), static_cast<int>(stateBlock.getSize()));
                    }
                }
            }
        }
        else
        {
            unloadPlugin();
        }
    }

private:
    int index{ 0 };
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    juce::PluginDescription description;
    std::atomic<bool> isLoaded{ false };
    std::atomic<bool> bypassed{ false };
    std::mutex processMutex;
    ChainedPlayHead* playHead{ nullptr };
    juce::AudioBuffer<float> monoBuffer;

    std::unique_ptr<PluginWindow> window;
};

class PluginWindow : public juce::DocumentWindow
{
public:
    PluginWindow(PluginSlot& ownerSlot, juce::AudioPluginInstance* plugin)
        : DocumentWindow(juce::String(ownerSlot.getIndex() + 1) + ". " + plugin->getName() + " - CHAINED",
                         juce::Colour(0xff181a1d),
                         juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
          slot(ownerSlot)
    {
        setSize(700, 500);
        setUsingNativeTitleBar(true);
        setResizable(true, true);

        if (auto* editor = plugin->createEditorIfNeeded())
        {
            setContentOwned(editor, true);
        }
        else
        {
            auto* fallback = new juce::GenericAudioProcessorEditor(*plugin);
            setContentOwned(fallback, true);
        }

        setVisible(true);
    }

    void closeButtonPressed() override
    {
        slot.closeEditorWindow();
    }

private:
    PluginSlot& slot;
};

inline void PluginSlot::openEditorWindow()
{
    if (pluginInstance == nullptr)
        return;

    if (window == nullptr)
    {
        window = std::make_unique<PluginWindow>(*this, pluginInstance.get());
    }
    else
    {
        window->setVisible(true);
        window->toFront(true);
    }
}

inline void PluginSlot::closeEditorWindow()
{
    if (window != nullptr)
    {
        window->setVisible(false);
        window.reset();
    }
}

inline bool PluginSlot::isWindowOpen() const
{
    return window != nullptr && window->isVisible();
}

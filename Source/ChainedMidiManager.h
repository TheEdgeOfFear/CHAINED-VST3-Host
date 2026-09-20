#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <string>
#include <atomic>
#include <functional>
#include <mutex>

enum class ChainedMidiType
{
    CC_Continuous,
    CC_Toggle,
    Note_Toggle,
    Program_Change
};

struct ChainedMidiBinding
{
    ChainedMidiType type{ ChainedMidiType::CC_Continuous };
    int channel{ 0 };          // 0 = Any / Omni, 1-16
    int controlNumber{ 11 };   // CC #, Note #, or PC #
    juce::String targetId;     // e.g. "slot_bypass:0" or "row1_footswitch" or "mp3_play_pause"
    juce::String targetName;   // e.g. "Slot 1 Footswitch"
    int slotIndex{ -1 };       // >= 0 if mapping to a hosted VST plugin
    int paramIndex{ -1 };      // parameter index in the VST
};

class ChainedMidiManager
{
public:
    ChainedMidiManager() = default;

    void startLearning(const juce::String& targetId, const juce::String& targetName, int slotIndex = -1, int paramIndex = -1)
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        learningTargetId = targetId;
        learningTargetName = targetName;
        learningSlotIndex = slotIndex;
        learningParamIndex = paramIndex;
        isLearning.store(true);
    }

    void cancelLearning()
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        isLearning.store(false);
        learningTargetId.clear();
        learningTargetName.clear();
    }

    bool getIsLearning() const { return isLearning.load(); }
    
    juce::String getLearningTargetName() const
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        return learningTargetName;
    }

    juce::String getLearningTargetId() const
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        return learningTargetId;
    }

    bool getBindingForTarget(const juce::String& targetId, ChainedMidiBinding& outBind) const
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        for (const auto& b : bindings)
        {
            if (b.targetId == targetId)
            {
                outBind = b;
                return true;
            }
        }
        return false;
    }

    void removeBindingForTarget(const juce::String& targetId)
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
            [&](const ChainedMidiBinding& b) { return b.targetId == targetId; }),
            bindings.end());
    }

    void assignCC(const juce::String& targetId, const juce::String& targetName, int ccNumber, int channel = 0, bool isToggle = true, int slotIdx = -1, int paramIdx = -1)
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        ChainedMidiBinding b;
        b.type = isToggle ? ChainedMidiType::CC_Toggle : ChainedMidiType::CC_Continuous;
        b.channel = channel;
        b.controlNumber = ccNumber;
        b.targetId = targetId;
        b.targetName = targetName;
        b.slotIndex = slotIdx;
        b.paramIndex = paramIdx;
        addOrReplaceBindingInternal(b);
    }

    void addOrReplaceBinding(const ChainedMidiBinding& binding)
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        addOrReplaceBindingInternal(binding);
    }

    void removeBinding(int index)
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        if (index >= 0 && index < static_cast<int>(bindings.size()))
            bindings.erase(bindings.begin() + index);
    }

    void clearAllBindings()
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        bindings.clear();
    }

    std::vector<ChainedMidiBinding> getBindings() const
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        return bindings;
    }

    // Process MIDI in real-time audio thread
    void processMidi(const juce::MidiBuffer& midiMessages,
                     std::function<void(const juce::String& targetId, int slotIdx, int paramIdx, float val)> callback)
    {
        for (const auto metadata : midiMessages)
        {
            const auto msg = metadata.getMessage();

            if (msg.isController())
            {
                const int ch = msg.getChannel();
                const int cc = msg.getControllerNumber();
                const int val = msg.getControllerValue();
                const float normalized = static_cast<float>(val) / 127.0f;

                if (isLearning.load())
                {
                    std::lock_guard<std::recursive_mutex> lock(bindingMutex);
                    ChainedMidiBinding newBind;
                    const bool isToggle = (learningTargetId.contains("footswitch") ||
                                           learningTargetId.contains("toggle") ||
                                           learningTargetId.contains("play") ||
                                           learningTargetId.contains("mode") ||
                                           learningTargetId.contains("bypass") ||
                                           learningTargetId.contains("enable"));

                    newBind.type = isToggle ? ChainedMidiType::CC_Toggle : ChainedMidiType::CC_Continuous;
                    newBind.channel = ch;
                    newBind.controlNumber = cc;
                    newBind.targetId = learningTargetId;
                    newBind.targetName = learningTargetName;
                    newBind.slotIndex = learningSlotIndex;
                    newBind.paramIndex = learningParamIndex;

                    addOrReplaceBindingInternal(newBind);
                    isLearning.store(false);
                    learningTargetId.clear();
                    learningTargetName.clear();
                    continue;
                }

                // Execute active bindings
                std::lock_guard<std::recursive_mutex> lock(bindingMutex);
                for (const auto& b : bindings)
                {
                    if ((b.channel == 0 || b.channel == ch) && b.controlNumber == cc)
                    {
                        if (b.type == ChainedMidiType::CC_Continuous)
                        {
                            if (callback) callback(b.targetId, b.slotIndex, b.paramIndex, normalized);
                        }
                        else if (b.type == ChainedMidiType::CC_Toggle)
                        {
                            if (val >= 64)
                            {
                                if (callback) callback(b.targetId, b.slotIndex, b.paramIndex, 1.0f);
                            }
                        }
                    }
                }
            }
            else if (msg.isNoteOn())
            {
                const int ch = msg.getChannel();
                const int note = msg.getNoteNumber();

                if (isLearning.load())
                {
                    std::lock_guard<std::recursive_mutex> lock(bindingMutex);
                    ChainedMidiBinding newBind;
                    newBind.type = ChainedMidiType::Note_Toggle;
                    newBind.channel = ch;
                    newBind.controlNumber = note;
                    newBind.targetId = learningTargetId;
                    newBind.targetName = learningTargetName;
                    newBind.slotIndex = learningSlotIndex;
                    newBind.paramIndex = learningParamIndex;

                    addOrReplaceBindingInternal(newBind);
                    isLearning.store(false);
                    learningTargetId.clear();
                    learningTargetName.clear();
                    continue;
                }

                std::lock_guard<std::recursive_mutex> lock(bindingMutex);
                for (const auto& b : bindings)
                {
                    if (b.type == ChainedMidiType::Note_Toggle && (b.channel == 0 || b.channel == ch) && b.controlNumber == note)
                    {
                        if (callback) callback(b.targetId, b.slotIndex, b.paramIndex, 1.0f);
                    }
                }
            }
        }
    }

    juce::ValueTree saveToValueTree() const
    {
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        juce::ValueTree tree("MidiBindings");
        for (const auto& b : bindings)
        {
            juce::ValueTree item("Binding");
            item.setProperty("type", static_cast<int>(b.type), nullptr);
            item.setProperty("channel", b.channel, nullptr);
            item.setProperty("controlNumber", b.controlNumber, nullptr);
            item.setProperty("targetId", b.targetId, nullptr);
            item.setProperty("targetName", b.targetName, nullptr);
            item.setProperty("slotIndex", b.slotIndex, nullptr);
            item.setProperty("paramIndex", b.paramIndex, nullptr);
            tree.addChild(item, -1, nullptr);
        }
        return tree;
    }

    void restoreFromValueTree(const juce::ValueTree& tree)
    {
        if (!tree.hasType("MidiBindings")) return;
        std::lock_guard<std::recursive_mutex> lock(bindingMutex);
        bindings.clear();

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto item = tree.getChild(i);
            ChainedMidiBinding b;
            b.type = static_cast<ChainedMidiType>(static_cast<int>(item.getProperty("type", 0)));
            b.channel = item.getProperty("channel", 0);
            b.controlNumber = item.getProperty("controlNumber", 11);
            b.targetId = item.getProperty("targetId", "");
            b.targetName = item.getProperty("targetName", "");
            b.slotIndex = item.getProperty("slotIndex", -1);
            b.paramIndex = item.getProperty("paramIndex", -1);
            bindings.push_back(b);
        }
    }

private:
    void addOrReplaceBindingInternal(const ChainedMidiBinding& binding)
    {
        bindings.erase(std::remove_if(bindings.begin(), bindings.end(),
            [&](const ChainedMidiBinding& b) { return b.targetId == binding.targetId; }),
            bindings.end());
        bindings.push_back(binding);
    }

    std::vector<ChainedMidiBinding> bindings;
    std::atomic<bool> isLearning{ false };
    juce::String learningTargetId;
    juce::String learningTargetName;
    int learningSlotIndex{ -1 };
    int learningParamIndex{ -1 };
    mutable std::recursive_mutex bindingMutex;
};

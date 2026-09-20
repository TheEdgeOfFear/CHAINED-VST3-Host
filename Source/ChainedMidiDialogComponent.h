#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ChainedAudioEngine.h"

class ChainedMidiDialogComponent : public juce::Component,
                                   public juce::Timer
{
public:
    ChainedMidiDialogComponent(ChainedAudioEngine& engineRef)
        : engine(engineRef)
    {
        titleLabel.setText("MIDI CONTROLLER & FOOTSWITCH MAPPINGS", juce::dontSendNotification);
        titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
        titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00e5ff));
        addAndMakeVisible(titleLabel);

        statusLabel.setText("Step on any MIDI foot controller switch or turn a knob to assign.", juce::dontSendNotification);
        statusLabel.setFont(juce::FontOptions(11.5f));
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
        addAndMakeVisible(statusLabel);

        // Auto Map Button
        addAndMakeVisible(autoMapButton);
        autoMapButton.setButtonText("AUTO-MAP SLOTS (CC 80-91)");
        autoMapButton.onClick = [this]
        {
            for (int i = 0; i < 12; ++i)
            {
                engine.getMidiManager().assignCC("slot_bypass:" + juce::String(i),
                    "Slot " + juce::String(i + 1) + " Footswitch",
                    80 + i, 0, true);
            }
            repaint();
        };

        // Clear All Button
        addAndMakeVisible(clearAllButton);
        clearAllButton.setButtonText("CLEAR ALL");
        clearAllButton.onClick = [this]
        {
            engine.getMidiManager().clearAllBindings();
            repaint();
        };

        // Setup Mapping Entries
        setupTargets();

        startTimerHz(20);
        setSize(680, 520);
    }

    ~ChainedMidiDialogComponent() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        if (engine.getMidiManager().getIsLearning())
        {
            statusLabel.setText("LEARNING: " + engine.getMidiManager().getLearningTargetName() + " (Waiting for MIDI message...)", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffbb00));
        }
        else
        {
            statusLabel.setText("Ready. Click [LEARN] on any target, then trigger your MIDI foot controller.", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8b949e));
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff14171c));

        // Header separator
        g.setColour(juce::Colour(0xff2a303a));
        g.drawHorizontalLine(55, 0, (float)getWidth());
    }

    void resized() override
    {
        titleLabel.setBounds(20, 10, 360, 22);
        statusLabel.setBounds(20, 32, 430, 18);
        autoMapButton.setBounds(getWidth() - 250, 14, 160, 26);
        clearAllButton.setBounds(getWidth() - 84, 14, 70, 26);

        // Layout rows
        const int startY = 65;
        const int rowH = 26;
        const int col1W = 310;
        const int col2X = col1W + 40;
        const int col2W = 310;

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const int col = (i < 12) ? 0 : 1;
            const int idxInCol = (i < 12) ? static_cast<int>(i) : static_cast<int>(i - 12);
            const int x = (col == 0) ? 20 : col2X;
            const int y = startY + idxInCol * (rowH + 4);

            if (rows[i])
                rows[i]->setBounds(x, y, (col == 0) ? col1W : col2W, rowH);
        }
    }

private:
    struct TargetRow : public juce::Component
    {
        juce::String targetId;
        juce::String targetName;
        ChainedAudioEngine& engine;

        juce::Label nameLabel;
        juce::Label bindLabel;
        juce::TextButton learnButton;
        juce::TextButton clearButton;

        TargetRow(const juce::String& id, const juce::String& name, ChainedAudioEngine& eng)
            : targetId(id), targetName(name), engine(eng)
        {
            nameLabel.setText(targetName, juce::dontSendNotification);
            nameLabel.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
            nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffeceff4));
            addAndMakeVisible(nameLabel);

            bindLabel.setFont(juce::FontOptions(10.5f));
            bindLabel.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(bindLabel);

            addAndMakeVisible(learnButton);
            learnButton.setButtonText("LEARN");
            learnButton.onClick = [this]
            {
                if (engine.getMidiManager().getIsLearning() && engine.getMidiManager().getLearningTargetId() == targetId)
                    engine.getMidiManager().cancelLearning();
                else
                    engine.getMidiManager().startLearning(targetId, targetName);
            };

            addAndMakeVisible(clearButton);
            clearButton.setButtonText("X");
            clearButton.onClick = [this]
            {
                engine.getMidiManager().removeBindingForTarget(targetId);
            };
        }

        void paint(juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const bool isLearning = engine.getMidiManager().getIsLearning() &&
                                    engine.getMidiManager().getLearningTargetId() == targetId;
            ChainedMidiBinding bind;
            const bool hasBind = engine.getMidiManager().getBindingForTarget(targetId, bind);

            g.setColour(isLearning ? juce::Colour(0xff3a2e05) : juce::Colour(0xff1b1f26));
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(isLearning ? juce::Colour(0xffffbb00) : juce::Colour(0xff2d333f));
            g.drawRoundedRectangle(bounds, 4.0f, isLearning ? 1.5f : 1.0f);

            if (isLearning)
            {
                bindLabel.setText("WAITING...", juce::dontSendNotification);
                bindLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffbb00));
            }
            else if (hasBind)
            {
                juce::String desc = bind.type == ChainedMidiType::Note_Toggle ? "Note " : "CC ";
                desc += juce::String(bind.controlNumber);
                if (bind.channel > 0)
                    desc += " (Ch" + juce::String(bind.channel) + ")";
                bindLabel.setText(desc, juce::dontSendNotification);
                bindLabel.setColour(juce::Label::textColourId, juce::Colour(0xff00ff66));
            }
            else
            {
                bindLabel.setText("---", juce::dontSendNotification);
                bindLabel.setColour(juce::Label::textColourId, juce::Colour(0xff555d68));
            }
        }

        void resized() override
        {
            nameLabel.setBounds(6, 3, 140, 20);
            bindLabel.setBounds(146, 3, 85, 20);
            learnButton.setBounds(235, 3, 48, 20);
            clearButton.setBounds(286, 3, 20, 20);
        }
    };

    ChainedAudioEngine& engine;
    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::TextButton autoMapButton;
    juce::TextButton clearAllButton;
    std::vector<std::unique_ptr<TargetRow>> rows;

    void setupTargets()
    {
        // Slots 1 to 12
        for (int i = 0; i < 12; ++i)
        {
            juce::String name = "Slot " + juce::String(i + 1) + " Footswitch";
            if (auto* slot = engine.getSlot(i))
            {
                if (slot->isOccupied())
                    name += " (" + slot->getPluginName() + ")";
            }
            auto row = std::make_unique<TargetRow>("slot_bypass:" + juce::String(i), name, engine);
            addAndMakeVisible(row.get());
            rows.push_back(std::move(row));
        }

        // Global / Transport controls
        struct GlobalTarget { juce::String id; juce::String name; };
        std::vector<GlobalTarget> globals = {
            { "row1_footswitch", "Row 1 Footswitch" },
            { "row2_footswitch", "Row 2 Footswitch" },
            { "mode_switch", "Unified / Duality Mode" },
            { "tube_comp_mode", "Tube Compressor Dial" },
            { "limiter_thresh", "Limiter Threshold" },
            { "noise_gate_thresh", "Noise Gate Threshold" },
            { "master_input_gain", "Master Input Gain" },
            { "master_output_gain", "Master Output Gain" },
            { "mp3_play_pause", "MP3 Play / Pause" },
            { "mp3_loop_toggle", "MP3 Loop Toggle" },
            { "mp3_prev", "MP3 Previous Track" },
            { "mp3_next", "MP3 Next Track" }
        };

        for (const auto& g : globals)
        {
            auto row = std::make_unique<TargetRow>(g.id, g.name, engine);
            addAndMakeVisible(row.get());
            rows.push_back(std::move(row));
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChainedMidiDialogComponent)
};

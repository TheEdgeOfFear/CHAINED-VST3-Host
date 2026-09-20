#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginSlot.h"
#include "ChainedAudioEngine.h"

class SlotFootswitchButton : public juce::Button
{
public:
    SlotFootswitchButton(int slotIdx, ChainedAudioEngine& eng)
        : juce::Button("SlotFootswitch"), slotIndex(slotIdx), engine(eng)
    {
        setClickingTogglesState(true);
    }

    std::function<void()> onRightClick;

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            if (onRightClick)
                onRightClick();
            return;
        }
        juce::Button::mouseDown(e);
    }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        const float corner = 6.0f;
        const bool engaged = getToggleState(); // true = engaged, false = bypassed

        const juce::String targetId = "slot_bypass:" + juce::String(slotIndex);
        const bool isLearningThis = engine.getMidiManager().getIsLearning() &&
                                    engine.getMidiManager().getLearningTargetId() == targetId;
        ChainedMidiBinding existingBind;
        const bool hasBind = engine.getMidiManager().getBindingForTarget(targetId, existingBind);

        // Footswitch background plate
        juce::Colour bgTop = isLearningThis ? juce::Colour(0xff3d2e05) : (engaged ? juce::Colour(0xff0c2c26) : juce::Colour(0xff221b18));
        juce::Colour bgBot = isLearningThis ? juce::Colour(0xff211802) : (engaged ? juce::Colour(0xff051714) : juce::Colour(0xff14100e));

        if (shouldDrawButtonAsHighlighted)
        {
            bgTop = bgTop.brighter(0.3f);
            bgBot = bgBot.brighter(0.3f);
        }
        if (shouldDrawButtonAsDown)
        {
            bgTop = bgTop.darker(0.3f);
            bgBot = bgBot.darker(0.3f);
        }

        g.setGradientFill(juce::ColourGradient(bgTop, 0, bounds.getY(), bgBot, 0, bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, corner);

        // Glowing border
        juce::Colour borderCol = isLearningThis ? juce::Colour(0xffffbb00) : (engaged ? juce::Colour(0xff00e5ff) : juce::Colour(0xffe67e22));
        g.setColour(borderCol.withAlpha(isLearningThis ? 1.0f : (engaged ? 0.95f : 0.6f)));
        g.drawRoundedRectangle(bounds, corner, isLearningThis ? 2.0f : (engaged ? 1.5f : 1.0f));

        // Circular Chrome Stomp Switch on Left
        const float stompDiam = 20.0f;
        const float stompX = bounds.getX() + 8.0f;
        const float stompY = bounds.getCentreY() - stompDiam * 0.5f;

        // Outer nut (metallic)
        g.setColour(juce::Colour(0xff555d68));
        g.fillEllipse(stompX - 2.0f, stompY - 2.0f, stompDiam + 4.0f, stompDiam + 4.0f);
        g.setColour(juce::Colour(0xff8b949e));
        g.drawEllipse(stompX - 2.0f, stompY - 2.0f, stompDiam + 4.0f, stompDiam + 4.0f, 1.0f);

        // Chrome button plunger
        juce::Colour plungerTop = shouldDrawButtonAsDown ? juce::Colour(0xff666666) : juce::Colour(0xffcccccc);
        juce::Colour plungerBot = shouldDrawButtonAsDown ? juce::Colour(0xff333333) : juce::Colour(0xff777777);
        g.setGradientFill(juce::ColourGradient(plungerTop, stompX, stompY, plungerBot, stompX, stompY + stompDiam, false));
        g.fillEllipse(stompX, stompY, stompDiam, stompDiam);

        // LED Indicator on Right
        const float ledSize = 10.0f;
        const float ledX = bounds.getRight() - 20.0f;
        const float ledY = bounds.getCentreY() - ledSize * 0.5f;

        juce::Colour ledLight = isLearningThis ? juce::Colour(0xffffbb00) : (engaged ? juce::Colour(0xff00ff66) : juce::Colour(0xffff4400));
        if (engaged || isLearningThis)
        {
            // LED Glow halo
            g.setColour(ledLight.withAlpha(0.35f));
            g.fillEllipse(ledX - 4.0f, ledY - 4.0f, ledSize + 8.0f, ledSize + 8.0f);
        }
        g.setColour(ledLight);
        g.fillEllipse(ledX, ledY, ledSize, ledSize);
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.7f));
        g.fillEllipse(ledX + 2.0f, ledY + 2.0f, 3.0f, 3.0f);

        // Footswitch text & MIDI CC indicator
        juce::String labelText;
        if (isLearningThis)
        {
            labelText = "MIDI LEARN...";
            g.setColour(juce::Colour(0xffffdd55));
        }
        else
        {
            labelText = engaged ? "ENGAGED" : "BYPASS";
            if (hasBind)
            {
                if (existingBind.type == ChainedMidiType::Note_Toggle)
                    labelText += " [N" + juce::String(existingBind.controlNumber) + "]";
                else
                    labelText += " [CC" + juce::String(existingBind.controlNumber) + "]";
            }
            g.setColour(engaged ? juce::Colour(0xffffffff) : juce::Colour(0xffdcdcdc));
        }

        g.setFont(juce::FontOptions(10.5f).withStyle("Bold"));
        g.drawText(labelText,
                   bounds.withTrimmedLeft(32.0f).withTrimmedRight(24.0f),
                   juce::Justification::centred,
                   true);
    }

private:
    int slotIndex{ 0 };
    ChainedAudioEngine& engine;
};

class PluginSlotComponent : public juce::Component
{
public:
    PluginSlotComponent(PluginSlot& slotRef, ChainedAudioEngine& engineRef)
        : slot(slotRef), engine(engineRef), footswitchButton(slotRef.getIndex(), engineRef)
    {
        addAndMakeVisible(loadButton);
        loadButton.setButtonText("+ LOAD VST3");
        loadButton.onClick = [this] { showPluginMenu(); };

        addAndMakeVisible(editButton);
        editButton.setButtonText("GUI");
        editButton.onClick = [this]
        {
            if (slot.isOccupied())
            {
                if (slot.isWindowOpen())
                    slot.closeEditorWindow();
                else
                    slot.openEditorWindow();
            }
            else
            {
                showPluginMenu();
            }
        };

        addAndMakeVisible(footswitchButton);
        footswitchButton.onClick = [this]
        {
            slot.setBypassed(!footswitchButton.getToggleState());
            updateState();
        };
        footswitchButton.onRightClick = [this]
        {
            showFootswitchMidiMenu();
        };

        addAndMakeVisible(removeButton);
        removeButton.setButtonText("X");
        removeButton.onClick = [this]
        {
            slot.unloadPlugin();
            updateState();
        };

        updateState();
    }

    void updateState()
    {
        const bool occupied = slot.isOccupied();
        const bool bypassed = slot.isBypassed();

        footswitchButton.setVisible(true); // Always visible on every slot!
        footswitchButton.setToggleState(!bypassed, juce::dontSendNotification);

        editButton.setVisible(occupied);
        removeButton.setVisible(occupied);
        loadButton.setVisible(!occupied);

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float corner = 8.0f;
        const bool occupied = slot.isOccupied();
        const bool bypassed = slot.isBypassed();

        // Dark metallic pedal slot chassis
        juce::Colour bgTop = occupied ? juce::Colour(0xff23272e) : juce::Colour(0xff181a1d);
        juce::Colour bgBot = occupied ? juce::Colour(0xff15171a) : juce::Colour(0xff0e1012);

        g.setGradientFill(juce::ColourGradient(bgTop, 0, bounds.getY(), bgBot, 0, bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, corner);

        // Border
        juce::Colour borderCol = occupied ? (bypassed ? juce::Colour(0xffe6a100) : juce::Colour(0xff00d2ff)) : juce::Colour(0xff333842);
        g.setColour(borderCol.withAlpha(occupied ? 0.7f : 0.4f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner, occupied ? 1.5f : 1.0f);

        // Header Slot Number (e.g. S1, S2...)
        g.setColour(juce::Colour(0xff8b949e));
        g.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
        g.drawText("S" + juce::String(slot.getIndex() + 1), bounds.reduced(10, 8), juce::Justification::topLeft, true);

        // Slot status LED
        juce::Colour ledCol = juce::Colour(0xff3a3f47);
        if (occupied)
            ledCol = bypassed ? juce::Colour(0xffff9900) : juce::Colour(0xff00ff66);

        g.setColour(ledCol);
        g.fillEllipse(bounds.getRight() - 16.0f, bounds.getY() + 10.0f, 8.0f, 8.0f);
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.3f));
        g.drawEllipse(bounds.getRight() - 16.0f, bounds.getY() + 10.0f, 8.0f, 8.0f, 1.0f);

        // Large Plugin Name or Slot Watermark
        if (occupied)
        {
            g.setColour(bypassed ? juce::Colour(0xff8b949e) : juce::Colour(0xffeceff4));
            g.setFont(juce::FontOptions(13.5f).withStyle("Bold"));
            auto textArea = bounds.withTrimmedTop(26).withTrimmedBottom(48).reduced(8, 0);
            g.drawFittedText(slot.getPluginName(), textArea.toNearestInt(), juce::Justification::centred, 3);
        }
        else
        {
            // Empty slot watermark
            g.setColour(juce::Colour(0xff2a303c));
            g.setFont(juce::FontOptions(22.0f).withStyle("Bold"));
            g.drawText("EMPTY", bounds.withTrimmedTop(18).withTrimmedBottom(80), juce::Justification::centred, true);
        }

        // Draw metallic 1/4" input/output jack sockets on edges
        drawJackSocket(g, bounds.getX() + 4.0f, bounds.getCentreY() + 8.0f);
        drawJackSocket(g, bounds.getRight() - 14.0f, bounds.getCentreY() + 8.0f);
    }

    void resized() override
    {
        const auto bounds = getLocalBounds();

        // Top right controls when occupied
        editButton.setBounds(bounds.getRight() - 68, 6, 36, 18);
        removeButton.setBounds(bounds.getRight() - 28, 6, 20, 18);

        // Center Load button when empty
        loadButton.setBounds(bounds.getCentreX() - 55, bounds.getCentreY() - 18, 110, 28);

        // Footswitch at bottom of every slot
        const int fH = 34;
        footswitchButton.setBounds(bounds.getX() + 8, bounds.getBottom() - fH - 8, bounds.getWidth() - 16, fH);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showContextMenu();
        }
        else if (slot.isOccupied())
        {
            if (slot.isWindowOpen())
                slot.closeEditorWindow();
            else
                slot.openEditorWindow();
        }
    }

private:
    PluginSlot& slot;
    ChainedAudioEngine& engine;

    juce::TextButton loadButton;
    juce::TextButton editButton;
    SlotFootswitchButton footswitchButton;
    juce::TextButton removeButton;

    void drawJackSocket(juce::Graphics& g, float x, float y)
    {
        g.setColour(juce::Colour(0xff181a1d));
        g.fillEllipse(x, y - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colour(0xff555d68));
        g.drawEllipse(x, y - 5.0f, 10.0f, 10.0f, 1.5f);
        g.setColour(juce::Colour(0xff000000));
        g.fillEllipse(x + 2.5f, y - 2.5f, 5.0f, 5.0f);
    }

    void showContextMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Load VST3 Plugin...");
        if (slot.isOccupied())
        {
            menu.addItem(2, "Open GUI Window");
            menu.addItem(3, slot.isBypassed() ? "Engage Slot (Unbypass)" : "Disengage Slot (Bypass)");
            menu.addItem(4, "MIDI Learn Footswitch (Engage / Disengage)");
            menu.addItem(5, "Unload Plugin");

            // Submenu for MIDI Learn parameters
            juce::PopupMenu paramMenu;
            if (auto* inst = slot.getInstance())
            {
                int pIdx = 0;
                for (auto* p : inst->getParameters())
                {
                    if (pIdx > 50) break; // cap list length
                    paramMenu.addItem(100 + pIdx, p->getName(64));
                    pIdx++;
                }
            }
            menu.addSubMenu("MIDI Learn Parameter", paramMenu);
        }

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [this](int result)
            {
                if (result == 1)
                {
                    showPluginMenu();
                }
                else if (result == 2)
                {
                    slot.openEditorWindow();
                }
                else if (result == 3)
                {
                    slot.toggleBypass();
                    updateState();
                }
                else if (result == 4)
                {
                    engine.getMidiManager().startLearning("slot_bypass:" + juce::String(slot.getIndex()),
                                                         "Slot " + juce::String(slot.getIndex() + 1) + " Footswitch");
                    updateState();
                }
                else if (result == 5)
                {
                    slot.unloadPlugin();
                    updateState();
                }
                else if (result >= 100)
                {
                    const int paramIdx = result - 100;
                    if (auto* inst = slot.getInstance())
                    {
                        const auto& params = inst->getParameters();
                        if (paramIdx >= 0 && paramIdx < params.size())
                        {
                            if (auto* p = params[paramIdx])
                            {
                                engine.getMidiManager().startLearning("vst:" + juce::String(slot.getIndex()) + ":" + juce::String(paramIdx),
                                                                     "Slot " + juce::String(slot.getIndex() + 1) + ": " + p->getName(32),
                                                                     slot.getIndex(), paramIdx);
                            }
                        }
                    }
                }
            });
    }

    void showFootswitchMidiMenu()
    {
        const juce::String targetId = "slot_bypass:" + juce::String(slot.getIndex());
        ChainedMidiBinding existingBind;
        const bool hasBind = engine.getMidiManager().getBindingForTarget(targetId, existingBind);

        juce::PopupMenu menu;
        juce::String header = "Slot " + juce::String(slot.getIndex() + 1) + " Footswitch";
        if (hasBind)
        {
            juce::String bindDesc = (existingBind.type == ChainedMidiType::Note_Toggle) ? "Note #" : "CC #";
            bindDesc += juce::String(existingBind.controlNumber);
            if (existingBind.channel > 0)
                bindDesc += " (Ch " + juce::String(existingBind.channel) + ")";
            else
                bindDesc += " (Omni)";
            menu.addItem(0, "✓ Assigned: " + bindDesc, false);
        }
        else
        {
            menu.addItem(0, "(No MIDI Assigned)", false);
        }
        menu.addSeparator();

        menu.addItem(1, "MIDI Learn (Step / Press Foot Controller)...");

        // Quick Assign CCs submenu
        juce::PopupMenu quickCcMenu;
        const std::vector<int> commonCCs = { 64, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 1, 2, 4, 11 };
        for (int cc : commonCCs)
        {
            juce::String ccName = "CC #" + juce::String(cc);
            if (cc == 64) ccName += " (Sustain / Switch)";
            else if (cc >= 80 && cc <= 91) ccName += " (Pedal " + juce::String(cc - 79) + ")";
            quickCcMenu.addItem(1000 + cc, ccName);
        }
        juce::PopupMenu allCcMenu;
        for (int cc = 0; cc <= 127; ++cc)
        {
            allCcMenu.addItem(1000 + cc, "CC #" + juce::String(cc));
        }
        quickCcMenu.addSubMenu("All MIDI CCs (0-127)", allCcMenu);
        menu.addSubMenu("Direct Assign MIDI CC", quickCcMenu);

        if (hasBind)
            menu.addItem(2, "Clear MIDI Assignment");

        menu.addSeparator();
        menu.addItem(3, slot.isBypassed() ? "Engage (Turn ON)" : "Disengage (Bypass)");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&footswitchButton),
            [this, targetId](int result)
            {
                if (result == 1)
                {
                    engine.getMidiManager().startLearning(targetId,
                        "Slot " + juce::String(slot.getIndex() + 1) + " Footswitch");
                    updateState();
                }
                else if (result == 2)
                {
                    engine.getMidiManager().removeBindingForTarget(targetId);
                    updateState();
                }
                else if (result == 3)
                {
                    slot.toggleBypass();
                    updateState();
                }
                else if (result >= 1000)
                {
                    const int cc = result - 1000;
                    engine.getMidiManager().assignCC(targetId,
                        "Slot " + juce::String(slot.getIndex() + 1) + " Footswitch",
                        cc, 0, true);
                    updateState();
                }
            });
    }

    void showPluginMenu()
    {
        auto& list = engine.getKnownPluginList();
        juce::PopupMenu menu;
        menu.addItem(1, "Scan / Refresh All VST3 Plugins...", true, false);
        menu.addItem(2, "Load .vst3 from file...", true, false);
        menu.addSeparator();

        const auto types = list.getTypes();
        if (types.isEmpty())
        {
            if (engine.isPluginScanRunning())
                menu.addItem(0, "Scanning in progress... (please wait a moment)", false);
            else
                menu.addItem(0, "No VST3 plugins found (Click 'Scan / Refresh')", false);
        }
        else
        {
            // Group by category or manufacturer
            std::map<juce::String, std::vector<juce::PluginDescription>> categorized;
            std::vector<juce::PluginDescription> sortedAll;

            for (const auto& desc : types)
            {
                juce::String cat = desc.manufacturerName.isNotEmpty() && desc.manufacturerName != "Unknown"
                    ? desc.manufacturerName
                    : (desc.category.isNotEmpty() ? desc.category : "Other");
                categorized[cat].push_back(desc);
                sortedAll.push_back(desc);
            }

            std::sort(sortedAll.begin(), sortedAll.end(), [](const juce::PluginDescription& a, const juce::PluginDescription& b) {
                return a.name.compareIgnoreCase(b.name) < 0;
            });

            int idCounter = 10;
            std::vector<juce::PluginDescription> idMap;
            idMap.resize(5000);

            // Manufacturers / Categories
            for (auto& [category, plugins] : categorized)
            {
                std::sort(plugins.begin(), plugins.end(), [](const juce::PluginDescription& a, const juce::PluginDescription& b) {
                    return a.name.compareIgnoreCase(b.name) < 0;
                });

                juce::PopupMenu sub;
                for (const auto& desc : plugins)
                {
                    if (idCounter < 4900)
                    {
                        sub.addItem(idCounter, desc.name);
                        idMap[idCounter] = desc;
                        idCounter++;
                    }
                }
                menu.addSubMenu(category, sub);
            }

            // All Plugins (A - Z) submenu
            juce::PopupMenu allMenu;
            for (const auto& desc : sortedAll)
            {
                if (idCounter < 4900)
                {
                    allMenu.addItem(idCounter, desc.name + " (" + desc.manufacturerName + ")");
                    idMap[idCounter] = desc;
                    idCounter++;
                }
            }
            menu.addSubMenu("All Plugins (A - Z)", allMenu);

            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
                [this, idMap](int result)
                {
                    if (result == 1)
                    {
                        engine.startPluginScan(true);
                    }
                    else if (result == 2)
                    {
                        loadPluginFromFileChooser();
                    }
                    else if (result >= 10 && result < static_cast<int>(idMap.size()))
                    {
                        const auto& desc = idMap[result];
                        juce::String error;
                        const double sr = engine.getSampleRate();
                        const int bs = engine.getBlockSize();
                        auto instance = engine.getFormatManager().createPluginInstance(desc, sr, bs, error);
                        if (instance != nullptr)
                        {
                            slot.loadPlugin(std::move(instance), desc, sr, bs);
                            updateState();
                        }
                        else
                        {
                            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Load Failed", error);
                        }
                    }
                });
            return;
        }

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [this](int result)
            {
                if (result == 1) engine.startPluginScan(true);
                else if (result == 2) loadPluginFromFileChooser();
            });
    }

    void loadPluginFromFileChooser()
    {
        auto fileChooser = std::make_shared<juce::FileChooser>("Select .vst3 Plugin",
            juce::File("C:\\Program Files\\Common Files\\VST3"),
            "*.vst3");

        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories,
            [this, fileChooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.exists())
                {
                    juce::VST3PluginFormat vst3Format;
                    juce::OwnedArray<juce::PluginDescription> types;
                    vst3Format.findAllTypesForFile(types, file.getFullPathName());

                    if (!types.isEmpty() && types.getFirst() != nullptr)
                    {
                        const auto& desc = *types.getFirst();
                        juce::String error;
                        const double sr = engine.getSampleRate();
                        const int bs = engine.getBlockSize();
                        auto instance = engine.getFormatManager().createPluginInstance(desc, sr, bs, error);
                        if (instance != nullptr)
                        {
                            slot.loadPlugin(std::move(instance), desc, sr, bs);
                            updateState();
                        }
                        else
                        {
                            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Load Failed", error);
                        }
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Invalid VST3", "Could not load a valid VST3 description from selected file.");
                    }
                }
            });
    }
};

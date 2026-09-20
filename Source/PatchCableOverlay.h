#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ChainedAudioEngine.h"

class PatchCableOverlay : public juce::Component
{
public:
    PatchCableOverlay(ChainedAudioEngine& engineRef)
        : engine(engineRef)
    {
        setInterceptsMouseClicks(false, false);
    }

    void setSlotBoundsProvider(std::function<juce::Rectangle<int>(int slotIndex)> provider)
    {
        slotBoundsProvider = provider;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (!slotBoundsProvider)
            return;

        const auto mode = engine.getRoutingMode();

        if (mode == RoutingMode::Unified)
        {
            // Row 1 connections
            for (int i = 0; i < 5; ++i)
                drawCable(g, i, i + 1, juce::Colour(0xffe6a100), 28.0f);

            // Bridge from Row 1 Slot 5 to Row 2 Slot 6
            drawBridgeCable(g, 5, 6, juce::Colour(0xffff5500));

            // Row 2 connections
            for (int i = 6; i < 11; ++i)
                drawCable(g, i, i + 1, juce::Colour(0xff00d2ff), 28.0f);
        }
        else // Duality Mode
        {
            // Row 1 connections (Amber / Gold theme)
            if (engine.getRow1Enabled())
            {
                for (int i = 0; i < 5; ++i)
                    drawCable(g, i, i + 1, juce::Colour(0xffe6a100), 32.0f);
            }

            // Row 2 connections (Cyan / Ice-blue theme)
            if (engine.getRow2Enabled())
            {
                for (int i = 6; i < 11; ++i)
                    drawCable(g, i, i + 1, juce::Colour(0xff00d2ff), 32.0f);
            }
        }
    }

private:
    ChainedAudioEngine& engine;
    std::function<juce::Rectangle<int>(int)> slotBoundsProvider;

    void drawCable(juce::Graphics& g, int fromSlot, int toSlot, juce::Colour cableColor, float droop)
    {
        const auto bFrom = slotBoundsProvider(fromSlot).toFloat();
        const auto bTo = slotBoundsProvider(toSlot).toFloat();

        if (bFrom.isEmpty() || bTo.isEmpty())
            return;

        // Jack socket output position on fromSlot (right side)
        const juce::Point<float> p1(bFrom.getRight() - 16.0f, bFrom.getCentreY() + 18.0f);
        // Jack socket input position on toSlot (left side)
        const juce::Point<float> p2(bTo.getX() + 16.0f, bTo.getCentreY() + 18.0f);

        // Cubic bezier droop
        const float dx = p2.x - p1.x;
        const juce::Point<float> c1(p1.x + dx * 0.35f, p1.y + droop);
        const juce::Point<float> c2(p2.x - dx * 0.35f, p2.y + droop);

        juce::Path path;
        path.startNewSubPath(p1);
        path.cubicTo(c1, c2, p2);

        // Drop shadow
        g.setColour(juce::Colour(0x80000000));
        juce::Path shadowPath = path;
        shadowPath.applyTransform(juce::AffineTransform::translation(2.0f, 4.0f));
        g.strokePath(shadowPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Braided Cable Outer Layer
        g.setColour(cableColor.darker(0.5f));
        g.strokePath(path, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Cable Core Highlight
        g.setColour(cableColor.brighter(0.2f));
        g.strokePath(path, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Nickel Plugs at endpoints
        drawJackPlug(g, p1, true);
        drawJackPlug(g, p2, false);
    }

    void drawBridgeCable(juce::Graphics& g, int fromSlot, int toSlot, juce::Colour cableColor)
    {
        const auto bFrom = slotBoundsProvider(fromSlot).toFloat();
        const auto bTo = slotBoundsProvider(toSlot).toFloat();

        if (bFrom.isEmpty() || bTo.isEmpty())
            return;

        const juce::Point<float> p1(bFrom.getRight() - 16.0f, bFrom.getCentreY() + 18.0f);
        const juce::Point<float> p2(bTo.getX() + 16.0f, bTo.getCentreY() + 18.0f);

        // Wide looping curve from top-right to bottom-left
        const juce::Point<float> c1(p1.x + 40.0f, p1.y + 60.0f);
        const juce::Point<float> c2(p2.x - 40.0f, p2.y - 60.0f);

        juce::Path path;
        path.startNewSubPath(p1);
        path.cubicTo(c1, c2, p2);

        // Shadow
        g.setColour(juce::Colour(0x80000000));
        juce::Path shadowPath = path;
        shadowPath.applyTransform(juce::AffineTransform::translation(3.0f, 5.0f));
        g.strokePath(shadowPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Outer Cable
        g.setColour(cableColor.darker(0.4f));
        g.strokePath(path, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Highlight
        g.setColour(cableColor.brighter(0.3f));
        g.strokePath(path, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        drawJackPlug(g, p1, true);
        drawJackPlug(g, p2, false);
    }

    void drawJackPlug(juce::Graphics& g, juce::Point<float> pt, bool /*isOutput*/)
    {
        // 1/4" metal plug barrel
        const float w = 10.0f;
        const float h = 14.0f;
        juce::Rectangle<float> barrel(pt.x - w * 0.5f, pt.y - h * 0.5f, w, h);

        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffdcdcdc), barrel.getX(), barrel.getY(),
                                              juce::Colour(0xff606870), barrel.getRight(), barrel.getBottom(), false));
        g.fillRoundedRectangle(barrel, 2.0f);

        g.setColour(juce::Colour(0xff181a1d));
        g.drawRoundedRectangle(barrel, 2.0f, 1.0f);

        // Strain relief spring collar
        g.setColour(juce::Colour(0xff22252a));
        g.fillEllipse(pt.x - 3.5f, pt.y - 3.5f, 7.0f, 7.0f);
    }
};

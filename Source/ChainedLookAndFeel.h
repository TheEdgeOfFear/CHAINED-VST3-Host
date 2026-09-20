#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

class ChainedLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ChainedLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff121417));
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff22262c));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff00d2ff));
        setColour(juce::TextButton::textColourOffId, juce::Colour(0xffdcdcdc));
        setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1c1f24));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a404a));
        setColour(juce::ComboBox::textColourId, juce::Colour(0xffeceff4));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff181b20));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2e3440));
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xffeceff4));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff00e5ff));
        setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffeceff4));
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& /*slider*/) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        const auto radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto center = bounds.getCentre();
        const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Outer dark metallic bezel
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3a3f47), center.x - radius, center.y - radius,
                                              juce::Colour(0xff15181c), center.x + radius, center.y + radius, false));
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        // Outer rim highlight
        g.setColour(juce::Colour(0xff4a525d).withAlpha(0.6f));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // Rotary Arc Track (background)
        const float trackRadius = radius - 4.0f;
        juce::Path trackPath;
        trackPath.addCentredArc(center.x, center.y, trackRadius, trackRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xff121417));
        g.strokePath(trackPath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Value Arc (Cyan / Ice-blue glow)
        if (sliderPosProportional > 0.001f)
        {
            juce::Path valuePath;
            valuePath.addCentredArc(center.x, center.y, trackRadius, trackRadius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(juce::Colour(0xff00d2ff));
            g.strokePath(valuePath, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Inner knob body (brushed metal cap)
        const float innerRadius = radius - 8.0f;
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2a2e36), center.x, center.y - innerRadius,
                                              juce::Colour(0xff181a1f), center.x, center.y + innerRadius, false));
        g.fillEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

        // Center notch / pointer indicator
        juce::Path p;
        const float pointerLength = innerRadius * 0.75f;
        const float pointerThickness = 3.0f;
        p.addRoundedRectangle(-pointerThickness * 0.5f, -innerRadius, pointerThickness, pointerLength, 1.5f);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(center.x, center.y));

        g.setColour(juce::Colour(0xff00e5ff));
        g.fillPath(p);

        // Center subtle cap reflection
        g.setColour(juce::Colour(0xffffffff).withAlpha(0.08f));
        g.fillEllipse(center.x - innerRadius * 0.4f, center.y - innerRadius * 0.4f, innerRadius * 0.8f, innerRadius * 0.8f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat();
        const float corner = 4.0f;

        juce::Colour baseColor = button.getToggleState() ? juce::Colour(0xff00a4cc) : backgroundColour;
        if (shouldDrawButtonAsDown)
            baseColor = baseColor.darker(0.3f);
        else if (shouldDrawButtonAsHighlighted)
            baseColor = baseColor.brighter(0.15f);

        // Metallic button body
        g.setGradientFill(juce::ColourGradient(baseColor.brighter(0.15f), 0, bounds.getY(),
                                              baseColor.darker(0.2f), 0, bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, corner);

        // Bevel border
        g.setColour(button.getToggleState() ? juce::Colour(0xff00e5ff) : juce::Colour(0xff4a525d));
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner, button.getToggleState() ? 1.5f : 1.0f);

        if (button.getToggleState())
        {
            // Subtle glow
            g.setColour(juce::Colour(0xff00e5ff).withAlpha(0.2f));
            g.fillRoundedRectangle(bounds.reduced(2.0f), corner);
        }
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float>(0, 0, (float)width, (float)height);
        const float corner = 4.0f;

        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, corner);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

        // Arrow
        juce::Path arrow;
        const float arrowX = (float)buttonX + (float)buttonW * 0.5f;
        const float arrowY = (float)buttonY + (float)buttonH * 0.5f;
        arrow.addTriangle(arrowX - 4.0f, arrowY - 2.0f,
                          arrowX + 4.0f, arrowY - 2.0f,
                          arrowX, arrowY + 3.0f);

        g.setColour(juce::Colour(0xff00d2ff));
        g.fillPath(arrow);
    }
};

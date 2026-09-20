#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BackingTrackPlayer.h"
#include <iomanip>
#include <sstream>

class WaveformLooperComponent : public juce::Component,
                                public juce::Timer
{
public:
    WaveformLooperComponent(BackingTrackPlayer& playerRef)
        : player(playerRef)
    {
        startTimerHz(30); // Smooth 30fps refresh
    }

    ~WaveformLooperComponent() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float corner = 5.0f;

        // Dark waveform display chassis
        g.setColour(juce::Colour(0xff0d0f12));
        g.fillRoundedRectangle(bounds, corner);

        g.setColour(juce::Colour(0xff2d333b));
        g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.0f);

        if (!player.isLoaded())
        {
            g.setColour(juce::Colour(0xff707880));
            g.setFont(juce::FontOptions(13.0f));
            g.drawText("NO AUDIO LOADED - CLICK 'LOAD MP3' OR 'LOAD FOLDER'", getLocalBounds(), juce::Justification::centred, true);
            return;
        }

        const auto& peaks = player.getWaveformThumbnail();
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();
        const float midY = h * 0.5f;

        // Draw center grid line
        g.setColour(juce::Colour(0xff1f242c));
        g.drawHorizontalLine(static_cast<int>(midY), 0.0f, w);

        // Draw waveform peaks
        if (!peaks.empty())
        {
            juce::Path wavePath;
            const float step = w / static_cast<float>(peaks.size());

            for (size_t i = 0; i < peaks.size(); ++i)
            {
                const float x = static_cast<float>(i) * step;
                const float peakHeight = peaks[i] * (h * 0.42f);

                g.setColour(juce::Colour(0xff00d2ff).withAlpha(0.65f));
                g.drawVerticalLine(static_cast<int>(x), midY - peakHeight, midY + peakHeight);
            }
        }

        const double duration = player.getDurationSeconds();
        if (duration <= 0.0) return;

        // Draw Loop Region Overlay
        if (player.getLoopEnabled())
        {
            const float loopStartX = static_cast<float>((player.getLoopStart() / duration) * w);
            const float loopEndX = static_cast<float>((player.getLoopEnd() / duration) * w);
            const auto loopRect = juce::Rectangle<float>(loopStartX, 0.0f, loopEndX - loopStartX, h);

            // Glowing translucent loop box
            g.setColour(juce::Colour(0xffe6a100).withAlpha(0.20f));
            g.fillRoundedRectangle(loopRect, 2.0f);

            // Loop boundaries
            g.setColour(juce::Colour(0xffe6a100));
            g.drawVerticalLine(static_cast<int>(loopStartX), 0.0f, h);
            g.drawVerticalLine(static_cast<int>(loopEndX), 0.0f, h);

            // Loop Handles (Triangles at top)
            juce::Path leftHandle, rightHandle;
            leftHandle.addTriangle(loopStartX - 6.0f, 0.0f, loopStartX + 6.0f, 0.0f, loopStartX, 8.0f);
            rightHandle.addTriangle(loopEndX - 6.0f, 0.0f, loopEndX + 6.0f, 0.0f, loopEndX, 8.0f);

            g.fillPath(leftHandle);
            g.fillPath(rightHandle);
        }

        // Draw Playhead Marker
        const double curTime = player.getCurrentPositionSeconds();
        const float playheadX = static_cast<float>((curTime / duration) * w);

        g.setColour(juce::Colour(0xffffffff));
        g.drawVerticalLine(static_cast<int>(playheadX), 0.0f, h);

        juce::Path playheadArrow;
        playheadArrow.addTriangle(playheadX - 5.0f, 0.0f, playheadX + 5.0f, 0.0f, playheadX, 7.0f);
        g.fillPath(playheadArrow);

        // Time display overlay in bottom corners
        g.setColour(juce::Colour(0xffdcdcdc));
        g.setFont(juce::FontOptions(11.0f).withStyle("Bold"));

        juce::String timeStr = formatTime(curTime) + " / " + formatTime(duration);
        if (player.getLoopEnabled())
        {
            timeStr += " [LOOP: " + formatTime(player.getLoopStart()) + " - " + formatTime(player.getLoopEnd()) + "]";
        }
        g.drawText(timeStr, getLocalBounds().reduced(6, 2), juce::Justification::bottomRight, true);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const double duration = player.getDurationSeconds();
        if (duration <= 0.0) return;

        const float w = static_cast<float>(getWidth());
        const double mouseTime = (e.x / w) * duration;
        const float loopStartX = static_cast<float>((player.getLoopStart() / duration) * w);
        const float loopEndX = static_cast<float>((player.getLoopEnd() / duration) * w);

        dragStartMouseX = e.x;
        initialLoopStart = player.getLoopStart();
        initialLoopEnd = player.getLoopEnd();

        // Check if clicking near loop start or loop end
        if (player.getLoopEnabled() && std::abs(e.x - loopStartX) < 8.0f)
        {
            dragMode = DragMode::ResizeStart;
        }
        else if (player.getLoopEnabled() && std::abs(e.x - loopEndX) < 8.0f)
        {
            dragMode = DragMode::ResizeEnd;
        }
        else if (player.getLoopEnabled() && e.x > loopStartX && e.x < loopEndX)
        {
            // Inside loop: Slide the entire loop window!
            dragMode = DragMode::SlideLoop;
        }
        else
        {
            // Click outside: seek playback
            dragMode = DragMode::Seek;
            player.seekToTime(mouseTime);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const double duration = player.getDurationSeconds();
        if (duration <= 0.0) return;

        const float w = static_cast<float>(getWidth());
        const double deltaSec = ((e.x - dragStartMouseX) / w) * duration;
        const double currentMouseTime = std::clamp((static_cast<double>(e.x) / w) * duration, 0.0, duration);

        if (dragMode == DragMode::SlideLoop)
        {
            player.setLoopRange(initialLoopStart + deltaSec, initialLoopEnd + deltaSec);
        }
        else if (dragMode == DragMode::ResizeStart)
        {
            player.setLoopRange(currentMouseTime, player.getLoopEnd());
        }
        else if (dragMode == DragMode::ResizeEnd)
        {
            player.setLoopRange(player.getLoopStart(), currentMouseTime);
        }
        else if (dragMode == DragMode::Seek)
        {
            player.seekToTime(currentMouseTime);
        }

        repaint();
    }

    void mouseUp(const juce::MouseEvent& /*e*/) override
    {
        dragMode = DragMode::None;
    }

private:
    BackingTrackPlayer& player;

    enum class DragMode
    {
        None,
        Seek,
        ResizeStart,
        ResizeEnd,
        SlideLoop
    };

    DragMode dragMode{ DragMode::None };
    int dragStartMouseX{ 0 };
    double initialLoopStart{ 0.0 };
    double initialLoopEnd{ 0.0 };

    static juce::String formatTime(double seconds)
    {
        if (seconds < 0.0) seconds = 0.0;
        const int mins = static_cast<int>(seconds) / 60;
        const int secs = static_cast<int>(seconds) % 60;
        const int ms = static_cast<int>((seconds - std::floor(seconds)) * 10);
        return juce::String::formatted("%02d:%02d.%1d", mins, secs, ms);
    }
};

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class ChainedPlayHead : public juce::AudioPlayHead
{
public:
    ChainedPlayHead() = default;

    void setBpm(double newBpm)
    {
        bpm.store(newBpm, std::memory_order_relaxed);
    }

    double getBpm() const
    {
        return bpm.load(std::memory_order_relaxed);
    }

    void setPlaying(bool playing)
    {
        isPlaying.store(playing, std::memory_order_relaxed);
    }

    bool getIsPlaying() const
    {
        return isPlaying.load(std::memory_order_relaxed);
    }

    void advanceSamples(int numSamples, double sampleRate)
    {
        samplePosition.fetch_add(numSamples, std::memory_order_relaxed);
        if (sampleRate > 0.0)
            timeSeconds.store(static_cast<double>(samplePosition.load(std::memory_order_relaxed)) / sampleRate, std::memory_order_relaxed);
    }

    void resetPosition()
    {
        samplePosition.store(0, std::memory_order_relaxed);
        timeSeconds.store(0.0, std::memory_order_relaxed);
    }

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm(bpm.load(std::memory_order_relaxed));
        info.setIsPlaying(isPlaying.load(std::memory_order_relaxed));
        info.setIsRecording(false);
        info.setIsLooping(false);
        info.setTimeInSamples(samplePosition.load(std::memory_order_relaxed));
        info.setTimeInSeconds(timeSeconds.load(std::memory_order_relaxed));
        info.setTimeSignature(juce::AudioPlayHead::TimeSignature{ 4, 4 });
        return info;
    }

private:
    std::atomic<double> bpm{ 120.0 };
    std::atomic<bool> isPlaying{ true };
    std::atomic<int64_t> samplePosition{ 0 };
    std::atomic<double> timeSeconds{ 0.0 };
};

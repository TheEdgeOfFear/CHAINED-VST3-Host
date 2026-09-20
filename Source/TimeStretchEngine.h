#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * High quality real-time pitch-preserving stereo time-stretching engine.
 * Bit-exact transparent pass-through at 1.0x speed, and smooth SOLA for other speeds.
 */
class TimeStretchEngine
{
public:
    TimeStretchEngine()
    {
        initWindow();
    }

    void reset(double sampleRate)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        initWindow();
        clear();
    }

    void setSpeed(float newSpeed)
    {
        speed = std::clamp(newSpeed, 0.25f, 2.5f);
    }

    float getSpeed() const { return speed; }

    bool isNormalSpeed() const
    {
        return std::abs(speed - 1.0f) < 0.005f;
    }

    void clear()
    {
        inFifo.clear();
        outFifo.clear();
        synthOverlap.assign(windowSize * numChannels, 0.0f);
        lastAnalysisPos = 0;
    }

    int getAvailableOutputSamples() const
    {
        return static_cast<int>(outFifo.size()) / numChannels;
    }

    void pushInput(const float* const* inputChannels, int numInputChannels, int numSamples)
    {
        if (numSamples <= 0 || inputChannels == nullptr)
            return;

        const int oldSamples = static_cast<int>(inFifo.size()) / numChannels;
        const int newTotal = oldSamples + numSamples;
        inFifo.resize(newTotal * numChannels);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* src = (ch < numInputChannels && inputChannels[ch] != nullptr) ? inputChannels[ch] : inputChannels[0];
            for (int i = 0; i < numSamples; ++i)
            {
                inFifo[(oldSamples + i) * numChannels + ch] = src[i];
            }
        }

        processStretch();
    }

    int readOutput(float* const* outputChannels, int numOutputChannels, int numSamplesRequested)
    {
        const int available = static_cast<int>(outFifo.size()) / numChannels;
        const int toRead = std::min(available, numSamplesRequested);

        if (toRead > 0)
        {
            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                float* dst = outputChannels[ch];
                const int srcCh = std::min(ch, numChannels - 1);
                for (int i = 0; i < toRead; ++i)
                {
                    dst[i] = outFifo[i * numChannels + srcCh];
                }
            }

            outFifo.erase(outFifo.begin(), outFifo.begin() + toRead * numChannels);
        }

        return toRead;
    }

private:
    static constexpr int numChannels = 2;
    static constexpr int windowSize = 2048;   // Window size in samples
    static constexpr int synthHop = 512;     // Synthesis hop (75% overlap)
    static constexpr int maxDelta = 128;     // Search delta for phase matching

    double currentSampleRate{ 44100.0 };
    float speed{ 1.0f };
    int lastAnalysisPos{ 0 };

    std::vector<float> hannWindow;
    std::vector<float> inFifo;
    std::vector<float> outFifo;
    std::vector<float> synthOverlap;

    void initWindow()
    {
        hannWindow.resize(windowSize);
        synthOverlap.assign(windowSize * numChannels, 0.0f);

        for (int i = 0; i < windowSize; ++i)
        {
            hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(windowSize)));
        }
    }

    void processStretch()
    {
        // If speed is 1.0x, copy directly without any grain/WSOLA processing
        if (isNormalSpeed())
        {
            const int inSamples = static_cast<int>(inFifo.size()) / numChannels;
            if (inSamples > 0)
            {
                const int oldOut = static_cast<int>(outFifo.size());
                outFifo.resize(oldOut + inSamples * numChannels);
                std::copy(inFifo.begin(), inFifo.end(), outFifo.begin() + oldOut);
                inFifo.clear();
            }
            return;
        }

        const int requiredSamples = windowSize + maxDelta * 2;
        const int analysisHop = std::max(1, static_cast<int>(std::round(static_cast<float>(synthHop) * speed)));

        while (static_cast<int>(inFifo.size()) / numChannels >= requiredSamples)
        {
            // Find best cross-correlation offset near analysisHop
            int bestOffset = analysisHop;
            float maxCorr = -1e9f;

            const int minSearch = std::max(0, analysisHop - maxDelta);
            const int maxSearch = std::min(static_cast<int>(inFifo.size()) / numChannels - windowSize, analysisHop + maxDelta);

            for (int pos = minSearch; pos <= maxSearch; pos += 2)
            {
                float corr = 0.0f;
                for (int i = 0; i < windowSize; i += 8)
                {
                    const int inIdx = (pos + i) * numChannels;
                    const int ovIdx = i * numChannels;
                    corr += inFifo[inIdx] * synthOverlap[ovIdx] + inFifo[inIdx + 1] * synthOverlap[ovIdx + 1];
                }

                if (corr > maxCorr)
                {
                    maxCorr = corr;
                    bestOffset = pos;
                }
            }

            // Overlap add into synthOverlap
            for (int i = 0; i < windowSize; ++i)
            {
                const float w = hannWindow[i];
                const int inIdx = (bestOffset + i) * numChannels;
                const int ovIdx = i * numChannels;

                synthOverlap[ovIdx]     += inFifo[inIdx] * w;
                synthOverlap[ovIdx + 1] += inFifo[inIdx + 1] * w;
            }

            // Output synthHop samples
            const int oldOut = static_cast<int>(outFifo.size());
            outFifo.resize(oldOut + synthHop * numChannels);
            for (int i = 0; i < synthHop * numChannels; ++i)
            {
                outFifo[oldOut + i] = synthOverlap[i];
            }

            // Shift synthOverlap left by synthHop
            const int remaining = (windowSize - synthHop) * numChannels;
            for (int i = 0; i < remaining; ++i)
            {
                synthOverlap[i] = synthOverlap[i + synthHop * numChannels];
            }
            for (int i = remaining; i < windowSize * numChannels; ++i)
            {
                synthOverlap[i] = 0.0f;
            }

            // Discard consumed input samples up to bestOffset
            const int discard = std::min(bestOffset, static_cast<int>(inFifo.size()) / numChannels);
            if (discard > 0)
            {
                inFifo.erase(inFifo.begin(), inFifo.begin() + discard * numChannels);
            }
            else
            {
                break;
            }
        }
    }
};

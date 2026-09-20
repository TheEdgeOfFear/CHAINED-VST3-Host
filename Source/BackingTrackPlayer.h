#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "TimeStretchEngine.h"
#include <vector>
#include <atomic>
#include <mutex>

class BackingTrackPlayer
{
public:
    BackingTrackPlayer()
    {
        formatManager.registerBasicFormats();
        timeStretch.setSpeed(1.0f);
    }

    ~BackingTrackPlayer() = default;

    void prepareToPlay(double sampleRate, int samplesPerBlockExpected)
    {
        currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        blockSize = samplesPerBlockExpected;
        timeStretch.reset(currentSampleRate);
    }

    void loadFile(const juce::File& file)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader != nullptr)
        {
            std::lock_guard<std::mutex> lock(audioMutex);

            trackDurationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
            trackSampleRate = reader->sampleRate;

            audioBuffer.setSize(2, static_cast<int>(reader->lengthInSamples));
            reader->read(&audioBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

            // Generate thumbnail peaks for waveform display
            generateThumbnail();

            // Estimate track BPM
            const double bpmEst = estimateBpm();
            detectedBpm.store(bpmEst);
            trackOriginalBpm.store(bpmEst);

            currentFile = file;
            playPositionFractional.store(0.0);
            timeStretch.clear();

            // Default loop to full track or 15s
            loopStartSec.store(0.0);
            loopEndSec.store(std::min(trackDurationSeconds, 15.0));

            trackLoaded.store(true);

            if (onBpmDetected)
            {
                juce::MessageManager::callAsync([this, bpmEst]()
                {
                    if (onBpmDetected) onBpmDetected(bpmEst);
                });
            }
        }
    }

    void loadFolder(const juce::File& folder)
    {
        if (!folder.isDirectory())
            return;

        playlist.clear();
        juce::Array<juce::File> files;
        folder.findChildFiles(files, juce::File::findFiles, false, "*.mp3;*.wav;*.aif;*.aiff;*.flac;*.ogg");

        for (const auto& f : files)
            playlist.push_back(f);

        if (!playlist.empty())
        {
            currentPlaylistIndex = 0;
            loadFile(playlist[0]);
        }
    }

    void nextTrack()
    {
        if (playlist.empty()) return;
        currentPlaylistIndex = (currentPlaylistIndex + 1) % static_cast<int>(playlist.size());
        const bool wasPlaying = isPlaying.load();
        loadFile(playlist[currentPlaylistIndex]);
        if (wasPlaying) play();
    }

    void previousTrack()
    {
        if (playlist.empty()) return;
        currentPlaylistIndex = (currentPlaylistIndex - 1 + static_cast<int>(playlist.size())) % static_cast<int>(playlist.size());
        const bool wasPlaying = isPlaying.load();
        loadFile(playlist[currentPlaylistIndex]);
        if (wasPlaying) play();
    }

    void play()
    {
        if (trackLoaded.load())
            isPlaying.store(true);
    }

    void pause()
    {
        isPlaying.store(false);
    }

    void stop()
    {
        isPlaying.store(false);
        playPositionFractional.store(0.0);
        timeStretch.clear();
    }

    void togglePlayPause()
    {
        if (isPlaying.load())
            pause();
        else
            play();
    }

    bool getIsPlaying() const { return isPlaying.load(); }
    bool isLoaded() const { return trackLoaded.load(); }

    void seekToTime(double seconds)
    {
        std::lock_guard<std::mutex> lock(audioMutex);
        if (trackDurationSeconds > 0.0)
        {
            const double clamped = std::clamp(seconds, 0.0, trackDurationSeconds);
            const double targetSample = clamped * trackSampleRate;
            playPositionFractional.store(targetSample);
            timeStretch.clear();
        }
    }

    void skipForward(double deltaSeconds = 5.0)
    {
        seekToTime(getCurrentPositionSeconds() + deltaSeconds);
    }

    void skipBackward(double deltaSeconds = 5.0)
    {
        seekToTime(getCurrentPositionSeconds() - deltaSeconds);
    }

    double getCurrentPositionSeconds() const
    {
        if (trackSampleRate <= 0.0) return 0.0;
        return playPositionFractional.load() / trackSampleRate;
    }

    double getDurationSeconds() const
    {
        return trackDurationSeconds;
    }

    juce::String getCurrentFileName() const
    {
        return currentFile.getFileName();
    }

    // Looping controls
    void setLoopEnabled(bool enabled) { loopEnabled.store(enabled); }
    bool getLoopEnabled() const { return loopEnabled.load(); }

    void setLoopRange(double startSec, double endSec)
    {
        if (endSec > startSec + 0.1)
        {
            loopStartSec.store(std::max(0.0, startSec));
            loopEndSec.store(std::min(trackDurationSeconds, endSec));
        }
    }

    void slideLoopRange(double deltaSeconds)
    {
        double start = loopStartSec.load();
        double end = loopEndSec.load();
        double length = end - start;

        double newStart = start + deltaSeconds;
        double newEnd = end + deltaSeconds;

        if (newStart < 0.0)
        {
            newStart = 0.0;
            newEnd = length;
        }
        else if (newEnd > trackDurationSeconds)
        {
            newEnd = trackDurationSeconds;
            newStart = std::max(0.0, newEnd - length);
        }

        loopStartSec.store(newStart);
        loopEndSec.store(newEnd);
    }

    double getLoopStart() const { return loopStartSec.load(); }
    double getLoopEnd() const { return loopEndSec.load(); }

    // Speed & BPM control (without pitch shifting)
    void setSpeed(float newSpeed)
    {
        manualSpeed.store(newSpeed);
        updateEffectiveSpeed();
    }

    float getSpeed() const { return manualSpeed.load(); }

    void setBpmLinked(bool linked)
    {
        bpmLinked.store(linked);
        updateEffectiveSpeed();
    }

    bool getBpmLinked() const { return bpmLinked.load(); }

    void setMasterBpm(double bpm)
    {
        masterBpm.store(bpm);
        if (bpmLinked.load())
            updateEffectiveSpeed();
    }

    void setTrackBpm(double bpm)
    {
        if (bpm > 10.0)
        {
            trackOriginalBpm.store(bpm);
            if (bpmLinked.load())
                updateEffectiveSpeed();
        }
    }

    double getTrackBpm() const { return trackOriginalBpm.load(); }
    double getDetectedBpm() const { return detectedBpm.load(); }

    std::function<void(double detectedBpm)> onBpmDetected;

    void setVolume(float volumeLinear) { volume.store(volumeLinear); }
    float getVolume() const { return volume.load(); }

    void setSequentialMode(bool sequential) { sequentialMode.store(sequential); }
    bool getSequentialMode() const { return sequentialMode.load(); }

    const std::vector<float>& getWaveformThumbnail() const { return thumbnailPeaks; }
    const std::vector<juce::File>& getPlaylist() const { return playlist; }
    int getCurrentPlaylistIndex() const { return currentPlaylistIndex; }

    static inline float hermiteInterpolate(float y0, float y1, float y2, float y3, float frac)
    {
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    // Audio rendering
    void processBlock(juce::AudioBuffer<float>& outputBuffer, int numSamples)
    {
        if (!trackLoaded.load() || !isPlaying.load() || audioBuffer.getNumSamples() == 0)
            return;

        std::unique_lock<std::mutex> lock(audioMutex, std::try_to_lock);
        if (!lock.owns_lock())
            return;

        const float vol = volume.load();
        const bool isLoop = loopEnabled.load();
        const double lStart = loopStartSec.load();
        const double lEnd = loopEndSec.load();
        const double loopStartSample = std::max(0.0, lStart * trackSampleRate);
        const double loopEndSample = std::min(static_cast<double>(audioBuffer.getNumSamples()), lEnd * trackSampleRate);
        const int totalTrackSamples = audioBuffer.getNumSamples();

        // 1. Pristine Hermite-interpolated playback when speed is normal (1.0x)
        if (timeStretch.isNormalSpeed())
        {
            const double resampleRatio = (trackSampleRate > 0.0 && currentSampleRate > 0.0)
                                       ? (trackSampleRate / currentSampleRate)
                                       : 1.0;

            const float* const inL = audioBuffer.getReadPointer(0);
            const float* const inR = audioBuffer.getNumChannels() > 1 ? audioBuffer.getReadPointer(1) : inL;

            double pos = playPositionFractional.load();

            for (int i = 0; i < numSamples; ++i)
            {
                if (isLoop)
                {
                    if (pos >= loopEndSample || pos < loopStartSample)
                    {
                        pos = loopStartSample;
                    }
                }
                else
                {
                    if (pos >= totalTrackSamples)
                    {
                        if (sequentialMode.load() && !playlist.empty())
                        {
                            juce::MessageManager::callAsync([this]() { nextTrack(); });
                        }
                        else
                        {
                            isPlaying.store(false);
                            pos = 0.0;
                        }
                        break;
                    }
                }

                const int idx = static_cast<int>(pos);
                const float frac = static_cast<float>(pos - idx);

                const int idx0 = std::max(0, idx - 1);
                const int idx1 = idx;
                const int idx2 = std::min(totalTrackSamples - 1, idx + 1);
                const int idx3 = std::min(totalTrackSamples - 1, idx + 2);

                const float sL = hermiteInterpolate(inL[idx0], inL[idx1], inL[idx2], inL[idx3], frac);
                const float sR = hermiteInterpolate(inR[idx0], inR[idx1], inR[idx2], inR[idx3], frac);

                outputBuffer.addSample(0, i, sL * vol);
                if (outputBuffer.getNumChannels() > 1)
                    outputBuffer.addSample(1, i, sR * vol);

                pos += resampleRatio;
            }

            playPositionFractional.store(pos);
            return;
        }

        // 2. High-quality SOLA pitch-preserving time stretch when speed != 1.0x
        int loopGuard = 0;
        while (timeStretch.getAvailableOutputSamples() < numSamples && loopGuard++ < 64)
        {
            double pos = playPositionFractional.load();

            if (isLoop)
            {
                if (pos >= loopEndSample || pos < loopStartSample)
                {
                    pos = loopStartSample;
                    playPositionFractional.store(pos);
                }
            }
            else
            {
                if (pos >= totalTrackSamples)
                {
                    if (sequentialMode.load() && !playlist.empty())
                    {
                        juce::MessageManager::callAsync([this]() { nextTrack(); });
                        break;
                    }
                    else
                    {
                        isPlaying.store(false);
                        playPositionFractional.store(0.0);
                        break;
                    }
                }
            }

            const int chunkSize = 1024;
            int samplesToRead = chunkSize;

            if (isLoop)
            {
                if (pos + samplesToRead > loopEndSample)
                    samplesToRead = static_cast<int>(loopEndSample - pos);
            }
            else
            {
                if (pos + samplesToRead > totalTrackSamples)
                    samplesToRead = static_cast<int>(totalTrackSamples - pos);
            }

            if (samplesToRead <= 0)
            {
                if (isLoop)
                {
                    pos = loopStartSample;
                    playPositionFractional.store(pos);
                    continue;
                }
                break;
            }

            const int startIdx = static_cast<int>(pos);
            const float* const channels[2] = {
                audioBuffer.getReadPointer(0, startIdx),
                audioBuffer.getReadPointer(std::min(1, audioBuffer.getNumChannels() - 1), startIdx)
            };

            timeStretch.pushInput(channels, 2, samplesToRead);
            playPositionFractional.store(pos + samplesToRead);
        }

        // Read stretched samples and add to output
        tempStretchBuffer.setSize(2, numSamples, false, false, true);
        float* const outPointers[2] = {
            tempStretchBuffer.getWritePointer(0),
            tempStretchBuffer.getWritePointer(1)
        };

        const int samplesRead = timeStretch.readOutput(outPointers, 2, numSamples);

        if (samplesRead > 0)
        {
            for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
            {
                const int srcCh = std::min(ch, 1);
                outputBuffer.addFrom(ch, 0, tempStretchBuffer, srcCh, 0, samplesRead, vol);
            }
        }
    }

private:
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> audioBuffer;
    juce::AudioBuffer<float> tempStretchBuffer;
    juce::File currentFile;

    std::vector<juce::File> playlist;
    int currentPlaylistIndex{ 0 };

    TimeStretchEngine timeStretch;
    std::mutex audioMutex;

    std::atomic<bool> trackLoaded{ false };
    std::atomic<bool> isPlaying{ false };
    std::atomic<double> playPositionFractional{ 0.0 };

    double currentSampleRate{ 44100.0 };
    double trackSampleRate{ 44100.0 };
    double trackDurationSeconds{ 0.0 };
    int blockSize{ 512 };

    std::atomic<bool> loopEnabled{ false };
    std::atomic<double> loopStartSec{ 0.0 };
    std::atomic<double> loopEndSec{ 0.0 };

    std::atomic<float> manualSpeed{ 1.0f };
    std::atomic<bool> bpmLinked{ false };
    std::atomic<double> masterBpm{ 120.0 };
    std::atomic<double> trackOriginalBpm{ 120.0 };
    std::atomic<double> detectedBpm{ 120.0 };
    std::atomic<float> volume{ 0.8f };
    std::atomic<bool> sequentialMode{ true };

    std::vector<float> thumbnailPeaks;

    double estimateBpm()
    {
        const int totalSamples = audioBuffer.getNumSamples();
        if (totalSamples < 44100 || trackSampleRate <= 0.0)
            return 120.0;

        // Analyze up to 60 seconds of audio
        const int maxSamples = std::min(totalSamples, static_cast<int>(trackSampleRate * 60.0));
        const int hopSize = std::max(1, static_cast<int>(trackSampleRate / 100.0)); // 100 Hz frame rate
        const int numFrames = maxSamples / hopSize;
        if (numFrames < 200)
            return 120.0;

        std::vector<float> energy(numFrames, 0.0f);
        const float* lData = audioBuffer.getReadPointer(0);
        const float* rData = audioBuffer.getNumChannels() > 1 ? audioBuffer.getReadPointer(1) : lData;

        for (int f = 0; f < numFrames; ++f)
        {
            const int start = f * hopSize;
            const int end = start + hopSize;
            float e = 0.0f;
            for (int s = start; s < end; ++s)
            {
                const float smp = 0.5f * (lData[s] + rData[s]);
                e += smp * smp;
            }
            energy[f] = std::sqrt(e / static_cast<float>(hopSize));
        }

        // Onset novelty curve
        std::vector<float> novelty(numFrames, 0.0f);
        for (int f = 1; f < numFrames; ++f)
        {
            const float diff = energy[f] - energy[f - 1];
            novelty[f] = diff > 0.0f ? diff : 0.0f;
        }

        // Autocorrelation over 30..150 lag (200 BPM down to 40 BPM)
        const int minLag = 30;
        const int maxLag = 150;
        int bestLag = 50;
        float maxCorr = -1.0f;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float sum = 0.0f;
            const int n = numFrames - lag;
            for (int i = 0; i < n; ++i)
            {
                sum += novelty[i] * novelty[i + lag];
            }

            const float bpmVal = 6000.0f / static_cast<float>(lag);
            float weight = 1.0f;
            if (bpmVal >= 85.0f && bpmVal <= 145.0f)
                weight = 1.25f;

            const float score = sum * weight;
            if (score > maxCorr)
            {
                maxCorr = score;
                bestLag = lag;
            }
        }

        double bpm = 6000.0 / static_cast<double>(bestLag);
        bpm = std::round(bpm * 2.0) / 2.0; // round to 0.5 BPM
        return std::clamp(bpm, 50.0, 220.0);
    }

    void updateEffectiveSpeed()
    {
        if (bpmLinked.load())
        {
            const double tBpm = trackOriginalBpm.load();
            if (tBpm > 10.0)
            {
                const float ratio = static_cast<float>(masterBpm.load() / tBpm);
                timeStretch.setSpeed(ratio);
                return;
            }
        }
        timeStretch.setSpeed(manualSpeed.load());
    }

    void generateThumbnail()
    {
        thumbnailPeaks.clear();
        const int numPoints = 600;
        thumbnailPeaks.resize(numPoints, 0.0f);

        const int totalSamples = audioBuffer.getNumSamples();
        if (totalSamples == 0) return;

        const int samplesPerPoint = std::max(1, totalSamples / numPoints);
        const float* lData = audioBuffer.getReadPointer(0);
        const float* rData = audioBuffer.getNumChannels() > 1 ? audioBuffer.getReadPointer(1) : lData;

        for (int i = 0; i < numPoints; ++i)
        {
            const int start = i * samplesPerPoint;
            const int end = std::min(totalSamples, start + samplesPerPoint);
            float maxVal = 0.0f;
            for (int s = start; s < end; ++s)
            {
                maxVal = std::max(maxVal, std::abs(lData[s]));
                maxVal = std::max(maxVal, std::abs(rData[s]));
            }
            thumbnailPeaks[i] = maxVal;
        }
    }
};

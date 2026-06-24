//
// PluginAudioSampleSink.h
//

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <nba/device/audio_sample_sink.hpp>

#include <vector>

class PluginAudioSampleSink : public nba::AudioSampleSink {
public:
    static constexpr int RING_SIZE = 4096;
    static constexpr double ROM_SAMPLE_RATE = 65536.0;

    bool IsActive() const override { return active_; }

    uint64_t getBufferReadyCount() const { return bufferReadyCount_; }

    void OnBufferReady(const s8* left, const s8* right, int count) override {
        for (int i = 0; i < count; ++i) {
            ringL_[static_cast<size_t>(writeIdx_)] = static_cast<float>(left[i]) / 128.0f;
            ringR_[static_cast<size_t>(writeIdx_)] = static_cast<float>(right[i]) / 128.0f;
            writeIdx_ = (writeIdx_ + 1) % RING_SIZE;
        }
        ++bufferReadyCount_;
    }

    void prepare(double hostSampleRate, int /*blockSize*/) {
        speedRatio_ = ROM_SAMPLE_RATE / hostSampleRate;
        interpL_.reset();
        interpR_.reset();
        writeIdx_ = 0;
        readIdx_ = 0;
        ringL_.assign(RING_SIZE, 0.0f);
        ringR_.assign(RING_SIZE, 0.0f);
        scratchL_.assign(RING_SIZE, 0.0f);
        scratchR_.assign(RING_SIZE, 0.0f);
        active_ = true;
    }

    void render(juce::AudioBuffer<float>& buffer, int numSamples) {
        const int needed = static_cast<int>(std::ceil(numSamples * speedRatio_)) + 8;
        const int available = readableSamples();
        const int toCopy = juce::jmin(needed, available);

        copyOutOfRing(scratchL_.data(), scratchR_.data(), toCopy);

        for (int i = toCopy; i < needed; ++i) {
            scratchL_[static_cast<size_t>(i)] = 0.0f;
            scratchR_[static_cast<size_t>(i)] = 0.0f;
        }

        auto* outL = buffer.getWritePointer(0);
        auto* outR = buffer.getWritePointer(1);

        const int consumedL = interpL_.process(speedRatio_, scratchL_.data(), outL, numSamples);
        interpR_.process(speedRatio_, scratchR_.data(), outR, numSamples);

        const int unused = toCopy - juce::jmin(consumedL, toCopy);
        rewindRing(unused);
    }

private:
    int readableSamples() const {
        const int diff = writeIdx_ - readIdx_;
        return diff >= 0 ? diff : diff + RING_SIZE;
    }

    void copyOutOfRing(float* dstL, float* dstR, int count) {
        for (int i = 0; i < count; ++i) {
            dstL[i] = ringL_[static_cast<size_t>(readIdx_)];
            dstR[i] = ringR_[static_cast<size_t>(readIdx_)];
            readIdx_ = (readIdx_ + 1) % RING_SIZE;
        }
    }

    void rewindRing(int count) {
        readIdx_ = (readIdx_ - count + RING_SIZE) % RING_SIZE;
    }

    bool active_ = false;
    double speedRatio_ = 1.0;

    std::vector<float> ringL_ { std::vector<float>(RING_SIZE, 0.0f) };
    std::vector<float> ringR_ { std::vector<float>(RING_SIZE, 0.0f) };
    std::vector<float> scratchL_ { std::vector<float>(RING_SIZE, 0.0f) };
    std::vector<float> scratchR_ { std::vector<float>(RING_SIZE, 0.0f) };

    int writeIdx_ = 0;
    int readIdx_ = 0;
    uint64_t bufferReadyCount_ = 0;

    juce::CatmullRomInterpolator interpL_;
    juce::CatmullRomInterpolator interpR_;
};

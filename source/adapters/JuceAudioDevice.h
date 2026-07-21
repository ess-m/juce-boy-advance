//
// JuceAudioDevice.h
//

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <nba/device/audio_device.hpp>
#include <vector>

struct JuceAudioDevice : nba::AudioDevice {
    auto GetSampleRate() -> int override { return sampleRate_; }
    auto GetBlockSize() -> int override { return blockSize_; }

    bool Open(void* userdata, Callback cb) override {
        userData_ = userdata;
        callback_ = cb;

        return true;
    }

    void SetPause(bool) override {}
    void Close() override {}

    void prepare(int sampleRate, int blockSize) {
        sampleRate_ = sampleRate;
        blockSize_ = blockSize;

        tempBuffer_.resize(static_cast<size_t>(blockSize * 2));
    }

    void render(juce::AudioBuffer<float>& buffer, int numSamples) {
        if (!callback_ || !userData_) return;


        const int count = juce::jmin(
            numSamples,
            static_cast<int>(tempBuffer_.size()) / 2,
            buffer.getNumSamples()
        );
        if (count <= 0) return;

        callback_(userData_, tempBuffer_.data(), count * 2 * static_cast<int>(sizeof(int16_t)));

        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getWritePointer(1);

        for (int i = 0; i < count; ++i) {
            left[i] = static_cast<float>(tempBuffer_[static_cast<size_t>(i * 2)]) / 32768.0f;
            right[i] = static_cast<float>(tempBuffer_[static_cast<size_t>(i * 2 + 1)]) / 32768.0f;
        }
    }

    private:
        int sampleRate_ = 44100;
        int blockSize_ = 256;

        void* userData_ = nullptr;
        Callback callback_ = nullptr;

        std::vector<int16_t> tempBuffer_;
};
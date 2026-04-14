//
// VideoService.h
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

static constexpr int SCREEN_W = 240;
static constexpr int SCREEN_H = 160;

class VideoService {
private:
    juce::Image frontBuffer_;
    juce::Image backBuffer_;
    juce::SpinLock swapLock_;

    juce::Image scaledFrame_;
    int frameScale_ = 1;

    void copyToBackBuffer(const uint32_t* buffer) {
        const juce::Image::BitmapData bits(backBuffer_, juce::Image::BitmapData::writeOnly);

        for (uint8_t y = 0; y < SCREEN_H; ++y) {
            auto* dest = reinterpret_cast<uint32_t*>(bits.getLinePointer(y));

            for (uint8_t x = 0; x < SCREEN_W; ++x) {
                dest[x] = buffer[y * SCREEN_W + x] | 0xFF000000;
            }
        }
    }

    juce::Image getFrontBuffer() {
        const juce::SpinLock::ScopedLockType lock(swapLock_);
        return frontBuffer_;
    }

    void scaleFrame(const juce::Image& inpFrame, juce::Image& outFrame, int scale) {
        const juce::Image::BitmapData inpData(inpFrame, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData outData(outFrame, juce::Image::BitmapData::writeOnly);

        for (uint8_t y = 0; y < SCREEN_H; ++y) {
            const auto* inpRow = reinterpret_cast<const uint32_t*>(inpData.getLinePointer(y));
            auto* outRow = reinterpret_cast<uint32_t*>(outData.getLinePointer(y * scale));

            for (uint8_t x = 0; x < SCREEN_W; ++x) {
                uint32_t pixel = inpRow[x];

                for (int s = 0; s < scale; ++s) {
                    outRow[x * scale + s] = pixel;
                }
            }
            
            for (int s = 1; s < scale; ++s) {
                std::memcpy(
                    outData.getLinePointer(y * scale + s),
                    outRow,
                    static_cast<size_t>(SCREEN_W * scale) * sizeof(uint32_t)
                );
            }
        }
    }

public:
    VideoService() 
    : frontBuffer_(juce::Image::ARGB, SCREEN_W, SCREEN_H, true)
    , backBuffer_(juce::Image::ARGB, SCREEN_W, SCREEN_H, true)
    , scaledFrame_(juce::Image::ARGB, SCREEN_W, SCREEN_H, true)
    {

    }
    ~VideoService() = default;

    void pushFrame(const uint32_t* buffer) {
        copyToBackBuffer(buffer);

        const juce::SpinLock::ScopedLockType lock(swapLock_);
        std::swap(frontBuffer_, backBuffer_);
    }

    juce::Image renderFrame() {
        const juce::Image frame = getFrontBuffer();
        scaleFrame(frame, scaledFrame_, frameScale_);

        return scaledFrame_;
    }

    void setScale(int scale) {
        if (scale != frameScale_) {
            scaledFrame_ = juce::Image(juce::Image::ARGB, SCREEN_W * scale, SCREEN_H * scale, false);
            frameScale_ = scale;
        }
    }
};

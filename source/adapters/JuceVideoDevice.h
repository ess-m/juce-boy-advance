//
// JuceVideoDevice.h
//

#pragma once

#include <nba/device/video_device.hpp>
#include <services/VideoService.h>

struct JuceVideoDevice : nba::VideoDevice {
    VideoService& video;

    explicit JuceVideoDevice(VideoService& v) : video(v) {}

    void Draw(uint32_t* buffer) override {
        video.pushFrame(buffer);
    }
};
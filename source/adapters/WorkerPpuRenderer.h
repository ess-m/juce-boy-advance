//
// WorkerPpuRenderer.h
//

#pragma once

#include <juce_core/juce_core.h>
#include <nba/device/frame_renderer.hpp>

#include <cstring>
#include <vector>

#include "ppu_renderer/Pipeline.h"
#include "services/VideoService.h"

class WorkerPpuRenderer : public nba::FrameRenderer, private juce::Thread {
public:
    static constexpr int SCREEN_W = 240;
    static constexpr int SCREEN_H = 160;

    explicit WorkerPpuRenderer(VideoService& videoService)
        : juce::Thread("GBA PPU Renderer")
        , videoService_(videoService)
        , framebuffer_(SCREEN_W * SCREEN_H, 0)
    {}

    ~WorkerPpuRenderer() override {
        stop();
    }

    void start() {
        if (!isThreadRunning()) startThread(juce::Thread::Priority::normal);
    }

    void stop() {
        signalThreadShouldExit();
        snapshotReady_.signal();
        stopThread(1000);
    }

    bool IsActive() const override { return true; }

    // on audio thread: copy snapshot into a free slot and publish it.
    void OnFrameSnapshot(const nba::PpuFrameSnapshot& snapshot) override {
        const int slot = getWriteSlot();
        std::memcpy(&snapshots_[slot], &snapshot, sizeof(nba::PpuFrameSnapshot));
        publishSlot(slot);
        snapshotReady_.signal();
    }

private:
    void run() override {
        while (!threadShouldExit()) {
            snapshotReady_.wait(50);

            const int slot = claimReadSlot();
            if (slot < 0) continue;

            pipeline_.RenderFrame(snapshots_[slot], framebuffer_.data());
            videoService_.pushFrame(framebuffer_.data());

            releaseReadSlot();
        }
    }

    int getWriteSlot() {
        const juce::SpinLock::ScopedLockType lock(publishLock_);
        int slot = (currentPublished_ + 1) % 3;
        if (slot == workerReading_) slot = (slot + 1) % 3;
        return slot;
    }

    void publishSlot(int slot) {
        const juce::SpinLock::ScopedLockType lock(publishLock_);
        currentPublished_ = slot;
        newSnapshotAvailable_ = true;
    }

    int claimReadSlot() {
        const juce::SpinLock::ScopedLockType lock(publishLock_);
        if (!newSnapshotAvailable_) return -1;
        workerReading_ = currentPublished_;
        newSnapshotAvailable_ = false;
        return workerReading_;
    }

    void releaseReadSlot() {
        const juce::SpinLock::ScopedLockType lock(publishLock_);
        workerReading_ = -1;
    }

    VideoService& videoService_;

    nba::PpuFrameSnapshot snapshots_[3] {};

    juce::SpinLock publishLock_;
    int currentPublished_ = -1;
    int workerReading_ = -1;
    bool newSnapshotAvailable_ = false;

    juce::WaitableEvent snapshotReady_;

    std::vector<uint32_t> framebuffer_;
    ppu_renderer::Pipeline pipeline_;
};

//
// EmulatorService.h
//

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "BinaryData.h"

#include <nba/core.hpp>
#include <nba/config.hpp>
#include <nba/save_state.hpp>
#include <nba/rom/rom.hpp>
#include <nba/rom/gpio/gpio.hpp>
#include <nba/rom/backup/flash.hpp>

#include "AudioService.h"
#include "InputService.h"
#include "VideoService.h"
#include "adapters/JuceAudioDevice.h"
#include "adapters/JuceVideoDevice.h"
#include "adapters/WorkerPpuRenderer.h"

#include <vector>

struct SyncEvent {
    int sampleOffset;
    uint8_t value;
};

class EmulatorService {
private:    
    AudioService audio_;
    VideoService video_;
    InputService input_;

    std::shared_ptr<JuceVideoDevice> videoDevice_;
    std::shared_ptr<JuceAudioDevice> audioDevice_;
    std::shared_ptr<WorkerPpuRenderer> ppuRenderer_;
    std::shared_ptr<nba::Config> config_;
    std::unique_ptr<nba::CoreBase> core_;

    std::vector<uint8_t> biosData_;

    juce::TemporaryFile flashFile_ { ".flash" };
    nba::FLASH* flashBackup_ = nullptr;

    double lastSampleRate_ = 0.0;
    int lastBlockSize_ = 0;

    void initCore() {
        core_ = nba::CreateCore(config_);
        core_->Attach(biosData_);

        const auto* romAddr = reinterpret_cast<const uint8_t*>(BinaryData::fms_gba);
        std::vector<uint8_t> romData(romAddr, romAddr + BinaryData::fms_gbaSize);

        auto flash = std::make_unique<nba::FLASH>(
            flashFile_.getFile().getFullPathName().toStdString(),
            nba::FLASH::SIZE_128K
        );
        flashBackup_ = flash.get();

        nba::ROM rom(std::move(romData), std::move(flash), std::make_unique<nba::GPIO>());
        core_->Attach(std::move(rom));
    }

public:
    EmulatorService()
    : videoDevice_(std::make_shared<JuceVideoDevice>(video_))
    , audioDevice_(std::make_shared<JuceAudioDevice>())
    , ppuRenderer_(std::make_shared<WorkerPpuRenderer>(video_))
    , config_(std::make_shared<nba::Config>())
    {
        const auto* biosAddr = reinterpret_cast<const uint8_t*>(BinaryData::gba_bios_bin);
        biosData_.assign(biosAddr, biosAddr + BinaryData::gba_bios_binSize);

        config_->video_dev = videoDevice_;
        config_->audio_dev = audioDevice_;
        config_->frame_renderer = ppuRenderer_;
        config_->skip_bios = true;
        config_->audio.interpolation = nba::Config::Audio::Interpolation::Sinc_128;
    }

    ~EmulatorService() {
        ppuRenderer_->stop();
    }

    void prepare(double sampleRate, int blockSize) {
        audio_.prepare(sampleRate, blockSize);
        audioDevice_->prepare(static_cast<int>(sampleRate), blockSize);

        if (!core_) {
            initCore();
        } else if (!juce::approximatelyEqual(sampleRate, lastSampleRate_) || blockSize != lastBlockSize_) {
            nba::SaveState state;
            core_->CopyState(state);
            state.apu.resolution_old = 0xFF;
            core_->LoadState(state);
        }

        lastSampleRate_ = sampleRate;
        lastBlockSize_ = blockSize;

        ppuRenderer_->start();
    }

    void getState(juce::MemoryBlock& destData) {
        flashFile_.getFile().loadFileAsData(destData);
    }

    void setState(const void* data, int sizeInBytes) {
        flashFile_.getFile().replaceWithData(data, static_cast<size_t>(sizeInBytes));
        if (flashBackup_) flashBackup_->Reset();
    }

    int calculateLatencySamples(double hostSampleRate) const {
        constexpr double romSampleRate     = 32768.0;
        constexpr int    romPongSamples    = 256 * 2;
        constexpr double apuMixerRate      = 65536.0;
        constexpr int    sincCenterTaps    = 64;

        const double seconds = romPongSamples / romSampleRate
                             + sincCenterTaps / apuMixerRate;

        return static_cast<int>(std::round(seconds * hostSampleRate));
    }

    void render(juce::AudioBuffer<float>& buffer, int numSamples,
                const std::vector<SyncEvent>& events) {
        if (!core_) return;

        input_.syncToCore(*core_);

        int rendered = 0;

        for (const auto& ev : events) {
            if (ev.sampleOffset > rendered) {
                core_->Run(audio_.calcCycles(ev.sampleOffset - rendered));
                rendered = ev.sampleOffset;
            }
            core_->SendSerial8(ev.value);
        }

        if (rendered < numSamples) {
            core_->Run(audio_.calcCycles(numSamples - rendered));
        }
        audioDevice_->render(buffer, numSamples);
    }

    InputService& getInput() { return input_; }
    VideoService& getVideo() { return video_; }
};
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

class EmulatorService {
private:    
    AudioService audio_;
    VideoService video_;
    InputService input_;

    std::shared_ptr<JuceVideoDevice> videoDevice_;
    std::shared_ptr<JuceAudioDevice> audioDevice_;
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
    , config_(std::make_shared<nba::Config>())
    {
        const auto* biosAddr = reinterpret_cast<const uint8_t*>(BinaryData::gba_bios_bin);
        biosData_.assign(biosAddr, biosAddr + BinaryData::gba_bios_binSize);

        config_->video_dev = videoDevice_;
        config_->audio_dev = audioDevice_;
        config_->skip_bios = true;
        config_->audio.interpolation = nba::Config::Audio::Interpolation::Sinc_128;
    }
    ~EmulatorService() = default;

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
    }

    void getState(juce::MemoryBlock& destData) {
        flashFile_.getFile().loadFileAsData(destData);
    }

    void setState(const void* data, int sizeInBytes) {
        flashFile_.getFile().replaceWithData(data, static_cast<size_t>(sizeInBytes));
        if (flashBackup_) flashBackup_->Reset();
    }

    void render(juce::AudioBuffer<float>& buffer, int numSamples) {
        if (!core_) return;

        input_.syncToCore(*core_);

        const int cycles = audio_.calcCycles(numSamples);
        core_->Run(cycles);

        audioDevice_->render(buffer, numSamples);
    }

    InputService& getInput() { return input_; }
    VideoService& getVideo() { return video_; }
};
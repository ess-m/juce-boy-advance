//
// EmulatorService.h
//

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "BinaryData.h"

#include <nba/core.hpp>
#include <nba/config.hpp>
#include <nba/rom/rom.hpp>
#include <nba/rom/gpio/gpio.hpp>
#include <nba/rom/backup/sram.hpp>
#include <nba/rom/backup/flash.hpp>
#include <nba/rom/backup/eeprom.hpp>

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

    bool running_ = false;
    std::vector<uint8_t> biosData_;

    static bool romContains(const std::vector<uint8_t>& rom, const char* id) {
        auto len = std::strlen(id);
        return std::search(rom.begin(), rom.end(), id, id + len) != rom.end();
    }

    std::unique_ptr<nba::Backup> createBackup(const std::vector<uint8_t>& rom, const std::string& savePath) {
        if (romContains(rom, "SRAM_V"))
            return std::make_unique<nba::SRAM>(savePath);
        if (romContains(rom, "FLASH1M_V"))
            return std::make_unique<nba::FLASH>(savePath, nba::FLASH::SIZE_128K);
        if (romContains(rom, "FLASH512_V") || romContains(rom, "FLASH_V"))
            return std::make_unique<nba::FLASH>(savePath, nba::FLASH::SIZE_64K);
        if (romContains(rom, "EEPROM_V"))
            return std::make_unique<nba::EEPROM>(savePath, nba::EEPROM::DETECT, core_->GetScheduler());
        return nullptr;
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

        // Re-reset core so APU resampler picks up new sample rate
        if (core_) {
            core_->Reset();
            if (!biosData_.empty())
                core_->Attach(biosData_);
        }
    }

    bool loadROM(const juce::File& romFile) {
        juce::MemoryBlock data;
        if (!romFile.loadFileAsData(data)) return false;

        const auto* bytes = static_cast<const uint8_t*>(data.getData());
        std::vector<uint8_t> romData(bytes, bytes + data.getSize());

        DBG("loadROM: audioDevice sampleRate=" << audioDevice_->GetSampleRate() << " blockSize=" << audioDevice_->GetBlockSize());
        core_ = nba::CreateCore(config_);

        core_->Attach(biosData_);

        auto savePath = romFile.withFileExtension(".sav").getFullPathName().toStdString();
        auto backup = createBackup(romData, savePath);
        auto gpio = std::make_unique<nba::GPIO>();
        nba::ROM rom(std::move(romData), std::move(backup), std::move(gpio));
        core_->Attach(std::move(rom));

        running_ = true;
        return true;
    }

    void render(juce::AudioBuffer<float>& buffer, int numSamples) {
        if (!running_ || !core_) return;

        input_.syncToCore(*core_);

        const int cycles = audio_.calcCycles(numSamples);
        core_->Run(cycles);

        audioDevice_->render(buffer, numSamples);
    }

    InputService& getInput() { return input_; }
    VideoService& getVideo() { return video_; }
    bool isRunning() const { return running_; }
};
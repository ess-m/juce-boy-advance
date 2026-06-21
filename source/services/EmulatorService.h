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
#include "ThemeColors.h"
#include "VideoService.h"
#include "adapters/JuceAudioDevice.h"
#include "adapters/JuceVideoDevice.h"
#include "adapters/PluginAudioSampleSink.h"
#include "adapters/WorkerPpuRenderer.h"

#include <optional>
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
    std::shared_ptr<PluginAudioSampleSink> audioSampleSink_;
    std::shared_ptr<WorkerPpuRenderer> ppuRenderer_;
    std::shared_ptr<nba::Config> config_;
    std::unique_ptr<nba::CoreBase> core_;

    juce::AudioBuffer<float> apuTempBuffer_;
    juce::AudioBuffer<float> noiseDelay_;
    int noiseDelayWrite_ = 0;
    int noiseDelaySamples_ = 0;

    std::vector<uint8_t> biosData_;

    juce::TemporaryFile flashFile_ { ".flash" };
    nba::FLASH* flashBackup_ = nullptr;

    std::optional<nba::SaveState> pendingSaveState_;

    double lastSampleRate_ = 0.0;
    int lastBlockSize_ = 0;

    void initCore() {
        core_ = nba::CreateCore(config_);
        core_->Attach(biosData_);

        const auto* romAddr = reinterpret_cast<const uint8_t*>(BinaryData::fmsplugin_gba);
        std::vector<uint8_t> romData(romAddr, romAddr + BinaryData::fmsplugin_gbaSize);

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
    , audioSampleSink_(std::make_shared<PluginAudioSampleSink>())
    , ppuRenderer_(std::make_shared<WorkerPpuRenderer>(video_))
    , config_(std::make_shared<nba::Config>())
    {
        const auto* biosAddr = reinterpret_cast<const uint8_t*>(BinaryData::gba_bios_bin);
        biosData_.assign(biosAddr, biosAddr + BinaryData::gba_bios_binSize);

        config_->video_dev = videoDevice_;
        config_->audio_dev = audioDevice_;
        config_->audio_sample_sink = audioSampleSink_;
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
        audioSampleSink_->prepare(sampleRate, blockSize);

        apuTempBuffer_.setSize(2, blockSize);
        setNoiseDelay(sampleRate, blockSize);

        if (!core_) {
            initCore();
        }

        if (pendingSaveState_) {
            core_->LoadState(*pendingSaveState_);
            pendingSaveState_.reset();
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

    void resetCore() {
        pendingSaveState_.reset();
        initCore();
    }

    void importFlash(const juce::File& file) {
        juce::MemoryBlock bytes;
        if (!file.loadFileAsData(bytes)) return;
        flashFile_.getFile().replaceWithData(bytes.getData(), bytes.getSize());
        if (flashBackup_) flashBackup_->Reset();
    }

    void clearFlash() {
        constexpr size_t kFlashSize = 131072;  // FLASH_128K
        juce::MemoryBlock empty(kFlashSize);
        empty.fillWith(0xFF);  // erased FLASH default state
        flashFile_.getFile().replaceWithData(empty.getData(), empty.getSize());
        if (flashBackup_) flashBackup_->Reset();
    }

    bool exportFlash(const juce::File& file) {
        juce::MemoryBlock bytes;
        if (!flashFile_.getFile().loadFileAsData(bytes)) return false;
        return file.replaceWithData(bytes.getData(), bytes.getSize());
    }

    void getState(juce::MemoryBlock& destData) {
        juce::MemoryBlock flashBytes;
        flashFile_.getFile().loadFileAsData(flashBytes);

        const uint32_t flashSize = static_cast<uint32_t>(flashBytes.getSize());
        const uint32_t stateSize = core_ ? static_cast<uint32_t>(sizeof(nba::SaveState)) : 0u;

        destData.reset();
        destData.append(&flashSize, sizeof(flashSize));
        destData.append(&stateSize, sizeof(stateSize));
        destData.append(flashBytes.getData(), flashBytes.getSize());

        if (stateSize > 0) {
            nba::SaveState state;
            core_->CopyState(state);
            destData.append(&state, sizeof(state));
        }
    }

    void setState(const void* data, int sizeInBytes) {
        if (sizeInBytes < 8) return;

        const auto* bytes = static_cast<const uint8_t*>(data);
        uint32_t flashSize = 0;
        uint32_t stateSize = 0;
        std::memcpy(&flashSize, bytes + 0, sizeof(flashSize));
        std::memcpy(&stateSize, bytes + 4, sizeof(stateSize));

        const size_t total = 8 + flashSize + stateSize;
        if (total > static_cast<size_t>(sizeInBytes)) return;

        flashFile_.getFile().replaceWithData(bytes + 8, flashSize);
        if (flashBackup_) flashBackup_->Reset();

        if (stateSize == sizeof(nba::SaveState)) {
            nba::SaveState state;
            std::memcpy(&state, bytes + 8 + flashSize, sizeof(state));
            if (state.magic == nba::SaveState::kMagicNumber) {
                if (core_) {
                    core_->LoadState(state);
                } else {
                    pendingSaveState_ = state;
                }
            }
        }
    }

    ThemeColors getThemeColors() const {
        if (!core_) return {};
        const auto* pram = core_->GetPRAM();
        if (!pram) return {};

        const auto readColor = [pram](int offset) {
            const auto bgr15 = static_cast<uint16_t>(pram[offset] | (pram[offset + 1] << 8));
            const auto expand = [](int v) { return juce::uint8((v << 3) | (v >> 2)); };
            return juce::Colour(
                expand( bgr15 & 0x1F),
                expand((bgr15 >> 5) & 0x1F),
                expand((bgr15 >> 10) & 0x1F)
            );
        };

        return {
            .bg = readColor(0),
            .lo = readColor(2),
            .hi = readColor(34),
            .accent = readColor(6),
        };
    }

    int calculateLatencySamples(double hostSampleRate) const {
        constexpr double romSampleRate = 65536.0;
        constexpr int bufferFillSamples = 512;
        constexpr int ringDwellSamples = 256;
        constexpr int interpInputSamples = 2;
        constexpr int renderExecSamples = 256;

        const double seconds =
            (bufferFillSamples + ringDwellSamples + interpInputSamples + renderExecSamples) / romSampleRate;

        return static_cast<int>(std::round(seconds * hostSampleRate));
    }

    void render(juce::AudioBuffer<float>& buffer, int numSamples,
                const std::vector<SyncEvent>& events) {
        if (!core_) return;

        input_.syncToCore(*core_);

        // endSerial8 has no busy check, back-to-back calls choke siodata8 before the IRQ read
        // 2048c covers transfer (512c) + IRQ entry/handler
        constexpr int MIN_SERIAL_GAP = 2048;

        const int totalCyclesForBlock = audio_.calcCycles(numSamples);
        const int cyclesPerSampleNum = totalCyclesForBlock;
        const int cyclesPerSampleDen = (numSamples > 0) ? numSamples : 1;

        int cyclesRun = 0;
        bool prevWasSerial = false;

        for (const auto& ev : events) {
            int targetCycles = (ev.sampleOffset * cyclesPerSampleNum) / cyclesPerSampleDen;
            if (targetCycles > totalCyclesForBlock) targetCycles = totalCyclesForBlock;

            int runCycles = targetCycles - cyclesRun;

            if (prevWasSerial && runCycles < MIN_SERIAL_GAP) {
                runCycles = MIN_SERIAL_GAP;
            }

            if (runCycles > 0) {
                core_->Run(runCycles);
                cyclesRun += runCycles;
            }
            core_->SendSerial8(ev.value);
            prevWasSerial = true;
        }

        if (cyclesRun < totalCyclesForBlock) {
            core_->Run(totalCyclesForBlock - cyclesRun);
        }
        audioSampleSink_->render(buffer, numSamples);

        audioDevice_->render(apuTempBuffer_, numSamples);
        mixDelayedNoise(buffer, apuTempBuffer_, numSamples);
    }

    void setNoiseDelay(double sampleRate, int blockSize) {
        noiseDelaySamples_ = calculateLatencySamples(sampleRate);
        noiseDelay_.setSize(2, noiseDelaySamples_ + blockSize);
        noiseDelay_.clear();
        noiseDelayWrite_ = 0;
    }

    void mixDelayedNoise(juce::AudioBuffer<float>& out,
                         const juce::AudioBuffer<float>& src,
                         int numSamples) {
        const int ringLen = noiseDelay_.getNumSamples();

        if (ringLen == 0 || noiseDelaySamples_ == 0) {
            out.addFrom(0, 0, src, 0, 0, numSamples);
            out.addFrom(1, 0, src, 1, 0, numSamples);
            return;
        }

        const float* srcL = src.getReadPointer(0);
        const float* srcR = src.getReadPointer(1);
        float* dstL = out.getWritePointer(0);
        float* dstR = out.getWritePointer(1);
        float* ringL = noiseDelay_.getWritePointer(0);
        float* ringR = noiseDelay_.getWritePointer(1);

        int w = noiseDelayWrite_;
        int r = w - noiseDelaySamples_;
        if (r < 0) r += ringLen;

        for (int i = 0; i < numSamples; ++i) {
            ringL[w] = srcL[i];
            ringR[w] = srcR[i];

            dstL[i] += ringL[r];
            dstR[i] += ringR[r];

            if (++w == ringLen) w = 0;
            if (++r == ringLen) r = 0;
        }

        noiseDelayWrite_ = w;
    }

    InputService& getInput() { return input_; }
    VideoService& getVideo() { return video_; }

    void setPluginAutomation(uint8_t bank, const uint8_t* slots5, uint8_t resetMask, const uint8_t* levels5) {
        if (!core_) return;
        core_->SetPluginAutomation(bank, slots5, resetMask, levels5);
        core_->SetNoiseLevel(levels5[4]);
    }
};
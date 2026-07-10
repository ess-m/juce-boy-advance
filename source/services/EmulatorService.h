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

    uint64_t lastBufferReadyCount_ = 0;
    int stalledBlockCount_ = 0;
    bool watchdogArmed_ = false;
    static constexpr int kWatchdogStallLimitBlocks = 16;

    bool automationSeeded_ = false;
    bool reapplyOnSeed_ = false;

    uint8_t shadow_[11] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    uint8_t lastBank_ = 0;
    uint8_t lastSlots_[5] {};
    uint8_t lastLevels_[5] {};
    uint8_t lastMask_ = 0;

    void resetAutomationSeed(bool reapply = false) {
        automationSeeded_ = false;
        reapplyOnSeed_ = reapply;
        std::memset(shadow_, 0xFF, sizeof(shadow_));
    }

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

        resetAutomationSeed();
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
            resetAutomationSeed();
        } else if (!juce::approximatelyEqual(sampleRate, lastSampleRate_) || blockSize != lastBlockSize_) {
            nba::SaveState state;
            core_->CopyState(state);
            state.apu.resolution_old = 0xFF;
            core_->LoadState(state);
            resetAutomationSeed();
        }

        lastSampleRate_ = sampleRate;
        lastBlockSize_ = blockSize;

        ppuRenderer_->start();
    }

    void resetCore(bool reapplyAutomation = false) {
        pendingSaveState_.reset();
        resetWatchdog();
        initCore();
        reapplyOnSeed_ = reapplyAutomation;
    }

    void resetWatchdog() {
        lastBufferReadyCount_ = audioSampleSink_ ? audioSampleSink_->getBufferReadyCount() : 0;
        stalledBlockCount_ = 0;
        watchdogArmed_ = false;
    }

    static uint32_t computeRomFingerprint() {
        static const uint32_t cached = []{
            const auto* bytes = reinterpret_cast<const uint8_t*>(BinaryData::fmsplugin_gba);
            const auto size = BinaryData::fmsplugin_gbaSize;

            uint32_t h = 0x811C9DC5u;
            
            for (int i = 0; i < size; ++i) {
                h = (h ^ bytes[i]) * 0x01000193u;
            }
            return h;
        }();
        return cached;
    }

    static bool isSaveStateValid(const nba::SaveState& s) {
        if (s.magic != nba::SaveState::kMagicNumber) return false;
        if (s.version != nba::SaveState::kCurrentVersion) return false;

        const uint32_t pc = s.arm.regs.gpr[15];
        const uint32_t page = pc >> 24;

        const bool pcMapped =
            page == 0x00 || page == 0x02 || page == 0x03 || (page >= 0x08 && page <= 0x0D);
            
        if (!pcMapped) return false;

        const uint32_t mode = s.arm.regs.cpsr & 0x1Fu;

        const bool modeOk =
            mode == 0x10 || mode == 0x11 || mode == 0x12 ||
            mode == 0x13 || mode == 0x17 || mode == 0x1B || mode == 0x1F;

        if (!modeOk) return false;
        return true;
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
        const uint32_t romFingerprint = computeRomFingerprint();

        destData.reset();
        destData.append(&flashSize, sizeof(flashSize));
        destData.append(&stateSize, sizeof(stateSize));
        destData.append(&romFingerprint, sizeof(romFingerprint));
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

        int headerSize = 0;
        uint32_t romFingerprint = 0;
        const size_t v2Total = 12 + flashSize + stateSize;
        const size_t v1Total = 8 + flashSize + stateSize;

        if (static_cast<size_t>(sizeInBytes) == v2Total && sizeInBytes >= 12) {
            headerSize = 12;
            std::memcpy(&romFingerprint, bytes + 8, sizeof(romFingerprint));
        } else if (static_cast<size_t>(sizeInBytes) == v1Total) {
            headerSize = 8;
        } else {
            return;
        }

        flashFile_.getFile().replaceWithData(bytes + headerSize, flashSize);
        if (flashBackup_) flashBackup_->Reset();

        if (headerSize == 8 || romFingerprint != computeRomFingerprint()) {
            resetCore();
            return;
        }

        if (stateSize == sizeof(nba::SaveState)) {
            nba::SaveState state;
            std::memcpy(&state, bytes + headerSize + flashSize, sizeof(state));
            if (isSaveStateValid(state)) {
                if (core_) {
                    core_->LoadState(state);
                    resetWatchdog();
                    resetAutomationSeed();
                } else {
                    pendingSaveState_ = state;
                    resetAutomationSeed();
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

        const uint64_t sinkCountBefore = audioSampleSink_->getBufferReadyCount();

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

        const uint64_t sinkCountAfter = audioSampleSink_->getBufferReadyCount();
        const bool produced = sinkCountAfter != sinkCountBefore;

        if (!watchdogArmed_) {
            if (produced) watchdogArmed_ = true;
        } else if (produced) {
            stalledBlockCount_ = 0;
        } else if (++stalledBlockCount_ > kWatchdogStallLimitBlocks) {
            resetCore(true);
        }

        lastBufferReadyCount_ = sinkCountAfter;
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

        if (!automationSeeded_) {
            lastBank_ = bank;
            std::memcpy(lastSlots_, slots5, 5);
            std::memcpy(lastLevels_, levels5, 5);
            lastMask_ = resetMask;

            core_->SetNoiseLevel(levels5[4]);

            if (reapplyOnSeed_) {
                shadow_[0] = bank;
                std::memcpy(&shadow_[1], slots5, 5);
                shadow_[6] = resetMask;
                std::memcpy(&shadow_[7], levels5, 4);
                core_->SetPluginAutomation(shadow_[0], &shadow_[1], shadow_[6], &shadow_[7]);
            }

            reapplyOnSeed_ = false;
            automationSeeded_ = true;
            return;
        }

        bool dirty = false;

        if (bank != lastBank_) {
            lastBank_ = bank;
            shadow_[0] = bank;

            for (int t = 0; t < 5; ++t) {
                lastSlots_[t] = slots5[t];
                shadow_[t + 1] = slots5[t];
            }
            dirty = true;
        }

        for (int t = 0; t < 5; ++t) {
            if (slots5[t] != lastSlots_[t]) {
                lastSlots_[t] = slots5[t];
                shadow_[t + 1] = slots5[t];
                dirty = true;
            }
        }

        if (resetMask != lastMask_) {
            lastMask_ = resetMask;
            dirty = true;
        }

        for (int t = 0; t < 4; ++t) {
            if (levels5[t] != lastLevels_[t]) {
                lastLevels_[t] = levels5[t];
                shadow_[t + 7] = levels5[t];
                dirty = true;
            }
        }

        if (levels5[4] != lastLevels_[4]) {
            lastLevels_[4] = levels5[4];
            core_->SetNoiseLevel(levels5[4]);
        }

        if (dirty) {
            shadow_[6] = resetMask;
            core_->SetPluginAutomation(shadow_[0], &shadow_[1], shadow_[6], &shadow_[7]);
        }
    }
};
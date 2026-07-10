//
// PluginProcessor.cpp
//

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstring>

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Main", juce::AudioChannelSet::stereo()))
{
    syncEvents_.reserve(64); // max ticks per block

    static const char* trackNames[NUM_TRACKS] = {
        "FM 1", "FM 2", "FM 3", "FM 4", "Noise"
    };

    for (int t = 0; t < NUM_TRACKS; ++t) {
        patternParams_[t] = new juce::AudioParameterInt(
            juce::ParameterID { "pattern_" + juce::String(t), 1 },
            juce::String("Pattern ") + trackNames[t],
            0, 15, 0
        );
        addParameter(patternParams_[t]);
    }

    for (int t = 0; t < NUM_TRACKS; ++t) {
        resetParams_[t] = new juce::AudioParameterBool(
            juce::ParameterID { "reset_" + juce::String(t), 1 },
            juce::String("Reset ") + trackNames[t],
            false
        );
        addParameter(resetParams_[t]);
    }

    for (int t = 0; t < NUM_TRACKS; ++t) {
        levelParams_[t] = new juce::AudioParameterInt(
            juce::ParameterID { "level_" + juce::String(t), 1 },
            juce::String("Level ") + trackNames[t],
            0, 127, 127
        );
        addParameter(levelParams_[t]);
    }

    bankParam_ = new juce::AudioParameterInt(
        juce::ParameterID { "bank", 1 },
        "Bank",
        0, 7, 0
    );
    addParameter(bankParam_);

    emulator_.setHostIsPlugin(wrapperType != wrapperType_Standalone);
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    emulator_.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(emulator_.calculateLatencySamples(sampleRate));
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    buildSyncEvents(numSamples);

    const uint8_t bank = static_cast<uint8_t>(bankParam_->get());

    uint8_t slots[NUM_TRACKS];
    uint8_t levels[NUM_TRACKS];
    uint8_t resetMask = 0;
    
    for (int t = 0; t < NUM_TRACKS; ++t) {
        slots[t] = static_cast<uint8_t>(patternParams_[t]->get());
        levels[t] = static_cast<uint8_t>(levelParams_[t]->get());
        if (resetParams_[t]->get()) resetMask |= (uint8_t)(1u << t);
    }
    emulator_.setPluginAutomation(bank, slots, resetMask, levels);

    emulator_.render(buffer, numSamples, syncEvents_);
}

void PluginProcessor::buildSyncEvents(int numSamples) {
    syncEvents_.clear();

    auto* playHead = getPlayHead();
    if (playHead == nullptr) return;

    const auto pos = playHead->getPosition();
    if (!pos.hasValue()) return;

    const bool isPlaying = pos->getIsPlaying();
 
    if (isPlaying && !lastPlaying_) {
        syncEvents_.push_back({ 0, 0x02 }); // SYNC_MSG_START
    } else if (!isPlaying && lastPlaying_) {
        syncEvents_.push_back({ 0, 0x03 }); // SYNC_MSG_STOP
    }

    const auto ppqOpt = pos->getPpqPosition();

    if (isPlaying && ppqValid_ && ppqOpt.hasValue()) {
        const double currentPpq = *ppqOpt;
        const double delta = currentPpq - lastPpq_;

        if (delta > 0.0 && delta < 1.0) {
            const auto lastTick = static_cast<int64_t>(std::floor(lastPpq_ * 24.0));
            const auto currentTick = static_cast<int64_t>(std::floor(currentPpq * 24.0));

            for (int64_t i = lastTick; i < currentTick; ++i) {
                const double tickPpq = static_cast<double>(i + 1) / 24.0;
                const double t = (tickPpq - lastPpq_) / delta;

                const int sample = 
                    juce::jlimit(0, numSamples - 1, static_cast<int>(t * numSamples));

                syncEvents_.push_back({ sample, 0x01 }); // SYNC_MSG_TICK
            }
        }
    }

    if (ppqOpt.hasValue()) {
        lastPpq_ = *ppqOpt;
        ppqValid_ = true;
    }

    lastPlaying_ = isPlaying;
}

void PluginProcessor::getStateInformation(juce::MemoryBlock &destData) {
    emulator_.getState(destData);

    uint8_t block[16];
    for (int t = 0; t < NUM_TRACKS; ++t) {
        block[t] = static_cast<uint8_t>(patternParams_[t]->get());
        block[t + 5] = resetParams_[t]->get() ? 1 : 0;
        block[t + 10] = static_cast<uint8_t>(levelParams_[t]->get());
    }
    block[15] = static_cast<uint8_t>(bankParam_->get());
    destData.append(block, sizeof(block));
}

void PluginProcessor::setStateInformation(const void *data, int sizeInBytes) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    int emuSize = sizeInBytes;

    if (sizeInBytes >= 12 + 16) {
        uint32_t flashSize = 0;
        uint32_t stateSize = 0;
        std::memcpy(&flashSize, bytes + 0, sizeof(flashSize));
        std::memcpy(&stateSize, bytes + 4, sizeof(stateSize));
        const uint64_t emuTotal = 12ull + flashSize + stateSize;

        if (static_cast<uint64_t>(sizeInBytes) == emuTotal + 16) {
            const uint8_t* p = bytes + emuTotal;

            for (int t = 0; t < NUM_TRACKS; ++t) {
                *patternParams_[t] = p[t];
                *resetParams_[t] = p[t + 5] != 0;
                *levelParams_[t] = p[t + 10];
            }
            *bankParam_ = p[15];
            emuSize = static_cast<int>(emuTotal);
        }
    }

    const juce::ScopedLock sl(getCallbackLock());
    emulator_.setState(bytes, emuSize);
}

juce::AudioProcessor * JUCE_CALLTYPE createPluginFilter() {
    return new PluginProcessor();
}

juce::AudioProcessorEditor *PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}
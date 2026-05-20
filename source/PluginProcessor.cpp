//
// PluginProcessor.cpp
//

#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Main", juce::AudioChannelSet::stereo()))
{
    syncEvents_.reserve(64); // max ticks per block
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
}

void PluginProcessor::setStateInformation(const void *data, int sizeInBytes) {
    const juce::ScopedLock sl(getCallbackLock());
    emulator_.setState(data, sizeInBytes);
}

juce::AudioProcessor * JUCE_CALLTYPE createPluginFilter() {
    return new PluginProcessor();
}

juce::AudioProcessorEditor *PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}
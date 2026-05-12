//
// PluginProcessor.cpp
//

#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withOutput("Main", juce::AudioChannelSet::stereo()))
{

}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    DBG("prepareToPlay: sampleRate=" << sampleRate << " blockSize=" << samplesPerBlock);
    emulator_.prepare(sampleRate, samplesPerBlock);
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();

    handleSync();
    emulator_.render(buffer, buffer.getNumSamples());
}

void PluginProcessor::handleSync() {
    auto* playHead = getPlayHead();
    if (playHead == nullptr) return;

    const auto pos = playHead->getPosition();
    if (!pos.hasValue()) return;

    const bool isPlaying = pos->getIsPlaying();

    if (isPlaying && !lastPlaying_) {        
        emulator_.sendSerial8(0x02); // SYNC_MSG_START
    } else if (!isPlaying && lastPlaying_) {        
        emulator_.sendSerial8(0x03); // SYNC_MSG_STOP
    }

    const auto ppqOpt = pos->getPpqPosition();

    if (isPlaying && ppqValid_ && ppqOpt.hasValue()) {
        const double currentPpq = *ppqOpt;
        const double delta = currentPpq - lastPpq_;

        // Skip ticks on loops/locates (delta negative or large).
        if (delta > 0.0 && delta < 1.0) {
            const auto lastTick = static_cast<int64_t>(std::floor(lastPpq_ * 24.0));
            const auto currentTick = static_cast<int64_t>(std::floor(currentPpq * 24.0));
            
            for (int64_t i = lastTick; i < currentTick; ++i) {
                emulator_.sendSerial8(0x01); // SYNC_MSG_TICK
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
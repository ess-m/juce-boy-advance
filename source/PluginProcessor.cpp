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
    emulator_.render(buffer, buffer.getNumSamples());
}

void PluginProcessor::getStateInformation(juce::MemoryBlock &destData) {
}

void PluginProcessor::setStateInformation(const void *data, int sizeInBytes) {
}

juce::AudioProcessor * JUCE_CALLTYPE createPluginFilter() {
    return new PluginProcessor();
}

juce::AudioProcessorEditor *PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}
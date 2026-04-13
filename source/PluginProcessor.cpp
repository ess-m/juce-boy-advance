#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cassert>

PluginProcessor::PluginProcessor() :
    AudioProcessor(BusesProperties()
        .withOutput("Main", juce::AudioChannelSet::stereo(), true))
{
    
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    juce::ignoreUnused(samplesPerBlock);
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages) {
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    mainBuffer.clear();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample) {
        if (mainBuffer.getNumChannels() > 0) {
            mainBuffer.getWritePointer(0)[sample] = 0.f;

            if (mainBuffer.getNumChannels() > 1) {
                mainBuffer.getWritePointer(1)[sample] = 0.f;
            }
        }
    }
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
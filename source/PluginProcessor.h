//
// PluginProcessor.h
//


#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "services/EmulatorService.h"

class PluginProcessor final : public juce::AudioProcessor {
private:
    EmulatorService emulator_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)

public:
    PluginProcessor();
    ~PluginProcessor() override;

    EmulatorService& getEmulator() { return emulator_; }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;

    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override {return false; }

    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }

    const juce::String getProgramName(int index) override {
        juce::ignoreUnused(index);
        return {};
    }

    void changeProgramName(int index, const juce::String &newName) override {
        juce::ignoreUnused(index, newName);
    }

    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;
};
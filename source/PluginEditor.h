#pragma once

#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor {
public:
    explicit PluginEditor(PluginProcessor &);

    ~PluginEditor() override;

    void paint(juce::Graphics &) override;

    void resized() override;

    void parentHierarchyChanged() override;

private:
    PluginProcessor &processor_;

    juce::VBlankAttachment vblankAttachment_;

    void onVBlank() {
        // Do stuff
    }

    const int baseWidth_ = 240;
    const int baseHeight_ = 160;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
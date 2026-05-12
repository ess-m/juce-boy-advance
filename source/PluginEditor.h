//
// PluginEditor.h
//

#pragma once

#include "PluginProcessor.h"

class PluginEditor final : public juce::AudioProcessorEditor {
public:
    explicit PluginEditor(PluginProcessor &);
    ~PluginEditor() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void parentHierarchyChanged() override;

    bool keyStateChanged(bool isKeyDown) override;

private:
    PluginProcessor &processor_;
    juce::VBlankAttachment vblankAttachment_;

    void onVBlank() {
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
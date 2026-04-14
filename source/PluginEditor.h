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

    void mouseDown(const juce::MouseEvent& e) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    PluginProcessor &processor_;
    juce::VBlankAttachment vblankAttachment_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    void onVBlank() {
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
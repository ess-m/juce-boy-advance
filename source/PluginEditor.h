//
// PluginEditor.h
//

#pragma once

#include "PluginProcessor.h"

#include "UI/ConfigMenuButton.h"
#include "UI/ZoomMenuButton.h"

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

    ConfigMenuButton configMenu_;
    ZoomMenuButton zoomMenu_;

    void onVBlank() {
        repaint();
    }

    std::atomic<juce::Colour> borderColor_{ juce::Colours::black };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
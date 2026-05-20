//
// PluginEditor.h
//

#pragma once

#include "PluginProcessor.h"

#include "UI/ConfigMenuButton.h"
#include "UI/HoverBracket.h"
#include "UI/PopupLook.h"
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
    
    HoverBracket configHover_;
    HoverBracket zoomHover_;

    PopupLook popupLook_;

    void onVBlank() {
        repaint();
        syncTitlebarColor();
    }

    void syncTitlebarColor();

    void restartCore() {
        const juce::ScopedLock sl(processor_.getCallbackLock());
        processor_.getEmulator().resetCore();
    }

    void importSave() {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import sav", juce::File{}, "*.sav");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                const auto file = fc.getResult();
                if (!file.existsAsFile()) return;

                const juce::ScopedLock sl(processor_.getCallbackLock());

                processor_.getEmulator().importFlash(file);
                processor_.getEmulator().resetCore();
            }
        );
    }

    void exportSave() {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Export sav", juce::File{}, "*.sav");
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode 
                | juce::FileBrowserComponent::canSelectFiles 
                | juce::FileBrowserComponent::warnAboutOverwriting,
            [this, chooser](const juce::FileChooser& fc) {
                const auto file = fc.getResult();
                if (file == juce::File{}) return;

                processor_.getEmulator().exportFlash(file);
            }
        );
    }

    void clearSaveData() {
        juce::NativeMessageBox::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Clear data")
                .withMessage("This will erase your data\nand restart the ROM")
                .withButton("Cancel")
                .withButton("Erase"),
            [this](int confirm) {
                if (confirm != 1) return;
                const juce::ScopedLock sl(processor_.getCallbackLock());

                processor_.getEmulator().clearFlash();
                processor_.getEmulator().resetCore();
            }
        );
    }

    std::atomic<juce::Colour> borderColor_{ juce::Colours::black };

#if JUCE_MAC && JUCE_STANDALONE_APPLICATION && !DEBUG
    juce::Colour lastTitlebarColor_ = juce::Colours::transparentBlack;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
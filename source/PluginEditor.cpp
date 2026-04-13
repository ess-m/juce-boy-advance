#include "PluginEditor.h"

#if JUCE_MAC
#include "macOS/MacWindow.h"
#endif

PluginEditor::PluginEditor(PluginProcessor &p)
    : AudioProcessorEditor(&p)
    , processor_(p)
    , vblankAttachment_(this, [this] { onVBlank(); })
{
    setSize(baseWidth_, baseHeight_);
    setResizable(true, false);
    
    getConstrainer()->setFixedAspectRatio(
        static_cast<float>(baseWidth_) / static_cast<float>(baseHeight_)
    );    
}
PluginEditor::~PluginEditor() {
}

void PluginEditor::paint(juce::Graphics &g) {
}

void PluginEditor::resized() {
}

void PluginEditor::parentHierarchyChanged() {
    AudioProcessorEditor::parentHierarchyChanged();

#if JUCE_MAC && JUCE_STANDALONE_APPLICATION &&!DEBUG
    static bool windowStyled = false;

    if (!windowStyled) {
        if (auto* topLevel = juce::TopLevelWindow::getTopLevelWindow(0)) {
            topLevel->setUsingNativeTitleBar(true);
            windowStyled = true;

            juce::MessageManager::callAsync([safeThis = SafePointer(this), this] {
                if (safeThis == nullptr) return;

                if (const auto* topLevel = safeThis->getTopLevelComponent()) {
                    if (const auto* peer = topLevel->getPeer()) {
                        MacWindow::styleWindow(peer->getNativeHandle());
                        MacWindow::setTitlebarColor(peer->getNativeHandle(), juce::Colours::black);
                    }
                }
            });
        }
    }
#endif
}
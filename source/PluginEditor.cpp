//
// PluginEditor.cpp
//

#include "PluginEditor.h"

#if JUCE_MAC
#include "macOS/MacWindow.h"
#endif

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p)
    , processor_(p)
    , vblankAttachment_(this, [this] { onVBlank(); })
{
    setSize(SCREEN_W * 3, SCREEN_H * 3);
    setResizable(true, false);
    setResizeLimits(SCREEN_W, SCREEN_H, 4096, 4096);
    getConstrainer()->setFixedAspectRatio(
        static_cast<float>(SCREEN_W) / static_cast<float>(SCREEN_H));
    setWantsKeyboardFocus(true);
}

PluginEditor::~PluginEditor() {
}

void PluginEditor::paint(juce::Graphics &g) {
    g.fillAll(juce::Colours::black);

    const auto bounds = getLocalBounds();
    const auto frame = processor_.getEmulator().getVideo().renderFrame();

    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImageAt(
        frame,
        (bounds.getWidth() - frame.getWidth()) / 2,
        (bounds.getHeight() - frame.getHeight()) / 2
    );
}

void PluginEditor::resized() {
    const auto bounds = getLocalBounds();
    const int scale = std::max(
        1, 
        std::min(bounds.getWidth() / SCREEN_W, bounds.getHeight() / SCREEN_H)
    );

    processor_.getEmulator().getVideo().setScale(scale);
}

bool PluginEditor::keyStateChanged(bool /*isKeyDown*/) {
    processor_.getEmulator().getInput().pollKeyboard();
    return true;
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
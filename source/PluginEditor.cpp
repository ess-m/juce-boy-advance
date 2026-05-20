//
// PluginEditor.cpp
//

#include "PluginEditor.h"

#if JUCE_MAC
#include "macOS/MacWindow.h"
#endif

static constexpr int WINDOW_MARGIN_W = 36;
static constexpr int WINDOW_MARGIN_H = 18;

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p)
    , processor_(p)
    , vblankAttachment_(this, [this] { onVBlank(); })
{
    setSize(SCREEN_W * 3.25 + WINDOW_MARGIN_W * 3.25, SCREEN_H * 3.25 + WINDOW_MARGIN_H * 3.25);
    setResizable(true, false);
    setResizeLimits(
        SCREEN_W + WINDOW_MARGIN_W, 
        SCREEN_H + WINDOW_MARGIN_H, 
        SCREEN_W * 16 + WINDOW_MARGIN_W * 16, 
        SCREEN_H * 16 + WINDOW_MARGIN_H * 16
    );
    
    getConstrainer()->setFixedAspectRatio(
        static_cast<float>(SCREEN_W + WINDOW_MARGIN_W) / static_cast<float>(SCREEN_H + WINDOW_MARGIN_H)
    );

    setWantsKeyboardFocus(true);

    auto themeProvider = [this] {
        return processor_.getEmulator().getThemeColors();
    };

    addAndMakeVisible(zoomMenu_);
    zoomMenu_.setButtonCallback([this] {
        // TODO: popup menu
    });
    zoomMenu_.setThemeProvider(themeProvider);

    addAndMakeVisible(configMenu_);
    configMenu_.setButtonCallback([this] {
        // TODO: popup menu
    });
    configMenu_.setThemeProvider(themeProvider);
}

PluginEditor::~PluginEditor() {
}

void PluginEditor::paint(juce::Graphics &g) {
    const auto inner = 
        getLocalBounds().reduced(WINDOW_MARGIN_W, WINDOW_MARGIN_H);
    const auto frame = processor_.getEmulator().getVideo().renderFrame();

    if (frame.isValid() && frame.getWidth() > 0) {
        borderColor_.store(frame.getPixelAt(0, 0));
    }

    g.fillAll(borderColor_.load());    

    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImageAt(
        frame,
        inner.getX() + (inner.getWidth()  - frame.getWidth())  / 2,
        inner.getY() + (inner.getHeight() - frame.getHeight()) / 2
    );
}

void PluginEditor::resized() {
    const auto inner = 
        getLocalBounds().reduced(WINDOW_MARGIN_W, WINDOW_MARGIN_H);
    const int scale = std::max(
        1, 
        std::min(inner.getWidth() / SCREEN_W, inner.getHeight() / SCREEN_H)
    );

    processor_.getEmulator().getVideo().setScale(scale);
    
    zoomMenu_.setBounds(getWidth() - 36, getHeight() - 36, 36, 36);
    configMenu_.setBounds(getWidth() - 36, getHeight() - 72, 36, 36);
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
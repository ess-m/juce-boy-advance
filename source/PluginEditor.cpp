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
    setSize(
        static_cast<int>(SCREEN_W * 3.25 + WINDOW_MARGIN_W * 3.25), 
        static_cast<int>(SCREEN_H * 3.25 + WINDOW_MARGIN_H * 3.25)
    );
    setResizable(true, false);
    setResizeLimits(
        SCREEN_W + WINDOW_MARGIN_W, 
        SCREEN_H + WINDOW_MARGIN_H, 
        SCREEN_W * 16 + WINDOW_MARGIN_W * 16, 
        SCREEN_H * 16 + WINDOW_MARGIN_H * 16
    );

    setWantsKeyboardFocus(true);

    auto themeProvider = [this] {
        return processor_.getEmulator().getThemeColors();
    };

    addAndMakeVisible(zoomMenu_);
    zoomMenu_.setButtonCallback([this] {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&popupLook_);

        menu.addSectionHeader("scale");
        menu.addSeparator();

        for (int idx = 1; idx <= 4; ++idx) {
            menu.addItem(idx, juce::String::charToString(0x00D7) + " " + juce::String(idx));
        }

        menu.showMenuAsync(
            juce::PopupMenu::Options()
                .withParentComponent(this)
                .withTargetComponent(zoomMenu_), 
            [this](int result) {
                if (result > 0) {               
                    const float factor = (result + .25f);

                    setSize(
                        static_cast<int>(SCREEN_W * factor + WINDOW_MARGIN_W * factor), 
                        static_cast<int>(SCREEN_H * factor + WINDOW_MARGIN_H * factor)
                    );
                    resized();
                }
            }
        );
    });
    zoomMenu_.setThemeProvider(themeProvider);

    addAndMakeVisible(configMenu_);
    configMenu_.setButtonCallback([this] {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&popupLook_);

        menu.addSectionHeader("config");
        menu.addSeparator();

        menu.addItem(1, "input");

        if (processor_.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
            menu.addItem(2, "audio");

        menu.addSeparator();
        menu.addSectionHeader("state");
        menu.addSeparator();

        menu.addItem(3, "restart");
        menu.addItem(4, "import");
        menu.addItem(5, "export");
        menu.addItem(6, "clear");

        menu.showMenuAsync(
            juce::PopupMenu::Options()
                .withParentComponent(this)
                .withTargetComponent(configMenu_),
            [this](int result) {
                switch (result) {
                    case 1: {
                        if (!inputConfig_.isVisible()) {
                            audioConfig_.hide();
                            inputConfig_.show();
                        } else {
                            inputConfig_.hide();
                        }
                        break;
                    }

                    case 2: {
                        if (!audioConfig_.isVisible()) {
                            inputConfig_.hide();
                            audioConfig_.show();
                        } else {
                            audioConfig_.hide();
                        }
                        break;
                    }
                    
                    case 3: restartCore(); break;
                    case 4: importSave(); break;
                    case 5: exportSave(); break;
                    case 6: clearSaveData(); break;
                    default: break;
                }
            }
        );
    });
    configMenu_.setThemeProvider(themeProvider);

    popupLook_.setThemeProvider(themeProvider);

    addAndMakeVisible(zoomHover_);
    zoomHover_.setTargetComponent(&zoomMenu_);
    zoomHover_.setThemeProvider(themeProvider);
    zoomHover_.setColorField(&ThemeColors::bg);

    addAndMakeVisible(configHover_);
    configHover_.setTargetComponent(&configMenu_);
    configHover_.setThemeProvider(themeProvider);
    
    addChildComponent(inputConfig_);
    inputConfig_.setThemeProvider(themeProvider);

    addChildComponent(audioConfig_);
    audioConfig_.setThemeProvider(themeProvider);
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

    
    const float overlayAlpha = std::max(inputConfig_.getAlpha(), audioConfig_.getAlpha());
    g.setOpacity(1.f - overlayAlpha);

    g.drawImageAt(
        frame,
        inner.getX() + (inner.getWidth()  - frame.getWidth())  / 2,
        inner.getY() + (inner.getHeight() - frame.getHeight()) / 2
    );

    g.setOpacity(1.f);
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

    zoomHover_.setBounds(getWidth() - 36, getHeight() - 36, 36, 36);
    configHover_.setBounds(getWidth() - 36, getHeight() - 72, 36, 36);

    inputConfig_.place(getWidth() / 2, getHeight() / 2);
    audioConfig_.place(getWidth() / 2, getHeight() / 2);
}

bool PluginEditor::keyStateChanged(bool /*isKeyDown*/) {
    processor_.getEmulator().getInput().pollKeyboard();
    return true;
}

void PluginEditor::parentHierarchyChanged() {
    AudioProcessorEditor::parentHierarchyChanged();

    #if JUCE_STANDALONE_APPLICATION && !DEBUG
    static bool windowStyled = false;

    if (!windowStyled) {
        if (auto* topLevel = juce::TopLevelWindow::getTopLevelWindow(0)) {
            topLevel->setUsingNativeTitleBar(true);
            windowStyled = true;

            #if JUCE_MAC
            juce::MessageManager::callAsync([safeThis = SafePointer(this), this] {
                if (safeThis == nullptr) return;

                if (const auto* topLevel = safeThis->getTopLevelComponent()) {
                    if (const auto* peer = topLevel->getPeer()) {
                        MacWindow::styleWindow(peer->getNativeHandle());
                    }
                }
            });
            #endif
        }
    }
    #endif
}

void PluginEditor::syncTitlebarColor() {
#if JUCE_MAC && JUCE_STANDALONE_APPLICATION && !DEBUG
    const auto bg = processor_.getEmulator().getThemeColors().bg;
    if (bg == lastTitlebarColor_) return;

    if (const auto* topLevel = getTopLevelComponent()) {
        if (const auto* peer = topLevel->getPeer()) {
            MacWindow::setTitlebarColor(peer->getNativeHandle(), bg);
            lastTitlebarColor_ = bg;
        }
    }
#endif
}
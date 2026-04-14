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

void PluginEditor::mouseDown(const juce::MouseEvent& e) {
    if (processor_.getEmulator().isRunning()) return;

    fileChooser_ =
        std::make_unique<juce::FileChooser>("Load ROM", juce::File{}, "*.gba");

    fileChooser_->launchAsync(
    juce::FileBrowserComponent::openMode |
        juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser &fc) {
                auto file = fc.getResult();
                if (file.existsAsFile()) processor_.getEmulator().loadROM(file);
            }
    );
}

struct KeyMapping {
    int juceKeyCode;
    nba::Key gbaKey;
};

static const KeyMapping keyMappings[] = {
    { 'Z', nba::Key::A },
    { 'X', nba::Key::B },
    { juce::KeyPress::returnKey, nba::Key::Start },
    { juce::KeyPress::backspaceKey, nba::Key::Select},
    { juce::KeyPress::upKey, nba::Key::Up },
    { juce::KeyPress::downKey, nba::Key::Down },
    { juce::KeyPress::leftKey, nba::Key::Left },
    { juce::KeyPress::rightKey, nba::Key::Right },
    { 'A', nba::Key::L },
    { 'S', nba::Key::R },
};

bool PluginEditor::keyStateChanged(bool /*isKeyDown*/) {
    auto& input = processor_.getEmulator().getInput();

    for (const auto& [juceKey, gbaKey] : keyMappings) {
        if (juce::KeyPress::isKeyCurrentlyDown(juceKey)) {
            input.keyDown(gbaKey);
        } else {
            input.keyUp(gbaKey);
        }
    }
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
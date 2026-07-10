//
// InputConfigOverlay.h
//

#pragma once

#include <array>

#include <juce_animation/juce_animation.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "services/InputService.h"
#include "services/ThemeColors.h"
#include "Font.h"
#include "BackButton.h"
#include "KeyMapButton.h"
#include "MenuLabel.h"
#include "PopupLook.h"

class InputConfigOverlay : public juce::Component {
public:
    using ThemeProvider = std::function<ThemeColors()>;

    explicit InputConfigOverlay(InputService& input) : input_(input) {
        for (size_t i = 0; i < 10; ++i) {
            const auto gbaKey = ROW_KEYS[i];

            // keyboard column
            keyboardButtons_[i].setOnBind([this, i, gbaKey](int code) {
                input_.setKeyboardMapping(gbaKey, code);
                keyboardButtons_[i].setLabel(juce::KeyPress(code).getTextDescription());
            });

            keyboardButtons_[i].setLabel(
                juce::KeyPress(input_.getKeyboardMapping(gbaKey)).getTextDescription());

            keyboardButtons_[i].place(220 + 48, 80 + static_cast<int>(i) * 25 + 9);
            addAndMakeVisible(keyboardButtons_[i]);

            // gamepad column
            const juce::Component::SafePointer<KeyMapButton> safeBtn { &gamepadButtons_[i] };

            gamepadButtons_[i].setOnStartCapture([this, safeBtn]() {
                input_.beginGamepadCapture([safeBtn](int sdlButton) {
                    if (safeBtn) safeBtn->commit(sdlButton);
                });
            });

            gamepadButtons_[i].setOnCancelCapture([this]() {
                input_.cancelGamepadCapture();
            });

            gamepadButtons_[i].setOnBind([this, i, gbaKey](int sdlButton) {
                input_.setGamepadMapping(gbaKey, sdlButton);
                gamepadButtons_[i].setLabel(InputService::gamepadButtonName(sdlButton));
            });

            gamepadButtons_[i].setLabel(
                InputService::gamepadButtonName(input_.getGamepadMapping(gbaKey)));

            gamepadButtons_[i].place(386 + 48, 80 + static_cast<int>(i) * 25 + 9);
            addAndMakeVisible(gamepadButtons_[i]);
        }

        controllerSelector_.setTextProvider([this] {
            return input_.getCurrentControllerName().upToFirstOccurrenceOf(" ", false, false);
        });
        controllerSelector_.setOnClick([this] { showControllerMenu(); });
        controllerSelector_.setJustification(juce::Justification::right);
        controllerSelector_.place(280, 363, 200, 18);
        addAndMakeVisible(controllerSelector_);

        backButton_.setButtonCallback([this] { hide(); });
        backButton_.setBounds(0, 0, 18, 18);
        addAndMakeVisible(backButton_);

        setAlpha(0.f);

        updater_.addAnimator(fadeAnimation_);
    }

    ~InputConfigOverlay() override {
        updater_.removeAnimator(fadeAnimation_);
    }

    void show() {
        if (isVisible() && fadingIn_) return;
        fadingIn_ = true;
        setAlpha(0.f);
        setVisible(true);
        toFront(true);
        fadeAnimation_.start();
    }

    void hide() {
        if (!isVisible()) return;
        if (!fadingIn_ && fadeAnimation_.isComplete()) return;
        fadingIn_ = false;
        fadeAnimation_.start();
    }

    void place(int x, int y) {
        setBounds(x - 240, y - 200, 484, 400);
    }

    void setThemeProvider(ThemeProvider cb) {
        themeProvider_ = cb;
        for (auto& b : keyboardButtons_) b.setThemeProvider(themeProvider_);
        for (auto& b : gamepadButtons_) b.setThemeProvider(themeProvider_);
        controllerSelector_.setThemeProvider(themeProvider_);
        backButton_.setThemeProvider(themeProvider_);
        popupLook_.setThemeProvider(themeProvider_);
        repaint();
    }

    ThemeColors getColors() const {
        return themeProvider_ ? themeProvider_() : ThemeColors{};
    }

private:
    static constexpr std::array<nba::Key, 10> ROW_KEYS = {
        nba::Key::B,      nba::Key::A,
        nba::Key::L,      nba::Key::R,
        nba::Key::Select, nba::Key::Start,
        nba::Key::Up,     nba::Key::Down,
        nba::Key::Left,   nba::Key::Right,
    };

    InputService& input_;
    ThemeProvider themeProvider_;
    std::array<KeyMapButton, 10> keyboardButtons_;
    std::array<KeyMapButton, 10> gamepadButtons_;

    PopupLook popupLook_;
    MenuLabel controllerSelector_;
    BackButton backButton_;

    bool fadingIn_ = false;
    juce::VBlankAnimatorUpdater updater_ { this };
    juce::Animator fadeAnimation_ =
        juce::ValueAnimatorBuilder{}
            .withEasing(juce::Easings::createEaseInOut())
            .withDurationMs(150)
            .withValueChangedCallback([this](auto value) {
                const float p = fadingIn_
                    ? static_cast<float>(value)
                    : 1.f - static_cast<float>(value);
                setAlpha(p);
            })
            .withOnCompleteCallback([this] {
                if (!fadingIn_) setVisible(false);
            })
            .build();

    void showControllerMenu() {
        const auto controllers = input_.enumerateControllers();

        juce::PopupMenu menu;
        menu.setLookAndFeel(&popupLook_);

        menu.addItem("None", true, false, [this] {
            input_.selectController(-1);
            controllerSelector_.repaint();
        });

        for (const auto& [joyIdx, name] : controllers) {
            menu.addItem(name, true, false, [this, joyIdx = joyIdx] {
                input_.selectController(joyIdx);
                controllerSelector_.repaint();
            });
        }

        menu.showMenuAsync(
            juce::PopupMenu::Options()
                .withMousePosition()
                .withParentComponent(this));
    }

    void paint(juce::Graphics& g) override {
        static const std::array<juce::String, 10> buttonLabels = {
            "B", "A", "Shoulder Left", "Shoulder Right", "Select", "Start",
            "Up", "Down", "Left", "Right"
        };

        g.setColour(getColors().lo);

        g.setFont(UIFont::getInstance().getUIFont().withHeight(21.0f));
        g.drawText("Input config", 24, 0, 200, 18, juce::Justification::left);

        g.fillRect(0, 25, 480, 1);

        g.drawText("Button", 0, 45, 100, 18, juce::Justification::left);
        g.drawText("Keyboard", 220, 45, 96, 18, juce::Justification::centred);
        g.drawText("Controller", 386, 45, 96, 18, juce::Justification::centred);

        for (size_t i = 0; i < 10; ++i) {
            g.drawText(buttonLabels[i], 0, 80 + static_cast<int>(i) * 25,
                200, 18, juce::Justification::left
            );
        }

        g.fillRect(0, 343, 480, 1);

        g.drawText("Connected controller", 0, 363, 200, 18, juce::Justification::left);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputConfigOverlay)
};

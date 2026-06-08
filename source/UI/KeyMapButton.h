//
// KeyMapButton.h
//

#pragma once

#include <cmath>

#include <juce_animation/juce_animation.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "services/ThemeColors.h"
#include "Font.h"
#include "HoverBracket.h"

class KeyMapButton : public juce::Component {
public:
    static constexpr int WIDTH = 96;
    static constexpr int HEIGHT = 18;

    using ThemeProvider = std::function<ThemeColors()>;
    using OnBindCallback = std::function<void(int code)>;
    using OnStartCapture = std::function<void()>;
    using OnCancelCapture = std::function<void()>;

    KeyMapButton() {
        setWantsKeyboardFocus(true);
        hover_.setTargetComponent(this);
        addAndMakeVisible(hover_);
        updater_.addAnimator(dotAnimation_);
    }

    ~KeyMapButton() override {
        updater_.removeAnimator(dotAnimation_);
    }

    void place(int x, int y) {
        setBounds(x - WIDTH / 2, y - HEIGHT / 2, WIDTH, HEIGHT);
    }

    void setLabel(const juce::String& s) {
        label_ = s;
        repaint();
    }

    void setOnBind(OnBindCallback cb) { onBind_ = std::move(cb); }
    void setOnStartCapture(OnStartCapture cb) { onStartCapture_ = std::move(cb); }
    void setOnCancelCapture(OnCancelCapture cb) { onCancelCapture_ = std::move(cb); }

    void setThemeProvider(ThemeProvider cb) {
        themeProvider_ = cb;
        hover_.setThemeProvider(std::move(cb));
    }

    void commit(int code) {
        if (!capturing_) return;
        exitCapture();
        if (onBind_) onBind_(code);
    }

    void cancel() {
        if (!capturing_) return;
        exitCapture();
    }

private:
    juce::String label_;
    bool capturing_ = false;

    OnBindCallback onBind_;
    OnStartCapture onStartCapture_;
    OnCancelCapture onCancelCapture_;
    ThemeProvider themeProvider_;

    HoverBracket hover_;

    float dotPhase_ = 0.f;

    juce::VBlankAnimatorUpdater updater_ { this };
    juce::Animator dotAnimation_ =
        juce::ValueAnimatorBuilder{}
            .withDurationMs(1000)
            .withValueChangedCallback([this](auto value) {
                dotPhase_ = static_cast<float>(value);
                repaint();
            })
            .withOnCompleteCallback([this]() {
                if (capturing_) dotAnimation_.start();
            })
            .build();

    ThemeColors getColors() const {
        return themeProvider_ ? themeProvider_() : ThemeColors{};
    }

    void exitCapture() {
        capturing_ = false;
        repaint();
    }

    void cancelInternal() {
        if (!capturing_) return;
        capturing_ = false;
        if (onCancelCapture_) onCancelCapture_();
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto colors = getColors();

        if (capturing_) {
            g.setColour(colors.bg);
            
            g.fillRoundedRectangle(
                0.f, 0.f,
                static_cast<float>(WIDTH), static_cast<float>(HEIGHT),
                2.f
            );

            g.setColour(colors.lo);

            constexpr float radius = 1.75f;
            constexpr float stagger = 0.2f;
            constexpr float hop = 0.5f;

            const float cx = static_cast<float>(WIDTH) * 0.5f;
            const float cy = static_cast<float>(HEIGHT) * 0.5f;

            for (int i = 0; i < 3; ++i) {
                const float thisPhase = dotPhase_ - stagger * static_cast<float>(i);
                const float dy = (thisPhase >= 0.f && thisPhase <= hop)
                    ? -std::sin(thisPhase / hop * juce::MathConstants<float>::pi) * 3.f : 0.f;
                const float dx = static_cast<float>(i - 1) * 7.f;

                g.fillEllipse(cx + dx - radius, cy + dy - radius, radius * 2.f, radius * 2.f);
            }
        } else {
            g.setColour(colors.lo);

            g.setFont(UIFont::getInstance().getUIFont().withHeight(21.0f));
            g.drawText(label_, 0, 0, WIDTH, HEIGHT, juce::Justification::centred);
        }
    }

    void resized() override {
        hover_.setBounds(getLocalBounds());
    }

    void mouseDown(const juce::MouseEvent&) override {
        if (capturing_) {
            cancelInternal();
            return;
        }

        capturing_ = true;
        dotAnimation_.start();

        grabKeyboardFocus();
        
        if (onStartCapture_) onStartCapture_();
        repaint();
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if (!capturing_) return false;

        if (key == juce::KeyPress::escapeKey) {
            cancelInternal();
            return true;
        }

        if (onStartCapture_) return true;

        exitCapture();
        if (onBind_) onBind_(key.getKeyCode());
        return true;
    }

    void focusLost(FocusChangeType) override {
        cancelInternal();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyMapButton)
};

//
// MenuLabel.h
//

#pragma once

#include <juce_animation/juce_animation.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "services/ThemeColors.h"
#include "Font.h"

class MenuLabel : public juce::Component {
public:
    using OnClickCallback = std::function<void()>;
    using ThemeProvider = std::function<ThemeColors()>;
    using TextProvider = std::function<juce::String()>;

    MenuLabel() {
        updater_.addAnimator(hoverAnimation_);
    }

    ~MenuLabel() override {
        updater_.removeAnimator(hoverAnimation_);
    }

    void place(int x, int y, int w, int h) {
        setBounds(x - 2, y - 2, w + 4, h + 6);
    }

    void setOnClick(OnClickCallback cb) { onClick_ = std::move(cb); }
    void setTextProvider(TextProvider cb) { textProvider_ = std::move(cb); }
    void setThemeProvider(ThemeProvider cb) { themeProvider_ = std::move(cb); }
    void setJustification(juce::Justification j) { just_ = j; }

private:
    OnClickCallback onClick_;
    TextProvider textProvider_;
    ThemeProvider themeProvider_;

    juce::Justification just_ { juce::Justification::left };
    bool hovering_ = false;
    float hoverValue_ = 0.f;

    juce::VBlankAnimatorUpdater updater_ { this };
    juce::Animator hoverAnimation_ =
        juce::ValueAnimatorBuilder{}
            .withEasing(juce::Easings::createEaseInOut())
            .withDurationMs(75)
            .withValueChangedCallback([this](auto value) {
                hoverValue_ = hovering_ ? static_cast<float>(value) : 1.f - static_cast<float>(value);
                repaint();
            })
            .build();

    void paint(juce::Graphics& g) override {
        const auto colors = themeProvider_ ? themeProvider_() : ThemeColors{};
        const juce::String text = textProvider_ ? textProvider_() : juce::String{};

        g.setColour(colors.lo);

        const auto font = UIFont::getInstance().getUIFont().withHeight(21.0f);
        g.setFont(font);

        const auto textArea = getLocalBounds()
            .withTrimmedLeft(2).withTrimmedRight(2)
            .withTrimmedTop(2).withTrimmedBottom(4);

        g.drawText(text, textArea, just_);

        const float textWidth = juce::GlyphArrangement::getStringWidth(font, text);

        const float hoverOffset = hoverValue_ * 2.f;
        auto underline = textArea.toFloat().withHeight(1.f).withY(static_cast<float>(textArea.getBottom()));
        underline = underline.expanded(4.f, 0.f).translated(-4.f, 1.f + hoverOffset);

        g.setColour(colors.lo.interpolatedWith(colors.bg, hoverValue_));
        g.fillRect(just_.appliedToRectangle(juce::Rectangle<float>(0.f, 0.f, textWidth, 1.f), underline));
    }

    void mouseEnter(const juce::MouseEvent&) override {
        hovering_ = true;
        hoverAnimation_.start();
    }

    void mouseExit(const juce::MouseEvent&) override {
        hovering_ = false;
        hoverAnimation_.start();
    }

    void mouseDown(const juce::MouseEvent&) override {
        if (onClick_) onClick_();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MenuLabel)
};

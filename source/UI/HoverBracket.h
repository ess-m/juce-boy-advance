//
// HoverBracket.h
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_animation/juce_animation.h>

#include "services/ThemeColors.h"

class HoverBracket final : public juce::Component {
public:
    HoverBracket() {
        setInterceptsMouseClicks(false, false);
        updater_.addAnimator(hoverAnimation_);
    }
    ~HoverBracket() override {
        updater_.removeAnimator(hoverAnimation_);
    }

    using ThemeProvider = std::function<ThemeColors()>;

    void setThemeProvider(ThemeProvider cb) {
        themeProvider_ = std::move(cb);
    }

    ThemeColors getColors() const {
        return themeProvider_ ? themeProvider_() : ThemeColors{};
    }

    void setColorField(juce::Colour ThemeColors::* field) {
        colorField_ = field;
    }

    void setTargetComponent(Component *target) {
        if (targetComponent_)
            targetComponent_->removeMouseListener(this);

        targetComponent_ = target;

        if (targetComponent_)
            targetComponent_->addMouseListener(this, false);
    }

    void mouseEnter(const juce::MouseEvent&) override {
        isHovered_ = true;
        hoverAnimation_.start();
    }

    void mouseExit(const juce::MouseEvent&) override {
        isHovered_ = false;
        hoverAnimation_.start();
    }

private:
    ThemeProvider themeProvider_;
    Component* targetComponent_ = nullptr;
    juce::Colour ThemeColors::* colorField_ = &ThemeColors::lo;

    float hoverValue_ = 0.f;
    bool isHovered_ = false;

    juce::VBlankAnimatorUpdater updater_ { this };

    juce::Animator hoverAnimation_ =
        juce::ValueAnimatorBuilder{}
            .withEasing(juce::Easings::createEaseInOut())
            .withDurationMs(75)
            .withValueChangedCallback([this](auto value) {
                hoverValue_ = isHovered_ ? value : 1.f - value;
                repaint();
            })
            .build();

    void paint(juce::Graphics &g) override {
        if (hoverValue_ > 0.f) {
            const float h = hoverValue_ * 2.f;
            const float rx = static_cast<float>(getWidth()) - 8.f;
            const float ry = static_cast<float>(getHeight()) - 8.f;

            juce::Path p;
            g.setColour(getColors().*colorField_);

            p.startNewSubPath(6.f, 2.5f);
            p.lineTo(2.5f, 2.5f);
            p.lineTo(2.5f, 6.f);

            auto transform = juce::AffineTransform::translation(-2.f + h, -2.f + h);

            g.strokePath(p, juce::PathStrokeType(1.f), transform);
            p.clear();

            p.startNewSubPath(2.5f, 2.f);
            p.lineTo(2.5f, 5.5f);
            p.lineTo(6.f, 5.5f);

            transform = juce::AffineTransform::translation(-2.f + h, 2.f - h + ry);

            g.strokePath(p, juce::PathStrokeType(1.f), transform);
            p.clear();

            p.startNewSubPath(2.f, 5.5f);
            p.lineTo(5.5f, 5.5f);
            p.lineTo(5.5f, 2.f);

            transform = juce::AffineTransform::translation(2.f - h + rx, 2.f - h + ry);

            g.strokePath(p, juce::PathStrokeType(1.f), transform);
            p.clear();

            p.startNewSubPath(5.5f, 6.f);
            p.lineTo(5.5f, 2.5f);
            p.lineTo(2.f, 2.5f);

            transform = juce::AffineTransform::translation(2.f - h + rx, -2.f + h);

            g.strokePath(p, juce::PathStrokeType(1.f), transform);
            p.clear();
        }
    }
};
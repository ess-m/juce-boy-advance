//
// BackButton.h
//

#pragma once

#include <juce_animation/juce_animation.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "services/ThemeColors.h"
#include "MenuButton.h"

struct BackButton : MenuButton {
    BackButton() {
        updater_.addAnimator(hoverAnimation_);
    }

    ~BackButton() override {
        updater_.removeAnimator(hoverAnimation_);
    }

    void paint(juce::Graphics& g) override {
        const auto colors = getColors();

        g.setColour(colors.lo.withAlpha(1.f - hoverValue_));
        g.fillRoundedRectangle(0.f, 0.f, 18.f, 18.f, 2.f);

        g.setColour(colors.lo.withAlpha(hoverValue_));
        g.drawRoundedRectangle(0.5f, 0.5f, 17.f, 17.f, 2.f, 1.f);

        g.setColour(colors.bg.interpolatedWith(colors.lo, hoverValue_));

        juce::Path p;

        p.startNewSubPath(9, 3);
        p.lineTo(9, 10);
        p.lineTo(4.f, 6.5f);
        p.closeSubPath();

        g.fillPath(p);

        p.clear();
        p.startNewSubPath(8.f, 6.5f);
        p.lineTo(13.5f, 6.5f);
        p.lineTo(13.5f, 12.5f);
        p.lineTo(5.f, 12.5f);

        g.strokePath(p, juce::PathStrokeType(1.f));
    }

    void mouseEnter(const juce::MouseEvent&) override {
        hovering_ = true;
        hoverAnimation_.start();
    }

    void mouseExit(const juce::MouseEvent&) override {
        hovering_ = false;
        hoverAnimation_.start();
    }

private:
    bool hovering_ = false;
    float hoverValue_ = 0.f;

    juce::VBlankAnimatorUpdater updater_ { this };
    juce::Animator hoverAnimation_ =
        juce::ValueAnimatorBuilder{}
            .withEasing(juce::Easings::createEaseInOut())
            .withDurationMs(75)
            .withValueChangedCallback([this](auto value) {
                hoverValue_ = hovering_
                    ? static_cast<float>(value)
                    : 1.f - static_cast<float>(value);
                repaint();
            })
            .build();
};

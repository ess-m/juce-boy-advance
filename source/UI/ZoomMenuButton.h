//
// ZoomMenuButton.h
//

#pragma once

#include "MenuButton.h"

class ZoomMenuButton final : public MenuButton {
private:
    void paint(juce::Graphics& g) override {
        static const juce::Path iconPath = [] {
            juce::Path p;

            p.startNewSubPath(18, 7);
            p.lineTo(18, 29);

            p.startNewSubPath(7, 18);
            p.lineTo(29, 18);

            return p;
        }();
        
        g.fillAll(getColors().lo);

        g.setColour(getColors().bg);
        g.strokePath(iconPath, juce::PathStrokeType(2.f));
    }
};
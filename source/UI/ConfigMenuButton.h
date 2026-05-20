//
// ConfigMenuButton.h
//

#pragma once

#include "MenuButton.h"

class ConfigMenuButton final : public MenuButton {
private:
    void paint(juce::Graphics& g) override {
        static const juce::Path iconPath = [] {
            juce::Path p;

            p.startNewSubPath(18, 7);
            p.lineTo(18, 14);

            p.startNewSubPath(26, 10);
            p.lineTo(21, 15);

            p.startNewSubPath(29, 18);
            p.lineTo(22, 18);

            p.startNewSubPath(26, 26);
            p.lineTo(21, 21);

            p.startNewSubPath(18, 29);
            p.lineTo(18, 22);

            p.startNewSubPath(10, 26);
            p.lineTo(15, 21);

            p.startNewSubPath(7, 18);
            p.lineTo(14, 18);
            
            p.startNewSubPath(10, 10);
            p.lineTo(15, 15);

            return p;
        }();
        
        g.setColour(getColors().lo);
        g.strokePath(iconPath, juce::PathStrokeType(2.f));
    }
};
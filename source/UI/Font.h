//
// Font.h
//

#pragma once

#include "BinaryData.h"

#include <juce_graphics/juce_graphics.h>

class UIFont {
public:
    static UIFont& getInstance() {
        static UIFont instance;
        return instance;
    }
    [[nodiscard]] juce::Font getUIFont() const { return uiFont_; }

private:
    UIFont() : uiFont_(makeFont()) {}

    static juce::Font makeFont() {
        const auto typeface = juce::Typeface::createSystemTypefaceFor(
            BinaryData::uifont_otf, BinaryData::uifont_otfSize
        );
        return juce::Font(juce::FontOptions(typeface));
    }
    juce::Font uiFont_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UIFont)
};

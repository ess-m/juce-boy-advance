//
// PopupLook.h
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "services/ThemeColors.h"
#include "Font.h"

class PopupLook : public juce::LookAndFeel_V4 {
public:
    PopupLook() {
        setColour(juce::PopupMenu::backgroundColourId,
            juce::Colours::transparentBlack);
    }

    using ThemeProvider = std::function<ThemeColors()>;

    void setThemeProvider(ThemeProvider cb) {
        themeProvider_ = std::move(cb);
    }

    ThemeColors getColors() const {
        return themeProvider_ ? themeProvider_() : ThemeColors{};
    }

    int getMenuWindowFlags() override {
        return juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIsSemiTransparent;
    }

    int getPopupMenuBorderSize() override { return 0; }

    void drawPopupMenuBackgroundWithOptions(
        juce::Graphics &g, int w, int h,
        const juce::PopupMenu::Options &) override 
    {
        g.setColour(getColors().bg);
        g.fillRoundedRectangle(0, 0, w, h, 4.f);

        g.setColour(getColors().lo);
        g.fillRoundedRectangle(2, 2, w - 4, h - 4, 2.f);
    }

    juce::Font getPopupMenuFont() override {
        return UIFont::getInstance().getUIFont().withHeight(21.0f);
    }

    void getIdealPopupMenuSectionHeaderSizeWithOptions(
        const juce::String &, int, int &, int &idealHeight,
        const juce::PopupMenu::Options &) override {
        idealHeight = 30;
    }

    void drawPopupMenuSectionHeader(
        juce::Graphics& g, const juce::Rectangle<int>& area, 
        const juce::String& text) override 
    {
        g.setColour(getColors().bg);
        g.setFont(getPopupMenuFont());
        g.drawText(text, area, juce::Justification::centred, true);
    }

    void drawPopupMenuItem(
        juce::Graphics& g, const juce::Rectangle<int>& area,
        bool separator, bool, bool highlight,
        bool, bool, const juce::String& text,
        const juce::String&,
        const juce::Drawable*,
        const juce::Colour*) override
    {
        if (separator) {
            g.setColour(getColors().bg);
            g.fillRect(area.reduced(4, 0).withHeight(1).withCentre(area.getCentre()));
        }

        if (highlight && !separator) {
            g.setColour(getColors().bg);
            g.fillRoundedRectangle(4, 4, area.getWidth() - 8, area.getHeight() - 8, 1.f);

            g.setColour(getColors().lo);
        } else {
            g.setColour(getColors().bg);
        } 

        g.setFont(getPopupMenuFont());
        g.drawText(text, area, juce::Justification::centred, true);
    }

    void getIdealPopupMenuItemSize(const juce::String& text, bool separator,
                                   int, int &idealWidth,
                                   int &idealHeight) override {
                                    
        const int textWidth = juce::GlyphArrangement::getStringWidthInt(getPopupMenuFont(), text);

        idealWidth = std::max(110, textWidth + 24);
        idealHeight = separator ? 3 : 30;
    }

private:
    ThemeProvider themeProvider_;
};
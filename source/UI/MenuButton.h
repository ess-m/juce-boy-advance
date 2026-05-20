//
// MenuButton.h
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "services/ThemeColors.h"

class MenuButton : public juce::Component {
public:
    using ButtonCallback = std::function<void()>;
    using ThemeProvider = std::function<ThemeColors()>;

    void setButtonCallback(ButtonCallback cb) {
        buttonCallback_ = std::move(cb);
    }

    void setThemeProvider(ThemeProvider cb) {
        themeProvider_ = std::move(cb);
    }

    ThemeColors getColors() const {
        return themeProvider_ ? themeProvider_() : ThemeColors{};
    }

    void place(int x, int y) {
        setBounds(x, y, 36, 36);
    }

    void mouseDown(const juce::MouseEvent&) override {
        if (buttonCallback_)
            buttonCallback_();
    }

private:
    ButtonCallback buttonCallback_;
    ThemeProvider themeProvider_;
};

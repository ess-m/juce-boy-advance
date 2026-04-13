//
// MacWindow.h
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace MacWindow {
void styleWindow(void* nativeHandle);
void setTitlebarColor(void* nativeHandle, juce::Colour color);
}
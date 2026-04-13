#include "MacWindow.h"
#import <Cocoa/Cocoa.h>

namespace {
    NSColor* juceToNSColor(juce::Colour c) {
        return [NSColor colorWithRed:c.getFloatRed()
                               green:c.getFloatGreen()
                                blue:c.getFloatBlue()
                               alpha:c.getFloatAlpha()];
    }
}

@interface WindowStyler : NSObject
@property (nonatomic, assign) NSWindow* window;
@property (nonatomic, strong) NSColor* titlebarColor;
- (void)applyStyle;
@end

@implementation WindowStyler
- (void)applyStyle {
    if (!self.window) return;

    self.window.titlebarAppearsTransparent = YES;
    self.window.titleVisibility = NSWindowTitleHidden;
    self.window.collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;

    if (self.titlebarColor) {
        self.window.backgroundColor = self.titlebarColor;
    }
}

- (void)windowDidExitFullScreen:(NSNotification *)notification {
    [self applyStyle];
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}
@end

namespace MacWindow {

static WindowStyler* getStyler() {
    static WindowStyler* styler = [[WindowStyler alloc] init];
    return styler;
}

void styleWindow(void* nativeHandle) {
    if (!nativeHandle) return;

    auto view = (__bridge NSView*)nativeHandle;
    NSWindow* window = [view window];

    if (!window) return;

    WindowStyler* styler = getStyler();
    styler.window = window;
    [styler applyStyle];

    [[NSNotificationCenter defaultCenter] removeObserver:styler
                                                    name:NSWindowDidExitFullScreenNotification
                                                  object:window];
    [[NSNotificationCenter defaultCenter] addObserver:styler
                                             selector:@selector(windowDidExitFullScreen:)
                                                 name:NSWindowDidExitFullScreenNotification
                                               object:window];
}

void setTitlebarColor(void* nativeHandle, juce::Colour color) {
    if (!nativeHandle) return;

    auto view = (__bridge NSView*)nativeHandle;
    NSWindow* window = [view window];

    if (!window) return;

    WindowStyler* styler = getStyler();
    styler.titlebarColor = juceToNSColor(color);
    window.backgroundColor = styler.titlebarColor;
}
}
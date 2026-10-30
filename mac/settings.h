#pragma once

#import <Cocoa/Cocoa.h>

@class MilkRenderer;

// The settings window: the audio device to listen to, the presets folder, and clearing the thumbnail cache or opening
// its folder in Finder.
@interface SettingsController : NSObject

- (instancetype)initWithRenderer:(MilkRenderer*)renderer;
- (void)showWindow;

@end

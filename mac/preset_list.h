#pragma once

#import <Cocoa/Cocoa.h>

@class MilkRenderer;

// A window listing every preset the renderer found, with a filter field. Double-clicking a row, or
// pressing Return on it, plays that preset. The row of the preset playing now is marked, and the
// list follows the preset when it changes by key.
@interface PresetListController : NSObject <NSTableViewDataSource, NSTableViewDelegate, NSSearchFieldDelegate>

- (instancetype)initWithRenderer:(MilkRenderer*)renderer;
- (void)showWindow;

@end

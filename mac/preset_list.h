#pragma once

#import <Cocoa/Cocoa.h>

@class MilkRenderer;

// A window listing every preset the renderer found, with a filter field, as a list or as a gallery
// of thumbnails rendered by this app. Double-clicking a preset, or pressing Return on it, plays it.
// The preset playing now is marked, and the window follows it when it changes by key.
@interface PresetListController
    : NSObject <NSTableViewDataSource, NSTableViewDelegate, NSSearchFieldDelegate, NSCollectionViewDataSource, NSCollectionViewDelegate>

- (instancetype)initWithRenderer:(MilkRenderer*)renderer;
- (void)showWindow;

@end

#pragma once

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

// Posted on the main thread when a thumbnail is ready. userInfo["index"] is the preset's index.
extern NSString* const MilkThumbnailDidRenderNotification;

// Posted on the main thread after the cache is cleared.
extern NSString* const MilkThumbnailCacheDidClearNotification;

// A thumbnail of every preset, rendered by this app's own renderer on a background thread and
// cached as PNGs in ~/Library/Caches. A cached thumbnail is reused until its preset file changes.
@interface ThumbnailStore : NSObject

// The folder the thumbnails are cached in. Created if it does not exist.
+ (NSString*)cacheDirectory;

// Deletes every cached thumbnail, then posts MilkThumbnailCacheDidClearNotification.
+ (void)clearCache;

// How many thumbnails are cached and what they take on disk, as a sentence.
+ (NSString*)cacheSummary;

// `paths` are the presets' .milk files, in the order the indices refer to.
- (instancetype)initWithPresetPaths:(NSArray<NSString*>*)paths;

// Starts rendering the presets that have no cached thumbnail. Calling it again does nothing.
- (void)start;

// The thumbnail for a preset, or nil while it has not been rendered.
- (nullable NSImage*)imageAtIndex:(NSInteger)index;

@property (nonatomic, readonly) NSInteger readyCount;
@property (nonatomic, readonly) NSInteger totalCount;

@end

NS_ASSUME_NONNULL_END

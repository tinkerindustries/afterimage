#pragma once

#import <MetalKit/MetalKit.h>

NS_ASSUME_NONNULL_BEGIN

// Posted on the default centre after a preset loads, whether by key, menu or the preset list.
extern NSString* const MilkPresetDidChangeNotification;

@interface MilkRenderer : NSObject <MTKViewDelegate>

// For the window: reads shaders.metal from the app bundle, finds the presets and listens to
// system audio.
- (nullable instancetype)initWithView:(MTKView*)view error:(NSString* _Nullable* _Nullable)error;

// For offline rendering: synthesised audio, a fixed random seed, no preset list.
- (nullable instancetype)initWithDevice:(id<MTLDevice>)device
                           outputFormat:(MTLPixelFormat)format
                           shaderSource:(NSString*)source
                                  error:(NSString* _Nullable* _Nullable)error;

- (BOOL)loadPresetAtPath:(NSString*)path;

// For offline rendering: plays `count` stereo samples at 48 kHz from sample `start` instead of the
// synthesiser. The samples before `start` (up to two seconds) settle the band levels.
- (void)useAudioLeft:(const float*)left right:(const float*)right count:(size_t)count start:(size_t)start;

// Encodes one frame of `dt` seconds with a feedback image of `size` pixels, composited into
// `target` (a texture in the output format).
- (void)renderFrame:(float)dt internalSize:(CGSize)size target:(id<MTLTexture>)target commands:(id<MTLCommandBuffer>)commands;

- (void)setMotion:(int)motion;
- (void)nextPreset;
- (void)previousPreset;
- (void)nextWaveMode;
- (void)toggleColourCycle;
- (void)toggleWrap;
- (void)toggleAudioSource;
- (void)clear;

// Every preset the app can step through, named by its path under the presets folder without the
// .milk extension, in stepping order.
@property (nonatomic, readonly) NSArray<NSString*>* presetNames;
// The index in presetNames of the preset playing now, or -1 when none has been chosen.
@property (nonatomic, readonly) NSInteger currentPresetIndex;
- (void)selectPresetAtIndex:(NSInteger)index;
// Starts the next frame from random noise instead of black, the way a preset in MilkDrop starts
// from whatever image came before it.
- (void)seedNoise;

// One line naming the motion, wave mode and audio source, for the window title.
@property (nonatomic, readonly) NSString* status;

@end

NS_ASSUME_NONNULL_END

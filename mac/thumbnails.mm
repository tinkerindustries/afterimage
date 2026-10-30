#import "thumbnails.h"

#import "renderer.h"

#import <ImageIO/ImageIO.h>
#import <Metal/Metal.h>

#include <functional>
#include <string>

NSString* const MilkThumbnailDidRenderNotification = @"MilkThumbnailDidRender";
NSString* const MilkThumbnailCacheDidClearNotification = @"MilkThumbnailCacheDidClear";

namespace {

constexpr int SIZE = 256;
constexpr int FRAMES = 240;
// Raise this when the renderer changes enough that old thumbnails should be redrawn.
constexpr int VERSION = 1;

} // namespace

@implementation ThumbnailStore {
    NSArray<NSString*>* m_paths;
    NSArray<NSString*>* m_files;     // each preset's cache file
    NSMutableIndexSet* m_ready;      // presets whose cache file exists; read and written on the main thread
    NSCache<NSNumber*, NSImage*>* m_images;
    dispatch_queue_t m_queue;
    BOOL m_started;
}

+ (NSString*)cacheDirectory {
    NSURL* caches = [NSFileManager.defaultManager URLsForDirectory:NSCachesDirectory inDomains:NSUserDomainMask].firstObject;
    NSString* dir = [[caches.path stringByAppendingPathComponent:@"com.geoffrich.milk"] stringByAppendingPathComponent:@"thumbnails"];
    [NSFileManager.defaultManager createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    return dir;
}

+ (void)clearCache {
    NSString* dir = [self cacheDirectory];
    for (NSString* name in [NSFileManager.defaultManager contentsOfDirectoryAtPath:dir error:nil]) {
        if ([name.pathExtension isEqualToString:@"png"]) {
            [NSFileManager.defaultManager removeItemAtPath:[dir stringByAppendingPathComponent:name] error:nil];
        }
    }
    [[NSNotificationCenter defaultCenter] postNotificationName:MilkThumbnailCacheDidClearNotification object:nil];
}

+ (NSString*)cacheSummary {
    NSString* dir = [self cacheDirectory];
    NSUInteger count = 0;
    unsigned long long bytes = 0;
    for (NSString* name in [NSFileManager.defaultManager contentsOfDirectoryAtPath:dir error:nil]) {
        if (![name.pathExtension isEqualToString:@"png"]) continue;
        ++count;
        bytes += [NSFileManager.defaultManager attributesOfItemAtPath:[dir stringByAppendingPathComponent:name] error:nil].fileSize;
    }
    return [NSString stringWithFormat:@"%lu thumbnails, %@", (unsigned long)count,
                                      [NSByteCountFormatter stringFromByteCount:static_cast<long long>(bytes)
                                                                     countStyle:NSByteCountFormatterCountStyleFile]];
}

- (instancetype)initWithPresetPaths:(NSArray<NSString*>*)paths {
    if ((self = [super init])) {
        m_paths = [paths copy];
        m_images = [NSCache new];
        m_images.countLimit = 400;
        m_ready = [NSMutableIndexSet indexSet];
        m_queue = dispatch_queue_create("milk.thumbnails", dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0));

        NSString* dir = [ThumbnailStore cacheDirectory];
        NSMutableArray<NSString*>* files = [NSMutableArray arrayWithCapacity:paths.count];
        for (NSUInteger i = 0; i < paths.count; ++i) {
            NSDictionary* attributes = [NSFileManager.defaultManager attributesOfItemAtPath:paths[i] error:nil];
            const double modified = attributes.fileModificationDate.timeIntervalSince1970;
            const std::string key = std::string(paths[i].UTF8String) + "|" + std::to_string(modified) + "|" + std::to_string(VERSION);
            NSString* name = [NSString stringWithFormat:@"%016zx.png", std::hash<std::string>{}(key)];
            NSString* file = [dir stringByAppendingPathComponent:name];
            [files addObject:file];
            if ([NSFileManager.defaultManager fileExistsAtPath:file]) [m_ready addIndex:i];
        }
        m_files = files;
    }
    return self;
}

- (NSInteger)readyCount { return static_cast<NSInteger>(m_ready.count); }

- (NSInteger)totalCount { return static_cast<NSInteger>(m_paths.count); }

- (nullable NSImage*)imageAtIndex:(NSInteger)index {
    if (index < 0 || index >= static_cast<NSInteger>(m_paths.count) || ![m_ready containsIndex:index]) return nil;
    NSImage* image = [m_images objectForKey:@(index)];
    if (image == nil) {
        image = [[NSImage alloc] initWithContentsOfFile:m_files[index]];
        if (image) [m_images setObject:image forKey:@(index)];
    }
    return image;
}

- (void)start {
    if (m_started) return;
    m_started = YES;
    NSMutableArray<NSNumber*>* missing = [NSMutableArray array];
    for (NSUInteger i = 0; i < m_paths.count; ++i) {
        if (![m_ready containsIndex:i]) [missing addObject:@(i)];
    }
    if (missing.count == 0) return;
    NSArray<NSString*>* paths = m_paths;
    NSArray<NSString*>* files = m_files;
    __weak ThumbnailStore* weakSelf = self;
    dispatch_async(m_queue, ^{
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        NSString* shaderPath = [NSBundle.mainBundle pathForResource:@"shaders" ofType:@"metal"];
        NSString* source = shaderPath ? [NSString stringWithContentsOfFile:shaderPath encoding:NSUTF8StringEncoding error:nil] : nil;
        if (device == nil || source == nil) return;
        id<MTLCommandQueue> queue = [device newCommandQueue];
        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                        width:SIZE
                                                                                       height:SIZE
                                                                                    mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModeShared;
        id<MTLTexture> target = [device newTextureWithDescriptor:desc];
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        std::vector<uint8_t> pixels(SIZE * SIZE * 4);
        for (NSNumber* number in missing) {
            if (weakSelf == nil) break;
            @autoreleasepool {
                const NSInteger index = number.integerValue;
                // A fresh renderer per preset, so nothing carries over from the one before.
                MilkRenderer* renderer = [[MilkRenderer alloc] initWithDevice:device
                                                                 outputFormat:MTLPixelFormatBGRA8Unorm
                                                                 shaderSource:source
                                                                        error:nil];
                if (renderer == nil || ![renderer loadPresetAtPath:paths[index]]) continue;
                [renderer seedNoise];
                for (int f = 0; f < FRAMES; ++f) {
                    id<MTLCommandBuffer> commands = [queue commandBuffer];
                    [renderer renderFrame:1.0f / 60.0f internalSize:CGSizeMake(SIZE, SIZE) target:target commands:commands];
                    [commands commit];
                    if (f == FRAMES - 1) [commands waitUntilCompleted];
                }
                [target getBytes:pixels.data() bytesPerRow:SIZE * 4 fromRegion:MTLRegionMake2D(0, 0, SIZE, SIZE) mipmapLevel:0];
                CGContextRef context = CGBitmapContextCreate(pixels.data(), SIZE, SIZE, 8, SIZE * 4, space,
                                                             kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little);
                CGImageRef image = CGBitmapContextCreateImage(context);
                NSURL* url = [NSURL fileURLWithPath:files[index]];
                CGImageDestinationRef out = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
                bool written = false;
                if (out) {
                    CGImageDestinationAddImage(out, image, nullptr);
                    written = CGImageDestinationFinalize(out);
                    CFRelease(out);
                }
                CGImageRelease(image);
                CGContextRelease(context);
                if (!written) continue;
                dispatch_async(dispatch_get_main_queue(), ^{
                    ThumbnailStore* store = weakSelf;
                    if (store == nil) return;
                    [store->m_ready addIndex:index];
                    [[NSNotificationCenter defaultCenter] postNotificationName:MilkThumbnailDidRenderNotification
                                                                        object:store
                                                                      userInfo:@{@"index" : @(index)}];
                });
            }
        }
        CGColorSpaceRelease(space);
    });
}

@end

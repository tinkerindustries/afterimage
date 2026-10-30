// Renders presets offline and writes a contact sheet that puts each preset's MilkDrop preview
// (the .jpg beside the .milk) next to this project's frame after the given number of seconds.
// Audio comes from the synthesiser, so a run is repeatable.
//
//   milk_render <sheet.png> <seconds> <size> <preset.milk>...
//
// Each preset's line on stdout ends with the frame's mean brightness, its contrast (standard
// deviation of brightness) and its mean colour saturation, all 0..1. Pass "-" as the sheet to
// print only those lines.

#import "renderer.h"

#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <Metal/Metal.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr int CELL = 256;
constexpr int LABEL = 18;
constexpr int GAP = 8;
constexpr int COLUMNS = 3;

CGImageRef loadImage(NSString* path) {
    CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:path], nullptr);
    if (source == nullptr) return nullptr;
    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    return image;
}

struct FrameStats {
    double mean = 0.0;
    double contrast = 0.0;
    double saturation = 0.0;
};

FrameStats measure(id<MTLTexture> texture) {
    const size_t w = texture.width, h = texture.height;
    std::vector<uint8_t> bgra(w * h * 4);
    [texture getBytes:bgra.data() bytesPerRow:w * 4 fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
    double sum = 0.0, sumSq = 0.0, sat = 0.0;
    const size_t n = w * h;
    for (size_t i = 0; i < n; ++i) {
        const double b = bgra[i * 4] / 255.0, g = bgra[i * 4 + 1] / 255.0, r = bgra[i * 4 + 2] / 255.0;
        const double luma = 0.299 * r + 0.587 * g + 0.114 * b;
        const double hi = std::max({r, g, b}), lo = std::min({r, g, b});
        sum += luma;
        sumSq += luma * luma;
        sat += hi > 0.0 ? (hi - lo) / hi : 0.0;
    }
    FrameStats out;
    out.mean = sum / n;
    out.contrast = std::sqrt(std::max(0.0, sumSq / n - out.mean * out.mean));
    out.saturation = sat / n;
    return out;
}

CGImageRef imageFromTexture(id<MTLTexture> texture) {
    const size_t w = texture.width, h = texture.height;
    std::vector<uint8_t> bgra(w * h * 4);
    [texture getBytes:bgra.data() bytesPerRow:w * 4 fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
    CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(bgra.data(), w, h, 8, w * 4, space,
                                                 kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little);
    CGImageRef image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    CGColorSpaceRelease(space);
    return image;
}

void drawLabel(CGContextRef context, NSString* text, CGFloat x, CGFloat y) {
    NSDictionary* attributes = @{
        (__bridge NSString*)kCTFontAttributeName: (__bridge id)CTFontCreateWithName(CFSTR("Helvetica"), 12, nullptr),
        (__bridge NSString*)kCTForegroundColorFromContextAttributeName: @YES,
    };
    NSAttributedString* string = [[NSAttributedString alloc] initWithString:text attributes:attributes];
    CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)string);
    CGContextSetRGBFillColor(context, 0.85, 0.85, 0.85, 1.0);
    CGContextSetTextPosition(context, x, y);
    CTLineDraw(line, context);
    CFRelease(line);
}

} // namespace

int main(int argc, char** argv) {
    @autoreleasepool {
        if (argc < 5) {
            std::fprintf(stderr, "usage: %s <sheet.png> <seconds> <size> <preset.milk>...\n", argv[0]);
            return 2;
        }
        NSString* outPath = @(argv[1]);
        const bool writeSheet = std::string(argv[1]) != "-";
        const int frames = std::max(1, static_cast<int>(std::atof(argv[2]) * 60.0));
        const int size = std::max(16, std::atoi(argv[3]));
        std::vector<std::string> presets(argv + 4, argv + argc);

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        id<MTLCommandQueue> queue = [device newCommandQueue];
        NSString* source = [NSString stringWithContentsOfFile:@MILK_SOURCE_DIR "/mac/shaders.metal" encoding:NSUTF8StringEncoding error:nil];
        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                        width:size
                                                                                       height:size
                                                                                    mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        desc.storageMode = MTLStorageModeShared;
        id<MTLTexture> target = [device newTextureWithDescriptor:desc];

        const int rows = static_cast<int>((presets.size() + COLUMNS - 1) / COLUMNS);
        const int tileW = CELL * 2 + GAP;
        const int tileH = CELL + LABEL + GAP;
        const int sheetW = COLUMNS * tileW + GAP;
        const int sheetH = rows * tileH + GAP;
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef sheet = nullptr;
        if (writeSheet) {
            sheet = CGBitmapContextCreate(nullptr, sheetW, sheetH, 8, 0, space, kCGImageAlphaNoneSkipLast);
            CGContextSetRGBFillColor(sheet, 0.12, 0.12, 0.12, 1.0);
            CGContextFillRect(sheet, CGRectMake(0, 0, sheetW, sheetH));
            CGContextSetInterpolationQuality(sheet, kCGInterpolationHigh);
        }

        for (size_t i = 0; i < presets.size(); ++i) {
            NSString* path = @(presets[i].c_str());
            NSString* error = nil;
            MilkRenderer* renderer = [[MilkRenderer alloc] initWithDevice:device
                                                             outputFormat:MTLPixelFormatBGRA8Unorm
                                                             shaderSource:source
                                                                    error:&error];
            if (renderer == nil || ![renderer loadPresetAtPath:path]) {
                std::fprintf(stderr, "%s: could not load (%s)\n", presets[i].c_str(), error ? error.UTF8String : "preset");
                continue;
            }
            // In MilkDrop a preset starts from the previous preset's image. Noise stands in for it,
            // so presets that only transform what is already there have something to work on.
            [renderer seedNoise];
            for (int f = 0; f < frames; ++f) {
                id<MTLCommandBuffer> commands = [queue commandBuffer];
                [renderer renderFrame:1.0f / 60.0f internalSize:CGSizeMake(size, size) target:target commands:commands];
                [commands commit];
                if (f % 30 == 29 || f == frames - 1) [commands waitUntilCompleted];
            }
            const FrameStats stats = measure(target);
            std::printf("%zu\t%s\t%s\tmean=%.3f\tcontrast=%.3f\tsaturation=%.3f\n", i + 1, path.UTF8String,
                        renderer.status.UTF8String, stats.mean, stats.contrast, stats.saturation);
            std::fflush(stdout);
            if (!writeSheet) continue;

            const int col = static_cast<int>(i % COLUMNS);
            const int row = static_cast<int>(i / COLUMNS);
            const CGFloat x = GAP + col * tileW;
            const CGFloat top = sheetH - GAP - row * tileH;
            const CGRect previewRect = CGRectMake(x, top - LABEL - CELL, CELL, CELL);
            const CGRect oursRect = CGRectMake(x + CELL + GAP / 2, top - LABEL - CELL, CELL, CELL);
            CGImageRef preview = loadImage([path.stringByDeletingPathExtension stringByAppendingPathExtension:@"jpg"]);
            if (preview != nullptr) {
                CGContextDrawImage(sheet, previewRect, preview);
                CGImageRelease(preview);
            }
            CGImageRef ours = imageFromTexture(target);
            CGContextDrawImage(sheet, oursRect, ours);
            CGImageRelease(ours);
            NSString* name = path.lastPathComponent.stringByDeletingPathExtension;
            if (name.length > 60) name = [[name substringToIndex:57] stringByAppendingString:@"..."];
            drawLabel(sheet, [NSString stringWithFormat:@"%zu  %@", i + 1, name], x, top - LABEL + 4);
        }

        if (!writeSheet) {
            CGColorSpaceRelease(space);
            return 0;
        }
        CGImageRef image = CGBitmapContextCreateImage(sheet);
        CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:outPath],
                                                                     (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
        CGImageDestinationAddImage(dest, image, nullptr);
        const bool ok = CGImageDestinationFinalize(dest);
        CFRelease(dest);
        CGImageRelease(image);
        CGContextRelease(sheet);
        CGColorSpaceRelease(space);
        if (!ok) {
            std::fprintf(stderr, "could not write %s\n", outPath.UTF8String);
            return 1;
        }
        std::printf("wrote %s (left: MilkDrop preview, right: this renderer after %.1f s)\n", outPath.UTF8String, frames / 60.0);
    }
    return 0;
}

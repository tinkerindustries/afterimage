// Renders presets offline and writes a contact sheet that puts each preset's MilkDrop preview
// (the .jpg beside the .milk) next to this project's frame after the given number of seconds.
// Audio comes from the synthesiser, so a run is repeatable.
//
//   milk_render [--audio <file> [--audio-start <seconds>]] [--black-start] [--frames <dir> --root <dir>] <sheet.png> <seconds> <size> <preset.milk>...
//
// With --audio every preset hears the same stretch of the file (any format Core Audio reads),
// starting at --audio-start, in place of the synthesiser. --black-start begins from a black image
// instead of noise, as a fresh MilkDrop process does. --frames <dir> --root <dir> also writes
// each preset's frame to <dir>/<path under root, extension dropped, '/' as '__'>.png.
//
// Each preset's line on stdout ends with the frame's mean brightness, its contrast (standard
// deviation of brightness) and its mean colour saturation, all 0..1. Pass "-" as the sheet to
// print only those lines.

#import "renderer.h"

#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <ImageIO/ImageIO.h>
#import <Metal/Metal.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "audio_synth.h"

using Milk::SAMPLE_RATE;

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


struct AudioClip {
    std::vector<float> left, right;
    size_t start = 0;
};

// Decodes `seconds` of the file from `startSeconds`, preceded by up to two seconds of lead-in,
// as 48 kHz stereo floats. A mono file fills both channels.
bool loadAudio(const char* path, double startSeconds, double seconds, AudioClip& out) {
    ExtAudioFileRef file = nullptr;
    if (ExtAudioFileOpenURL((__bridge CFURLRef)[NSURL fileURLWithPath:@(path)], &file) != noErr) return false;
    AudioStreamBasicDescription format = {};
    format.mSampleRate = SAMPLE_RATE;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = 32;
    format.mBytesPerFrame = 8;
    format.mFramesPerPacket = 1;
    format.mBytesPerPacket = 8;
    if (ExtAudioFileSetProperty(file, kExtAudioFileProperty_ClientDataFormat, sizeof(format), &format) != noErr) {
        ExtAudioFileDispose(file);
        return false;
    }
    const double lead = std::min(startSeconds, 2.0);
    const SInt64 from = static_cast<SInt64>((startSeconds - lead) * SAMPLE_RATE);
    if (ExtAudioFileSeek(file, from) != noErr) {
        ExtAudioFileDispose(file);
        return false;
    }
    const size_t wanted = static_cast<size_t>((lead + seconds) * SAMPLE_RATE);
    std::vector<float> interleaved(wanted * 2);
    size_t got = 0;
    while (got < wanted) {
        UInt32 frames = static_cast<UInt32>(std::min<size_t>(wanted - got, 16384));
        AudioBufferList list;
        list.mNumberBuffers = 1;
        list.mBuffers[0].mNumberChannels = 2;
        list.mBuffers[0].mDataByteSize = frames * 8;
        list.mBuffers[0].mData = interleaved.data() + got * 2;
        if (ExtAudioFileRead(file, &frames, &list) != noErr || frames == 0) break;
        got += frames;
    }
    ExtAudioFileDispose(file);
    if (got == 0) return false;
    out.left.assign(wanted, 0.0f);
    out.right.assign(wanted, 0.0f);
    for (size_t i = 0; i < got; ++i) {
        out.left[i] = interleaved[i * 2];
        out.right[i] = interleaved[i * 2 + 1];
    }
    out.start = static_cast<size_t>(lead * SAMPLE_RATE);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    @autoreleasepool {
        const char* audioPath = nullptr;
        double audioStart = 0.0;
        bool blackStart = false;
        std::string framesDir, rootDir;
        int first = 1;
        while (first < argc && (std::string(argv[first]) == "--black-start" ||
                                (first + 1 < argc && (std::string(argv[first]).rfind("--audio", 0) == 0 ||
                                                      std::string(argv[first]) == "--frames" || std::string(argv[first]) == "--root")))) {
            if (std::string(argv[first]) == "--black-start") {
                blackStart = true;
                first += 1;
                continue;
            }
            if (std::string(argv[first]) == "--frames") framesDir = argv[first + 1];
            else if (std::string(argv[first]) == "--root") rootDir = argv[first + 1];
            else if (std::string(argv[first]) == "--audio") audioPath = argv[first + 1];
            else if (std::string(argv[first]) == "--audio-start") audioStart = std::atof(argv[first + 1]);
            else break;
            first += 2;
        }
        if (argc - first < 4) {
            std::fprintf(stderr, "usage: %s [--audio <file> [--audio-start <seconds>]] [--black-start] [--frames <dir> --root <dir>] <sheet.png> <seconds> <size> <preset.milk>...\n", argv[0]);
            return 2;
        }
        argv += first - 1;
        argc -= first - 1;
        NSString* outPath = @(argv[1]);
        const bool writeSheet = std::string(argv[1]) != "-";
        const int frames = std::max(1, static_cast<int>(std::atof(argv[2]) * 60.0));
        const int size = std::max(16, std::atoi(argv[3]));
        std::vector<std::string> presets(argv + 4, argv + argc);

        AudioClip clip;
        if (audioPath != nullptr && !loadAudio(audioPath, audioStart, frames / 60.0 + 1.0, clip)) {
            std::fprintf(stderr, "could not read audio from %s at %.1f s\n", audioPath, audioStart);
            return 1;
        }

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
            if (!blackStart) [renderer seedNoise];
            if (audioPath != nullptr) {
                [renderer useAudioLeft:clip.left.data() right:clip.right.data() count:clip.left.size() start:clip.start];
            }
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
            if (!framesDir.empty()) {
                std::string name = presets[i];
                if (!rootDir.empty() && name.rfind(rootDir, 0) == 0) name = name.substr(rootDir.size());
                while (!name.empty() && name.front() == '/') name.erase(name.begin());
                name = name.substr(0, name.size() - 5);
                for (size_t at = name.find('/'); at != std::string::npos; at = name.find('/', at)) name.replace(at, 1, "__");
                CGImageRef frame = imageFromTexture(target);
                NSURL* url = [NSURL fileURLWithPath:[@(framesDir.c_str()) stringByAppendingPathComponent:[@(name.c_str()) stringByAppendingString:@".png"]]];
                CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
                CGImageDestinationAddImage(dest, frame, nullptr);
                CGImageDestinationFinalize(dest);
                CFRelease(dest);
                CGImageRelease(frame);
            }
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

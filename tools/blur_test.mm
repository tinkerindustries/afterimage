// Runs the app's blur shaders on the GPU and compares each blur level with a model of MilkDrop's
// blur under Direct3D 9, which samples at whole pixel coordinates and flips the image on every
// pass. Exits non-zero on a failure.

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace {

int failures = 0;

constexpr float BLUR_W[8] = {4.0f, 3.8f, 3.5f, 2.9f, 1.9f, 1.2f, 0.7f, 0.3f};

struct Image {
    int w = 0, h = 0;
    std::vector<float> px;
    float at(int x, int y) const { return px[std::clamp(y, 0, h - 1) * w + std::clamp(x, 0, w - 1)]; }
    float sample(float u, float v) const {
        const float x = u * w - 0.5f, y = v * h - 0.5f;
        const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
        const float fx = x - x0, fy = y - y0;
        return at(x0, y0) * (1 - fx) * (1 - fy) + at(x0 + 1, y0) * fx * (1 - fy) + at(x0, y0 + 1) * (1 - fx) * fy +
               at(x0 + 1, y0 + 1) * fx * fy;
    }
};

struct BlurUniforms {
    float scale = 1.0f, bias = 0.0f, edgeDarken = 0.0f, pad = 0.0f;
    float destTexel[2] = {0.0f, 0.0f};
    float pad2[2] = {0.0f, 0.0f};
};

std::array<std::array<int, 2>, 6> blurSizes(int width, int height) {
    std::array<std::array<int, 2>, 6> out{};
    int w = width, h = height;
    for (int i = 0; i < 6; ++i) {
        if (!(i & 1) || i < 2) {
            w = std::max(16, w / 2);
            h = std::max(16, h / 2);
        }
        out[i] = {((w + 3) / 16) * 16, ((h + 3) / 4) * 4};
    }
    return out;
}

// One pass as Direct3D 9 runs it: pixel (i, j) from the top reads uv (i / w, 1 - j / h).
Image d3d9Pass(const Image& src, int w, int h, bool horizontal) {
    Image out{w, h, std::vector<float>(static_cast<size_t>(w) * h)};
    const float tx = 1.0f / src.w, ty = 1.0f / src.h;
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            const float u = static_cast<float>(i) / w, v = 1.0f - static_cast<float>(j) / h;
            float sum = 0.0f;
            if (horizontal) {
                const float w1 = BLUR_W[0] + BLUR_W[1], w2 = BLUR_W[2] + BLUR_W[3], w3 = BLUR_W[4] + BLUR_W[5], w4 = BLUR_W[6] + BLUR_W[7];
                const float ws[4] = {w1, w2, w3, w4};
                const float ds[4] = {2 * BLUR_W[1] / w1, 2 + 2 * BLUR_W[3] / w2, 4 + 2 * BLUR_W[5] / w3, 6 + 2 * BLUR_W[7] / w4};
                for (int k = 0; k < 4; ++k) {
                    sum += (src.sample(u + tx + ds[k] * tx, v + ty) + src.sample(u + tx - ds[k] * tx, v + ty)) * ws[k];
                }
                sum *= 0.5f / (w1 + w2 + w3 + w4);
            } else {
                const float w1 = BLUR_W[0] + BLUR_W[1] + BLUR_W[2] + BLUR_W[3], w2 = BLUR_W[4] + BLUR_W[5] + BLUR_W[6] + BLUR_W[7];
                const float ws[2] = {w1, w2};
                const float ds[2] = {2 * ((BLUR_W[2] + BLUR_W[3]) / w1), 2 + 2 * ((BLUR_W[6] + BLUR_W[7]) / w2)};
                for (int k = 0; k < 2; ++k) {
                    sum += (src.sample(u + tx, v + ds[k] * ty) + src.sample(u + tx, v - ds[k] * ty)) * ws[k];
                }
                sum *= 1.0f / ((w1 + w2) * 2);
            }
            out.px[static_cast<size_t>(j) * w + i] = sum;
        }
    }
    return out;
}

id<MTLTexture> makeTexture(id<MTLDevice> device, int w, int h) {
    MTLTextureDescriptor* d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
                                                                                 width:w
                                                                                height:h
                                                                             mipmapped:NO];
    d.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
    d.storageMode = MTLStorageModeShared;
    return [device newTextureWithDescriptor:d];
}

Image readBack(id<MTLTexture> t) {
    Image out{static_cast<int>(t.width), static_cast<int>(t.height), {}};
    out.px.resize(static_cast<size_t>(out.w) * out.h);
    [t getBytes:out.px.data() bytesPerRow:out.w * sizeof(float) fromRegion:MTLRegionMake2D(0, 0, out.w, out.h) mipmapLevel:0];
    return out;
}

std::array<float, 2> centroid(const Image& img) {
    double sx = 0, sy = 0, s = 0;
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            const double v = img.px[static_cast<size_t>(y) * img.w + x];
            sx += (x + 0.5) / img.w * v;
            sy += (y + 0.5) / img.h * v;
            s += v;
        }
    }
    return {static_cast<float>(sx / s), static_cast<float>(sy / s)};
}

void check(id<MTLDevice> device, id<MTLRenderPipelineState> horizontal, id<MTLRenderPipelineState> vertical,
           id<MTLSamplerState> sampler, const Image& input, const char* label, bool compareCentroid) {
    id<MTLTexture> source = makeTexture(device, input.w, input.h);
    [source replaceRegion:MTLRegionMake2D(0, 0, input.w, input.h) mipmapLevel:0 withBytes:input.px.data() bytesPerRow:input.w * sizeof(float)];
    id<MTLCommandQueue> queue = [device newCommandQueue];
    const auto sizes = blurSizes(input.w, input.h);
    Image reference = input;
    id<MTLTexture> gpuSource = source;
    for (int pass = 0; pass < 6; ++pass) {
        const bool isHorizontal = pass % 2 == 0;
        id<MTLTexture> target = makeTexture(device, sizes[pass][0], sizes[pass][1]);
        id<MTLCommandBuffer> commands = [queue commandBuffer];
        MTLRenderPassDescriptor* rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = target;
        rp.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:rp];
        BlurUniforms u;
        u.destTexel[0] = 1.0f / sizes[pass][0];
        u.destTexel[1] = 1.0f / sizes[pass][1];
        [encoder setRenderPipelineState:isHorizontal ? horizontal : vertical];
        [encoder setFragmentTexture:gpuSource atIndex:0];
        [encoder setFragmentSamplerState:sampler atIndex:0];
        [encoder setFragmentBytes:&u length:sizeof(u) atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [encoder endEncoding];
        [commands commit];
        [commands waitUntilCompleted];
        gpuSource = target;
        reference = d3d9Pass(reference, sizes[pass][0], sizes[pass][1], isHorizontal);
        if (isHorizontal) continue;

        // After its vertical pass a level has been flipped twice, so it is upright in both.
        const Image ours = readBack(target);
        const int level = pass / 2 + 1;
        float worst = 0.0f;
        for (size_t i = 0; i < ours.px.size(); ++i) worst = std::max(worst, std::fabs(ours.px[i] - reference.px[i]));
        const float peak = *std::max_element(reference.px.begin(), reference.px.end());
        if (worst > 0.02f * peak) {
            ++failures;
            std::printf("FAIL %s blur%d differs from Direct3D 9 by %.4f (peak %.4f)\n", label, level, worst, peak);
        }
        if (compareCentroid) {
            const auto a = centroid(ours), b = centroid(reference);
            const float dx = (a[0] - b[0]) * input.w, dy = (a[1] - b[1]) * input.h;
            if (std::fabs(dx) > 0.1f || std::fabs(dy) > 0.1f) {
                ++failures;
                std::printf("FAIL %s blur%d sits %.2f, %.2f px from Direct3D 9's\n", label, level, dx, dy);
            }
        }
    }
}

} // namespace

int main() {
    @autoreleasepool {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        NSString* source = [NSString stringWithContentsOfFile:@MILK_SOURCE_DIR "/mac/shaders.metal" encoding:NSUTF8StringEncoding error:nil];
        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
        if (library == nil) {
            std::printf("FAIL shaders.metal: %s\n", error.localizedDescription.UTF8String);
            return 1;
        }
        auto pipeline = [&](NSString* fragment) {
            MTLRenderPipelineDescriptor* d = [MTLRenderPipelineDescriptor new];
            d.vertexFunction = [library newFunctionWithName:@"fullscreenVertex"];
            d.fragmentFunction = [library newFunctionWithName:fragment];
            d.colorAttachments[0].pixelFormat = MTLPixelFormatR32Float;
            NSError* e = nil;
            return [device newRenderPipelineStateWithDescriptor:d error:&e];
        };
        id<MTLRenderPipelineState> horizontal = pipeline(@"blurHorizontalFragment");
        id<MTLRenderPipelineState> vertical = pipeline(@"blurVerticalFragment");
        MTLSamplerDescriptor* sd = [MTLSamplerDescriptor new];
        sd.minFilter = MTLSamplerMinMagFilterLinear;
        sd.magFilter = MTLSamplerMinMagFilterLinear;
        sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
        sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
        id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sd];

        for (const auto& size : {std::array<int, 2>{512, 512}, std::array<int, 2>{600, 450}}) {
            Image dot{size[0], size[1], std::vector<float>(static_cast<size_t>(size[0]) * size[1], 0.0f)};
            dot.px[static_cast<size_t>(size[1] / 2 + 5) * size[0] + size[0] / 2 - 7] = 1.0f;
            char label[64];
            std::snprintf(label, sizeof(label), "dot %dx%d", size[0], size[1]);
            check(device, horizontal, vertical, sampler, dot, label, true);

            std::mt19937 rng(3);
            std::uniform_real_distribution<float> unit(0.0f, 1.0f);
            Image noise{size[0], size[1], std::vector<float>(static_cast<size_t>(size[0]) * size[1])};
            for (float& v : noise.px) v = unit(rng);
            std::snprintf(label, sizeof(label), "noise %dx%d", size[0], size[1]);
            check(device, horizontal, vertical, sampler, noise, label, false);
        }
        std::printf("%d failures\n", failures);
        return failures == 0 ? 0 : 1;
    }
}

#pragma once

#include "milk_shader.h"
#include "texture_bank.h"

#import <Metal/Metal.h>

#include <array>
#include <map>
#include <string>
#include <vector>

namespace Milk {

// The values a frame gives the shader header's uniforms. Matrices are the HLSL float4x3 rows,
// m[i * 3 + j] for row i and column j.
struct ShaderFrameValues {
    std::map<std::string, std::array<float, 4>> vectors;
    std::map<std::string, std::array<float, 12>> matrices;
};

// One preset's warp and comp shaders, compiled for Metal, with the textures and samplers they
// read. The renderer supplies the images that change every frame: sampler_main and blur1..3.
class PresetGpu {
public:
    PresetGpu(id<MTLDevice> device, id<MTLFunction> warpVertex, id<MTLFunction> compVertex,
              MTLPixelFormat feedbackFormat, MTLPixelFormat drawableFormat, TextureBank& bank,
              const std::string& warpCode, const std::string& compCode, uint32_t seed, bool milkwave = false);
    // Milkwave's FFT texture, which sampler_fft reads when the shaders were built for Milkwave.
    void setFftTexture(id<MTLTexture> texture) { m_fft = texture; }

    bool hasWarp() const { return m_warp.pipeline != nil; }
    bool hasComp() const { return m_comp.pipeline != nil; }
    // The highest blur level either shader samples, 0 to 3.
    int blurLevels() const { return m_blurLevels; }
    const std::vector<std::string>& errors() const { return m_errors; }

    void bindWarp(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3], bool wrapMain,
                  const ShaderFrameValues& values);
    // `flipAlpha` draws the comp over what is there by one minus the vertex alpha, as MilkDrop
    // does when blending from a preset with a comp shader to one without.
    void bindComp(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3], bool wrapMain,
                  const ShaderFrameValues& values, bool flipAlpha = false);

private:
    enum Source { SOURCE_MAIN, SOURCE_BLUR1, SOURCE_BLUR2, SOURCE_BLUR3, SOURCE_FFT, SOURCE_TEXTURE };

    struct Binding {
        uint32_t textureIndex = 0;
        uint32_t samplerIndex = 0;
        Source source = SOURCE_TEXTURE;
        id<MTLTexture> texture = nil;
        TextureBank::Lookup lookup;
    };

    struct Stage {
        TranslatedShader shader;
        id<MTLRenderPipelineState> pipeline = nil;
        id<MTLRenderPipelineState> flippedPipeline = nil;
        std::vector<Binding> bindings;
        std::map<std::string, std::array<float, 4>> texsizes;
        std::vector<uint8_t> uniforms;
        // A sampler named a texture MilkDrop could not load. MilkDrop then binds no constants for
        // the shader, so they stay zero, and that sampler and the ones after it read black.
        bool missingTexture = false;
    };

    void build(Stage& stage, ShaderStage kind, const std::string& code, id<MTLFunction> vertex, MTLPixelFormat format,
               bool fallback = false);
    void bind(Stage& stage, id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3],
              bool wrapMain, const ShaderFrameValues& values, bool flipAlpha = false);

    id<MTLDevice> m_device;
    TextureBank& m_bank;
    uint32_t m_seed;
    bool m_milkwave;
    id<MTLTexture> m_fft = nil;
    Stage m_warp;
    Stage m_comp;
    int m_blurLevels = 0;
    std::vector<std::string> m_errors;
};

} // namespace Milk

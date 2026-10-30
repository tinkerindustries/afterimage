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
              const std::string& warpCode, const std::string& compCode, uint32_t seed);

    bool hasWarp() const { return m_warp.pipeline != nil; }
    bool hasComp() const { return m_comp.pipeline != nil; }
    // The highest blur level either shader samples, 0 to 3.
    int blurLevels() const { return m_blurLevels; }
    const std::vector<std::string>& errors() const { return m_errors; }

    void bindWarp(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3], bool wrapMain,
                  const ShaderFrameValues& values);
    void bindComp(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3], bool wrapMain,
                  const ShaderFrameValues& values);

private:
    enum Source { SOURCE_MAIN, SOURCE_BLUR1, SOURCE_BLUR2, SOURCE_BLUR3, SOURCE_TEXTURE };

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
        std::vector<Binding> bindings;
        std::map<std::string, std::array<float, 4>> texsizes;
        std::vector<uint8_t> uniforms;
    };

    void build(Stage& stage, ShaderStage kind, const std::string& code, id<MTLFunction> vertex, MTLPixelFormat format);
    void bind(Stage& stage, id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3],
              bool wrapMain, const ShaderFrameValues& values);

    id<MTLDevice> m_device;
    TextureBank& m_bank;
    uint32_t m_seed;
    Stage m_warp;
    Stage m_comp;
    int m_blurLevels = 0;
    std::vector<std::string> m_errors;
};

} // namespace Milk

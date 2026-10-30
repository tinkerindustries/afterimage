#import "preset_gpu.h"

#include <algorithm>
#include <cstring>

namespace Milk {

PresetGpu::PresetGpu(id<MTLDevice> device, id<MTLFunction> warpVertex, id<MTLFunction> compVertex,
                     MTLPixelFormat feedbackFormat, MTLPixelFormat drawableFormat, TextureBank& bank,
                     const std::string& warpCode, const std::string& compCode, uint32_t seed)
    : m_device(device), m_bank(bank), m_seed(seed) {
    if (warpCode.find("shader_body") != std::string::npos) build(m_warp, ShaderStage::WARP, warpCode, warpVertex, feedbackFormat);
    if (compCode.find("shader_body") != std::string::npos) build(m_comp, ShaderStage::COMP, compCode, compVertex, drawableFormat);
}

void PresetGpu::build(Stage& stage, ShaderStage kind, const std::string& code, id<MTLFunction> vertex, MTLPixelFormat format,
                      bool fallback) {
    const char* label = kind == ShaderStage::WARP ? "warp" : "comp";
    std::string error;
    if (!translateShader(kind, code, stage.shader, error)) {
        m_errors.push_back(std::string(label) + " shader: " + error.substr(0, error.find('\n')));
        return;
    }
    // MilkDrop cannot load a shader that samples a texture it does not have, and uses its plain
    // default shader for that stage instead.
    if (!fallback) {
        for (const TranslatedShader::Sampler& s : stage.shader.samplers) {
            const TextureBank::Lookup lookup = TextureBank::parse(s.name);
            const bool builtIn = lookup.name == "main" || lookup.name == "blur1" || lookup.name == "blur2" || lookup.name == "blur3";
            if (!builtIn && !m_bank.has(lookup.name)) {
                m_errors.push_back(std::string(label) + " shader: no texture named " + lookup.name + ", using the default shader");
                stage = Stage{};
                const char* plain = kind == ShaderStage::WARP
                                        ? "shader_body\n{\n    ret = tex2D( sampler_main, uv ).xyz;\n    ret -= 0.004;\n}\n"
                                        : "shader_body\n{\n    ret = tex2D(sampler_main, uv).xyz;\n}\n";
                build(stage, kind, plain, vertex, format, true);
                return;
            }
        }
    }
    NSError* nsError = nil;
    // The translated shader turns NaN into 0 as Direct3D 9 does, which fast math would remove.
    MTLCompileOptions* options = [MTLCompileOptions new];
    if (@available(macOS 15.0, *)) options.mathMode = MTLMathModeSafe;
    else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        options.fastMathEnabled = NO;
#pragma clang diagnostic pop
    }
    id<MTLLibrary> library = [m_device newLibraryWithSource:@(stage.shader.msl.c_str()) options:options error:&nsError];
    id<MTLFunction> fragment = [library newFunctionWithName:@(stage.shader.entryPoint.c_str())];
    if (fragment == nil) {
        m_errors.push_back(std::string(label) + " shader: Metal compile failed: " +
                           (nsError ? nsError.localizedDescription.UTF8String : "no entry point"));
        return;
    }
    MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
    desc.vertexFunction = vertex;
    desc.fragmentFunction = fragment;
    desc.colorAttachments[0].pixelFormat = format;
    stage.pipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];
    if (stage.pipeline == nil) {
        m_errors.push_back(std::string(label) + " shader: pipeline failed: " + nsError.localizedDescription.UTF8String);
        return;
    }

    for (const TranslatedShader::Sampler& s : stage.shader.samplers) {
        Binding b;
        b.textureIndex = s.texture;
        b.samplerIndex = s.sampler;
        b.lookup = TextureBank::parse(s.name);
        if (b.lookup.name == "main") b.source = SOURCE_MAIN;
        else if (b.lookup.name == "blur1") b.source = SOURCE_BLUR1;
        else if (b.lookup.name == "blur2") b.source = SOURCE_BLUR2;
        else if (b.lookup.name == "blur3") b.source = SOURCE_BLUR3;
        else b.texture = m_bank.texture(b.lookup.name, m_seed);
        if (b.texture != nil && (b.texture.textureType == MTLTextureType3D) != s.volume) {
            b.texture = m_bank.texture(s.volume ? "noisevol_hq" : "noise_hq", m_seed);
        }
        if (b.source >= SOURCE_BLUR1 && b.source <= SOURCE_BLUR3) {
            m_blurLevels = std::max(m_blurLevels, static_cast<int>(b.source - SOURCE_BLUR1) + 1);
            if (!b.lookup.prefixed) b.lookup.wrap = false;
        }
        stage.bindings.push_back(b);
    }
    for (const auto& [name, offset] : stage.shader.uniformOffsets) {
        if (name.rfind("texsize_", 0) != 0) continue;
        id<MTLTexture> tex = m_bank.texture(name.substr(8), m_seed);
        const float w = tex ? static_cast<float>(tex.width) : 1.0f;
        const float h = tex ? static_cast<float>(tex.height) : 1.0f;
        stage.texsizes[name] = {w, h, 1.0f / w, 1.0f / h};
    }
    stage.uniforms.assign(stage.shader.uniformSize, 0);
}

void PresetGpu::bindWarp(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3], bool wrapMain,
                         const ShaderFrameValues& values) {
    bind(m_warp, encoder, main, blurs, wrapMain, values);
}

void PresetGpu::bindComp(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3], bool wrapMain,
                         const ShaderFrameValues& values) {
    bind(m_comp, encoder, main, blurs, wrapMain, values);
}

// The header's float4x3 arrives as a Metal float3x4: three float4 columns, so HLSL element
// [i][j] sits at byte j * 16 + i * 4.
void PresetGpu::bind(Stage& stage, id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3],
                     bool wrapMain, const ShaderFrameValues& values) {
    [encoder setRenderPipelineState:stage.pipeline];
    uint8_t* bytes = stage.uniforms.data();
    for (const auto& [name, offset] : stage.shader.uniformOffsets) {
        if (auto v = values.vectors.find(name); v != values.vectors.end()) {
            if (offset + 16 <= stage.uniforms.size()) std::memcpy(bytes + offset, v->second.data(), 16);
        } else if (auto m = values.matrices.find(name); m != values.matrices.end()) {
            if (offset + 48 > stage.uniforms.size()) continue;
            float packed[12];
            for (int i = 0; i < 4; ++i) {
                for (int j = 0; j < 3; ++j) packed[j * 4 + i] = m->second[i * 3 + j];
            }
            std::memcpy(bytes + offset, packed, sizeof(packed));
        } else if (auto t = stage.texsizes.find(name); t != stage.texsizes.end()) {
            if (offset + 16 <= stage.uniforms.size()) std::memcpy(bytes + offset, t->second.data(), 16);
        }
    }
    if (!stage.uniforms.empty()) {
        [encoder setFragmentBytes:stage.uniforms.data() length:stage.uniforms.size() atIndex:stage.shader.uniformBuffer];
    }
    for (const Binding& b : stage.bindings) {
        id<MTLTexture> tex = b.texture;
        bool wrap = b.lookup.wrap;
        switch (b.source) {
        case SOURCE_MAIN:
            tex = main;
            if (!b.lookup.prefixed) wrap = wrapMain;
            break;
        case SOURCE_BLUR1: tex = blurs[0]; break;
        case SOURCE_BLUR2: tex = blurs[1]; break;
        case SOURCE_BLUR3: tex = blurs[2]; break;
        case SOURCE_TEXTURE: break;
        }
        [encoder setFragmentTexture:tex atIndex:b.textureIndex];
        [encoder setFragmentSamplerState:m_bank.sampler(b.lookup.filtered, wrap) atIndex:b.samplerIndex];
    }
}

} // namespace Milk

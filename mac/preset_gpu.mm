#import "preset_gpu.h"

#include <algorithm>
#include <cstring>

namespace Milk {

PresetGpu::PresetGpu(id<MTLDevice> device, id<MTLFunction> warpVertex, id<MTLFunction> compVertex,
                     MTLPixelFormat feedbackFormat, MTLPixelFormat drawableFormat, TextureBank& bank,
                     const std::string& warpCode, const std::string& compCode, uint32_t seed, bool milkwave)
    : m_device(device), m_bank(bank), m_seed(seed), m_milkwave(milkwave) {
    if (warpCode.find("shader_body") != std::string::npos) build(m_warp, ShaderStage::WARP, warpCode, warpVertex, feedbackFormat);
    if (compCode.find("shader_body") != std::string::npos) build(m_comp, ShaderStage::COMP, compCode, compVertex, drawableFormat);
}

namespace {

// MilkDrop's fallback shaders (data/warp_ps.fx and comp_ps.fx), used when a preset's shader fails
// to compile.
const char* FALLBACK_WARP = "shader_body\n{\n    ret = tex2D( sampler_main, uv ).xyz;\n    ret -= 0.004;\n}\n";
const char* FALLBACK_COMP = "shader_body\n{\n    ret = tex2D(sampler_main, uv).xyz;\n}\n";

} // namespace

void PresetGpu::build(Stage& stage, ShaderStage kind, const std::string& code, id<MTLFunction> vertex, MTLPixelFormat format,
                      bool fallback) {
    const char* label = kind == ShaderStage::WARP ? "warp" : "comp";
    auto fail = [&](const std::string& message) {
        m_errors.push_back(std::string(label) + " shader: " + message);
        stage = Stage{};
        if (!fallback) build(stage, kind, kind == ShaderStage::WARP ? FALLBACK_WARP : FALLBACK_COMP, vertex, format, true);
    };
    std::string error;
    if (!translateShader(kind, code, stage.shader, error, m_milkwave)) {
        fail(error.substr(0, error.find('\n')));
        return;
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
        fail(std::string("Metal compile failed: ") + (nsError ? nsError.localizedDescription.UTF8String : "no entry point"));
        return;
    }
    MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
    desc.vertexFunction = vertex;
    desc.fragmentFunction = fragment;
    desc.colorAttachments[0].pixelFormat = format;
    // Blends between presets draw the new preset's shader over the old one's by the mesh alpha.
    desc.colorAttachments[0].blendingEnabled = YES;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorZero;
    stage.pipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:&nsError];
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorSourceAlpha;
    stage.flippedPipeline = [m_device newRenderPipelineStateWithDescriptor:desc error:nil];
    if (stage.pipeline == nil) {
        fail(std::string("pipeline failed: ") + nsError.localizedDescription.UTF8String);
        return;
    }

    // MilkDrop resolves samplers in the order of the shader's constant table, sorted by name, and
    // stops at the first texture it cannot load.
    std::vector<TranslatedShader::Sampler> samplers = stage.shader.samplers;
    std::sort(samplers.begin(), samplers.end(),
              [](const TranslatedShader::Sampler& a, const TranslatedShader::Sampler& b) { return a.name < b.name; });
    std::map<std::string, id<MTLTexture>> randomPicks;  // "randNN" -> the texture its sampler got
    for (const TranslatedShader::Sampler& s : samplers) {
        Binding b;
        b.textureIndex = s.texture;
        b.samplerIndex = s.sampler;
        b.lookup = TextureBank::parse(s.name);
        if (b.lookup.name == "main") b.source = SOURCE_MAIN;
        else if (b.lookup.name == "blur1") b.source = SOURCE_BLUR1;
        else if (b.lookup.name == "blur2") b.source = SOURCE_BLUR2;
        else if (b.lookup.name == "blur3") b.source = SOURCE_BLUR3;
        else if (m_milkwave && b.lookup.name == "fft") {
            b.source = SOURCE_FFT;
            if (!b.lookup.prefixed) {
                b.lookup.wrap = false;
                b.lookup.filtered = true;
            }
        } else if (!stage.missingTexture) {
            b.texture = m_bank.texture(b.lookup.name, m_seed);
            if (b.texture == nil) {
                stage.missingTexture = true;
                m_errors.push_back(std::string(label) + " shader: no texture named " + b.lookup.name);
            } else if (b.lookup.name.rfind("rand", 0) == 0 && b.lookup.name.size() >= 6) {
                randomPicks[b.lookup.name.substr(0, 6)] = b.texture;
            }
        }
        if (b.source == SOURCE_TEXTURE && (b.texture == nil || (b.texture.textureType == MTLTextureType3D) != s.volume)) {
            b.texture = m_bank.black(s.volume);
        }
        if (stage.missingTexture && b.source != SOURCE_TEXTURE) {
            b.source = SOURCE_TEXTURE;
            b.texture = m_bank.black(s.volume);
        }
        if (b.source >= SOURCE_BLUR1 && b.source <= SOURCE_BLUR3) {
            m_blurLevels = std::max(m_blurLevels, static_cast<int>(b.source - SOURCE_BLUR1) + 1);
            if (!b.lookup.prefixed) b.lookup.wrap = false;
        }
        stage.bindings.push_back(b);
    }
    // texsize_ names a noise texture, an image file or a random slot's pick. The blur levels'
    // sizes come with each frame's values, and any other name reads zero.
    for (const auto& [name, offset] : stage.shader.uniformOffsets) {
        if (name.rfind("texsize_", 0) != 0) continue;
        const std::string tex = name.substr(8);
        if (tex == "blur1" || tex == "blur2" || tex == "blur3") continue;
        id<MTLTexture> t = nil;
        if (tex.rfind("rand", 0) == 0 && tex.size() >= 6 && randomPicks.count(tex.substr(0, 6))) t = randomPicks[tex.substr(0, 6)];
        else if (m_bank.has(tex)) t = m_bank.texture(tex, m_seed);
        if (t == nil) {
            stage.texsizes[name] = {0.0f, 0.0f, 0.0f, 0.0f};
        } else {
            const float w = static_cast<float>(t.width), h = static_cast<float>(t.height);
            stage.texsizes[name] = {w, h, 1.0f / w, 1.0f / h};
        }
    }
    stage.uniforms.assign(stage.shader.uniformSize, 0);
}

void PresetGpu::bindWarp(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3], bool wrapMain,
                         const ShaderFrameValues& values) {
    bind(m_warp, encoder, main, blurs, wrapMain, values);
}

void PresetGpu::bindComp(id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3], bool wrapMain,
                         const ShaderFrameValues& values, bool flipAlpha) {
    bind(m_comp, encoder, main, blurs, wrapMain, values, flipAlpha);
}

// The header's float4x3 arrives as a Metal float3x4: three float4 columns, so HLSL element
// [i][j] sits at byte j * 16 + i * 4.
void PresetGpu::bind(Stage& stage, id<MTLRenderCommandEncoder> encoder, id<MTLTexture> main, id<MTLTexture> const blurs[3],
                     bool wrapMain, const ShaderFrameValues& values, bool flipAlpha) {
    [encoder setRenderPipelineState:(flipAlpha ? stage.flippedPipeline : stage.pipeline)];
    uint8_t* bytes = stage.uniforms.data();
    for (const auto& [name, offset] : stage.shader.uniformOffsets) {
        if (stage.missingTexture) break;
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
        case SOURCE_FFT: tex = m_fft; break;
        case SOURCE_TEXTURE: break;
        }
        [encoder setFragmentTexture:tex atIndex:b.textureIndex];
        [encoder setFragmentSamplerState:m_bank.sampler(b.lookup.filtered, wrap) atIndex:b.samplerIndex];
    }
}

} // namespace Milk

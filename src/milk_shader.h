#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Milk {

enum class ShaderStage {
    WARP,
    COMP,
};

// A preset shader translated to Metal Shading Language, with what the renderer needs to bind it.
struct TranslatedShader {
    struct Sampler {
        std::string name;       // as the preset wrote it, e.g. "sampler_fw_main"
        uint32_t texture = 0;   // [[texture(n)]]
        uint32_t sampler = 0;   // [[sampler(n)]]
        bool volume = false;    // a sampler3D
    };
    std::string msl;
    std::string entryPoint;
    std::vector<Sampler> samplers;
    uint32_t uniformBuffer = 0;
    size_t uniformSize = 0;
    // Byte offset of each uniform in the buffer, by its HLSL name (_c0, _qa, rot_s1, texsize_x...).
    std::map<std::string, uint32_t> uniformOffsets;
};

// Wraps a preset's warp_N or comp_N code in MilkDrop 2's shader header and entry point, then
// compiles HLSL -> SPIR-V (glslang) -> MSL (SPIRV-Cross). The fragment inputs are
// [[user(locn0)]] colour (float4), [[user(locn1)]] uv (float4: uv.xy, uv_orig.zw) and
// [[user(locn2)]] rad and ang (float2).
bool translateShader(ShaderStage stage, const std::string& presetCode, TranslatedShader& out, std::string& error);

// The HLSL that translateShader hands to glslang, for inspecting a failure.
bool wrappedShaderSource(ShaderStage stage, const std::string& presetCode, std::string& out, std::string& error);

// The names of the header's uniforms, which every translated shader's uniformOffsets should hold.
std::vector<std::string> shaderHeaderUniforms();

} // namespace Milk

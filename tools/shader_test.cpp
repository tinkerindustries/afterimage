// Translates small shaders that use HLSL glslang rejects as written, and checks each one reaches
// Metal. Exits non-zero on a failure.

#include "milk_shader.h"

#include <cstdio>
#include <string>

using namespace Milk;

namespace {

int failures = 0;

void check(const char* name, ShaderStage stage, const std::string& globals, const std::string& body) {
    TranslatedShader shader;
    std::string error;
    const std::string code = globals + "\nshader_body\n{\n" + body + "\n}\n";
    if (!translateShader(stage, code, shader, error)) {
        ++failures;
        std::printf("FAIL %s: %s\n", name, error.substr(0, error.find('\n')).c_str());
    }
}

} // namespace

int main() {
    check("float1 variables", ShaderStage::WARP, "", "float1 c = 0.5; ret.y += (ret.y - 0.1)*c + c;");
    check("negated comparison", ShaderStage::WARP, "", "ret = 0.5; ret -= -(lum(ret) < sin(time*90))*ret;");
    check("flat vector array", ShaderStage::COMP, "const float4 s[2] = { 1,0,0,0.5, 0,1,0,0.5 };", "ret = s[1].xyz;");
    check("sampler_state block", ShaderStage::COMP,
          "sampler sampler_grad = sampler_state { AddressU = WRAP; AddressV = WRAP; };", "ret = tex2D(sampler_grad, uv).xyz;");
    check("scalar normalize", ShaderStage::WARP, "", "float2 z = uv; ret.x = normalize(z.x);");
    check("operator word as a name", ShaderStage::COMP, "", "float3 or = tex2D(sampler_main, uv).xyz; ret = or * or;");
    check("vector passed to a float function", ShaderStage::COMP, "float tri(float x) { return abs(2*frac(x/2)-1); }",
          "ret = tri(tex2D(sampler_main, uv)).xxx;");
    check("uniform in a static const", ShaderStage::WARP, "static const float k = q7, m = 2.0*q8;", "ret = k*m;");
    check("declaration after a #define", ShaderStage::COMP, "#define sat saturate\nfloat a, b;", "a = 1; b = a; ret = sat(b).xxx;");
    check("comp uv_orig aliases uv", ShaderStage::COMP, "", "uv = 0.5; ret = tex2D(sampler_main, uv_orig).xyz;");
    check("rot matrices", ShaderStage::COMP, "", "ret = mul(float4(uv, 0, 1), rot_s1);");

    std::printf("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}

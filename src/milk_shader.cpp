#include "milk_shader.h"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <spirv_msl.hpp>

#include <cctype>
#include <cstring>
#include <mutex>
#include <regex>
#include <set>

namespace Milk {

namespace {

// MilkDrop 2's shader header: the uniforms and samplers every preset shader can read, and the
// macros that name parts of them.
const char* SHADER_HEADER = R"(
#define M_PI 3.14159265359
#define M_PI_2 6.28318530718
#define M_INV_PI_2 0.159154943091895

sampler2D sampler_main;
sampler2D sampler_fc_main;
sampler2D sampler_pc_main;
sampler2D sampler_fw_main;
sampler2D sampler_pw_main;
sampler2D sampler_blur1;
sampler2D sampler_blur2;
sampler2D sampler_blur3;
sampler2D sampler_noise_lq;
sampler2D sampler_noise_lq_lite;
sampler2D sampler_noise_mq;
sampler2D sampler_noise_hq;
sampler3D sampler_noisevol_lq;
sampler3D sampler_noisevol_hq;

#define sampler_FC_main sampler_fc_main
#define sampler_PC_main sampler_pc_main
#define sampler_FW_main sampler_fw_main
#define sampler_PW_main sampler_pw_main

#define time _c2.x
#define fps _c2.y
#define frame _c2.z
#define progress _c2.w
#define bass _c3.x
#define mid _c3.y
#define treb _c3.z
#define vol _c3.w
#define bass_att _c4.x
#define mid_att _c4.y
#define treb_att _c4.z
#define vol_att _c4.w
#define q1 _qa.x
#define q2 _qa.y
#define q3 _qa.z
#define q4 _qa.w
#define q5 _qb.x
#define q6 _qb.y
#define q7 _qb.z
#define q8 _qb.w
#define q9 _qc.x
#define q10 _qc.y
#define q11 _qc.z
#define q12 _qc.w
#define q13 _qd.x
#define q14 _qd.y
#define q15 _qd.z
#define q16 _qd.w
#define q17 _qe.x
#define q18 _qe.y
#define q19 _qe.z
#define q20 _qe.w
#define q21 _qf.x
#define q22 _qf.y
#define q23 _qf.z
#define q24 _qf.w
#define q25 _qg.x
#define q26 _qg.y
#define q27 _qg.z
#define q28 _qg.w
#define q29 _qh.x
#define q30 _qh.y
#define q31 _qh.z
#define q32 _qh.w
#define aspect _c0
#define texsize _c7
#define roam_cos _c8
#define roam_sin _c9
#define slow_roam_cos _c10
#define slow_roam_sin _c11
#define mip_x _c12.x
#define mip_y _c12.y
#define mip_xy _c12.xy
#define mip_avg _c12.z
#define blur1_min _c6.z
#define blur1_max _c6.w
#define blur2_min _c13.x
#define blur2_max _c13.y
#define blur3_min _c13.z
#define blur3_max _c13.w

#define GetMain(uv) (tex2D(sampler_main,uv).xyz)
#define GetPixel(uv) (tex2D(sampler_main,uv).xyz)
#define GetBlur1(uv) (tex2D(sampler_blur1,uv).xyz*_c5.x + _c5.y)
#define GetBlur2(uv) (tex2D(sampler_blur2,uv).xyz*_c5.z + _c5.w)
#define GetBlur3(uv) (tex2D(sampler_blur3,uv).xyz*_c6.x + _c6.y)

// Direct3D 9 compiles these to instructions that use the absolute value of their input, so
// presets take sqrt, pow and log of negative numbers and get a number back, not NaN.
#define sqrt(x) sqrt(abs(x))
#define rsqrt(x) rsqrt(abs(x))
#define pow(x, y) pow(abs(x), (y))
#define log(x) log(abs(x))
#define log2(x) log2(abs(x))
#define log10(x) log10(abs(x))

// C++ alternative tokens that Metal's compiler reads as operators, if a preset names a variable one.
#define or or_
#define and and_
#define not not_
#define xor xor_
#define compl compl_
#define bitand bitand_
#define bitor bitor_
#define float1 float
#define half1 float
#define double float
#define double2 float2
#define double3 float3
#define double4 float4

// HLSL normalizes a scalar to its sign.
float normalize(float x) { return sign(x); }

#define lum(x) (dot(x,float3(0.32,0.49,0.29)))
#define tex2d tex2D
#define tex3d tex3D
)";

// The header's uniforms. Presets may assign to them (DX9 allowed it), so each arrives as a uniform
// named MilkU<name> and is copied into a writable static of the plain name on entry. The prefix
// must not end in '_': names like _c0 would then contain "__", which SPIRV-Cross renames.
const char* UNIFORMS[][2] = {
    {"float4", "rand_frame"}, {"float4", "rand_preset"},
    {"float4", "_c0"}, {"float4", "_c1"}, {"float4", "_c2"}, {"float4", "_c3"}, {"float4", "_c4"},
    {"float4", "_c5"}, {"float4", "_c6"}, {"float4", "_c7"}, {"float4", "_c8"}, {"float4", "_c9"},
    {"float4", "_c10"}, {"float4", "_c11"}, {"float4", "_c12"}, {"float4", "_c13"},
    {"float4", "_qa"}, {"float4", "_qb"}, {"float4", "_qc"}, {"float4", "_qd"},
    {"float4", "_qe"}, {"float4", "_qf"}, {"float4", "_qg"}, {"float4", "_qh"},
    {"float4x3", "rot_s1"}, {"float4x3", "rot_s2"}, {"float4x3", "rot_s3"}, {"float4x3", "rot_s4"},
    {"float4x3", "rot_d1"}, {"float4x3", "rot_d2"}, {"float4x3", "rot_d3"}, {"float4x3", "rot_d4"},
    {"float4x3", "rot_f1"}, {"float4x3", "rot_f2"}, {"float4x3", "rot_f3"}, {"float4x3", "rot_f4"},
    {"float4x3", "rot_vf1"}, {"float4x3", "rot_vf2"}, {"float4x3", "rot_vf3"}, {"float4x3", "rot_vf4"},
    {"float4x3", "rot_uf1"}, {"float4x3", "rot_uf2"}, {"float4x3", "rot_uf3"}, {"float4x3", "rot_uf4"},
    {"float4x3", "rot_rand1"}, {"float4x3", "rot_rand2"}, {"float4x3", "rot_rand3"}, {"float4x3", "rot_rand4"},
    {"float4", "texsize_noise_lq"}, {"float4", "texsize_noise_lq_lite"}, {"float4", "texsize_noise_mq"},
    {"float4", "texsize_noise_hq"}, {"float4", "texsize_noisevol_lq"}, {"float4", "texsize_noisevol_hq"},
};

const char* UNIFORM_PREFIX = "MilkU";

std::string uniformDeclarations() {
    std::string out;
    for (const auto& u : UNIFORMS) out += std::string(u[0]) + " " + UNIFORM_PREFIX + u[1] + ";\n";
    for (const auto& u : UNIFORMS) out += std::string("static ") + u[0] + " " + u[1] + ";\n";
    return out;
}

std::string uniformCopies() {
    std::string out;
    for (const auto& u : UNIFORMS) out += std::string("    ") + u[1] + " = " + UNIFORM_PREFIX + u[1] + ";\n";
    return out;
}

const char* ENTRY_SIGNATURE =
    "void PS(float4 _vDiffuse : COLOR, float4 _uv : TEXCOORD0, float2 _rad_ang : TEXCOORD1, out float4 _return_value : COLOR0)\n";

const char* WARP_PROLOGUE = R"(
    float3 ret = 0;
    float2 uv = _uv.xy;
    float2 uv_orig = _uv.zw;
    float rad = _rad_ang.x;
    float ang = _rad_ang.y;
)";

const char* COMP_PROLOGUE = R"(
    float3 ret = 0;
    float2 uv = _uv.xy;
    float rad = _rad_ang.x;
    float ang = _rad_ang.y;
    float3 hue_shader = _vDiffuse.xyz;
)";

// MilkDrop's feedback image is 8 bits per channel, so the warp output is clamped to 0..1.
const char* WARP_EPILOGUE = "\n    _return_value = float4(saturate(ret.xyz), 1);\n";
const char* COMP_EPILOGUE = "\n    _return_value = float4(ret.xyz, 1);\n";

const std::set<std::string> HEADER_SAMPLERS = {
    "sampler_main", "sampler_fc_main", "sampler_pc_main", "sampler_fw_main", "sampler_pw_main",
    "sampler_blur1", "sampler_blur2", "sampler_blur3", "sampler_noise_lq", "sampler_noise_lq_lite",
    "sampler_noise_mq", "sampler_noise_hq", "sampler_noisevol_lq", "sampler_noisevol_hq",
};

bool isHeaderUniform(const std::string& name) {
    for (const auto& u : UNIFORMS) {
        if (name == u[1]) return true;
    }
    return false;
}

std::once_flag g_glslangInit;

// Preset code declares extra textures as `sampler sampler_name;`, the DX9 generic type. glslang
// needs a dimension, so these become sampler2D (sampler3D for the noise volumes). Declarations of
// samplers the header already has are dropped.
std::string fixSamplerDeclarations(const std::string& code) {
    static const std::regex decl(R"(\bsampler(2D|3D)?\s+(sampler_\w+)\s*;)", std::regex::icase);
    std::string out;
    auto begin = std::sregex_iterator(code.begin(), code.end(), decl);
    size_t last = 0;
    for (auto it = begin; it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        out += code.substr(last, m.position() - last);
        std::string name = m[2].str();
        std::string lower = name;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (!HEADER_SAMPLERS.count(lower)) {
            const bool volume = m[1].str() == "3D" || m[1].str() == "3d" || lower.find("noisevol") != std::string::npos;
            out += std::string(volume ? "sampler3D " : "sampler2D ") + name + ";";
        }
        last = m.position() + m.length();
    }
    out += code.substr(last);

    // Drop redeclarations of the header's uniforms, such as `float4 texsize_noise_lq;`.
    static const std::regex uniformDecl(R"(\bfloat4(x3)?\s+(\w+)\s*;)");
    std::string kept;
    last = 0;
    for (auto it = std::sregex_iterator(out.begin(), out.end(), uniformDecl); it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        kept += out.substr(last, m.position() - last);
        if (!isHeaderUniform(m[2].str())) kept += m.str();
        last = m.position() + m.length();
    }
    kept += out.substr(last);
    return kept;
}

// Names whose values come from the uniforms, directly or through the header's macros.
bool readsUniform(const std::string& text, const std::set<std::string>& deferredNames) {
    static const std::set<std::string> macros = {
        "time", "fps", "frame", "progress", "bass", "mid", "treb", "vol", "bass_att", "mid_att", "treb_att",
        "vol_att", "aspect", "texsize", "roam_cos", "roam_sin", "slow_roam_cos", "slow_roam_sin", "mip_x",
        "mip_y", "mip_xy", "mip_avg", "blur1_min", "blur1_max", "blur2_min", "blur2_max", "blur3_min",
        "blur3_max", "GetBlur1", "GetBlur2", "GetBlur3",
    };
    static const std::regex word(R"([A-Za-z_]\w*)");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), word); it != std::sregex_iterator(); ++it) {
        const std::string name = it->str();
        if (macros.count(name) || isHeaderUniform(name) || deferredNames.count(name)) return true;
        if (name.size() >= 2 && name[0] == 'q' && std::isdigit(static_cast<unsigned char>(name[1]))) return true;
    }
    return false;
}

std::vector<std::string> splitTopLevel(const std::string& text) {
    std::vector<std::string> parts;
    std::string part;
    int depth = 0;
    for (char c : text) {
        if (c == '(' || c == '[' || c == '{') ++depth;
        else if (c == ')' || c == ']' || c == '}') --depth;
        if (c == ',' && depth == 0) {
            parts.push_back(part);
            part.clear();
        } else {
            part += c;
        }
    }
    parts.push_back(part);
    return parts;
}

// Rewrites one top-level declaration (`float a = q1, b;`, `static const float3 c = 0.5;`) so that
// it works with the entry point's uniform copies. A variable becomes a static. When its
// initialiser reads a uniform, or a variable that does, the initialiser moves to `deferred`, which
// runs after the copies. Constants that read no uniform stay as written, since they may size an
// array or bound a loop. Returns false when the statement is not a plain declaration.
bool emitDeclaration(const std::string& statement, std::string& out, std::string& deferred, std::set<std::string>& deferredNames) {
    static const std::regex head(
        R"(^(\s*)((?:static\s+|const\s+|uniform\s+)*)((?:float|half|int|uint|bool|double)[1-4]?(?:x[1-4])?)\s+([\s\S]*);\s*$)");
    static const std::regex declarator(R"(^\s*(\w+)\s*(\[[^\]]*\])?\s*(?:=([\s\S]*))?$)");
    std::smatch m;
    if (!std::regex_match(statement, m, head)) return false;
    const std::string qualifiers = m[2].str();
    const bool isConst = qualifiers.find("const") != std::string::npos;
    const std::vector<std::string> parts = splitTopLevel(m[4].str());
    std::vector<std::string> names, arrays, inits;
    std::vector<bool> hasInit;
    for (const std::string& part : parts) {
        std::smatch d;
        if (!std::regex_match(part, d, declarator)) return false;
        names.push_back(d[1].str());
        arrays.push_back(d[2].str());
        inits.push_back(d[3].str());
        hasInit.push_back(d[3].matched);
    }
    std::string emitted = m[1].str();
    std::string later;
    for (size_t i = 0; i < names.size(); ++i) {
        const bool texsize = names[i].rfind("texsize_", 0) == 0 && !hasInit[i];
        if (texsize) {
            emitted += m[3].str() + " " + names[i] + arrays[i] + ";";
        } else if (hasInit[i] && inits[i].find('{') != std::string::npos) {
            emitted += qualifiers + (isConst ? "" : "static ") + m[3].str() + " " + names[i] + arrays[i] + " =" + inits[i] + ";";
        } else if (isConst && !readsUniform(inits[i], deferredNames)) {
            emitted += qualifiers + m[3].str() + " " + names[i] + arrays[i] + (hasInit[i] ? " =" + inits[i] : std::string()) + ";";
        } else {
            emitted += "static " + m[3].str() + " " + names[i] + arrays[i] + ";";
            if (hasInit[i]) {
                if (readsUniform(inits[i], deferredNames)) deferredNames.insert(names[i]);
                later += "    " + names[i] + " = " + inits[i] + ";\n";
            }
        }
        emitted += " ";
    }
    out += emitted;
    deferred += later;
    return true;
}

// DX9 treated a preset's own top-level variables (`float3 col;`) as uniforms it could still
// write. Here they become statics, except texsize_<name>, which the renderer fills in.
//
// DX9 also evaluated their initialisers with the real uniform values, as in
// `static float3 suncol = 1 + normalize(slow_roam_cos);`. Here the uniforms only reach their
// statics at the start of the entry point, so each such initialiser is removed from the
// declaration and returned in `deferred`, to run after those copies.
std::string makeGlobalsStatic(const std::string& code, std::string& deferred) {
    static const std::regex typeStart(R"(^\s*(float|half|int|uint|bool|double)[1-4]?(x[1-4])?\s+(\w+))");
    std::string out;
    std::string statement;
    std::set<std::string> deferredNames;
    int depth = 0;
    auto flush = [&] {
        // Preprocessor lines end at the newline, not at a ';', so they ride along at the front.
        for (;;) {
            const size_t first = statement.find_first_not_of(" \t\r\n");
            if (first == std::string::npos || statement[first] != '#') break;
            size_t end = statement.find('\n', first);
            if (end == std::string::npos) end = statement.size();
            out += statement.substr(0, end);
            statement.erase(0, end);
        }
        if (depth == 0 && emitDeclaration(statement, out, deferred, deferredNames)) {
            statement.clear();
            return;
        }
        std::smatch t;
        const bool declaration = depth == 0 && std::regex_search(statement, t, typeStart) &&
                                 statement.find('(') == std::string::npos && t[3].str().rfind("texsize_", 0) != 0;
        if (declaration) {
            const size_t first = statement.find_first_not_of(" \t\r\n");
            out += statement.substr(0, first) + "static " + statement.substr(first);
        } else {
            out += statement;
        }
        statement.clear();
    };
    for (size_t i = 0; i < code.size(); ++i) {
        const char c = code[i];
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '/') {
            const size_t end = code.find('\n', i);
            i = (end == std::string::npos ? code.size() : end) - 1;
            continue;
        }
        if (c == '/' && i + 1 < code.size() && code[i + 1] == '*') {
            const size_t end = code.find("*/", i + 2);
            i = (end == std::string::npos ? code.size() : end + 2) - 1;
            continue;
        }
        if (c == '{' || c == '}') {
            out += statement;
            statement.clear();
            out += c;
            depth += c == '{' ? 1 : -1;
            continue;
        }
        statement += c;
        if (c == ';') flush();
    }
    out += statement;
    return out;
}

// glslang's HLSL parser rejects a unary minus in front of a parenthesised comparison, such as
// `-(lum(ret)<sin(time))*ret`. Such a minus becomes `-1.0*`, which means the same.
std::string fixNegatedComparisons(const std::string& code) {
    std::string out;
    size_t i = 0;
    while (i < code.size()) {
        const char c = code[i];
        if (c == '-' && i + 1 < code.size()) {
            size_t open = i + 1;
            while (open < code.size() && std::isspace(static_cast<unsigned char>(code[open]))) ++open;
            size_t before = out.size();
            while (before > 0 && std::isspace(static_cast<unsigned char>(out[before - 1]))) --before;
            const char prev = before > 0 ? out[before - 1] : '(';
            const bool unary = !(std::isalnum(static_cast<unsigned char>(prev)) || prev == '_' || prev == ')' || prev == ']' ||
                                 prev == '.' || prev == '-');
            if (unary && open < code.size() && code[open] == '(') {
                int depth = 0;
                bool comparison = false;
                size_t j = open;
                for (; j < code.size(); ++j) {
                    const char d = code[j];
                    if (d == '(') ++depth;
                    else if (d == ')' && --depth == 0) break;
                    else if (depth == 1 && (d == '<' || d == '>' || ((d == '=' || d == '!') && j + 1 < code.size() && code[j + 1] == '='))) {
                        comparison = true;
                    }
                }
                if (comparison) {
                    out += "-1.0*";
                    i = open;
                    continue;
                }
            }
        }
        out += c;
        ++i;
    }
    return out;
}

// DX9 let a vector array take one flat list of numbers, `float4 s[2] = { 1,2,3,4, 5,6,7,8 };`.
// glslang wants each element braced, so the numbers are regrouped by the vector's width.
std::string groupArrayInitialisers(const std::string& code) {
    static const std::regex decl(R"(\b(?:float|half|int)([234])\s+\w+\s*\[[^\]]*\]\s*=\s*\{([^{}]*)\})");
    std::string out;
    size_t last = 0;
    for (auto it = std::sregex_iterator(code.begin(), code.end(), decl); it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        const int width = m[1].str()[0] - '0';
        const std::string list = m[2].str();
        std::vector<std::string> items;
        std::string item;
        int parens = 0;
        for (char c : list) {
            if (c == '(') ++parens;
            else if (c == ')') --parens;
            if (c == ',' && parens == 0) {
                items.push_back(item);
                item.clear();
            } else {
                item += c;
            }
        }
        if (item.find_first_not_of(" \t\r\n") != std::string::npos) items.push_back(item);
        if (items.size() <= static_cast<size_t>(width) || items.size() % width != 0) continue;
        std::string grouped;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i % width == 0) grouped += i == 0 ? "{" : ", {";
            grouped += (i % width == 0 ? "" : ",") + items[i];
            if (i % width == width - 1) grouped += "}";
        }
        const size_t brace = m.str().find('{');
        out += code.substr(last, m.position() - last) + m.str().substr(0, brace) + "{" + grouped + "}";
        last = m.position() + m.length();
    }
    return out + code.substr(last);
}

// `sampler sampler_x = sampler_state { AddressU = WRAP; };` carries sampler state that the renderer
// sets itself, so only the declaration is kept.
std::string dropSamplerState(const std::string& code) {
    static const std::regex state(R"(\bsampler(?:2D|3D)?\s+(\w+)\s*=\s*sampler_state\s*\{[^}]*\}\s*;)");
    return std::regex_replace(code, state, "sampler $1;");
}

// HLSL passes a vector to a function that takes a float by dropping all but the first component.
// glslang does not, so each one-parameter float function gets float2, float3 and float4 overloads.
std::string addVectorOverloads(const std::string& code) {
    static const std::regex func(R"((?:^|[;}\n])\s*(float[1-4]?)\s+(\w+)\s*\(\s*(?:in\s+)?float\s+(\w+)\s*\)\s*\{)");
    std::string out;
    size_t last = 0;
    for (auto it = std::sregex_iterator(code.begin(), code.end(), func); it != std::sregex_iterator(); ++it) {
        const std::smatch& m = *it;
        size_t pos = m.position() + m.length();
        if (pos < last) continue;
        int depth = 1;
        while (pos < code.size() && depth > 0) {
            if (code[pos] == '{') ++depth;
            else if (code[pos] == '}') --depth;
            ++pos;
        }
        out += code.substr(last, pos - last);
        for (const char* type : {"float2", "float3", "float4"}) {
            out += "\n" + m[1].str() + " " + m[2].str() + "(" + type + " v) { return " + m[2].str() + "(v.x); }";
        }
        out += "\n";
        last = pos;
    }
    return out + code.substr(last);
}

bool wrapBody(ShaderStage stage, const std::string& code, std::string& out, std::string& error) {
    const size_t marker = code.find("shader_body");
    if (marker == std::string::npos) {
        error = "no shader_body";
        return false;
    }
    const size_t open = code.find('{', marker);
    const size_t close = code.rfind('}');
    if (open == std::string::npos || close == std::string::npos || close < open) {
        error = "shader_body has no braces";
        return false;
    }
    out = uniformDeclarations();
    out += SHADER_HEADER;
    std::string deferred;
    out += addVectorOverloads(makeGlobalsStatic(
        fixSamplerDeclarations(dropSamplerState(groupArrayInitialisers(fixNegatedComparisons(code.substr(0, marker))))), deferred));
    // MilkDrop defines the comp shader's uv_orig as the same variable as uv.
    if (stage == ShaderStage::COMP) out += "#define uv_orig uv\n";
    out += ENTRY_SIGNATURE;
    out += "{\n";
    out += uniformCopies();
    out += deferred;
    out += stage == ShaderStage::WARP ? WARP_PROLOGUE : COMP_PROLOGUE;
    out += "{\n";
    out += groupArrayInitialisers(fixNegatedComparisons(code.substr(open + 1, close - open - 1)));
    out += "\n}\n";
    out += stage == ShaderStage::WARP ? WARP_EPILOGUE : COMP_EPILOGUE;
    out += "}\n";
    return true;
}

bool hlslToSpirv(const std::string& hlsl, std::vector<uint32_t>& spirv, std::string& error) {
    std::call_once(g_glslangInit, [] { glslang::InitializeProcess(); });
    glslang::TShader shader(EShLangFragment);
    const char* source = hlsl.c_str();
    shader.setStrings(&source, 1);
    shader.setEntryPoint("PS");
    shader.setSourceEntryPoint("PS");
    shader.setEnvInput(glslang::EShSourceHlsl, EShLangFragment, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    shader.setHlslIoMapping(true);
    shader.setAutoMapBindings(true);
    shader.setAutoMapLocations(true);
    const EShMessages messages =
        static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl | EShMsgHlslDX9Compatible);
    if (!shader.parse(GetDefaultResources(), 100, false, messages)) {
        error = shader.getInfoLog();
        return false;
    }
    glslang::TProgram program;
    program.addShader(&shader);
    if (!program.link(messages) || !program.mapIO()) {
        error = program.getInfoLog();
        return false;
    }
    glslang::GlslangToSpv(*program.getIntermediate(EShLangFragment), spirv);
    return true;
}

bool spirvToMsl(std::vector<uint32_t> spirv, TranslatedShader& out, std::string& error) {
    try {
        spirv_cross::CompilerMSL compiler(std::move(spirv));
        spirv_cross::CompilerMSL::Options options = compiler.get_msl_options();
        options.platform = spirv_cross::CompilerMSL::Options::macOS;
        options.set_msl_version(2, 4);
        compiler.set_msl_options(options);
        const spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        out.msl = compiler.compile();
        out.entryPoint = compiler.get_cleansed_entry_point_name("PS", spv::ExecutionModelFragment);

        out.samplers.clear();
        for (const spirv_cross::Resource& r : resources.sampled_images) {
            // Samplers the shader declares but never reads are left out of the MSL and have no slot.
            if (compiler.get_automatic_msl_resource_binding(r.id) == UINT32_MAX) continue;
            TranslatedShader::Sampler s;
            s.name = compiler.get_name(r.id);
            s.texture = compiler.get_automatic_msl_resource_binding(r.id);
            s.sampler = compiler.get_automatic_msl_resource_binding_secondary(r.id);
            s.volume = compiler.get_type(r.type_id).image.dim == spv::Dim3D;
            out.samplers.push_back(s);
        }
        out.uniformOffsets.clear();
        out.uniformSize = 0;
        for (const spirv_cross::Resource& r : resources.uniform_buffers) {
            out.uniformBuffer = compiler.get_automatic_msl_resource_binding(r.id);
            const spirv_cross::SPIRType& type = compiler.get_type(r.base_type_id);
            out.uniformSize = compiler.get_declared_struct_size(type);
            for (uint32_t i = 0; i < type.member_types.size(); ++i) {
                std::string name = compiler.get_member_name(r.base_type_id, i);
                if (name.rfind(UNIFORM_PREFIX, 0) == 0) name = name.substr(std::strlen(UNIFORM_PREFIX));
                out.uniformOffsets[name] = compiler.type_struct_member_offset(type, i);
            }
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}

} // namespace

std::vector<std::string> shaderHeaderUniforms() {
    std::vector<std::string> names;
    for (const auto& u : UNIFORMS) names.push_back(u[1]);
    return names;
}

bool wrappedShaderSource(ShaderStage stage, const std::string& presetCode, std::string& out, std::string& error) {
    return wrapBody(stage, presetCode, out, error);
}

bool translateShader(ShaderStage stage, const std::string& presetCode, TranslatedShader& out, std::string& error) {
    std::string hlsl;
    if (!wrapBody(stage, presetCode, hlsl, error)) return false;
    std::vector<uint32_t> spirv;
    if (!hlslToSpirv(hlsl, spirv, error)) return false;
    return spirvToMsl(std::move(spirv), out, error);
}

} // namespace Milk

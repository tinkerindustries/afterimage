#include "milk_preset.h"

#include "preset.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>

namespace Milk {

namespace {

// The keys MilkDrop reads from a preset, in the case it writes them, and the variable or setting
// name each becomes. Integer keys are read as sscanf("%d") reads them, so "0.9" is 0, and the rest
// as single-precision floats.
struct PresetKey {
    const char* key;
    const char* name;
    bool integer;
};

const PresetKey PRESET_KEYS[] = {
    {"fRating", "frating", false}, {"fDecay", "decay", false}, {"FFTAttack", "fftattack", false},
    {"FFTDecay", "fftdecay", false}, {"fGammaAdj", "gamma", false}, {"fVideoEchoZoom", "echo_zoom", false},
    {"fVideoEchoAlpha", "echo_alpha", false}, {"nVideoEchoOrientation", "echo_orient", true},
    {"bRedBlueStereo", "red_blue", true}, {"bBrighten", "brighten", true}, {"bDarken", "darken", true},
    {"bSolarize", "solarize", true}, {"bInvert", "invert", true}, {"fShader", "fshader", false},
    {"b1n", "blur1_min", false}, {"b2n", "blur2_min", false}, {"b3n", "blur3_min", false},
    {"b1x", "blur1_max", false}, {"b2x", "blur2_max", false}, {"b3x", "blur3_max", false},
    {"b1ed", "blur1_edge_darken", false}, {"nWaveMode", "wave_mode", true}, {"bAdditiveWaves", "wave_additive", true},
    {"bWaveDots", "wave_usedots", true}, {"bWaveThick", "wave_thick", true},
    {"bModWaveAlphaByVolume", "bmodwavealphabyvolume", true}, {"bMaximizeWaveColor", "wave_brighten", true},
    {"fWaveAlpha", "wave_a", false}, {"fWaveScale", "wave_scale", false}, {"fWaveSmoothing", "wave_smoothing", false},
    {"fWaveParam", "wave_mystery", false}, {"fModWaveAlphaStart", "fmodwavealphastart", false},
    {"fModWaveAlphaEnd", "fmodwavealphaend", false}, {"wave_r", "wave_r", false}, {"wave_g", "wave_g", false},
    {"wave_b", "wave_b", false}, {"wave_x", "wave_x", false}, {"wave_y", "wave_y", false},
    {"nMotionVectorsX", "mv_x", false}, {"nMotionVectorsY", "mv_y", false}, {"mv_dx", "mv_dx", false},
    {"mv_dy", "mv_dy", false}, {"mv_l", "mv_l", false}, {"mv_r", "mv_r", false}, {"mv_g", "mv_g", false},
    {"mv_b", "mv_b", false}, {"mv_a", "mv_a", false}, {"zoom", "zoom", false}, {"rot", "rot", false},
    {"cx", "cx", false}, {"cy", "cy", false}, {"dx", "dx", false}, {"dy", "dy", false}, {"warp", "warp", false},
    {"sx", "sx", false}, {"sy", "sy", false}, {"bTexWrap", "wrap", true}, {"bDarkenCenter", "darken_center", true},
    {"fWarpAnimSpeed", "warpanimspeed", false}, {"fWarpScale", "warpscale", false}, {"fZoomExponent", "zoomexp", false},
    {"ob_size", "ob_size", false}, {"ob_r", "ob_r", false}, {"ob_g", "ob_g", false}, {"ob_b", "ob_b", false},
    {"ob_a", "ob_a", false}, {"ib_size", "ib_size", false}, {"ib_r", "ib_r", false}, {"ib_g", "ib_g", false},
    {"ib_b", "ib_b", false}, {"ib_a", "ib_a", false},
};

const PresetKey WAVE_KEYS[] = {
    {"enabled", "enabled", true}, {"samples", "samples", true}, {"sep", "sep", true}, {"bSpectrum", "bspectrum", true},
    {"bUseDots", "busedots", true}, {"bDrawThick", "bdrawthick", true}, {"bAdditive", "badditive", true},
    {"scaling", "scaling", false}, {"smoothing", "smoothing", false}, {"r", "r", false}, {"g", "g", false},
    {"b", "b", false}, {"a", "a", false},
};

const PresetKey SHAPE_KEYS[] = {
    {"enabled", "enabled", true}, {"sides", "sides", true}, {"additive", "additive", true},
    {"thickOutline", "thickoutline", true}, {"textured", "textured", true}, {"num_inst", "num_inst", true},
    {"x", "x", false}, {"y", "y", false}, {"rad", "rad", false}, {"ang", "ang", false}, {"tex_ang", "tex_ang", false},
    {"tex_zoom", "tex_zoom", false}, {"r", "r", false}, {"g", "g", false}, {"b", "b", false}, {"a", "a", false},
    {"r2", "r2", false}, {"g2", "g2", false}, {"b2", "b2", false}, {"a2", "a2", false}, {"border_r", "border_r", false},
    {"border_g", "border_g", false}, {"border_b", "border_b", false}, {"border_a", "border_a", false},
};

// The variables MilkDrop registers for per_frame code. Each frame they start from the preset's
// values; any other name per_frame code uses is its own variable and keeps its value.
const char* FRAME_VARS[] = {
    "zoom", "zoomexp", "rot", "warp", "cx", "cy", "dx", "dy", "sx", "sy", "decay", "wave_a", "wave_r", "wave_g",
    "wave_b", "wave_x", "wave_y", "wave_mystery", "wave_mode", "ob_size", "ob_r", "ob_g", "ob_b", "ob_a", "ib_size",
    "ib_r", "ib_g", "ib_b", "ib_a", "mv_x", "mv_y", "mv_dx", "mv_dy", "mv_l", "mv_r", "mv_g", "mv_b", "mv_a",
    "echo_zoom", "echo_alpha", "echo_orient", "wave_usedots", "wave_thick", "wave_additive", "wave_brighten",
    "darken_center", "gamma", "wrap", "invert", "brighten", "darken", "solarize", "blur1_min", "blur2_min",
    "blur3_min", "blur1_max", "blur2_max", "blur3_max", "blur1_edge_darken",
};

// MilkDrop's values for variables a preset leaves out.
const std::map<std::string, double> DEFAULTS = {
    {"zoom", 1.0}, {"zoomexp", 1.0}, {"rot", 0.0}, {"warp", 1.0}, {"cx", 0.5}, {"cy", 0.5},
    {"dx", 0.0}, {"dy", 0.0}, {"sx", 1.0}, {"sy", 1.0}, {"decay", 0.98}, {"gamma", 2.0},
    // MilkDrop reads a missing wave_r, wave_g, wave_b, wave_x or wave_y as rot's default, 0.
    {"wave_mode", 0.0}, {"wave_x", 0.0}, {"wave_y", 0.0}, {"wave_r", 0.0}, {"wave_g", 0.0},
    {"wave_b", 0.0}, {"wave_a", 0.8}, {"wave_scale", 1.0}, {"wave_thick", 0.0},
    {"wave_brighten", 1.0}, {"warpanimspeed", 1.0}, {"warpscale", 1.0}, {"wrap", 1.0},
    {"echo_zoom", 2.0}, {"echo_alpha", 0.0}, {"echo_orient", 0.0}, {"brighten", 0.0}, {"darken", 0.0},
    {"solarize", 0.0}, {"invert", 0.0}, {"blur1_min", 0.0}, {"blur1_max", 1.0}, {"blur2_min", 0.0}, {"blur2_max", 1.0},
    {"blur3_min", 0.0}, {"blur3_max", 1.0}, {"blur1_edge_darken", 0.25},
    {"ob_size", 0.01}, {"ib_size", 0.01}, {"ib_r", 0.25}, {"ib_g", 0.25}, {"ib_b", 0.25},
    {"mv_x", 12.0}, {"mv_y", 9.0}, {"mv_l", 0.9}, {"mv_r", 1.0}, {"mv_g", 1.0}, {"mv_b", 1.0}, {"mv_a", 0.0},
};

const char* VERTEX_VARS[] = {"zoom", "zoomexp", "rot", "warp", "cx", "cy", "dx", "dy", "sx", "sy"};

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}


std::string joinLines(const std::map<int, std::string>& lines) {
    std::string out;
    for (const auto& [index, text] : lines) {
        out += text;
        out += '\n';
    }
    return out;
}

// Equation code is stored one editor line per key. MilkDrop strips each line's // comment and joins
// the lines with nothing between them, so a name its editor broke across two lines (`is_` then
// `beat`) is whole again. A line that ends without ';' runs into the next one, and the block it
// belongs to fails to compile and is dropped, as it is in MilkDrop.
std::string joinEquationLines(const std::map<int, std::string>& lines) {
    std::string out;
    for (const auto& [index, text] : lines) {
        std::string line = text;
        if (!line.empty() && line[0] == '`') line.erase(0, 1);
        line = line.substr(0, std::min(line.find("//"), line.find("\\\\")));
        out += line;
    }
    return out;
}

int waveModeFor(double milkMode) {
    switch (((static_cast<int>(milkMode) % 8) + 8) % 8) {
    case 0:
    case 1: return WAVE_CIRCLE;
    case 2:
    case 3:
    case 5: return WAVE_XY;
    default: return WAVE_LINE;
    }
}

float finiteOr(double value, float fallback) {
    return std::isfinite(value) ? static_cast<float>(value) : fallback;
}

} // namespace

bool loadPresetFile(const std::string& path, PresetFile& out, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    out = PresetFile{};
    const size_t slash = path.find_last_of('/');
    out.name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (out.name.size() > 5 && lower(out.name.substr(out.name.size() - 5)) == ".milk") {
        out.name.resize(out.name.size() - 5);
    }

    // A key runs to the first space or '=', and the value is everything after that one character.
    // Keys match exactly, and the first of two lines with the same key wins.
    std::map<std::string, std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t end = line.find_first_of(" =");
        if (end == std::string::npos || end == 0) continue;
        lines.emplace(line.substr(0, end), line.substr(end + 1));
    }
    auto find = [&](const std::string& key) -> const std::string* {
        auto it = lines.find(key);
        return it == lines.end() ? nullptr : &it->second;
    };
    // sscanf("%d") and sscanf("%f"): leading space, then as much of a number as there is.
    auto number = [&](const std::string& key, bool integer, double& value) {
        const std::string* text = find(key);
        if (!text) return false;
        const char* start = text->c_str();
        char* stop = nullptr;
        if (integer) {
            const long v = std::strtol(start, &stop, 10);
            if (stop == start) return false;
            value = static_cast<double>(static_cast<int>(v));
        } else {
            const float v = std::strtof(start, &stop);
            if (stop == start) return false;
            value = static_cast<double>(v);
        }
        return true;
    };
    // Code is read from line 1 until the first missing line number.
    auto code = [&](const std::string& prefix) {
        std::map<int, std::string> out;
        for (int n = 1;; ++n) {
            const std::string* text = find(prefix + std::to_string(n));
            if (!text) break;
            out[n] = *text;
        }
        return out;
    };
    auto shaderCode = [&](const std::string& prefix) {
        std::map<int, std::string> out = code(prefix);
        for (auto& [n, text] : out) {
            if (!text.empty() && text[0] == '`') text.erase(0, 1);
        }
        return joinLines(out);
    };

    for (const PresetKey& k : PRESET_KEYS) {
        double value;
        if (number(k.key, k.integer, value)) out.base[k.name] = value;
    }
    // Without mv_a, the motion vectors' alpha is 1 when bMotionVectorsOn is set and 0 otherwise.
    double motionVectorsOn;
    if (!out.base.count("mv_a") && number("bMotionVectorsOn", true, motionVectorsOn)) {
        out.base["mv_a"] = motionVectorsOn != 0.0 ? 1.0 : 0.0;
    }
    for (int i = 0; i < CUSTOM_COUNT; ++i) {
        const std::string wave = "wavecode_" + std::to_string(i) + "_";
        for (const PresetKey& k : WAVE_KEYS) {
            double value;
            if (number(wave + k.key, k.integer, value)) out.waves[i].settings[k.name] = value;
        }
        const std::string shape = "shapecode_" + std::to_string(i) + "_";
        for (const PresetKey& k : SHAPE_KEYS) {
            double value;
            if (number(shape + k.key, k.integer, value)) out.shapes[i].settings[k.name] = value;
        }
        if (const std::string* image = find(shape + "image")) out.shapes[i].image = *image;
        const std::string n = std::to_string(i);
        out.waves[i].init = joinEquationLines(code("wave_" + n + "_init"));
        out.waves[i].perFrame = joinEquationLines(code("wave_" + n + "_per_frame"));
        out.waves[i].perPoint = joinEquationLines(code("wave_" + n + "_per_point"));
        out.shapes[i].init = joinEquationLines(code("shape_" + n + "_init"));
        out.shapes[i].perFrame = joinEquationLines(code("shape_" + n + "_per_frame"));
    }
    out.perFrameInit = joinEquationLines(code("per_frame_init_"));
    out.perFrame = joinEquationLines(code("per_frame_"));
    out.perPixel = joinEquationLines(code("per_pixel_"));
    out.warpShader = shaderCode("warp_");
    out.compShader = shaderCode("comp_");

    auto version = [&](const char* key, int fallback) {
        double value;
        return number(key, true, value) ? static_cast<int>(value) : fallback;
    };
    out.presetVersion = version("MILKDROP_PRESET_VERSION", 100);
    if (out.presetVersion < 200) {
        out.warpVersion = 0;
        out.compVersion = 0;
    } else if (out.presetVersion == 200) {
        out.warpVersion = out.compVersion = version("PSVERSION", 2);
    } else {
        out.warpVersion = version("PSVERSION_WARP", 2);
        out.compVersion = version("PSVERSION_COMP", 2);
    }
    return true;
}

void applyMilkwaveRules(PresetFile& file) {
    static const std::map<std::string, double> MILKWAVE_DEFAULTS = {
        {"decay", 0.75}, {"zoom", 0.9}, {"dx", 0.04}, {"warp", 0.0},
        {"wave_mode", 16.0}, {"wave_additive", 1.0}, {"wave_thick", 1.0}, {"wave_a", 0.4},
        {"mv_x", 0.0}, {"mv_y", 0.0}, {"mv_l", 0.0},
    };
    for (const auto& [name, value] : MILKWAVE_DEFAULTS) file.base.emplace(name, value);
    file.milkwave = true;
    constexpr int MIN_PS_VERSION = 3;
    constexpr int MAX_PS_VERSION = 4;
    if (file.presetVersion > 100) {
        file.warpVersion = std::clamp(file.warpVersion, MIN_PS_VERSION, MAX_PS_VERSION);
        file.compVersion = std::clamp(file.compVersion, MIN_PS_VERSION, MAX_PS_VERSION);
    }
}

PresetPlayer::PresetPlayer(const PresetFile& file)
    : m_name(file.name),
      m_hasShaders(!file.warpShader.empty() || !file.compShader.empty()),
      m_milkwave(file.milkwave),
      m_base(DEFAULTS) {
    for (const auto& [name, value] : file.base) m_base[name] = value;
    for (int i = 0; i < 32; ++i) {
        const std::string q = "q" + std::to_string(i + 1);
        m_frameQ[i] = m_frameVars.slot(q);
        m_pixelQ[i] = m_pixelVars.slot(q);
    }

    const char* positional[] = {"x", "y", "rad", "ang"};
    for (int i = 0; i < 4; ++i) m_vertexSlots[i] = m_pixelVars.slot(positional[i]);
    for (size_t i = 0; i < std::size(VERTEX_VARS); ++i) m_vertexSlots[4 + i] = m_pixelVars.slot(VERTEX_VARS[i]);

    std::string error;
    if (!m_init.compile(file.perFrameInit, m_frameVars, error)) m_errors.push_back("per_frame_init: " + error);
    if (!m_perFrame.compile(file.perFrame, m_frameVars, error)) m_errors.push_back("per_frame: " + error);
    if (!m_perPixel.compile(file.perPixel, m_pixelVars, error)) m_errors.push_back("per_pixel: " + error);

    for (int i = 0; i < CUSTOM_COUNT; ++i) {
        m_waves.push_back(std::make_unique<CustomWave>(file.waves[i], m_globals));
        for (const std::string& e : m_waves.back()->errors()) m_errors.push_back("wave " + std::to_string(i) + " " + e);
        m_shapes.push_back(std::make_unique<CustomShape>(file.shapes[i], m_globals));
        for (const std::string& e : m_shapes.back()->errors()) m_errors.push_back("shape " + std::to_string(i) + " " + e);
    }

    if (!m_perPixel.empty()) {
        m_perVertex = [this](VertexVars& v, const AudioLevels&, float) { runVertex(v); };
    }
}

void PresetPlayer::setReadOnly(VarTable& vars, const AudioLevels& audio, float time, float fps, int width, int height) {
    const float aspectX = (height > width) ? static_cast<float>(width) / height : 1.0f;
    const float aspectY = (width > height) ? static_cast<float>(height) / width : 1.0f;
    vars.set("time", time);
    vars.set("fps", fps);
    vars.set("frame", static_cast<double>(m_frame));
    vars.set("progress", m_progress);
    vars.set("bass", audio.bass);
    vars.set("mid", audio.mid);
    vars.set("treb", audio.treb);
    vars.set("bass_att", audio.bassAtt);
    vars.set("mid_att", audio.midAtt);
    vars.set("treb_att", audio.trebAtt);
    vars.set("meshx", MESH_X - 1);
    vars.set("meshy", MESH_Y - 1);
    vars.set("pixelsx", width);
    vars.set("pixelsy", height);
    // MilkDrop reports the inverses: 1 on the short side, and the aspect ratio on the long one.
    vars.set("aspectx", 1.0f / aspectX);
    vars.set("aspecty", 1.0f / aspectY);
    if (m_milkwave) {
        vars.set("bass_smooth", audio.bassSmooth);
        vars.set("mid_smooth", audio.midSmooth);
        vars.set("treb_smooth", audio.trebSmooth);
        vars.set("mousex", 0.5);
        vars.set("mousey", 0.5);
        vars.set("mousedown", 0.0);
        vars.set("mouseclick", 0.0);
    }
}

void PresetPlayer::loadFrameVars() {
    for (const char* name : FRAME_VARS) {
        auto it = m_base.find(name);
        m_frameVars.set(name, it == m_base.end() ? 0.0 : it->second);
    }
}

FrameVars PresetPlayer::runFrame(const AudioLevels& audio, float time, float fps, int width, int height) {
    loadFrameVars();
    setReadOnly(m_frameVars, audio, time, fps, width, height);
    if (!m_initialised) {
        // MilkDrop runs per_frame_init when it loads the preset, then each custom wave's and
        // shape's init code with the q values per_frame_init left.
        m_init.run();
        for (int i = 0; i < 32; ++i) m_qInit[i] = *m_frameQ[i];
        m_initialised = true;
        CustomFrameInputs init;
        init.time = time;
        init.fps = fps;
        init.frame = static_cast<float>(m_frame);
        init.progress = m_progress;
        init.audio = audio;
        for (int i = 0; i < 32; ++i) init.q[i] = m_qInit[i];
        for (auto& wave : m_waves) wave->init(init);
        for (auto& shape : m_shapes) shape->init(init);
        loadFrameVars();
    }
    for (int i = 0; i < 32; ++i) *m_frameQ[i] = m_qInit[i];
    m_perFrame.run();
    // MilkDrop's range checks after per_frame, with Windows' min and max, which pass NaN through.
    auto windowsClamp = [](double x, double lo, double hi) {
        const double upper = hi < x ? hi : x;
        return lo > upper ? lo : upper;
    };
    m_frameVars.set("gamma", windowsClamp(m_frameVars.get("gamma"), 0.0, 8.0));
    m_frameVars.set("echo_zoom", windowsClamp(m_frameVars.get("echo_zoom"), 0.001, 1000.0));

    VarTable& v = m_frameVars;
    auto get = [&](const char* name) {
        const auto fallback = m_base.find(name);
        return finiteOr(v.get(name), fallback == m_base.end() ? 0.0f : static_cast<float>(fallback->second));
    };
    FrameVars f;
    f.zoom = get("zoom");
    f.zoomExp = get("zoomexp");
    f.rot = get("rot");
    f.warp = get("warp");
    // These come from the preset file only. MilkDrop has no per_frame variables for them.
    f.warpSpeed = static_cast<float>(m_base["warpanimspeed"]);
    f.warpScale = static_cast<float>(m_base["warpscale"]);
    f.cx = get("cx");
    f.cy = get("cy");
    f.dx = get("dx");
    f.dy = get("dy");
    f.sx = get("sx");
    f.sy = get("sy");
    f.decay = get("decay");
    f.waveMode = waveModeFor(get("wave_mode"));
    f.waveScale = static_cast<float>(m_base["wave_scale"]);
    f.waveX = get("wave_x");
    // MilkDrop's wave_y runs from the bottom of the screen.
    f.waveY = 1.0f - get("wave_y");
    f.waveR = std::clamp(get("wave_r"), 0.0f, 1.0f);
    f.waveG = std::clamp(get("wave_g"), 0.0f, 1.0f);
    f.waveB = std::clamp(get("wave_b"), 0.0f, 1.0f);
    if (std::fabs(get("wave_brighten")) > 0.00001f) {
        const float peak = std::max({f.waveR, f.waveG, f.waveB});
        if (peak > 0.0f) {
            f.waveR /= peak;
            f.waveG /= peak;
            f.waveB /= peak;
        }
    }
    f.waveA = get("wave_a");
    f.waveThick = get("wave_thick") != 0.0f;
    f.wrap = get("wrap") > 0.5f;
    m_gamma = get("gamma");
    // MilkDrop applies no limits to these, and the warp wraps a decay above 1 itself, so a negative
    // zoom still mirrors the image.
    const float decay = f.decay, zoom = f.zoom, sx = f.sx, sy = f.sy;
    clampFrame(f);
    f.decay = decay;
    f.zoom = zoom;
    f.sx = sx;
    f.sy = sy;

    m_customInputs.time = time;
    m_customInputs.fps = fps;
    m_customInputs.frame = static_cast<float>(m_frame);
    m_customInputs.progress = static_cast<float>(v.get("progress"));
    m_customInputs.audio = audio;
    for (int i = 0; i < 32; ++i) m_customInputs.q[i] = *m_frameQ[i];
    m_customInputs.waveScale = static_cast<float>(m_base["wave_scale"]);

    if (!m_perPixel.empty()) {
        setReadOnly(m_pixelVars, audio, time, fps, width, height);
        for (int i = 0; i < 32; ++i) *m_pixelQ[i] = *m_frameQ[i];
    }
    ++m_frame;
    return f;
}

void PresetPlayer::blendFrom(PresetPlayer& old, float mix, float snap) {
    VarTable& n = m_frameVars;
    VarTable& o = old.m_frameVars;
    const double m = mix, m2 = 1.0 - mix;
    // Border alpha counts only for a preset whose border has a size, as Milkwave gates it.
    auto gated = [](PresetPlayer& p, const char* size, const char* alpha) {
        auto it = p.m_base.find(size);
        return (it != p.m_base.end() && it->second > 0.001) ? p.m_frameVars.get(alpha) : 0.0;
    };
    const double obA = m * gated(*this, "ob_size", "ob_a") + m2 * gated(old, "ob_size", "ob_a");
    const double ibA = m * gated(*this, "ib_size", "ib_a") + m2 * gated(old, "ib_size", "ib_a");
    static const char* MIXED[] = {
        "decay", "wave_a", "wave_r", "wave_g", "wave_b", "wave_x", "wave_y", "wave_mystery", "ob_size", "ob_r", "ob_g",
        "ob_b", "ib_size", "ib_r", "ib_g", "ib_b", "mv_x", "mv_y", "mv_dx", "mv_dy", "mv_l", "mv_r", "mv_g", "mv_b",
        "mv_a", "echo_zoom", "echo_alpha", "gamma", "blur1_min", "blur2_min", "blur3_min", "blur1_max", "blur2_max",
        "blur3_max", "blur1_edge_darken",
    };
    static const char* SNAPPED[] = {
        "echo_orient", "wave_usedots", "wave_thick", "wave_additive", "wave_brighten", "darken_center", "wrap", "invert",
        "brighten", "darken", "solarize",
    };
    for (const char* name : MIXED) n.set(name, m * n.get(name) + m2 * o.get(name));
    for (const char* name : SNAPPED) {
        if (mix < snap) n.set(name, o.get(name));
    }
    n.set("ob_a", obA);
    n.set("ib_a", ibA);
    m_gamma = static_cast<float>(n.get("gamma"));
}

void PresetPlayer::runVertex(VertexVars& v) {
    float* fields[] = {&v.x, &v.y, &v.rad, &v.ang, &v.zoom, &v.zoomExp, &v.rot, &v.warp,
                       &v.cx, &v.cy, &v.dx, &v.dy, &v.sx, &v.sy};
    for (int i = 0; i < 14; ++i) *m_vertexSlots[i] = *fields[i];
    m_perPixel.run();
    for (int i = 4; i < 14; ++i) *fields[i] = finiteOr(*m_vertexSlots[i], *fields[i]);
}

} // namespace Milk

namespace Milk {

void PresetPlayer::runCustomShapes(std::vector<CustomShapeInstance>& out) {
    out.clear();
    for (int i = 0; i < CUSTOM_COUNT; ++i) m_shapes[i]->run(i, m_customInputs, out);
}

void PresetPlayer::runCustomWaves(const MilkAudioFrame& audio, std::vector<CustomWaveOutput>& out) {
    out.clear();
    for (auto& wave : m_waves) {
        CustomWaveOutput o;
        if (wave->run(m_customInputs, audio, o)) out.push_back(std::move(o));
    }
}

} // namespace Milk

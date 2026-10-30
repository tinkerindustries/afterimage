#include "milk_preset.h"

#include "preset.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <regex>
#include <fstream>

namespace Milk {

namespace {

// Base-value keys whose variable name differs from the key.
const std::map<std::string, std::string> KEY_TO_VAR = {
    {"fdecay", "decay"},
    {"fgammaadj", "gamma"},
    {"fvideoechozoom", "echo_zoom"},
    {"fvideoechoalpha", "echo_alpha"},
    {"nvideoechoorientation", "echo_orient"},
    {"nwavemode", "wave_mode"},
    {"badditivewaves", "wave_additive"},
    {"bwavedots", "wave_usedots"},
    {"bwavethick", "wave_thick"},
    {"bmaximizewavecolor", "wave_brighten"},
    {"btexwrap", "wrap"},
    {"bdarkencenter", "darken_center"},
    {"bredbluestereo", "red_blue"},
    {"bbrighten", "brighten"},
    {"bdarken", "darken"},
    {"bsolarize", "solarize"},
    {"binvert", "invert"},
    {"fwavealpha", "wave_a"},
    {"fwavescale", "wave_scale"},
    {"fwavesmoothing", "wave_smoothing"},
    {"fwaveparam", "wave_mystery"},
    {"fwarpanimspeed", "warpanimspeed"},
    {"fwarpscale", "warpscale"},
    {"fzoomexponent", "zoomexp"},
    {"nmotionvectorsx", "mv_x"},
    {"nmotionvectorsy", "mv_y"},
    {"b1n", "blur1_min"},
    {"b2n", "blur2_min"},
    {"b3n", "blur3_min"},
    {"b1x", "blur1_max"},
    {"b2x", "blur2_max"},
    {"b3x", "blur3_max"},
    {"b1ed", "blur1_edge_darken"},
};

// MilkDrop's values for variables a preset leaves out.
const std::map<std::string, double> DEFAULTS = {
    {"zoom", 1.0}, {"zoomexp", 1.0}, {"rot", 0.0}, {"warp", 1.0}, {"cx", 0.5}, {"cy", 0.5},
    {"dx", 0.0}, {"dy", 0.0}, {"sx", 1.0}, {"sy", 1.0}, {"decay", 0.98}, {"gamma", 2.0},
    {"wave_mode", 0.0}, {"wave_x", 0.5}, {"wave_y", 0.5}, {"wave_r", 1.0}, {"wave_g", 1.0},
    {"wave_b", 1.0}, {"wave_a", 0.8}, {"wave_scale", 1.0}, {"wave_thick", 0.0},
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

bool startsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

// Parses the line number after `prefix`, or returns -1 when the rest is not all digits.
int lineIndex(const std::string& key, const char* prefix) {
    if (!startsWith(key, prefix)) return -1;
    const std::string rest = key.substr(std::strlen(prefix));
    if (rest.empty() || !std::all_of(rest.begin(), rest.end(), ::isdigit)) return -1;
    return std::stoi(rest);
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
// `beat`) is whole again. A line that ends without ';' runs into the next one, and the statement
// that makes fails to compile and is dropped, as it is in MilkDrop.
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

bool parseNumber(const std::string& s, double& out) {
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    if (end == s.c_str()) return false;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) ++end;
    return *end == '\0';
}

struct CustomCode {
    std::map<int, std::string> init, perFrame, perPoint;
};

// Reads "<prefix><n>_<rest>" with n in 0..3. Returns n, or -1 if the key does not match.
int customIndex(const std::string& key, const char* prefix, std::string& rest) {
    if (!startsWith(key, prefix)) return -1;
    const size_t at = std::strlen(prefix);
    if (key.size() < at + 3 || !std::isdigit(static_cast<unsigned char>(key[at])) || key[at + 1] != '_') return -1;
    const int n = key[at] - '0';
    if (n >= CUSTOM_COUNT) return -1;
    rest = key.substr(at + 2);
    return n;
}

// Handles the wavecode_N_, shapecode_N_, wave_N_ and shape_N_ keys. Returns false for any other key.
bool parseCustomLine(const std::string& key, const std::string& value, PresetFile& out, CustomCode* waveCode,
                     CustomCode* shapeCode) {
    std::string rest;
    int n;
    double number;
    if ((n = customIndex(key, "wavecode_", rest)) >= 0) {
        if (parseNumber(value, number)) out.waves[n].settings[rest] = number;
        return true;
    }
    if ((n = customIndex(key, "shapecode_", rest)) >= 0) {
        if (rest == "image") out.shapes[n].image = value;
        else if (parseNumber(value, number)) out.shapes[n].settings[rest] = number;
        return true;
    }
    int line;
    if ((n = customIndex(key, "wave_", rest)) >= 0) {
        if ((line = lineIndex(rest, "init")) >= 0) waveCode[n].init[line] = value;
        else if ((line = lineIndex(rest, "per_frame")) >= 0) waveCode[n].perFrame[line] = value;
        else if ((line = lineIndex(rest, "per_point")) >= 0) waveCode[n].perPoint[line] = value;
        return true;
    }
    if ((n = customIndex(key, "shape_", rest)) >= 0) {
        if ((line = lineIndex(rest, "init")) >= 0) shapeCode[n].init[line] = value;
        else if ((line = lineIndex(rest, "per_frame")) >= 0) shapeCode[n].perFrame[line] = value;
        return true;
    }
    return false;
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

    std::map<int, std::string> perFrameInit, perFrame, perPixel, warp, comp;
    CustomCode waveCode[CUSTOM_COUNT], shapeCode[CUSTOM_COUNT];
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '[') continue;
        const std::string key = lower(line.substr(0, eq));
        std::string value = line.substr(eq + 1);

        int index;
        if ((index = lineIndex(key, "per_frame_init_")) >= 0) perFrameInit[index] = value;
        else if ((index = lineIndex(key, "per_frame_")) >= 0) perFrame[index] = value;
        else if ((index = lineIndex(key, "per_pixel_")) >= 0) perPixel[index] = value;
        else if ((index = lineIndex(key, "warp_")) >= 0) warp[index] = value.empty() || value[0] != '`' ? value : value.substr(1);
        else if ((index = lineIndex(key, "comp_")) >= 0) comp[index] = value.empty() || value[0] != '`' ? value : value.substr(1);
        else if (parseCustomLine(key, value, out, waveCode, shapeCode)) {
            continue;
        } else {
            double number;
            if (!parseNumber(value, number)) continue;
            auto alias = KEY_TO_VAR.find(key);
            out.base[alias == KEY_TO_VAR.end() ? key : alias->second] = number;
        }
    }
    out.perFrameInit = joinEquationLines(perFrameInit);
    out.perFrame = joinEquationLines(perFrame);
    out.perPixel = joinEquationLines(perPixel);
    out.warpShader = joinLines(warp);
    out.compShader = joinLines(comp);
    auto version = [&](const char* key, int fallback) {
        auto it = out.base.find(key);
        return it == out.base.end() ? fallback : static_cast<int>(it->second);
    };
    const int presetVersion = version("milkdrop_preset_version", 100);
    if (presetVersion < 200) {
        out.warpVersion = 0;
        out.compVersion = 0;
    } else if (presetVersion == 200) {
        out.warpVersion = out.compVersion = version("psversion", 2);
    } else {
        out.warpVersion = version("psversion_warp", 2);
        out.compVersion = version("psversion_comp", 2);
    }
    for (int i = 0; i < CUSTOM_COUNT; ++i) {
        out.waves[i].init = joinEquationLines(waveCode[i].init);
        out.waves[i].perFrame = joinEquationLines(waveCode[i].perFrame);
        out.waves[i].perPoint = joinEquationLines(waveCode[i].perPoint);
        out.shapes[i].init = joinEquationLines(shapeCode[i].init);
        out.shapes[i].perFrame = joinEquationLines(shapeCode[i].perFrame);
    }
    return true;
}

PresetPlayer::PresetPlayer(const PresetFile& file)
    : m_name(file.name),
      m_hasShaders(!file.warpShader.empty() || !file.compShader.empty()),
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
    vars.set("progress", std::clamp((time - std::max(m_startTime, 0.0f)) / 30.0f, 0.0f, 1.0f));
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
}

FrameVars PresetPlayer::runFrame(const AudioLevels& audio, float time, float fps, int width, int height) {
    if (m_startTime < 0.0f) m_startTime = time;
    for (const auto& [name, value] : m_base) m_frameVars.set(name, value);
    setReadOnly(m_frameVars, audio, time, fps, width, height);
    if (!m_initialised) {
        m_init.run();
        for (int i = 0; i < 32; ++i) m_qInit[i] = *m_frameQ[i];
        m_initialised = true;
        for (const auto& [name, value] : m_base) m_frameVars.set(name, value);
    }
    for (int i = 0; i < 32; ++i) *m_frameQ[i] = m_qInit[i];
    m_perFrame.run();

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
    f.warpSpeed = get("warpanimspeed");
    f.warpScale = get("warpscale");
    f.cx = get("cx");
    f.cy = get("cy");
    f.dx = get("dx");
    f.dy = get("dy");
    f.sx = get("sx");
    f.sy = get("sy");
    f.decay = get("decay");
    f.waveMode = waveModeFor(get("wave_mode"));
    f.waveScale = get("wave_scale");
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
    f.waveThick = std::fabs(get("wave_thick")) > 0.00001f;
    f.wrap = std::fabs(get("wrap")) > 0.00001f;
    m_gamma = get("gamma");
    // The warp wraps a decay above 1 itself, as MilkDrop does, so clampFrame must not cut it.
    const float decay = f.decay;
    clampFrame(f);
    f.decay = decay;

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

void PresetPlayer::runVertex(VertexVars& v) {
    float* fields[] = {&v.x, &v.y, &v.rad, &v.ang, &v.zoom, &v.zoomExp, &v.rot, &v.warp,
                       &v.cx, &v.cy, &v.dx, &v.dy, &v.sx, &v.sy};
    for (int i = 0; i < 14; ++i) *m_vertexSlots[i] = *fields[i];
    m_perPixel.run();
    for (int i = 4; i < 14; ++i) *fields[i] = finiteOr(*m_vertexSlots[i], *fields[i]);
    if (std::fabs(v.sx) < 0.01f) v.sx = 0.01f;
    if (std::fabs(v.sy) < 0.01f) v.sy = 0.01f;
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

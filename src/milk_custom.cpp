#include "milk_custom.h"

#include <algorithm>
#include <cmath>

namespace Milk {

namespace {

constexpr int MAX_WAVE_POINTS = 512;
constexpr int MAX_SHAPE_INSTANCES = 1024;

// MilkDrop packs each colour channel as ((int)(v * 255)) & 0xFF, so values above 1 or below 0 wrap
// and a fraction of a step truncates. Out of int range, x86 gives INT_MIN, whose low byte is 0.
float wrapColour(double value) {
    const double scaled = value * 255.0;
    if (!std::isfinite(scaled) || std::fabs(scaled) >= 2147483648.0) return 0.0f;
    return static_cast<float>(static_cast<int>(scaled) & 0xFF) / 255.0f;
}

// MilkDrop casts shape flags to int, so 0.5 is off.
bool flag(double value) { return std::isfinite(value) && std::fabs(value) < 2147483648.0 && static_cast<int>(value) != 0; }

float finiteOr(double value, float fallback) { return std::isfinite(value) ? static_cast<float>(value) : fallback; }

void setReadOnly(VarTable& vars, const CustomFrameInputs& in) {
    vars.set("time", in.time);
    vars.set("fps", in.fps);
    vars.set("frame", in.frame);
    vars.set("progress", in.progress);
    vars.set("bass", in.audio.bass);
    vars.set("mid", in.audio.mid);
    vars.set("treb", in.audio.treb);
    vars.set("bass_att", in.audio.bassAtt);
    vars.set("mid_att", in.audio.midAtt);
    vars.set("treb_att", in.audio.trebAtt);
}

void setQ(VarTable& vars, const std::array<double, 32>& q) {
    for (int i = 0; i < 32; ++i) vars.set("q" + std::to_string(i + 1), q[i]);
}

void setT(VarTable& vars, const std::array<double, 8>& t) {
    for (int i = 0; i < 8; ++i) vars.set("t" + std::to_string(i + 1), t[i]);
}

std::array<double, 8> getT(VarTable& vars) {
    std::array<double, 8> t{};
    for (int i = 0; i < 8; ++i) t[i] = vars.get("t" + std::to_string(i + 1));
    return t;
}

void compile(Program& program, const std::string& code, VarTable& vars, const char* label, std::vector<std::string>& errors) {
    std::string error;
    if (!program.compile(code, vars, error)) errors.push_back(std::string(label) + ": " + error);
}

// Inserts a point between each pair, placed by a four-tap filter, as MilkDrop does before
// drawing a custom wave. The new point takes the colour of the one before it.
void smoothWave(const std::vector<CustomWavePoint>& in, std::vector<CustomWavePoint>& out) {
    constexpr float C1 = -0.15f, C2 = 1.15f, C3 = 1.15f, C4 = -0.15f;
    constexpr float INV_SUM = 1.0f / (C1 + C2 + C3 + C4);
    out.clear();
    const size_t n = in.size();
    for (size_t i = 0; i + 1 < n; ++i) {
        const size_t below = i == 0 ? 0 : i - 1;
        const size_t above = i + 1;
        const size_t above2 = std::min(n - 1, i + 2);
        out.push_back(in[i]);
        CustomWavePoint mid = in[i];
        mid.x = (C1 * in[below].x + C2 * in[i].x + C3 * in[above].x + C4 * in[above2].x) * INV_SUM;
        mid.y = (C1 * in[below].y + C2 * in[i].y + C3 * in[above].y + C4 * in[above2].y) * INV_SUM;
        out.push_back(mid);
    }
    if (n > 0) out.push_back(in[n - 1]);
}

} // namespace

CustomWave::CustomWave(const CustomWaveDef& def, ExprGlobals& globals)
    : m_settings(def.settings), m_frameVars(globals), m_pointVars(globals) {
    m_enabled = setting("enabled", 0.0) != 0.0;
    std::vector<std::string> errors;
    compile(m_init, def.init, m_frameVars, "wave init", errors);
    compile(m_perFrame, def.perFrame, m_frameVars, "wave per_frame", errors);
    compile(m_perPoint, def.perPoint, m_pointVars, "wave per_point", errors);
    if (m_enabled) m_errors = std::move(errors);
}

double CustomWave::setting(const char* name, double fallback) const {
    auto it = m_settings.find(name);
    return it == m_settings.end() ? fallback : it->second;
}

void CustomWave::init(const CustomFrameInputs& in) {
    VarTable& f = m_frameVars;
    setReadOnly(f, in);
    setQ(f, in.q);
    setT(f, m_tInit);
    f.set("r", setting("r", 1.0));
    f.set("g", setting("g", 1.0));
    f.set("b", setting("b", 1.0));
    f.set("a", setting("a", 1.0));
    f.set("samples", setting("samples", MAX_WAVE_POINTS));
    m_init.run();
    m_tInit = getT(f);
}

bool CustomWave::run(const CustomFrameInputs& in, const MilkAudioFrame& audio, CustomWaveOutput& out) {
    if (!m_enabled) return false;
    VarTable& f = m_frameVars;
    setReadOnly(f, in);
    setQ(f, in.q);
    setT(f, m_tInit);
    f.set("r", setting("r", 1.0));
    f.set("g", setting("g", 1.0));
    f.set("b", setting("b", 1.0));
    f.set("a", setting("a", 1.0));
    f.set("samples", setting("samples", MAX_WAVE_POINTS));
    m_perFrame.run();

    const bool spectrum = setting("bspectrum", 0.0) != 0.0;
    const double samples = f.get("samples");
    const int sampleCount = std::isfinite(samples) ? static_cast<int>(std::clamp(samples, -1.0, static_cast<double>(MAX_WAVE_POINTS))) : 0;
    const int separation = std::clamp(static_cast<int>(setting("sep", 0.0)), 0, 1024);
    out.dots = setting("busedots", 0.0) != 0.0;
    out.thick = setting("bdrawthick", 0.0) != 0.0;
    out.additive = setting("badditive", 0.0) != 0.0;
    if (out.dots ? sampleCount < 1 : sampleCount < 2) return false;

    // MilkDrop reads the raw samples (or the spectrum) from a window centred on the first
    // MILK_WAVE_SAMPLES, offset by half the separation for each channel.
    const float* pcmL = spectrum ? audio.spectrumL.data() : audio.waveL.data();
    const float* pcmR = spectrum ? audio.spectrumR.data() : audio.waveR.data();
    const int available = spectrum ? MILK_SPECTRUM_SAMPLES : MILK_FFT_INPUT;
    const int maxSamples = spectrum ? MILK_SPECTRUM_SAMPLES : MILK_WAVE_SAMPLES;
    const float mult = static_cast<float>(setting("scaling", 1.0)) * in.waveScale * (spectrum ? 0.15f : 0.004f);
    const int offsetL = spectrum ? 0 : (maxSamples - sampleCount) / 2 - separation / 2;
    const int offsetR = spectrum ? 0 : (maxSamples - sampleCount) / 2 + separation / 2;
    const float step = spectrum ? static_cast<float>(maxSamples - separation) / sampleCount : 1.0f;
    // A negative smoothing gives NaN here, and the wave disappears, as in MilkDrop.
    const float mix1 = std::pow(static_cast<float>(setting("smoothing", 0.5)) * 0.98f, 0.5f);
    const float mix2 = 1.0f - mix1;
    // A 512-sample waveform wave starts 16 samples before the waveform. MilkDrop reads there what
    // sits before each channel in memory: the shell's band averages before the left channel, and
    // the end of the left channel before the right.
    auto sampleL = [&](int i) {
        const int at = static_cast<int>(i * step) + offsetL;
        if (at < 0) return at >= -30 ? audio.beforeWave[30 + at] : 0.0f;
        return pcmL[std::min(at, available - 1)];
    };
    auto sampleR = [&](int i) {
        const int at = static_cast<int>(i * step) + offsetR;
        if (at < 0) return at >= -MILK_FFT_INPUT ? audio.waveL[MILK_FFT_INPUT + at] : 0.0f;
        return pcmR[std::min(at, available - 1)];
    };

    std::vector<float> l(sampleCount), r(sampleCount);
    l[0] = sampleL(0);
    r[0] = sampleR(0);
    for (int i = 1; i < sampleCount; ++i) {
        l[i] = sampleL(i) * mix2 + l[i - 1] * mix1;
        r[i] = sampleR(i) * mix2 + r[i - 1] * mix1;
    }
    for (int i = sampleCount - 2; i >= 0; --i) {
        l[i] = l[i] * mix2 + l[i + 1] * mix1;
        r[i] = r[i] * mix2 + r[i + 1] * mix1;
    }

    VarTable& p = m_pointVars;
    setReadOnly(p, in);
    for (int i = 0; i < 32; ++i) {
        const std::string q = "q" + std::to_string(i + 1);
        p.set(q, f.get(q));
    }
    for (int i = 0; i < 8; ++i) {
        const std::string t = "t" + std::to_string(i + 1);
        p.set(t, f.get(t));
    }
    double* sample = p.slot("sample");
    double* value1 = p.slot("value1");
    double* value2 = p.slot("value2");
    double* x = p.slot("x");
    double* y = p.slot("y");
    double* colour[4] = {p.slot("r"), p.slot("g"), p.slot("b"), p.slot("a")};
    const double frameColour[4] = {f.get("r"), f.get("g"), f.get("b"), f.get("a")};

    std::vector<CustomWavePoint> points(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        const float v1 = l[i] * mult;
        const float v2 = r[i] * mult;
        // With one sample this is 0 * infinity, which is NaN in MilkDrop too.
        *sample = static_cast<double>(i) * (1.0 / static_cast<double>(sampleCount - 1));
        *value1 = v1;
        *value2 = v2;
        *x = 0.5 + v1;
        *y = 0.5 + v2;
        for (int c = 0; c < 4; ++c) *colour[c] = frameColour[c];
        m_perPoint.run();
        // A point with a non-finite position is not drawn, as Direct3D 9 drops it.
        points[i] = CustomWavePoint{static_cast<float>(*x), static_cast<float>(*y), wrapColour(*colour[0]), wrapColour(*colour[1]),
                                    wrapColour(*colour[2]), wrapColour(*colour[3])};
    }
    if (out.dots) {
        out.points = std::move(points);
    } else {
        smoothWave(points, out.points);
    }
    return true;
}

CustomShape::CustomShape(const CustomShapeDef& def, ExprGlobals& globals)
    : m_settings(def.settings), m_image(def.image), m_vars(globals) {
    m_enabled = setting("enabled", 0.0) != 0.0;
    std::vector<std::string> errors;
    compile(m_init, def.init, m_vars, "shape init", errors);
    compile(m_perFrame, def.perFrame, m_vars, "shape per_frame", errors);
    if (m_enabled) m_errors = std::move(errors);
}

double CustomShape::setting(const char* name, double fallback) const {
    auto it = m_settings.find(name);
    return it == m_settings.end() ? fallback : it->second;
}

void CustomShape::loadState(const CustomFrameInputs& in, int instance) {
    VarTable& v = m_vars;
    setReadOnly(v, in);
    setQ(v, in.q);
    setT(v, m_tInit);
    v.set("x", setting("x", 0.5));
    v.set("y", setting("y", 0.5));
    v.set("rad", setting("rad", 0.1));
    v.set("ang", setting("ang", 0.0));
    v.set("tex_zoom", setting("tex_zoom", 1.0));
    v.set("tex_ang", setting("tex_ang", 0.0));
    v.set("sides", setting("sides", 4.0));
    v.set("additive", setting("additive", 0.0));
    v.set("textured", setting("textured", 0.0));
    v.set("num_inst", setting("num_inst", 1.0));
    v.set("instance", instance);
    v.set("thick", setting("thickoutline", 0.0));
    v.set("r", setting("r", 1.0));
    v.set("g", setting("g", 0.0));
    v.set("b", setting("b", 0.0));
    v.set("a", setting("a", 1.0));
    v.set("r2", setting("r2", 0.0));
    v.set("g2", setting("g2", 1.0));
    v.set("b2", setting("b2", 0.0));
    v.set("a2", setting("a2", 0.0));
    v.set("border_r", setting("border_r", 1.0));
    v.set("border_g", setting("border_g", 1.0));
    v.set("border_b", setting("border_b", 1.0));
    v.set("border_a", setting("border_a", 0.1));
}

void CustomShape::init(const CustomFrameInputs& in) {
    loadState(in, 0);
    m_init.run();
    m_tInit = getT(m_vars);
}

void CustomShape::run(int index, const CustomFrameInputs& in, std::vector<CustomShapeInstance>& out) {
    if (!m_enabled) return;
    const int instances = std::clamp(static_cast<int>(setting("num_inst", 1.0)), 1, MAX_SHAPE_INSTANCES);
    VarTable& v = m_vars;
    for (int i = 0; i < instances; ++i) {
        loadState(in, i);
        m_perFrame.run();
        CustomShapeInstance s;
        s.shape = index;
        s.x = finiteOr(v.get("x"), 0.5f);
        s.y = finiteOr(v.get("y"), 0.5f);
        s.rad = finiteOr(v.get("rad"), 0.1f);
        s.ang = finiteOr(v.get("ang"), 0.0f);
        s.sides = std::clamp(static_cast<int>(finiteOr(v.get("sides"), 4.0f)), 3, 100);
        s.textured = flag(v.get("textured"));
        s.additive = flag(v.get("additive"));
        s.thick = flag(v.get("thick"));
        s.texZoom = finiteOr(v.get("tex_zoom"), 1.0f);
        if (std::fabs(s.texZoom) < 1e-4f) s.texZoom = 1e-4f;
        s.texAng = finiteOr(v.get("tex_ang"), 0.0f);
        s.r = wrapColour(v.get("r"));
        s.g = wrapColour(v.get("g"));
        s.b = wrapColour(v.get("b"));
        s.a = wrapColour(v.get("a"));
        s.r2 = wrapColour(v.get("r2"));
        s.g2 = wrapColour(v.get("g2"));
        s.b2 = wrapColour(v.get("b2"));
        s.a2 = wrapColour(v.get("a2"));
        s.borderR = wrapColour(v.get("border_r"));
        s.borderG = wrapColour(v.get("border_g"));
        s.borderB = wrapColour(v.get("border_b"));
        s.borderA = wrapColour(v.get("border_a"));
        s.border = v.get("border_a") > 0.0;
        out.push_back(s);
    }
}

} // namespace Milk

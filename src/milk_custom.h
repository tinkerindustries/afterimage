#pragma once

#include "audio_synth.h"
#include "milk_audio.h"
#include "milk_expr.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace Milk {

constexpr int CUSTOM_COUNT = 4;

// One wavecode_N block of a .milk file. Settings are keyed by the lower-case name after the
// prefix (samples, bspectrum, scaling and so on).
struct CustomWaveDef {
    std::map<std::string, double> settings;
    std::string init;
    std::string perFrame;
    std::string perPoint;
};

// One shapecode_N block.
struct CustomShapeDef {
    std::map<std::string, double> settings;
    std::string image;
    std::string init;
    std::string perFrame;
};

// What every custom wave and shape reads in a frame, after the preset's per_frame code ran.
struct CustomFrameInputs {
    float time = 0.0f;
    float fps = 60.0f;
    float frame = 0.0f;
    float progress = 0.0f;
    AudioLevels audio;
    std::array<double, 32> q{};
    float waveScale = 1.0f;
};

// A custom wave point in MilkDrop's wave space: x 0..1 left to right, y 0..1 bottom to top.
struct CustomWavePoint {
    float x, y, r, g, b, a;
};

struct CustomWaveOutput {
    std::vector<CustomWavePoint> points;
    bool dots = false;
    bool thick = false;
    bool additive = false;
};

// One drawn instance of a custom shape. x and y are 0..1 with y up; rad is a fraction of the
// screen height.
struct CustomShapeInstance {
    int shape;  // which of the preset's shapes, 0..3
    float x, y, rad, ang;
    int sides;
    bool textured, additive, thick;
    float texZoom, texAng;
    float r, g, b, a;
    float r2, g2, b2, a2;
    float borderR, borderG, borderB, borderA;
};

// Runs one custom wave's init, per_frame and per_point code each frame.
class CustomWave {
public:
    CustomWave(const CustomWaveDef& def, ExprGlobals& globals);
    CustomWave(const CustomWave&) = delete;
    CustomWave& operator=(const CustomWave&) = delete;

    bool enabled() const { return m_enabled; }
    const std::vector<std::string>& errors() const { return m_errors; }
    // Returns false when the wave draws nothing this frame.
    bool run(const CustomFrameInputs& in, const MilkAudioFrame& audio, CustomWaveOutput& out);

private:
    double setting(const char* name, double fallback) const;

    std::map<std::string, double> m_settings;
    bool m_enabled;
    std::vector<std::string> m_errors;
    VarTable m_frameVars;
    VarTable m_pointVars;
    Program m_init;
    Program m_perFrame;
    Program m_perPoint;
    bool m_initialised = false;
    std::array<double, 8> m_tInit{};
};

// Runs one custom shape's init code once and its per_frame code for every instance.
class CustomShape {
public:
    CustomShape(const CustomShapeDef& def, ExprGlobals& globals);
    CustomShape(const CustomShape&) = delete;
    CustomShape& operator=(const CustomShape&) = delete;

    bool enabled() const { return m_enabled; }
    const std::string& image() const { return m_image; }
    const std::vector<std::string>& errors() const { return m_errors; }
    void run(int index, const CustomFrameInputs& in, std::vector<CustomShapeInstance>& out);

private:
    void loadState(const CustomFrameInputs& in, int instance);
    double setting(const char* name, double fallback) const;

    std::map<std::string, double> m_settings;
    std::string m_image;
    bool m_enabled;
    std::vector<std::string> m_errors;
    VarTable m_vars;
    Program m_init;
    Program m_perFrame;
    bool m_initialised = false;
    std::array<double, 8> m_tInit{};
};

} // namespace Milk

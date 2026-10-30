#pragma once

#include "audio_synth.h"
#include "feedback.h"
#include "milk_custom.h"
#include "milk_expr.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Milk {

// The parts of a .milk file this project reads. Base values are keyed by the variable name the
// equations use (fDecay becomes decay, nWaveMode becomes wave_mode, and so on).
struct PresetFile {
    std::string name;
    std::map<std::string, double> base;
    std::string perFrameInit;
    std::string perFrame;
    std::string perPixel;
    std::string warpShader;
    std::string compShader;
    // Pixel shader versions as MilkDrop reads them. 0 means the stage runs without a shader: the
    // fixed warp, or MilkDrop 1's composite with gamma, hue tint, video echo and filters.
    int warpVersion = 2;
    int compVersion = 2;
    int presetVersion = 100; // MILKDROP_PRESET_VERSION
    // Set by applyMilkwaveRules: per_frame code also sees Milkwave's bass_smooth, mid_smooth,
    // treb_smooth and mouse variables.
    bool milkwave = false;
    std::array<CustomWaveDef, CUSTOM_COUNT> waves;
    std::array<CustomShapeDef, CUSTOM_COUNT> shapes;
};

bool loadPresetFile(const std::string& path, PresetFile& out, std::string& error);

// Applies the changes Milkwave 4.1 makes to stock MilkDrop 2 when it loads a preset, with its
// shipped settings: MilkDrop 2 presets run at pixel shader version 3 or more, capped at 4, and
// keys a preset leaves out take Milkwave's defaults.
void applyMilkwaveRules(PresetFile& file);

// Runs one preset's equations the way MilkDrop does. Each frame the preset's base values and the
// q variables left by per_frame_init are restored, per_frame runs, and its results seed the
// per_pixel code, which runs once per mesh vertex.
class PresetPlayer {
public:
    explicit PresetPlayer(const PresetFile& file);
    PresetPlayer(const PresetPlayer&) = delete;
    PresetPlayer& operator=(const PresetPlayer&) = delete;

    const std::string& name() const { return m_name; }
    bool hasShaders() const { return m_hasShaders; }
    // One line per code block that failed to compile in full.
    const std::vector<std::string>& errors() const { return m_errors; }

    // Runs per_frame for one frame and returns the variables for the warp and the wave.
    // Sets the frame counter that `frame` reads, which MilkDrop keeps across presets.
    void setFrame(uint64_t frame) { m_frame = frame; }
    // MilkDrop's `progress`: how far the preset is through its time before the next automatic change.
    void setProgress(float progress) { m_progress = progress; }

    FrameVars runFrame(const AudioLevels& audio, float time, float fps, int width, int height);
    // During a blend from `old`, replaces this frame's per-frame values that do not move pixels
    // with MilkDrop's mix of the two presets': numbers cross-fade by `mix`, and switches take the
    // old preset's value while `mix` is below `snap`. Call after both presets' runFrame.
    void blendFrom(PresetPlayer& old, float mix, float snap);
    float gamma() const { return m_gamma; }
    // q1..q32 (index 0..31) as per_frame left them, for the shaders.
    float q(int index) const { return static_cast<float>(*m_frameQ[index]); }
    // Any per-frame variable after the last runFrame, such as blur1_min.
    double frameValue(const std::string& name) { return m_frameVars.get(name); }
    // A value from the preset file, by variable name or lower-case key, before any equations.
    double baseValue(const std::string& name, double fallback) const {
        auto it = m_base.find(name);
        return it == m_base.end() ? fallback : it->second;
    }
    // Empty when the preset has no per_pixel code.
    const PerVertexFn& perVertex() const { return m_perVertex; }

    // Run the enabled custom shapes and waves for the frame the last runFrame computed.
    void runCustomShapes(std::vector<CustomShapeInstance>& out);
    void runCustomWaves(const MilkAudioFrame& audio, std::vector<CustomWaveOutput>& out);
    // shapecode_N_image, or empty when the shape samples the previous frame.
    const std::string& shapeImage(int shape) const { return m_shapes[shape]->image(); }

private:
    void setReadOnly(VarTable& vars, const AudioLevels& audio, float time, float fps, int width, int height);
    void loadFrameVars();
    void runVertex(VertexVars& v);

    std::string m_name;
    bool m_hasShaders;
    bool m_milkwave;
    std::vector<std::string> m_errors;
    ExprGlobals m_globals;
    VarTable m_frameVars{m_globals};
    VarTable m_pixelVars{m_globals};
    Program m_init;
    Program m_perFrame;
    Program m_perPixel;
    bool m_initialised = false;
    // x, y, rad, ang, then the VERTEX_VARS in order.
    double* m_vertexSlots[14] = {};
    std::map<std::string, double> m_base;
    double m_qInit[32] = {};
    double* m_frameQ[32] = {};
    double* m_pixelQ[32] = {};
    PerVertexFn m_perVertex;
    std::vector<std::unique_ptr<CustomWave>> m_waves;
    std::vector<std::unique_ptr<CustomShape>> m_shapes;
    CustomFrameInputs m_customInputs;
    uint64_t m_frame = 0;
    float m_progress = 0.0f;
    float m_gamma = 2.0f;
};

} // namespace Milk

#pragma once

#include "audio_synth.h"

#include <functional>
#include <vector>

namespace Milk {

// Vertices of the warp mesh: 64 by 48 cells, as MilkDrop's mesh at any window shape.
constexpr int MESH_X = 65;
constexpr int MESH_Y = 49;

enum WaveMode : int {
    WAVE_CIRCLE = 0,
    WAVE_XY = 1,
    WAVE_LINE = 2,
};

// One frame's MilkDrop variables after the per-frame equations have run.
struct FrameVars {
    float zoom = 1.0f;
    float zoomExp = 1.0f;
    float rot = 0.0f;
    float warp = 0.0f;
    float warpSpeed = 1.0f;
    float warpScale = 1.0f;
    float cx = 0.5f;
    float cy = 0.5f;
    float dx = 0.0f;
    float dy = 0.0f;
    float sx = 1.0f;
    float sy = 1.0f;
    float decay = 0.98f;
    int waveMode = WAVE_CIRCLE;
    float waveScale = 1.0f;
    float waveX = 0.5f;
    float waveY = 0.5f;
    float waveR = 1.0f;
    float waveG = 1.0f;
    float waveB = 1.0f;
    float waveA = 0.8f;
    bool waveThick = true;
    bool wrap = false;
};

// The per-vertex variables. x and y run 0..1 from the top-left; rad and ang are about the centre.
struct VertexVars {
    float x, y, rad, ang;
    float zoom, zoomExp, rot, warp, cx, cy, dx, dy, sx, sy;
};

using PerVertexFn = std::function<void(VertexVars&, const AudioLevels&, float time)>;

struct MeshUv {
    float u, v;
};

// Where each mesh vertex samples the previous frame, in 0..1 texture space.
void computeMesh(const FrameVars& vars, const PerVertexFn& perVertex, const AudioLevels& audio, float time,
                 int width, int height, std::vector<MeshUv>& out);

// The waveform polyline in pixels for a width x height image. Empty when count is under 4.
void wavePoints(const FrameVars& vars, const float* samples, int count, int width, int height,
                std::vector<float>& xs, std::vector<float>& ys);

class FeedbackBuffer {
public:
    FeedbackBuffer(int width, int height);

    int width() const { return m_width; }
    int height() const { return m_height; }
    // RGB floats, top row first.
    const std::vector<float>& pixels() const { return m_front; }
    std::vector<float>& pixels() { return m_front; }

    void clear();
    void warp(const FrameVars& vars, const PerVertexFn& perVertex, const AudioLevels& audio, float time);
    void drawWave(const FrameVars& vars, const float* samples, int count);

private:
    void splat(float x, float y, float r, float g, float b);
    void splatLine(float x0, float y0, float x1, float y1, float r, float g, float b, bool thick);

    int m_width;
    int m_height;
    std::vector<float> m_front;
    std::vector<float> m_back;
    std::vector<MeshUv> m_mesh;
    std::vector<MeshUv> m_row;
};

} // namespace Milk

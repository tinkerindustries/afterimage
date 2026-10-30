#pragma once

#include "milk_custom.h"
#include "milk_wave.h"

#include <vector>

namespace Milk {

// Matches CustomVertex in shaders.metal. Positions are in pixels of the feedback image.
// `across` runs from -outer to +outer across a line; a fill uses 0 with a huge halfWidth.
struct CustomVertex {
    float x, y, u, v, r, g, b, a, across, halfWidth, pad0, pad1;
};

// A run of vertices drawn with one pipeline and texture. `shape` names the textured shape whose
// texture to bind, and is -1 for untextured runs.
struct CustomBatch {
    size_t first = 0;
    size_t count = 0;
    bool additive = false;
    bool textured = false;
    int shape = -1;
};

// Turns custom shape instances, custom wave points and the main waveform into triangles,
// converting MilkDrop's coordinates into pixels the way MilkDrop places them. Lines are one
// pixel of the feedback image wide, as MilkDrop's are.
class CustomGeometry {
public:
    // `imageShapes[i]` is true when shape i samples its own image rather than the previous frame.
    void begin(int width, int height, const bool imageShapes[CUSTOM_COUNT]);
    void addShape(const CustomShapeInstance& shape);
    void addWave(const CustomWaveOutput& wave);
    void addMainWave(const MainWave& wave);
    // A frame `size` thick in clip units, running inwards from `outer` (1 is the screen edge).
    void addBorder(float outer, float size, const float colour[4]);
    // MilkDrop's darken_center: a small black diamond at the centre, fading out to its corners.
    void addDarkenCentre();
    // A one-pixel line between two points in texture coordinates (0..1, y down).
    void addTextureLine(float u0, float v0, float u1, float v1, const float colour[4]);

    const std::vector<CustomVertex>& vertices() const { return m_vertices; }
    const std::vector<CustomBatch>& batches() const { return m_batches; }

private:
    void toPixels(float clipX, float clipY, float& px, float& py) const;
    void startBatch(bool additive, bool textured, int shape);
    void addSegment(float x0, float y0, float x1, float y1, const float c0[4], const float c1[4], float halfWidth);
    void addSquare(float x, float y, float half, const float c[4]);

    int m_width = 1;
    int m_height = 1;
    float m_aspectX = 1.0f;
    float m_aspectY = 1.0f;
    float m_pixel = 1.0f;
    bool m_imageShapes[CUSTOM_COUNT] = {};
    std::vector<CustomVertex> m_vertices;
    std::vector<CustomBatch> m_batches;
};

} // namespace Milk

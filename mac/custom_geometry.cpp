#include "custom_geometry.h"

#include <algorithm>
#include <cmath>

namespace Milk {

namespace {

constexpr float PI = 3.14159265358979f;
constexpr float FILL = 1e6f;

} // namespace

void CustomGeometry::begin(int width, int height, const bool imageShapes[CUSTOM_COUNT]) {
    m_width = std::max(1, width);
    m_height = std::max(1, height);
    m_aspectX = (m_height > m_width) ? static_cast<float>(m_width) / m_height : 1.0f;
    m_aspectY = (m_width > m_height) ? static_cast<float>(m_height) / m_width : 1.0f;
    m_pixel = 1.0f;
    for (int i = 0; i < CUSTOM_COUNT; ++i) m_imageShapes[i] = imageShapes[i];
    m_vertices.clear();
    m_batches.clear();
}

// Clip space here has y up, as in MilkDrop.
void CustomGeometry::toPixels(float clipX, float clipY, float& px, float& py) const {
    px = (clipX + 1.0f) * 0.5f * m_width;
    py = (1.0f - clipY) * 0.5f * m_height;
}

void CustomGeometry::startBatch(bool additive, bool textured, int shape) {
    if (!m_batches.empty()) {
        CustomBatch& last = m_batches.back();
        if (last.additive == additive && last.textured == textured && last.shape == shape) return;
        if (last.count == 0) {
            last = CustomBatch{m_vertices.size(), 0, additive, textured, shape};
            return;
        }
    }
    m_batches.push_back(CustomBatch{m_vertices.size(), 0, additive, textured, shape});
}

void CustomGeometry::addSegment(float x0, float y0, float x1, float y1, const float c0[4], const float c1[4], float halfWidth) {
    const float len = std::hypot(x1 - x0, y1 - y0);
    if (len < 1e-4f) return;
    const float outer = halfWidth + 1.0f;
    const float nx = -(y1 - y0) / len * outer;
    const float ny = (x1 - x0) / len * outer;
    const CustomVertex a{x0 + nx, y0 + ny, 0, 0, c0[0], c0[1], c0[2], c0[3], outer, halfWidth, 0, 0};
    const CustomVertex b{x0 - nx, y0 - ny, 0, 0, c0[0], c0[1], c0[2], c0[3], -outer, halfWidth, 0, 0};
    const CustomVertex c{x1 + nx, y1 + ny, 0, 0, c1[0], c1[1], c1[2], c1[3], outer, halfWidth, 0, 0};
    const CustomVertex d{x1 - nx, y1 - ny, 0, 0, c1[0], c1[1], c1[2], c1[3], -outer, halfWidth, 0, 0};
    m_vertices.insert(m_vertices.end(), {a, b, c, b, d, c});
    m_batches.back().count += 6;
}

void CustomGeometry::addSquare(float x, float y, float half, const float c[4]) {
    const CustomVertex a{x - half, y - half, 0, 0, c[0], c[1], c[2], c[3], 0, FILL, 0, 0};
    const CustomVertex b{x + half, y - half, 0, 0, c[0], c[1], c[2], c[3], 0, FILL, 0, 0};
    const CustomVertex d{x - half, y + half, 0, 0, c[0], c[1], c[2], c[3], 0, FILL, 0, 0};
    const CustomVertex e{x + half, y + half, 0, 0, c[0], c[1], c[2], c[3], 0, FILL, 0, 0};
    m_vertices.insert(m_vertices.end(), {a, b, d, b, e, d});
    m_batches.back().count += 6;
}

// The centre sits at (2x - 1, 2y - 1) with no aspect correction. Corners are offset by
// rad * cos * aspectY across and rad * sin up, so a shape stays round on a wide screen.
void CustomGeometry::addShape(const CustomShapeInstance& s) {
    const int sides = std::clamp(s.sides, 3, 100);
    const float cx = s.x * 2.0f - 1.0f;
    const float cy = s.y * 2.0f - 1.0f;
    const bool image = s.shape >= 0 && s.shape < CUSTOM_COUNT && m_imageShapes[s.shape];
    const float texAspect = image ? 1.0f : m_aspectY;

    std::vector<float> px(sides), py(sides), pu(sides), pv(sides);
    for (int i = 0; i < sides; ++i) {
        const float progress = static_cast<float>(i) / sides;
        const float angle = progress * PI * 2.0f + s.ang + PI * 0.25f;
        toPixels(cx + s.rad * std::cos(angle) * m_aspectY, cy + s.rad * std::sin(angle), px[i], py[i]);
        const float texAngle = progress * PI * 2.0f + s.texAng + PI * 0.25f;
        pu[i] = 0.5f + 0.5f * std::cos(texAngle) / s.texZoom * texAspect;
        pv[i] = 0.5f - 0.5f * std::sin(texAngle) / s.texZoom;
    }
    float centreX, centreY;
    toPixels(cx, cy, centreX, centreY);

    const float inner[4] = {s.r, s.g, s.b, s.a};
    const float outer[4] = {s.r2, s.g2, s.b2, s.a2};
    startBatch(s.additive, s.textured, s.textured ? s.shape : -1);
    for (int i = 0; i < sides; ++i) {
        const int j = (i + 1) % sides;
        const CustomVertex centre{centreX, centreY, 0.5f, 0.5f, inner[0], inner[1], inner[2], inner[3], 0, FILL, 0, 0};
        const CustomVertex a{px[i], py[i], pu[i], pv[i], outer[0], outer[1], outer[2], outer[3], 0, FILL, 0, 0};
        const CustomVertex b{px[j], py[j], pu[j], pv[j], outer[0], outer[1], outer[2], outer[3], 0, FILL, 0, 0};
        m_vertices.insert(m_vertices.end(), {centre, a, b});
        m_batches.back().count += 3;
    }

    if (s.borderA > 0.0001f) {
        const float border[4] = {s.borderR, s.borderG, s.borderB, s.borderA};
        const float halfWidth = (s.thick ? 1.0f : 0.5f) * m_pixel;
        startBatch(s.additive, false, -1);
        for (int i = 0; i < sides; ++i) {
            const int j = (i + 1) % sides;
            addSegment(px[i], py[i], px[j], py[j], border, border, halfWidth);
        }
    }
}

// Wave points sit at ((2x - 1) / aspectX, (2y - 1) / aspectY), so one unit is the same number
// of pixels across and up.
void CustomGeometry::addWave(const CustomWaveOutput& wave) {
    if (wave.points.empty()) return;
    std::vector<float> px(wave.points.size()), py(wave.points.size());
    for (size_t i = 0; i < wave.points.size(); ++i) {
        const CustomWavePoint& p = wave.points[i];
        toPixels((p.x * 2.0f - 1.0f) / m_aspectX, (p.y * 2.0f - 1.0f) / m_aspectY, px[i], py[i]);
    }
    startBatch(wave.additive, false, -1);
    if (wave.dots) {
        // MilkDrop's point size: 1 pixel below a 1024 pixel texture and 2 from there up, plus 1 when thick.
        const float side = static_cast<float>((m_width >= 1024 ? 2 : 1) + (wave.thick ? 1 : 0));
        const float half = 0.5f * side * m_pixel;
        for (size_t i = 0; i < wave.points.size(); ++i) {
            const CustomWavePoint& p = wave.points[i];
            const float c[4] = {p.r, p.g, p.b, p.a};
            addSquare(px[i], py[i], half, c);
        }
        return;
    }
    const float halfWidth = (wave.thick ? 1.0f : 0.5f) * m_pixel;
    for (size_t i = 1; i < wave.points.size(); ++i) {
        const CustomWavePoint& p0 = wave.points[i - 1];
        const CustomWavePoint& p1 = wave.points[i];
        const float c0[4] = {p0.r, p0.g, p0.b, p0.a};
        const float c1[4] = {p1.r, p1.g, p1.b, p1.a};
        addSegment(px[i - 1], py[i - 1], px[i], py[i], c0, c1, halfWidth);
    }
}

// MilkDrop draws the main wave as one-pixel lines or dots. Thick lines, and dots always, are
// drawn four times, offset right, then up, then back left by one pixel.
void CustomGeometry::addMainWave(const MainWave& wave) {
    if (wave.a <= 0.0f) return;
    const float colour[4] = {wave.r, wave.g, wave.b, wave.a};
    // MilkDrop redraws thick and dotted waves four times only on an image 512 or more pixels wide.
    const int passes = ((wave.thick || wave.dots) && m_width >= 512) ? 4 : 1;
    const float offsets[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, -1.0f}, {0.0f, -1.0f}};
    startBatch(wave.additive, false, -1);
    for (const auto& line : wave.lines) {
        if (line.size() < (wave.dots ? 1u : 2u)) continue;
        std::vector<float> px(line.size()), py(line.size());
        for (size_t i = 0; i < line.size(); ++i) toPixels(line[i][0], line[i][1], px[i], py[i]);
        for (int pass = 0; pass < passes; ++pass) {
            const float ox = offsets[pass][0] * m_pixel;
            const float oy = offsets[pass][1] * m_pixel;
            if (wave.dots) {
                for (size_t i = 0; i < line.size(); ++i) addSquare(px[i] + ox, py[i] + oy, 0.5f * m_pixel, colour);
                continue;
            }
            for (size_t i = 1; i < line.size(); ++i) {
                addSegment(px[i - 1] + ox, py[i - 1] + oy, px[i] + ox, py[i] + oy, colour, colour, 0.5f * m_pixel);
            }
            if (wave.loop) {
                const size_t last = line.size() - 1;
                addSegment(px[last] + ox, py[last] + oy, px[0] + ox, py[0] + oy, colour, colour, 0.5f * m_pixel);
            }
        }
    }
}

// Four trapezoids between the outer and inner rectangles, so the corners are covered once.
void CustomGeometry::addBorder(float outer, float size, const float colour[4]) {
    if (colour[3] <= 0.001f || size <= 0.0f) return;
    const float inner = outer - size;
    float ox[4], oy[4], ix[4], iy[4];
    const float corners[4][2] = {{-1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f}};
    for (int i = 0; i < 4; ++i) {
        toPixels(corners[i][0] * outer, corners[i][1] * outer, ox[i], oy[i]);
        toPixels(corners[i][0] * inner, corners[i][1] * inner, ix[i], iy[i]);
    }
    startBatch(false, false, -1);
    auto vertex = [&](float x, float y) {
        return CustomVertex{x, y, 0, 0, colour[0], colour[1], colour[2], colour[3], 0, FILL, 0, 0};
    };
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) % 4;
        m_vertices.insert(m_vertices.end(), {vertex(ox[i], oy[i]), vertex(ox[j], oy[j]), vertex(ix[j], iy[j]),
                                             vertex(ox[i], oy[i]), vertex(ix[j], iy[j]), vertex(ix[i], iy[i])});
        m_batches.back().count += 6;
    }
}

void CustomGeometry::addDarkenCentre() {
    constexpr float HALF = 0.05f;
    float cx, cy;
    toPixels(0.0f, 0.0f, cx, cy);
    float px[4], py[4];
    const float corners[4][2] = {{-HALF * m_aspectY, 0.0f}, {0.0f, -HALF}, {HALF * m_aspectY, 0.0f}, {0.0f, HALF}};
    for (int i = 0; i < 4; ++i) toPixels(corners[i][0], corners[i][1], px[i], py[i]);
    startBatch(false, false, -1);
    const CustomVertex centre{cx, cy, 0, 0, 0, 0, 0, 3.0f / 32.0f, 0, FILL, 0, 0};
    for (int i = 0; i < 4; ++i) {
        const int j = (i + 1) % 4;
        const CustomVertex a{px[i], py[i], 0, 0, 0, 0, 0, 0, 0, FILL, 0, 0};
        const CustomVertex b{px[j], py[j], 0, 0, 0, 0, 0, 0, 0, FILL, 0, 0};
        m_vertices.insert(m_vertices.end(), {centre, a, b});
        m_batches.back().count += 3;
    }
}

void CustomGeometry::addTextureLine(float u0, float v0, float u1, float v1, const float colour[4]) {
    startBatch(false, false, -1);
    addSegment(u0 * m_width, v0 * m_height, u1 * m_width, v1 * m_height, colour, colour, 0.5f * m_pixel);
}

} // namespace Milk

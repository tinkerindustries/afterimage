#include "feedback.h"

#include <algorithm>
#include <cmath>

namespace Milk {

namespace {

constexpr float TAU = 6.2831853f;

int wrapIndex(int i, int n) {
    i %= n;
    return i < 0 ? i + n : i;
}

} // namespace

// Follows the warp mesh in MilkDrop 1.04's source: zoom about the screen centre, stretch about
// (cx, cy), the four-term animated warp, rotation about (cx, cy), then translation. The work is
// done in a space with equal units on both axes so rotation stays circular.
void computeMesh(const FrameVars& vars, const PerVertexFn& perVertex, const AudioLevels& audio, float time,
                 int width, int height, std::vector<MeshUv>& out) {
    out.resize(MESH_X * MESH_Y);
    const float aspectX = (height > width) ? static_cast<float>(width) / height : 1.0f;
    const float aspectY = (width > height) ? static_cast<float>(height) / width : 1.0f;
    const float warpTime = time * vars.warpSpeed;
    const float warpScaleInv = 1.0f / std::max(vars.warpScale, 0.01f);
    const float f0 = 11.68f + 4.0f * std::cos(warpTime * 1.413f + 10.0f);
    const float f1 = 8.77f + 3.0f * std::cos(warpTime * 1.113f + 7.0f);
    const float f2 = 10.54f + 3.0f * std::cos(warpTime * 1.233f + 3.0f);
    const float f3 = 11.49f + 4.0f * std::cos(warpTime * 0.933f + 5.0f);

    for (int j = 0; j < MESH_Y; ++j) {
        for (int i = 0; i < MESH_X; ++i) {
            VertexVars vv{};
            const float gridX = static_cast<float>(i) / (MESH_X - 1);
            const float gridY = static_cast<float>(j) / (MESH_Y - 1);
            const float x = gridX * 2.0f - 1.0f;
            const float y = -(gridY * 2.0f - 1.0f);
            // The per-pixel x and y span 0.5 +- half the aspect on each axis, so a wide window's y
            // covers less than 0..1.
            vv.x = 0.5f + x * 0.5f * aspectX;
            vv.y = 0.5f - y * 0.5f * aspectY;
            vv.rad = std::sqrt(x * x * aspectX * aspectX + y * y * aspectY * aspectY);
            vv.ang = std::atan2(y * aspectY, x * aspectX);
            vv.zoom = vars.zoom;
            vv.zoomExp = vars.zoomExp;
            vv.rot = vars.rot;
            vv.warp = vars.warp;
            vv.cx = vars.cx;
            vv.cy = vars.cy;
            vv.dx = vars.dx;
            vv.dy = vars.dy;
            vv.sx = vars.sx;
            vv.sy = vars.sy;
            if (perVertex) perVertex(vv, audio, time);

            const float zoom2 = std::pow(std::max(vv.zoom, 0.01f), std::pow(std::max(vv.zoomExp, 0.01f), vv.rad * 2.0f - 1.0f));
            const float zoomInv = 1.0f / std::max(zoom2, 1e-6f);
            float u = x * aspectX * 0.5f * zoomInv + 0.5f;
            float v = -y * aspectY * 0.5f * zoomInv + 0.5f;

            u = (u - vv.cx) / vv.sx + vv.cx;
            v = (v - vv.cy) / vv.sy + vv.cy;

            if (vv.warp != 0.0f) {
                u += vv.warp * 0.0035f * std::sin(warpTime * 0.333f + warpScaleInv * (x * f0 - y * f3));
                v += vv.warp * 0.0035f * std::cos(warpTime * 0.375f - warpScaleInv * (x * f2 + y * f1));
                u += vv.warp * 0.0035f * std::cos(warpTime * 0.753f - warpScaleInv * (x * f1 - y * f2));
                v += vv.warp * 0.0035f * std::sin(warpTime * 0.825f + warpScaleInv * (x * f0 + y * f3));
            }

            const float u2 = u - vv.cx;
            const float v2 = v - vv.cy;
            const float c = std::cos(vv.rot);
            const float s = std::sin(vv.rot);
            u = u2 * c - v2 * s + vv.cx;
            v = u2 * s + v2 * c + vv.cy;

            u -= vv.dx;
            v -= vv.dy;

            u = (u - 0.5f) / aspectX + 0.5f;
            v = (v - 0.5f) / aspectY + 0.5f;
            out[j * MESH_X + i] = MeshUv{u, v};
        }
    }
}

FeedbackBuffer::FeedbackBuffer(int width, int height)
    : m_width(width),
      m_height(height),
      m_front(static_cast<size_t>(width) * height * 3, 0.0f),
      m_back(static_cast<size_t>(width) * height * 3, 0.0f),
      m_row(MESH_X) {}

void FeedbackBuffer::clear() {
    std::fill(m_front.begin(), m_front.end(), 0.0f);
    std::fill(m_back.begin(), m_back.end(), 0.0f);
}

void FeedbackBuffer::warp(const FrameVars& vars, const PerVertexFn& perVertex, const AudioLevels& audio, float time) {
    computeMesh(vars, perVertex, audio, time, m_width, m_height, m_mesh);
    const float decay = vars.decay;
    const bool wrap = vars.wrap;
    const int w = m_width;
    const int h = m_height;
    const float* src = m_front.data();
    float* dst = m_back.data();

    for (int py = 0; py < h; ++py) {
        const float gy = (py + 0.5f) / h * (MESH_Y - 1);
        const int j0 = std::min(static_cast<int>(gy), MESH_Y - 2);
        const float fy = gy - j0;
        for (int i = 0; i < MESH_X; ++i) {
            const MeshUv& a = m_mesh[j0 * MESH_X + i];
            const MeshUv& b = m_mesh[(j0 + 1) * MESH_X + i];
            m_row[i] = MeshUv{a.u + (b.u - a.u) * fy, a.v + (b.v - a.v) * fy};
        }
        float* out = dst + static_cast<size_t>(py) * w * 3;
        for (int px = 0; px < w; ++px) {
            const float gx = (px + 0.5f) / w * (MESH_X - 1);
            const int i0 = std::min(static_cast<int>(gx), MESH_X - 2);
            const float fx = gx - i0;
            const float u = m_row[i0].u + (m_row[i0 + 1].u - m_row[i0].u) * fx;
            const float v = m_row[i0].v + (m_row[i0 + 1].v - m_row[i0].v) * fx;

            const float sx = u * w - 0.5f;
            const float sy = v * h - 0.5f;
            const float flx = std::floor(sx);
            const float fly = std::floor(sy);
            const float tx = sx - flx;
            const float ty = sy - fly;
            int x0 = static_cast<int>(flx);
            int y0 = static_cast<int>(fly);
            int x1 = x0 + 1;
            int y1 = y0 + 1;
            if (wrap) {
                x0 = wrapIndex(x0, w);
                x1 = wrapIndex(x1, w);
                y0 = wrapIndex(y0, h);
                y1 = wrapIndex(y1, h);
            } else {
                x0 = std::clamp(x0, 0, w - 1);
                x1 = std::clamp(x1, 0, w - 1);
                y0 = std::clamp(y0, 0, h - 1);
                y1 = std::clamp(y1, 0, h - 1);
            }
            const float* p00 = src + (static_cast<size_t>(y0) * w + x0) * 3;
            const float* p10 = src + (static_cast<size_t>(y0) * w + x1) * 3;
            const float* p01 = src + (static_cast<size_t>(y1) * w + x0) * 3;
            const float* p11 = src + (static_cast<size_t>(y1) * w + x1) * 3;
            const float w00 = (1.0f - tx) * (1.0f - ty);
            const float w10 = tx * (1.0f - ty);
            const float w01 = (1.0f - tx) * ty;
            const float w11 = tx * ty;
            for (int c = 0; c < 3; ++c) {
                out[px * 3 + c] = (p00[c] * w00 + p10[c] * w10 + p01[c] * w01 + p11[c] * w11) * decay;
            }
        }
    }
    m_front.swap(m_back);
}

void FeedbackBuffer::splat(float x, float y, float r, float g, float b) {
    const float fx = x - 0.5f;
    const float fy = y - 0.5f;
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - x0;
    const float ty = fy - y0;
    const float weights[4] = {(1 - tx) * (1 - ty), tx * (1 - ty), (1 - tx) * ty, tx * ty};
    const int xs[4] = {x0, x0 + 1, x0, x0 + 1};
    const int ys[4] = {y0, y0, y0 + 1, y0 + 1};
    for (int k = 0; k < 4; ++k) {
        if (xs[k] < 0 || ys[k] < 0 || xs[k] >= m_width || ys[k] >= m_height) continue;
        float* p = m_front.data() + (static_cast<size_t>(ys[k]) * m_width + xs[k]) * 3;
        p[0] = std::min(1.0f, p[0] + r * weights[k]);
        p[1] = std::min(1.0f, p[1] + g * weights[k]);
        p[2] = std::min(1.0f, p[2] + b * weights[k]);
    }
}

void FeedbackBuffer::splatLine(float x0, float y0, float x1, float y1, float r, float g, float b, bool thick) {
    const float len = std::hypot(x1 - x0, y1 - y0);
    const int steps = std::max(1, static_cast<int>(std::ceil(len)));
    for (int s = 0; s < steps; ++s) {
        const float t = static_cast<float>(s) / steps;
        const float x = x0 + (x1 - x0) * t;
        const float y = y0 + (y1 - y0) * t;
        splat(x, y, r, g, b);
        if (thick) {
            splat(x + 1.0f, y, r, g, b);
            splat(x, y + 1.0f, r, g, b);
        }
    }
}

void wavePoints(const FrameVars& vars, const float* samples, int count, int width, int height,
                std::vector<float>& xs, std::vector<float>& ys) {
    xs.clear();
    ys.clear();
    if (count < 4) return;
    std::vector<float> smooth(count);
    float acc = samples[0];
    for (int i = 0; i < count; ++i) {
        acc = acc * 0.6f + samples[i] * 0.4f;
        smooth[i] = acc;
    }

    const float cx = vars.waveX * width;
    const float cy = vars.waveY * height;
    const float minDim = static_cast<float>(std::min(width, height));
    const float scale = vars.waveScale;

    switch (vars.waveMode) {
    case WAVE_CIRCLE: {
        const int points = 256;
        for (int k = 0; k <= points; ++k) {
            const int idx = (k % points) * (count - 1) / points;
            // Blend the last eighth into the first sample so the loop closes without a step.
            float s = smooth[idx];
            const int tail = points / 8;
            if (k > points - tail) {
                const float t = static_cast<float>(k - (points - tail)) / tail;
                s = s * (1.0f - t) + smooth[0] * t;
            }
            const float rad = minDim * (0.22f + 0.15f * s * scale);
            const float ang = static_cast<float>(k) / points * TAU;
            xs.push_back(cx + std::cos(ang) * rad);
            ys.push_back(cy + std::sin(ang) * rad);
        }
        break;
    }
    case WAVE_XY: {
        const int lag = 37;
        for (int k = 0; k + lag < count; k += 2) {
            xs.push_back(cx + smooth[k] * scale * minDim * 0.4f);
            ys.push_back(cy + smooth[k + lag] * scale * minDim * 0.4f);
        }
        break;
    }
    case WAVE_LINE:
    default: {
        const int points = 320;
        for (int k = 0; k < points; ++k) {
            const int idx = k * (count - 1) / (points - 1);
            xs.push_back(static_cast<float>(k) / (points - 1) * width);
            ys.push_back(cy + smooth[idx] * scale * minDim * 0.25f);
        }
        break;
    }
    }
}

void FeedbackBuffer::drawWave(const FrameVars& vars, const float* samples, int count) {
    std::vector<float> xs;
    std::vector<float> ys;
    wavePoints(vars, samples, count, m_width, m_height, xs, ys);
    const float r = vars.waveR * vars.waveA;
    const float g = vars.waveG * vars.waveA;
    const float b = vars.waveB * vars.waveA;
    for (size_t k = 1; k < xs.size(); ++k) {
        splatLine(xs[k - 1], ys[k - 1], xs[k], ys[k], r, g, b, vars.waveThick);
    }
}

} // namespace Milk

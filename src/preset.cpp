#include "preset.h"

#include <algorithm>
#include <cmath>

namespace Milk {

const std::vector<std::string> MOTION_NAMES = {"still", "pulse", "sway", "storm", "ripple"};
const std::vector<std::string> WAVE_MODE_NAMES = {"circle", "xy", "line"};

namespace {

void motionPulse(FrameVars& f, const AudioLevels& a, float) {
    f.zoom += 0.06f * (a.bassAtt - 1.0f);
    f.rot += 0.01f * (a.midAtt - 1.0f);
    f.waveA *= std::clamp(0.4f + 0.4f * a.bass, 0.2f, 1.5f);
}

void motionSway(FrameVars& f, const AudioLevels& a, float t) {
    f.cx += 0.12f * std::sin(t * 0.37f);
    f.cy += 0.10f * std::cos(t * 0.29f);
    f.rot += 0.02f * std::sin(t * 0.61f);
    f.dx += 0.004f * std::sin(t * 0.23f);
    f.zoom += 0.02f * (a.bassAtt - 1.0f);
}

void motionStorm(FrameVars& f, const AudioLevels& a, float t) {
    f.warp += std::clamp(2.0f * (a.trebAtt - 1.0f), -1.0f, 3.0f);
    f.rot += 0.04f * std::sin(t * 0.13f) * a.bassAtt;
    f.zoom += 0.03f * std::sin(t * 0.71f);
}

void motionRippleVertex(VertexVars& v, const AudioLevels& a, float t) {
    v.zoom *= 1.0f + 0.015f * std::sin(v.rad * 18.0f - t * 3.0f) * a.bassAtt;
}

} // namespace

void applyMotion(int motion, FrameVars& f, const AudioLevels& a, float t) {
    switch (motion) {
    case MOTION_PULSE: motionPulse(f, a, t); break;
    case MOTION_SWAY: motionSway(f, a, t); break;
    case MOTION_STORM: motionStorm(f, a, t); break;
    default: break;
    }
}

PerVertexFn perVertexFor(int motion) {
    return motion == MOTION_RIPPLE ? motionRippleVertex : nullptr;
}

FrameVars frameFromParams(const Params& p, float t) {
    FrameVars f;
    f.zoom = p.zoom;
    f.zoomExp = p.zoomExp;
    f.rot = p.rot;
    f.warp = p.warp;
    f.warpSpeed = p.warpSpeed;
    f.warpScale = p.warpScale;
    f.cx = p.cx;
    f.cy = p.cy;
    f.dx = p.dx;
    f.dy = p.dy;
    f.sx = p.sx;
    f.sy = p.sy;
    f.decay = p.decay;
    f.waveMode = p.waveMode;
    f.waveScale = p.waveScale;
    f.waveX = p.waveX;
    f.waveY = p.waveY;
    f.waveA = p.waveAlpha;
    f.waveThick = p.waveThick;
    f.wrap = p.wrap;
    f.waveR = p.waveColour[0] / 255.0f;
    f.waveG = p.waveColour[1] / 255.0f;
    f.waveB = p.waveColour[2] / 255.0f;
    if (p.colourCycle) {
        f.waveR *= 0.6f + 0.4f * std::sin(t * 1.13f);
        f.waveG *= 0.6f + 0.4f * std::sin(t * 1.23f + 2.0f);
        f.waveB *= 0.6f + 0.4f * std::sin(t * 1.37f + 4.0f);
    }
    return f;
}

void clampFrame(FrameVars& f) {
    f.zoom = std::max(f.zoom, 0.05f);
    f.sx = std::abs(f.sx) < 0.01f ? 0.01f : f.sx;
    f.sy = std::abs(f.sy) < 0.01f ? 0.01f : f.sy;
    f.decay = std::clamp(f.decay, 0.0f, 1.0f);
    f.waveA = std::clamp(f.waveA, 0.0f, 1.0f);
}

} // namespace Milk

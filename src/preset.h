#pragma once

#include "audio_synth.h"
#include "feedback.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Milk {

enum Motion : int {
    MOTION_STILL = 0,
    MOTION_PULSE = 1,
    MOTION_SWAY = 2,
    MOTION_STORM = 3,
    MOTION_RIPPLE = 4,
};

extern const std::vector<std::string> MOTION_NAMES;
extern const std::vector<std::string> WAVE_MODE_NAMES;

// The knobs, named after the MilkDrop preset variables they set. Plain data so it saves as bytes.
struct Params {
    float zoom = 1.02f;
    float zoomExp = 1.0f;
    float rot = 0.01f;
    float warp = 1.0f;
    float warpSpeed = 1.0f;
    float warpScale = 1.0f;
    float cx = 0.5f;
    float cy = 0.5f;
    float dx = 0.0f;
    float dy = 0.0f;
    float sx = 1.0f;
    float sy = 1.0f;
    float decay = 0.975f;
    int waveMode = WAVE_CIRCLE;
    float waveScale = 1.0f;
    float waveX = 0.5f;
    float waveY = 0.5f;
    float waveAlpha = 0.8f;
    uint8_t waveColour[3] = {255, 170, 64};
    bool waveThick = true;
    bool colourCycle = true;
    bool wrap = false;
    int motion = MOTION_SWAY;
    float bpm = 124.0f;
    float gammaAdj = 1.6f;
};

// Per-frame equations. Each starts from the knob values, the way a MilkDrop preset's per-frame
// code starts from the preset's base values.
void applyMotion(int motion, FrameVars& f, const AudioLevels& a, float t);
PerVertexFn perVertexFor(int motion);
FrameVars frameFromParams(const Params& p, float t);
void clampFrame(FrameVars& f);

} // namespace Milk

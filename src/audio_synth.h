#pragma once

#include <cstdint>

namespace Milk {

constexpr int SAMPLE_RATE = 48000;

// The levels MilkDrop presets read. 1.0 is the band's recent average; _att is the smoothed value.
struct AudioLevels {
    float bass = 1.0f;
    float mid = 1.0f;
    float treb = 1.0f;
    float bassAtt = 1.0f;
    float midAtt = 1.0f;
    float trebAtt = 1.0f;
};

// Plain data so the world can be saved and restored as bytes.
struct SynthState {
    uint64_t sample = 0;
    double beatPhase = 0.0;
    uint64_t beatCount = 0;
    uint32_t seed = 0;
    float avgEnergy[3] = {0.0f, 0.0f, 0.0f};
    AudioLevels levels;
};

void resetSynth(SynthState& state, uint32_t seed, float bpm);

// Writes `frames` mono samples to `out`, updates the levels, and returns how many beats started.
int synthesise(SynthState& state, float bpm, int frames, float* out);

} // namespace Milk

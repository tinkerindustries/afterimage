#pragma once

#include "audio_synth.h"

#include <array>

namespace Milk {

// MilkDrop 2's sound analysis (DoCustomSoundAnalysis in vis_milk2): the spectrum of the latest 576
// left-channel samples, summed into bins 0-85, 85-170 and 170-256 of 512, then blended over time.
// bass, mid and treb are each band relative to its slow average, and the _att values are the
// fast average relative to the same slow average.
class BandLevels {
public:
    BandLevels() = default;

    // `left` holds this frame's new samples in -1..1. frameScale is the frame's length in 60 Hz
    // frames, so the averages settle at the same speed whatever the display rate.
    void update(const float* left, int count, float frameScale);
    const AudioLevels& levels() const { return m_levels; }
    // The treble band's summed spectrum before it is made relative to its average.
    float rawTreble() const { return m_imm[2]; }

private:
    std::array<float, 576> m_recent{};
    float m_imm[3] = {0.0f, 0.0f, 0.0f};
    float m_avg[3] = {0.0f, 0.0f, 0.0f};
    float m_longAvg[3] = {0.0f, 0.0f, 0.0f};
    int m_frame = 0;
    AudioLevels m_levels;
};

} // namespace Milk

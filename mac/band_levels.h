#pragma once

#include "audio_synth.h"

#include <array>
#include <deque>

namespace Milk {

// MilkDrop 2's sound analysis (DoCustomSoundAnalysis in vis_milk2): the spectrum of the latest 576
// left-channel samples, summed into bins 0-85, 85-170 and 170-256 of 512, then blended over time.
// bass, mid and treb are each band relative to its slow average, and the _att values are the
// fast average relative to the same slow average.
class BandLevels {
public:
    BandLevels() = default;

    // Milkwave's variant, for comparing against its captures: both channels summed, band edges
    // at 250 and 4000 Hz and 20 kHz taken at 48 kHz with inclusive end bins, and 0 instead of 1
    // when there is no signal. Call with `right` as well as `left`.
    void useMilkwaveBands(bool on) { m_milkwave = on; }

    // `left` holds this frame's new samples in -1..1. frameScale is the frame's length in 60 Hz
    // frames, so the averages settle at the same speed whatever the display rate. `right` is only
    // read by the Milkwave variant.
    void update(const float* left, int count, float frameScale, const float* right = nullptr);
    const AudioLevels& levels() const { return m_levels; }
    // The treble band's summed spectrum before it is made relative to its average.
    float rawTreble() const { return m_imm[2]; }

private:
    std::array<float, 576> m_recent{};
    std::array<float, 576> m_recentRight{};
    bool m_milkwave = false;
    float m_imm[3] = {0.0f, 0.0f, 0.0f};
    float m_avg[3] = {0.0f, 0.0f, 0.0f};
    float m_longAvg[3] = {0.0f, 0.0f, 0.0f};
    int m_frame = 0;
    std::array<std::deque<float>, 3> m_recentLevels;
    AudioLevels m_levels;
};

} // namespace Milk

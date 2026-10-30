#pragma once

#include "audio_synth.h"

namespace Milk {

// Splits live audio into bass, mid and treble with one-pole filters and normalises each band
// against its recent average, so 1.0 means "as loud as lately", like the synthesiser's levels.
class BandLevels {
public:
    explicit BandLevels(double sampleRate);

    // frameScale is the frame's length in 60 Hz frames, so the averages settle at the same speed
    // whatever the display rate.
    void update(const float* samples, int count, float frameScale);
    const AudioLevels& levels() const { return m_levels; }

private:
    float m_lowCoeff;
    float m_highCoeff;
    float m_low = 0.0f;
    float m_high = 0.0f;
    float m_avgEnergy[3] = {0.0f, 0.0f, 0.0f};
    AudioLevels m_levels;
};

} // namespace Milk

#include "band_levels.h"

#include <algorithm>
#include <cmath>

namespace Milk {

namespace {

constexpr float BASS_CUTOFF_HZ = 250.0f;
constexpr float TREBLE_CUTOFF_HZ = 4000.0f;

float onePoleCoeff(float cutoff, double sampleRate) {
    return 1.0f - std::exp(-6.2831853f * cutoff / static_cast<float>(sampleRate));
}

} // namespace

BandLevels::BandLevels(double sampleRate)
    : m_lowCoeff(onePoleCoeff(BASS_CUTOFF_HZ, sampleRate)),
      m_highCoeff(onePoleCoeff(TREBLE_CUTOFF_HZ, sampleRate)) {}

void BandLevels::update(const float* samples, int count, float frameScale) {
    if (count <= 0) return;
    double energy[3] = {0.0, 0.0, 0.0};
    for (int i = 0; i < count; ++i) {
        const float x = samples[i];
        m_low += (x - m_low) * m_lowCoeff;
        m_high += (x - m_high) * m_highCoeff;
        const float bass = m_low;
        const float mid = m_high - m_low;
        const float treb = x - m_high;
        energy[0] += bass * bass;
        energy[1] += mid * mid;
        energy[2] += treb * treb;
    }

    const float avgRate = 1.0f - std::pow(0.985f, frameScale);
    const float attRate = 1.0f - std::pow(0.9f, frameScale);
    float* level[3] = {&m_levels.bass, &m_levels.mid, &m_levels.treb};
    float* att[3] = {&m_levels.bassAtt, &m_levels.midAtt, &m_levels.trebAtt};
    for (int b = 0; b < 3; ++b) {
        const float mean = static_cast<float>(energy[b] / count);
        float& avg = m_avgEnergy[b];
        avg = (avg == 0.0f) ? mean : avg + (mean - avg) * avgRate;
        *level[b] = std::clamp(mean / std::max(avg, 1e-9f), 0.0f, 4.0f);
        *att[b] += (*level[b] - *att[b]) * attRate;
    }
}

} // namespace Milk

#include "band_levels.h"

#include "milk_audio.h"

#include <algorithm>
#include <cmath>

namespace Milk {

namespace {

// MilkDrop's AdjustRateToFPS: a per-frame rate tuned for 30 fps, converted to the actual rate.
float adjustRate(float rate, float fps) { return std::pow(rate, 30.0f / fps); }

} // namespace

void BandLevels::update(const float* left, int count, float frameScale, const float* right) {
    if (count <= 0) return;
    const int n = static_cast<int>(m_recent.size());
    const int keep = std::max(0, n - count);
    std::copy(m_recent.begin() + (n - keep), m_recent.end(), m_recent.begin());
    const int fresh = std::min(count, n);
    for (int i = 0; i < fresh; ++i) m_recent[keep + i] = left[count - fresh + i];

    std::array<float, MILK_FFT_INPUT> scaled;
    for (int i = 0; i < MILK_FFT_INPUT; ++i) scaled[i] = m_recent[i] * 128.0f;
    std::array<float, MILK_SPECTRUM_SAMPLES> spectrum;
    computeSpectrum(scaled.data(), spectrum);
    if (m_milkwave) {
        const float* source = right != nullptr ? right : left;
        std::copy(m_recentRight.begin() + (n - keep), m_recentRight.end(), m_recentRight.begin());
        for (int i = 0; i < fresh; ++i) m_recentRight[keep + i] = source[count - fresh + i];
        std::array<float, MILK_FFT_INPUT> scaledRight;
        for (int i = 0; i < MILK_FFT_INPUT; ++i) scaledRight[i] = m_recentRight[i] * 128.0f;
        std::array<float, MILK_SPECTRUM_SAMPLES> spectrumRight;
        computeSpectrum(scaledRight.data(), spectrumRight);
        for (int i = 0; i < MILK_SPECTRUM_SAMPLES; ++i) spectrum[i] += spectrumRight[i];
    }

    const float fps = 60.0f / std::max(frameScale, 0.01f);
    float* imm = m_imm;
    for (int b = 0; b < 3; ++b) {
        int start = MILK_SPECTRUM_SAMPLES * b / 6;
        int end = MILK_SPECTRUM_SAMPLES * (b + 1) / 6;
        if (m_milkwave) {
            const int edges[4] = {0, 250, 4000, 20000};
            start = std::clamp(edges[b] * MILK_SPECTRUM_SAMPLES / 24000, 0, MILK_SPECTRUM_SAMPLES - 1);
            end = std::clamp(edges[b + 1] * MILK_SPECTRUM_SAMPLES / 24000, 0, MILK_SPECTRUM_SAMPLES - 1) + 1;
        }
        imm[b] = 0.0f;
        for (int j = start; j < end; ++j) imm[b] += spectrum[j];
    }
    float* level[3] = {&m_levels.bass, &m_levels.mid, &m_levels.treb};
    float* att[3] = {&m_levels.bassAtt, &m_levels.midAtt, &m_levels.trebAtt};
    for (int b = 0; b < 3; ++b) {
        float rate = imm[b] > m_avg[b] ? 0.2f : 0.5f;
        rate = adjustRate(rate, fps);
        m_avg[b] = m_avg[b] * rate + imm[b] * (1.0f - rate);
        rate = adjustRate(m_frame < 50 ? 0.9f : 0.992f, fps);
        m_longAvg[b] = m_longAvg[b] * rate + imm[b] * (1.0f - rate);
        const bool silent = std::fabs(m_longAvg[b]) < 0.001f;
        const float quiet = m_milkwave ? 0.0f : 1.0f;
        *level[b] = silent ? quiet : imm[b] / m_longAvg[b];
        *att[b] = silent ? quiet : m_avg[b] / m_longAvg[b];
    }
    float* smooth[3] = {&m_levels.bassSmooth, &m_levels.midSmooth, &m_levels.trebSmooth};
    const size_t window = static_cast<size_t>(std::max(1.0f, fps));
    for (int b = 0; b < 3; ++b) {
        std::deque<float>& recent = m_recentLevels[b];
        recent.push_back(*level[b]);
        while (recent.size() > window) recent.pop_front();
        float sum = 0.0f;
        for (float v : recent) sum += v;
        *smooth[b] = sum / static_cast<float>(recent.size());
    }
    ++m_frame;
}

} // namespace Milk

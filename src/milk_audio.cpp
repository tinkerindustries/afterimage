#include "milk_audio.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace Milk {

namespace {

constexpr int FFT_SIZE = MILK_SPECTRUM_SAMPLES * 2;
constexpr float PI = 3.14159265358979f;

void fft(std::vector<std::complex<float>>& data) {
    const size_t n = data.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const std::complex<float> step = std::polar(1.0f, -2.0f * PI / static_cast<float>(len));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<float> a = data[i + k];
                const std::complex<float> b = data[i + k + len / 2] * w;
                data[i + k] = a + b;
                data[i + k + len / 2] = a - b;
                w *= step;
            }
        }
    }
}

// MilkDrop's analyser: a Hann window over the input, zero-padded to FFT_SIZE, and magnitudes
// tilted up towards the treble by -0.02 * log((N - i) / N).
void spectrum(const float* scaled, std::array<float, MILK_SPECTRUM_SAMPLES>& out, bool averagePairs) {
    std::vector<std::complex<float>> data(FFT_SIZE);
    float previous = scaled[0];
    for (int i = 0; i < MILK_FFT_INPUT; ++i) {
        const float window = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 2.0f * PI / MILK_FFT_INPUT - PI * 0.5f);
        data[i] = {(averagePairs ? 0.5f * (scaled[i] + previous) : scaled[i]) * window, 0.0f};
        previous = scaled[i];
    }
    fft(data);
    for (int i = 0; i < MILK_SPECTRUM_SAMPLES; ++i) {
        const float equalise = -0.02f * std::log(static_cast<float>(MILK_SPECTRUM_SAMPLES - i) / MILK_SPECTRUM_SAMPLES);
        out[i] = std::abs(data[i]) * equalise;
    }
}

} // namespace

void computeSpectrum(const float* scaled, std::array<float, MILK_SPECTRUM_SAMPLES>& out) { spectrum(scaled, out, false); }

void buildMilkAudioFrame(const float* left, const float* right, int count, MilkAudioFrame& out) {
    std::vector<float> l(MILK_FFT_INPUT, 0.0f), r(MILK_FFT_INPUT, 0.0f);
    const int n = std::min(count, MILK_FFT_INPUT);
    auto eightBit = [](float x) {
        x = std::clamp(x, -1.0f, 1.0f);
        return x >= 1.0f ? 127.0f : static_cast<float>(static_cast<int>(x * 128.0f));
    };
    for (int i = 0; i < n; ++i) {
        l[MILK_FFT_INPUT - n + i] = eightBit(left[count - n + i]);
        r[MILK_FFT_INPUT - n + i] = eightBit(right[count - n + i]);
    }
    std::copy(l.begin(), l.end(), out.waveL.begin());
    std::copy(r.begin(), r.end(), out.waveR.begin());
    spectrum(l.data(), out.spectrumL, true);
    spectrum(r.data(), out.spectrumR, true);
}

void FftShaderData::update(const float* left, const float* right, int count, float attack, float decay) {
    std::vector<std::complex<float>> l(SAMPLES), r(SAMPLES);
    for (int i = 0; i < SAMPLES; ++i) {
        const float window = 0.5f + 0.5f * std::sin(static_cast<float>(i) * 2.0f * PI / SAMPLES - PI * 0.5f);
        const int at = count - SAMPLES + i;
        l[i] = {at >= 0 ? left[at] * 128.0f * window : 0.0f, 0.0f};
        r[i] = {at >= 0 ? right[at] * 128.0f * window : 0.0f, 0.0f};
    }
    fft(l);
    fft(r);
    constexpr float BOOST = 1.0f;
    const float scale = BOOST * BOOST * 0.00030f * (static_cast<float>(BINS) / SAMPLES);
    constexpr float NOISE_GATE = 5e-5f;
    constexpr float VISIBLE_FLOOR = 2.5e-4f;
    const float pinkRef = 1000.0f * BINS / 22050.0f;
    const float hiLo = 4000.0f * BINS / 22050.0f, hiHi = 16000.0f * BINS / 22050.0f;
    for (int i = 0; i < BINS; ++i) {
        float mono = (std::abs(l[i]) + std::abs(r[i])) * 0.5f * scale;
        if (mono < NOISE_GATE) mono = 0.0f;
        mono *= std::sqrt(static_cast<float>(i + 1) / pinkRef);
        // Above 4 kHz the attack eases off towards half by 16 kHz, so high bins steady like low ones.
        const float hiBlend = std::clamp((i - hiLo) / (hiHi - hiLo), 0.0f, 1.0f);
        const float rise = attack * (0.7f - 0.2f * hiBlend);
        if (mono > m_smoothed[i]) m_smoothed[i] += (mono - m_smoothed[i]) * rise;
        else m_smoothed[i] += (mono - m_smoothed[i]) * (1.0f - decay) * (1.0f - decay) * 0.75f;
        if (m_smoothed[i] < VISIBLE_FLOOR) m_smoothed[i] = 0.0f;
    }
    for (int i = 0; i < BINS; ++i) {
        if (m_smoothed[i] >= m_peak[i]) {
            m_peak[i] = m_smoothed[i];
            m_hold[i] = 30;
        } else if (m_hold[i] > 0) {
            --m_hold[i];
        } else {
            m_peak[i] *= 0.97f;
            if (m_peak[i] < VISIBLE_FLOOR) m_peak[i] = 0.0f;
        }
    }
}

void ShellBands::update(MilkAudioFrame& frame, float fps) {
    const float minFreq = 20.0f, maxFreq = 20000.0f;
    const float mult = std::pow(2.0f, std::log(maxFreq / minFreq) / std::log(2.0f) / 3.0f);
    const float inverse[3] = {0.326781557f, 0.380873770f, 0.199888934f};
    const std::array<float, MILK_SPECTRUM_SAMPLES>* spectra[2] = {&frame.spectrumL, &frame.spectrumR};
    // AdjustRateToFPS with rates tuned for 14 fps.
    auto rate = [&](float r) { return std::pow(r, 14.0f / std::max(fps, 1.0f)); };
    float imm[2][3];
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 3; ++i) {
            const int start = std::max(0, static_cast<int>(MILK_SPECTRUM_SAMPLES * minFreq * std::pow(mult, static_cast<float>(i)) / maxFreq));
            const int end = std::min(MILK_SPECTRUM_SAMPLES, static_cast<int>(MILK_SPECTRUM_SAMPLES * minFreq * std::pow(mult, static_cast<float>(i + 1)) / maxFreq));
            float sum = 0.0f;
            for (int j = start; j < end; ++j) sum += (*spectra[ch])[j];
            imm[ch][i] = sum / static_cast<float>(std::max(1, end - start)) / inverse[i];
            const float avgMix = rate(imm[ch][i] > m_avg[ch][i] ? 0.2f : 0.5f);
            m_avg[ch][i] = m_avg[ch][i] * avgMix + imm[ch][i] * (1.0f - avgMix);
            const float medMix = rate(0.91f), longMix = rate(0.96f);
            m_medAvg[ch][i] = m_medAvg[ch][i] * medMix + imm[ch][i] * (1.0f - medMix);
            m_longAvg[ch][i] = m_longAvg[ch][i] * longMix + imm[ch][i] * (1.0f - longMix);
        }
    }
    // infinite_avg starts at 1 and is never updated.
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 3; ++i) {
            frame.beforeWave[ch * 3 + i] = imm[ch][i];
            frame.beforeWave[6 + ch * 3 + i] = m_avg[ch][i];
            frame.beforeWave[12 + ch * 3 + i] = m_medAvg[ch][i];
            frame.beforeWave[18 + ch * 3 + i] = m_longAvg[ch][i];
            frame.beforeWave[24 + ch * 3 + i] = 1.0f;
        }
    }
}

WaveAligner::WaveAligner() {
    int spls = MILK_FFT_INPUT;
    int space = MILK_FFT_INPUT - MILK_WAVE_SAMPLES;
    for (int octave = 0; octave < OCTAVES; ++octave) {
        const int compare = spls - space;
        auto& w = m_weight[octave];
        for (int n = 0; n < compare; ++n) {
            w[n] = n < compare / 2 ? n * 2 / static_cast<float>(compare) : (compare - 1 - n) * 2 / static_cast<float>(compare);
            w[n] = std::clamp((w[n] - 0.8f) * 5.0f + 0.8f, 0.0f, 1.0f);
        }
        int n = 0;
        while (n < compare && w[n] == 0.0f) ++n;
        m_firstWeight[octave] = n;
        n = compare - 1;
        while (n >= 0 && w[n] == 0.0f) --n;
        m_lastWeight[octave] = n;
        spls /= 2;
        space /= 2;
    }
}

void WaveAligner::reset() {
    for (auto& wave : m_old) wave.fill(0.0f);
    m_prevOffset = {0, 0};
}

void WaveAligner::apply(MilkAudioFrame& frame) {
    align(frame.waveL, 0);
    align(frame.waveR, 1);
}

// A coarse-to-fine search: each octave halves the samples by averaging pairs, the coarsest octave
// tries every offset, and each finer one tries the offsets around the previous best.
void WaveAligner::align(std::array<float, MILK_FFT_INPUT>& wave, int ch) {
    std::array<std::array<float, MILK_FFT_INPUT>, OCTAVES> fresh{};
    std::array<std::array<float, MILK_FFT_INPUT>, OCTAVES> old{};
    std::array<int, OCTAVES> spls{};
    std::array<int, OCTAVES> space{};
    fresh[0] = wave;
    std::copy_n(m_old[ch].begin() + m_prevOffset[ch], MILK_WAVE_SAMPLES, old[0].begin());
    spls[0] = MILK_FFT_INPUT;
    space[0] = MILK_FFT_INPUT - MILK_WAVE_SAMPLES;
    for (int octave = 1; octave < OCTAVES; ++octave) {
        spls[octave] = spls[octave - 1] / 2;
        space[octave] = space[octave - 1] / 2;
        for (int n = 0; n < spls[octave]; ++n) {
            fresh[octave][n] = 0.5f * (fresh[octave - 1][n * 2] + fresh[octave - 1][n * 2 + 1]);
            old[octave][n] = 0.5f * (old[octave - 1][n * 2] + old[octave - 1][n * 2 + 1]);
        }
    }

    int offset = 0;
    int n1 = 0;
    int n2 = space[OCTAVES - 1];
    for (int octave = OCTAVES - 1; octave >= 0; --octave) {
        int best = -1;
        float bestErr = 0.0f;
        for (int n = n1; n < n2; ++n) {
            float err = 0.0f;
            for (int i = m_firstWeight[octave]; i <= m_lastWeight[octave]; ++i) {
                err += std::fabs((fresh[octave][i + n] - old[octave][i]) * m_weight[octave][i]);
            }
            if (best == -1 || err < bestErr) {
                best = n;
                bestErr = err;
            }
        }
        if (octave > 0) {
            n1 = std::max(best * 2 - 1, 0);
            n2 = std::min(best * 2 + 3, space[octave - 1]);
        } else {
            offset = best;
        }
    }

    m_old[ch] = wave;
    m_prevOffset[ch] = offset;
    if (offset > 0) {
        for (int i = 0; i < MILK_WAVE_SAMPLES; ++i) wave[i] = wave[i + offset];
        std::fill(wave.begin() + MILK_WAVE_SAMPLES, wave.end(), 0.0f);
    }
}

} // namespace Milk

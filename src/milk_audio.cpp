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
    for (int i = 0; i < n; ++i) {
        l[MILK_FFT_INPUT - n + i] = left[count - n + i] * 128.0f;
        r[MILK_FFT_INPUT - n + i] = right[count - n + i] * 128.0f;
    }
    std::copy(l.begin(), l.end(), out.waveL.begin());
    std::copy(r.begin(), r.end(), out.waveR.begin());
    spectrum(l.data(), out.spectrumL, true);
    spectrum(r.data(), out.spectrumR, true);
}

} // namespace Milk

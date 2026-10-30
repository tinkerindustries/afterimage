#pragma once

#include <array>

namespace Milk {

constexpr int MILK_WAVE_SAMPLES = 480;
constexpr int MILK_SPECTRUM_SAMPLES = 512;
// How many recent samples the spectrum is computed from.
constexpr int MILK_FFT_INPUT = 576;

// The audio a MilkDrop frame exposes to waves: the latest 576 samples, oldest first, scaled to
// about +-128 as Winamp delivered them, and their spectrum, per channel. The waves read the first
// MILK_WAVE_SAMPLES of them, and a few modes read past that.
struct MilkAudioFrame {
    std::array<float, MILK_FFT_INPUT> waveL{};
    std::array<float, MILK_FFT_INPUT> waveR{};
    std::array<float, MILK_SPECTRUM_SAMPLES> spectrumL{};
    std::array<float, MILK_SPECTRUM_SAMPLES> spectrumR{};
};

// MilkDrop's spectrum of MILK_FFT_INPUT samples scaled to about +-128, oldest first: a Hann window,
// zero padding to 1024 points, and magnitudes tilted by -0.02 * log((512 - i) / 512). The band
// levels that feed bass, mid and treb use this one.
void computeSpectrum(const float* scaled, std::array<float, MILK_SPECTRUM_SAMPLES>& out);

// `left` and `right` hold at least MILK_FFT_INPUT samples in -1..1, newest last.
void buildMilkAudioFrame(const float* left, const float* right, int count, MilkAudioFrame& out);

} // namespace Milk

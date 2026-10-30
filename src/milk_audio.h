#pragma once

#include <array>

namespace Milk {

constexpr int MILK_WAVE_SAMPLES = 480;
constexpr int MILK_SPECTRUM_SAMPLES = 512;
// How many recent samples the spectrum is computed from.
constexpr int MILK_FFT_INPUT = 576;

// The audio a MilkDrop frame exposes to custom waves: the latest waveform, scaled to about
// +-128 as Winamp delivered it, and its spectrum, per channel.
struct MilkAudioFrame {
    std::array<float, MILK_WAVE_SAMPLES> waveL{};
    std::array<float, MILK_WAVE_SAMPLES> waveR{};
    std::array<float, MILK_SPECTRUM_SAMPLES> spectrumL{};
    std::array<float, MILK_SPECTRUM_SAMPLES> spectrumR{};
};

// `left` and `right` hold at least MILK_FFT_INPUT samples in -1..1, newest last.
void buildMilkAudioFrame(const float* left, const float* right, int count, MilkAudioFrame& out);

} // namespace Milk

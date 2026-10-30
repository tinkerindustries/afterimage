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
    // The 30 floats MilkDrop keeps just before its waveform (imm, avg, med_avg, long_avg and
    // infinite_avg, two channels by three bands each), which a 512-sample custom wave reads past
    // the start of the left channel.
    std::array<float, 30> beforeWave{};
    std::array<float, MILK_FFT_INPUT> waveL{};
    std::array<float, MILK_FFT_INPUT> waveR{};
    std::array<float, MILK_SPECTRUM_SAMPLES> spectrumL{};
    std::array<float, MILK_SPECTRUM_SAMPLES> spectrumR{};
};

// MilkDrop's spectrum of MILK_FFT_INPUT samples scaled to about +-128, oldest first: a Hann window,
// zero padding to 1024 points, and magnitudes tilted by -0.02 * log((512 - i) / 512). The band
// levels that feed bass, mid and treb use this one.
void computeSpectrum(const float* scaled, std::array<float, MILK_SPECTRUM_SAMPLES>& out);

// `left` and `right` hold at least MILK_FFT_INPUT samples in -1..1, newest last. MilkDrop's
// samples are 8-bit, so each is clamped to -1..1 and truncated to a whole step of 1/128.
void buildMilkAudioFrame(const float* left, const float* right, int count, MilkAudioFrame& out);

// The plugin shell's own band averages (AnalyzeNewSound), which fill MilkAudioFrame::beforeWave:
// each channel's spectrum averaged over three bands equally spaced in pitch from 20 Hz to 20 kHz,
// scaled by fixed inverse song averages, then smoothed over time.
class ShellBands {
public:
    void update(MilkAudioFrame& frame, float fps);

private:
    float m_avg[2][3] = {};
    float m_medAvg[2][3] = {};
    float m_longAvg[2][3] = {};
};

// Milkwave's FFT texture for get_fft: the latest SAMPLES samples of each channel at the analysis
// scale (x128), Hann-windowed, their magnitudes averaged across the channels, scaled, gated,
// tilted up 3 dB per octave around 1 kHz, then smoothed by the preset's attack and decay. A
// second row holds each bin's peak for half a second, then lets it fall.
class FftShaderData {
public:
    static constexpr int SAMPLES = 1024;
    static constexpr int BINS = SAMPLES / 2;
    // `left` and `right` hold at least SAMPLES samples in -1..1, newest last.
    void update(const float* left, const float* right, int count, float attack, float decay);
    const std::array<float, BINS>& smoothed() const { return m_smoothed; }
    const std::array<float, BINS>& peak() const { return m_peak; }

private:
    std::array<float, BINS> m_smoothed{};
    std::array<float, BINS> m_peak{};
    std::array<int, BINS> m_hold{};
};

// MilkDrop's AlignWaves: shifts each channel's waveform by up to MILK_FFT_INPUT - MILK_WAVE_SAMPLES
// samples so its first MILK_WAVE_SAMPLES best match the previous frame's, and zeroes the samples
// past them when it shifts. The spectrum is left as it was.
class WaveAligner {
public:
    WaveAligner();
    void reset();
    void apply(MilkAudioFrame& frame);

private:
    static constexpr int OCTAVES = 6;
    void align(std::array<float, MILK_FFT_INPUT>& wave, int ch);

    std::array<std::array<float, MILK_FFT_INPUT>, OCTAVES> m_weight{};
    std::array<int, OCTAVES> m_firstWeight{};
    std::array<int, OCTAVES> m_lastWeight{};
    std::array<std::array<float, MILK_FFT_INPUT>, 2> m_old{};
    std::array<int, 2> m_prevOffset{};
};

} // namespace Milk

#include "audio_synth.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Milk {

namespace {

constexpr double TAU = 6.283185307179586;

const float BASS_NOTES[4] = {55.0f, 55.0f, 73.42f, 65.41f};
const float CHORDS[4][3] = {
    {220.00f, 261.63f, 329.63f},
    {293.66f, 349.23f, 440.00f},
    {261.63f, 329.63f, 392.00f},
    {246.94f, 293.66f, 392.00f},
};

float hashNoise(uint64_t n) {
    n ^= n >> 33;
    n *= 0xff51afd7ed558ccdULL;
    n ^= n >> 33;
    n *= 0xc4ceb9fe1a85ec53ULL;
    n ^= n >> 33;
    return static_cast<float>(n >> 40) / static_cast<float>(1u << 24) * 2.0f - 1.0f;
}

double frac(double x) { return x - std::floor(x); }

} // namespace

void resetSynth(SynthState& state, uint32_t seed, float bpm) {
    state = SynthState{};
    state.seed = seed;
    // Run four seconds so the band averages start settled, then rewind the clock.
    std::vector<float> scratch(SAMPLE_RATE / 60);
    for (int i = 0; i < 240; ++i) {
        synthesise(state, bpm, static_cast<int>(scratch.size()), scratch.data());
    }
    state.sample = 0;
    state.beatPhase = 0.0;
    state.beatCount = 0;
    state.levels = AudioLevels{};
}

int synthesise(SynthState& state, float bpm, int frames, float* out) {
    const double beatLen = 60.0 / std::max(bpm, 1.0f);
    const double beatsPerSample = 1.0 / (beatLen * SAMPLE_RATE);
    const int barOffset = static_cast<int>(state.seed % 4);
    double energy[3] = {0.0, 0.0, 0.0};
    int beatsStarted = 0;

    for (int i = 0; i < frames; ++i) {
        if (state.sample == 0) ++beatsStarted;
        const double t = static_cast<double>(state.sample) / SAMPLE_RATE;
        const double beatPhase = state.beatPhase;
        const int bar = static_cast<int>(beatPhase / 4.0) + barOffset;

        const double tb = frac(beatPhase) * beatLen;
        const double kick = std::sin(TAU * (45.0 * tb + 110.0 / 35.0 * (1.0 - std::exp(-35.0 * tb)))) *
                            std::exp(-7.0 * tb) * 0.8;

        const double eighth = beatPhase * 2.0;
        const double te = frac(eighth) * beatLen * 0.5;
        const double bassFreq = BASS_NOTES[bar % 4] * ((static_cast<int>(eighth) % 2) ? 2.0 : 1.0);
        const double bass = std::sin(TAU * bassFreq * t) * std::exp(-5.0 * te) * 0.35;

        const float* chord = CHORDS[bar % 4];
        double pad = 0.0;
        for (int n = 0; n < 3; ++n) {
            pad += std::sin(TAU * chord[n] * t) + 0.3 * std::sin(TAU * 2.0 * chord[n] * t);
        }
        pad *= 0.05 * (0.6 + 0.4 * std::sin(TAU * 0.25 * t));

        const double sixteenth = beatPhase * 4.0;
        const double ts = frac(sixteenth) * beatLen * 0.25;
        const double accent = (static_cast<int>(sixteenth) % 4 == 2) ? 1.0 : 0.35;
        const double hat = hashNoise(state.sample + (static_cast<uint64_t>(state.seed) << 40)) *
                           std::exp(-60.0 * ts) * 0.25 * accent;

        energy[0] += (kick + bass) * (kick + bass);
        energy[1] += pad * pad;
        energy[2] += hat * hat;
        out[i] = static_cast<float>(std::tanh(kick + bass + pad + hat) * 0.6);

        const double next = beatPhase + beatsPerSample;
        if (std::floor(next) > std::floor(beatPhase)) ++beatsStarted;
        state.beatPhase = next;
        ++state.sample;
    }
    state.beatCount += beatsStarted;

    float* level[3] = {&state.levels.bass, &state.levels.mid, &state.levels.treb};
    float* att[3] = {&state.levels.bassAtt, &state.levels.midAtt, &state.levels.trebAtt};
    for (int b = 0; b < 3; ++b) {
        const float mean = static_cast<float>(energy[b] / std::max(frames, 1));
        float& avg = state.avgEnergy[b];
        avg = (avg == 0.0f) ? mean : avg * 0.985f + mean * 0.015f;
        *level[b] = std::clamp(mean / std::max(avg, 1e-9f), 0.0f, 4.0f);
        *att[b] = *att[b] * 0.9f + *level[b] * 0.1f;
    }
    return beatsStarted;
}

} // namespace Milk

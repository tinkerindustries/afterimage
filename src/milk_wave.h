#pragma once

#include "milk_audio.h"

#include <array>
#include <vector>

namespace Milk {

// The per-frame and preset values MilkDrop's main waveform reads.
struct MainWaveSettings {
    int mode = 0;
    float x = 0.5f;          // wave_x, 0..1 left to right
    float y = 0.5f;          // wave_y, 0..1 bottom to top
    float mystery = 0.0f;    // wave_mystery
    float alpha = 0.8f;      // wave_a
    float r = 1.0f, g = 1.0f, b = 1.0f;
    bool brighten = false;   // wave_brighten (bMaximizeWaveColor)
    bool dots = false;
    bool thick = false;
    bool additive = false;
    bool modAlphaByVolume = false;
    float modAlphaStart = 0.75f;
    float modAlphaEnd = 0.95f;
    float scale = 1.0f;      // fWaveScale
    float smoothing = 0.75f; // fWaveSmoothing
    float time = 0.0f;
    float vol = 1.0f;        // 0.333 * (bass + mid + treb)
    float treb = 1.0f;       // the raw treble band sum, as mode 3 reads it
};

// The waveform as MilkDrop draws it: up to two polylines in clip space (-1..1, y up), one colour,
// and how to draw them.
struct MainWave {
    std::array<std::vector<std::array<float, 2>>, 2> lines;
    bool loop = false;
    bool dots = false;
    bool thick = false;
    bool additive = false;
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 0.0f;
};

// Builds the main waveform for an image of width x height pixels, following MilkDrop's eight
// wave modes.
void buildMainWave(const MainWaveSettings& settings, const MilkAudioFrame& audio, int width, int height, MainWave& out);

} // namespace Milk

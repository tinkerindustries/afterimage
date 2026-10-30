#include "milk_wave.h"

#include <algorithm>
#include <cmath>

namespace Milk {

namespace {

using Point = std::array<float, 2>;
using Line = std::vector<Point>;

// Room past the waveform for modes that read 32 samples ahead. Those samples start at 0 and
// pick up the smoothing filter's tail.
constexpr int PCM_SIZE = 512;

enum Mode {
    MODE_CIRCLE = 0,
    MODE_XY_SPIRAL = 1,
    MODE_SPIRO = 2,
    MODE_SPIRO_VOLUME = 3,
    MODE_DERIVATIVE_LINE = 4,
    MODE_EXPLOSIVE_HASH = 5,
    MODE_LINE = 6,
    MODE_DOUBLE_LINE = 7,
};

struct Context {
    std::array<float, PCM_SIZE> l{};
    std::array<float, PCM_SIZE> r{};
    float aspectX = 1.0f;
    float aspectY = 1.0f;
    float waveX = 0.0f;
    float waveY = 0.0f;
    float mystery = 0.0f;
    float time = 0.0f;
    int width = 1;
};

// The line modes run along a direction set by wave_mystery, through a point offset by wave_x,
// clipped to just outside the screen.
struct LineSpan {
    float edgeX, edgeY, stepX, stepY, perpX, perpY;
    int offset;
};

LineSpan clipLine(const Context& c, float angle, int samples) {
    float dx = std::cos(angle);
    float dy = std::sin(angle);
    float edgeX[2] = {c.waveX * std::cos(angle + 1.57f) - dx * 3.0f, c.waveX * std::cos(angle + 1.57f) + dx * 3.0f};
    float edgeY[2] = {c.waveX * std::sin(angle + 1.57f) - dy * 3.0f, c.waveX * std::sin(angle + 1.57f) + dy * 3.0f};
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 4; ++j) {
            float t = 0.0f;
            bool clip = false;
            switch (j) {
            case 0: if (edgeX[i] > 1.1f) { t = (1.1f - edgeX[1 - i]) / (edgeX[i] - edgeX[1 - i]); clip = true; } break;
            case 1: if (edgeX[i] < -1.1f) { t = (-1.1f - edgeX[1 - i]) / (edgeX[i] - edgeX[1 - i]); clip = true; } break;
            case 2: if (edgeY[i] > 1.1f) { t = (1.1f - edgeY[1 - i]) / (edgeY[i] - edgeY[1 - i]); clip = true; } break;
            case 3: if (edgeY[i] < -1.1f) { t = (-1.1f - edgeY[1 - i]) / (edgeY[i] - edgeY[1 - i]); clip = true; } break;
            }
            if (clip) {
                const float diffX = edgeX[i] - edgeX[1 - i];
                const float diffY = edgeY[i] - edgeY[1 - i];
                edgeX[i] = edgeX[1 - i] + diffX * t;
                edgeY[i] = edgeY[1 - i] + diffY * t;
            }
        }
    }
    LineSpan s;
    s.offset = (MILK_WAVE_SAMPLES - samples) / 2;
    s.stepX = (edgeX[1] - edgeX[0]) / samples;
    s.stepY = (edgeY[1] - edgeY[0]) / samples;
    s.edgeX = edgeX[0];
    s.edgeY = edgeY[0];
    const float along = std::atan2(s.stepY, s.stepX);
    s.perpX = std::cos(along + 1.57f);
    s.perpY = std::sin(along + 1.57f);
    return s;
}

int lineSamples(const Context& c) {
    int samples = MILK_WAVE_SAMPLES / 2;
    if (samples > c.width / 3) samples /= 3;
    return samples;
}

void generate(int mode, const Context& c, Line& one, Line& two, bool& loop) {
    loop = false;
    switch (mode) {
    case MODE_CIRCLE: {
        loop = true;
        const int samples = MILK_WAVE_SAMPLES / 2;
        const int offset = (MILK_WAVE_SAMPLES - samples) / 2;
        for (int i = 0; i < samples; ++i) {
            float radius = 0.5f + 0.4f * c.r[i + offset] + c.mystery;
            const float angle = static_cast<float>(i) / samples * 6.28f + c.time * 0.2f;
            // Blend the start into samples from further on, so the loop closes smoothly.
            if (i < samples / 10) {
                float mix = static_cast<float>(i) / (samples * 0.1f);
                mix = 0.5f - 0.5f * std::cos(mix * 3.1416f);
                const float radius2 = 0.5f + 0.4f * c.r[i + samples + offset] + c.mystery;
                radius = radius2 * (1.0f - mix) + radius * mix;
            }
            one.push_back({radius * std::cos(angle) * c.aspectY + c.waveX, radius * std::sin(angle) * c.aspectX + c.waveY});
        }
        break;
    }
    case MODE_XY_SPIRAL: {
        loop = true;
        const int samples = MILK_WAVE_SAMPLES / 2;
        for (int i = 0; i < samples; ++i) {
            const float radius = 0.53f + 0.43f * c.r[i] + c.mystery;
            const float angle = c.l[i + 32] * 1.57f + c.time * 2.3f;
            one.push_back({radius * std::cos(angle) * c.aspectY + c.waveX, radius * std::sin(angle) * c.aspectX + c.waveY});
        }
        break;
    }
    case MODE_SPIRO:
    case MODE_SPIRO_VOLUME:
        for (int i = 0; i < MILK_WAVE_SAMPLES; ++i) {
            one.push_back({c.r[i] * c.aspectY + c.waveX, c.l[i + 32] * c.aspectX + c.waveY});
        }
        break;
    case MODE_DERIVATIVE_LINE: {
        int samples = MILK_WAVE_SAMPLES;
        if (samples > c.width / 3) samples /= 3;
        const int offset = (MILK_WAVE_SAMPLES - samples) / 2;
        const float w1 = 0.45f + 0.5f * (c.mystery * 0.5f + 0.5f);
        const float w2 = 1.0f - w1;
        for (int i = 0; i < samples; ++i) {
            const float x = -1.0f + 2.0f * (static_cast<float>(i) / samples) + c.waveX + c.r[i + 25 + offset] * 0.44f;
            const float y = c.l[i + offset] * 0.47f + c.waveY;
            if (i > 1) {
                one.push_back({x * w2 + w1 * (one[i - 1][0] * 2.0f - one[i - 2][0]), y * w2 + w1 * (one[i - 1][1] * 2.0f - one[i - 2][1])});
            } else {
                one.push_back({x, y});
            }
        }
        break;
    }
    case MODE_EXPLOSIVE_HASH: {
        const float cr = std::cos(c.time * 0.3f);
        const float sr = std::sin(c.time * 0.3f);
        for (int i = 0; i < MILK_WAVE_SAMPLES; ++i) {
            const float x0 = c.r[i] * c.l[i + 32] + c.l[i] * c.r[i + 32];
            const float y0 = c.r[i] * c.r[i] - c.l[i + 32] * c.l[i + 32];
            one.push_back({(x0 * cr - y0 * sr) * c.aspectY + c.waveX, (x0 * sr + y0 * cr) * c.aspectX + c.waveY});
        }
        break;
    }
    case MODE_LINE: {
        const int samples = lineSamples(c);
        const LineSpan s = clipLine(c, 1.57f * c.mystery, samples);
        for (int i = 0; i < samples; ++i) {
            const float v = 0.25f * c.l[i + s.offset];
            one.push_back({s.edgeX + s.stepX * i + s.perpX * v, s.edgeY + s.stepY * i + s.perpY * v});
        }
        break;
    }
    case MODE_DOUBLE_LINE:
    default: {
        const int samples = lineSamples(c);
        const LineSpan s = clipLine(c, 1.57f * c.mystery, samples);
        const float separation = std::pow(c.waveY * 0.5f + 0.5f, 2.0f);
        for (int i = 0; i < samples; ++i) {
            const float v1 = 0.25f * c.l[i + s.offset] + separation;
            const float v2 = 0.25f * c.r[i + s.offset] - separation;
            one.push_back({s.edgeX + s.stepX * i + s.perpX * v1, s.edgeY + s.stepY * i + s.perpY * v1});
            two.push_back({s.edgeX + s.stepX * i + s.perpX * v2, s.edgeY + s.stepY * i + s.perpY * v2});
        }
        break;
    }
    }
}

// Inserts a point between each pair with a four-tap filter, as MilkDrop does before drawing.
void smooth(const Line& in, Line& out) {
    constexpr float C1 = -0.15f, C2 = 1.15f, C3 = 1.15f, C4 = -0.15f;
    constexpr float INV_SUM = 1.0f / (C1 + C2 + C3 + C4);
    out.clear();
    const size_t n = in.size();
    if (n == 0) return;
    size_t below = 0;
    for (size_t i = 0; i + 1 < n; ++i) {
        const size_t above = i + 1;
        const size_t above2 = std::min(n - 1, i + 2);
        out.push_back(in[i]);
        out.push_back({(C1 * in[below][0] + C2 * in[i][0] + C3 * in[above][0] + C4 * in[above2][0]) * INV_SUM,
                       (C1 * in[below][1] + C2 * in[i][1] + C3 * in[above][1] + C4 * in[above2][1]) * INV_SUM});
        below = i;
    }
    out.push_back(in[n - 1]);
}

// Modes 2 and 5 scale alpha down, and mode 3 also ties it to treble. The factors grow with the
// image's larger side, so dense modes look the same at different sizes.
float modeAlpha(int mode, float alpha, int texsize, float treb) {
    auto pick = [&](float a, float b, float c, float d, float e) {
        if (texsize <= 256) return a;
        if (texsize <= 512) return b;
        if (texsize <= 1024) return c;
        if (texsize <= 2048) return d;
        return e;
    };
    if (mode == MODE_SPIRO || mode == MODE_EXPLOSIVE_HASH) return alpha * pick(0.07f, 0.09f, 0.11f, 0.13f, 0.15f);
    if (mode == MODE_SPIRO_VOLUME) return alpha * pick(0.075f, 0.15f, 0.22f, 0.33f, 0.44f) * 1.3f * treb * treb;
    return alpha;
}

} // namespace

void buildMainWave(const MainWaveSettings& s, const MilkAudioFrame& audio, int width, int height, MainWave& out) {
    out = MainWave{};
    const int mode = ((s.mode % 8) + 8) % 8;

    Context c;
    c.width = width;
    c.time = s.time;
    if (width > height) c.aspectY = static_cast<float>(height) / width;
    else c.aspectX = static_cast<float>(width) / height;
    c.waveX = 2.0f * s.x - 1.0f;
    c.waveY = 2.0f * s.y - 1.0f;
    c.mystery = s.mystery;
    if ((mode == MODE_CIRCLE || mode == MODE_XY_SPIRAL || mode == MODE_DERIVATIVE_LINE) && (c.mystery < -1.0f || c.mystery > 1.0f)) {
        c.mystery = c.mystery * 0.5f + 0.5f;
        c.mystery -= std::floor(c.mystery);
        c.mystery = std::fabs(c.mystery) * 2.0f - 1.0f;
    }

    // Each sample is scaled by fWaveScale / 128 and smoothed with a one-pole filter whose
    // feedback is fWaveSmoothing.
    std::copy(audio.waveL.begin(), audio.waveL.end(), c.l.begin());
    std::copy(audio.waveR.begin(), audio.waveR.end(), c.r.begin());
    const float scale = s.scale / 128.0f;
    const float mix2 = s.smoothing;
    const float mix1 = scale * (1.0f - mix2);
    c.l[0] *= scale;
    c.r[0] *= scale;
    for (int i = 1; i < PCM_SIZE; ++i) {
        c.l[i] = c.l[i] * mix1 + c.l[i - 1] * mix2;
        c.r[i] = c.r[i] * mix1 + c.r[i - 1] * mix2;
    }

    Line one, two;
    generate(mode, c, one, two, out.loop);
    smooth(one, out.lines[0]);
    smooth(two, out.lines[1]);

    float alpha = modeAlpha(mode, s.alpha, std::max(width, height), s.treb);
    if (s.modAlphaByVolume) {
        if (s.vol <= s.modAlphaStart) alpha = 0.0f;
        else if (s.vol >= s.modAlphaEnd) alpha = s.alpha;
        else alpha = s.alpha * (s.vol - s.modAlphaStart) / (s.modAlphaEnd - s.modAlphaStart);
    }
    out.a = std::clamp(alpha, 0.0f, 1.0f);
    // MilkDrop passes vertex colours as bytes, so each channel is clamped to 0..1.
    out.r = std::clamp(s.r, 0.0f, 1.0f);
    out.g = std::clamp(s.g, 0.0f, 1.0f);
    out.b = std::clamp(s.b, 0.0f, 1.0f);
    if (s.brighten) {
        const float peak = std::max({out.r, out.g, out.b});
        if (peak > 0.01f) {
            out.r /= peak;
            out.g /= peak;
            out.b /= peak;
        }
    }
    out.dots = s.dots;
    out.thick = s.thick;
    out.additive = s.additive;
}

} // namespace Milk

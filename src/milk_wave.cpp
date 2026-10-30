#include "milk_wave.h"

#include <algorithm>
#include <cmath>

namespace Milk {

namespace {

using Point = std::array<float, 2>;
using Line = std::vector<Point>;

// MilkDrop keeps the left channel's samples, the right channel's and then the left spectrum in
// one block, so a mode that reads past a channel's end reads the next array. Modes 13 and 14 read
// up to 24 values past the right channel.
constexpr int PCM_TAIL = 24;
constexpr int PCM_SIZE = MILK_FFT_INPUT + PCM_TAIL;

enum Mode {
    MODE_NONE = -1,
    MODE_CIRCLE = 0,
    MODE_XY_SPIRAL = 1,
    MODE_SPIRO = 2,
    MODE_SPIRO_VOLUME = 3,
    MODE_DERIVATIVE_LINE = 4,
    MODE_EXPLOSIVE_HASH = 5,
    MODE_LINE = 6,
    MODE_DOUBLE_LINE = 7,
    MODE_SPECTRUM_LINE = 8,
    MODE_LARGE_LINE = 9,
    MODE_X = 10,
    MODE_VERTICAL_PAIR = 11,
    MODE_SKEWED_SPIRAL = 12,
    MODE_STAR = 13,
    MODE_FLOWER = 14,
    MODE_LASSO = 15,
    MODE_TRIANGLE = 16,
    MODE_FIREWORKS = 17,
    MODE_LIMIT = 18,
};

struct Context {
    std::array<float, PCM_SIZE> l{};
    std::array<float, PCM_SIZE> r{};
    const float* spectrum = nullptr; // the left channel's MILK_SPECTRUM_SAMPLES bins
    float aspectX = 1.0f;
    float aspectY = 1.0f;
    float waveX = 0.0f;
    float waveY = 0.0f;
    float mystery = 0.0f;
    float time = 0.0f;
    float bass = 1.0f;
    float mid = 1.0f;
    int width = 1;
};

// The points of one mode, and the index where a second line starts, or -1.
struct Wave {
    Line points;
    int split = -1;
};

// Modes 0, 1 and 4 fold a wave_mystery outside -1..1 back into that range.
float waveParam(int mode, float mystery) {
    if ((mode == MODE_CIRCLE || mode == MODE_XY_SPIRAL || mode == MODE_DERIVATIVE_LINE) && (mystery < -1.0f || mystery > 1.0f)) {
        mystery = mystery * 0.5f + 0.5f;
        mystery -= std::floor(mystery);
        mystery = std::fabs(mystery) * 2.0f - 1.0f;
    }
    return mystery;
}

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

// Point i of a span, pushed v across it, with its start moved shiftX across the screen.
Point along(const LineSpan& s, int i, float v, float shiftX = 0.0f) {
    return {s.edgeX + shiftX + s.stepX * i + s.perpX * v, s.edgeY + s.stepY * i + s.perpY * v};
}

int lineSamples(const Context& c) {
    return std::min(MILK_WAVE_SAMPLES / 2, c.width / 3);
}

// `close` repeats the first point at the end of the closed shapes, which MilkDrop skips while
// blending presets. `alpha` arrives as the mode's alpha after its clamp: mode 12 reads it and
// mode 17 scales it. A mode MilkDrop's switch does not match leaves its zeroed buffer.
void generate(int mode, const Context& c, bool close, float& alpha, Wave& w) {
    Line& p = w.points;
    const float mystery = waveParam(mode, c.mystery);
    switch (mode) {
    case MODE_CIRCLE: {
        const int samples = MILK_WAVE_SAMPLES / 2;
        const int offset = (MILK_WAVE_SAMPLES - samples) / 2;
        for (int i = 0; i < samples; ++i) {
            float radius = 0.5f + 0.4f * c.r[i + offset] + mystery;
            const float angle = static_cast<float>(i) * (1.0f / (samples - 1)) * 6.28f + c.time * 0.2f;
            // Blend the start into samples from further on, so the loop closes smoothly.
            if (i < samples / 10) {
                float mix = static_cast<float>(i) / (samples * 0.1f);
                mix = 0.5f - 0.5f * std::cos(mix * 3.1416f);
                const float radius2 = 0.5f + 0.4f * c.r[i + samples + offset] + mystery;
                radius = radius2 * (1.0f - mix) + radius * mix;
            }
            p.push_back({radius * std::cos(angle) * c.aspectY + c.waveX, radius * std::sin(angle) * c.aspectX + c.waveY});
        }
        if (close) p.push_back(p[0]);
        break;
    }
    case MODE_XY_SPIRAL: {
        const int samples = MILK_WAVE_SAMPLES / 2;
        for (int i = 0; i < samples; ++i) {
            const float radius = 0.53f + 0.43f * c.r[i] + mystery;
            const float angle = c.l[i + 32] * 1.57f + c.time * 2.3f;
            p.push_back({radius * std::cos(angle) * c.aspectY + c.waveX, radius * std::sin(angle) * c.aspectX + c.waveY});
        }
        break;
    }
    case MODE_SPIRO:
    case MODE_SPIRO_VOLUME:
        for (int i = 0; i < MILK_WAVE_SAMPLES; ++i) {
            p.push_back({c.r[i] * c.aspectY + c.waveX, c.l[i + 32] * c.aspectX + c.waveY});
        }
        break;
    case MODE_DERIVATIVE_LINE: {
        const int samples = std::min(MILK_WAVE_SAMPLES, c.width / 3);
        const int offset = (MILK_WAVE_SAMPLES - samples) / 2;
        const float w1 = 0.45f + 0.5f * (mystery * 0.5f + 0.5f);
        const float w2 = 1.0f - w1;
        for (int i = 0; i < samples; ++i) {
            const float x = -1.0f + 2.0f * (static_cast<float>(i) / samples) + c.waveX + c.r[i + 25 + offset] * 0.44f;
            const float y = c.l[i + offset] * 0.47f + c.waveY;
            if (i > 1) {
                p.push_back({x * w2 + w1 * (p[i - 1][0] * 2.0f - p[i - 2][0]), y * w2 + w1 * (p[i - 1][1] * 2.0f - p[i - 2][1])});
            } else {
                p.push_back({x, y});
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
            p.push_back({(x0 * cr - y0 * sr) * c.aspectY + c.waveX, (x0 * sr + y0 * cr) * c.aspectX + c.waveY});
        }
        break;
    }
    case MODE_LINE: {
        const int samples = lineSamples(c);
        const LineSpan s = clipLine(c, 1.57f * mystery, samples);
        for (int i = 0; i < samples; ++i) p.push_back(along(s, i, 0.25f * c.l[i + s.offset]));
        break;
    }
    case MODE_DOUBLE_LINE: {
        const int samples = lineSamples(c);
        const LineSpan s = clipLine(c, 1.57f * mystery, samples);
        const float separation = std::pow(c.waveY * 0.5f + 0.5f, 2.0f);
        for (int i = 0; i < samples; ++i) p.push_back(along(s, i, 0.25f * c.l[i + s.offset] + separation));
        w.split = samples;
        for (int i = 0; i < samples; ++i) p.push_back(along(s, i, 0.25f * c.r[i + s.offset] - separation));
        break;
    }
    case MODE_SPECTRUM_LINE: {
        const int samples = 256;
        const LineSpan s = clipLine(c, 1.57f * mystery, samples);
        for (int i = 0; i < samples; ++i) p.push_back(along(s, i, 0.1f * std::log(c.spectrum[i * 2] + c.spectrum[i * 2 + 1])));
        break;
    }
    case MODE_LARGE_LINE: {
        // The second line is the untouched half of the buffer: as many points at the centre.
        const int samples = lineSamples(c);
        const LineSpan s = clipLine(c, 1.57f * mystery, samples);
        for (int i = 0; i < samples; ++i) p.push_back(along(s, i, c.l[i + s.offset]));
        w.split = samples;
        p.resize(2 * samples, Point{0.0f, 0.0f});
        break;
    }
    case MODE_X: {
        const int samples = lineSamples(c);
        const LineSpan s1 = clipLine(c, -0.75f + mystery * 3.15f, samples);
        const LineSpan s2 = clipLine(c, 0.75f + mystery * 3.15f, samples);
        for (int i = 0; i < samples; ++i) p.push_back(along(s1, i, 0.35f * c.l[i + s1.offset]));
        w.split = samples;
        for (int i = 0; i < samples; ++i) p.push_back(along(s2, i, 0.35f * c.r[i + s2.offset]));
        break;
    }
    case MODE_VERTICAL_PAIR: {
        const int samples = lineSamples(c);
        const LineSpan s = clipLine(c, 1.57f, samples);
        for (int i = 0; i < samples; ++i) p.push_back(along(s, i, 0.35f * c.l[i + s.offset], -0.45f));
        w.split = samples;
        for (int i = 0; i < samples; ++i) p.push_back(along(s, i, 0.35f * c.r[i + s.offset], 0.45f));
        break;
    }
    case MODE_SKEWED_SPIRAL: {
        const int samples = MILK_WAVE_SAMPLES / 2;
        for (int i = 0; i < samples; ++i) {
            const float radius = 0.63f + 0.23f * c.r[i] + mystery;
            const float angle = c.l[i + 32] * 0.9f + c.time * 3.3f;
            p.push_back({radius * std::cos(angle + alpha) * c.aspectY + c.waveX, radius * std::sin(angle) * c.aspectX + c.waveY});
        }
        break;
    }
    case MODE_STAR:
    case MODE_FLOWER: {
        const bool star = mode == MODE_STAR;
        const int samples = MILK_WAVE_SAMPLES / 2;
        const int offset = (MILK_WAVE_SAMPLES - samples) / 2;
        const float flip = std::cos(3.1416f);
        for (int i = 0; i < samples; ++i) {
            float radius = star ? 0.7f + 0.4f * c.r[i + offset] + mystery : 0.7f + 0.7f * c.r[i + offset] + mystery;
            const float angle = static_cast<float>(i) * (1.0f / (samples - 1)) * 6.28f + c.time * 0.2f;
            if (static_cast<float>(i) < static_cast<float>(samples) / radius) {
                float mix = static_cast<float>(i) / (samples * 0.1f);
                if (star) {
                    mix = 0.5f - 0.5f * std::cos(mix * 3.1416f);
                    const float radius2 = 0.5f + 0.4f * c.r[i + samples + offset] + mystery;
                    radius = radius2 * (1.0f - mix) + radius * mix;
                } else {
                    mix = 0.7f - 0.7f * std::cos(mix * 3.1416f);
                    const float radius2 = 0.7f + 0.7f * c.r[i + samples + offset] + mystery;
                    radius = radius2 * (1.0f - mix) + radius * (mix * 2.0f) / 8.0f;
                }
            }
            if (star) {
                p.push_back({radius * std::cos(angle) * c.aspectY + c.waveX, radius * std::sin(angle) * c.aspectX + c.waveY});
            } else {
                p.push_back({radius * std::cos(angle * 3.1416f) * c.aspectY / 1.5f + c.waveX * flip,
                             radius * std::sin(angle - c.time / 3.0f) * c.aspectX / 1.5f + c.waveY * flip});
            }
        }
        if (close) p.push_back(p[0]);
        break;
    }
    case MODE_LASSO: {
        const int samples = MILK_WAVE_SAMPLES / 2;
        for (int i = 0; i < samples; ++i) {
            const float angle = c.l[i + 32] * 1.57f + c.time * 2.0f;
            const float t = c.time / angle;
            p.push_back({std::cos(c.time) / 2.0f + std::cos(angle * 2.0f + std::tan(t)),
                         std::sin(c.time) * 2.0f * std::sin(angle * 3.14f) * c.aspectX / 2.8f + c.waveY});
        }
        break;
    }
    case MODE_TRIANGLE: {
        const int samples = 256;
        const float size = 0.575f;
        const float rotation = mystery * 3.141593f;
        const float cr = std::cos(rotation);
        const float sr = std::sin(rotation);
        const float step = 1.0f / static_cast<float>(samples - 1);
        for (int i = 0; i < samples; ++i) {
            const float phase = i * step;
            float x, y;
            if (phase < 0.3333f) {
                const float t = phase * 3.0f;
                x = -size + t * size;
                y = -size + t * 2.0f * size;
            } else if (phase < 0.6666f) {
                const float t = (phase - 0.3333f) * 3.0f;
                x = 0.0f + t * size;
                y = size - t * 2.0f * size;
            } else {
                const float t = (phase - 0.6666f) * 3.0f;
                x = size - t * 2.0f * size;
                y = -size;
            }
            const float gain = 1.0f + 0.3f * c.l[(i * 2) % MILK_WAVE_SAMPLES];
            x *= gain;
            y *= gain;
            p.push_back({(x * cr - y * sr) * c.aspectY + c.waveX, (x * sr + y * cr) * c.aspectX + c.waveY});
        }
        if (close) p.push_back(p[0]);
        break;
    }
    case MODE_FIREWORKS: {
        const int samples = 256;
        const float period = 1.0f - mystery + 0.001f;
        const float phase = std::fmod(c.time, period) / period;
        const int burst = static_cast<int>(c.time / period);
        const float seed = burst * 10.0f;
        float baseX = (seed * 0.1345f - std::floor(seed * 0.1345f)) * 2.0f - 1.0f;
        float baseY = (seed * 0.2783f - std::floor(seed * 0.2783f)) * 2.0f - 1.0f;
        if (std::fmod(seed, 1.0f) > 0.3f) {
            baseX *= 0.3f;
            baseY *= 0.3f;
        }
        const float size = std::min(1.0f, phase * 4.0f);
        const float fade = 1.0f - std::pow(phase, 3.0f);
        const float boost = 1.0f + 2.0f * (c.bass + c.mid) * 0.5f;
        for (int i = 0; i < samples; ++i) {
            const float angle = (i / static_cast<float>(samples)) * 6.283185f;
            const float spread = 0.7f + 0.3f * std::fmod(seed + i * 0.1f, 1.0f);
            const float dist = size * spread * (0.5f + 0.5f * c.r[(i * 3) % MILK_WAVE_SAMPLES]) * boost;
            float x = baseX + std::cos(angle) * dist;
            float y = baseY + std::sin(angle) * dist;
            const float swirl = c.time * 3.0f + angle;
            x += std::cos(swirl) * size * 0.1f;
            y += std::sin(swirl) * size * 0.1f;
            p.push_back({x * c.aspectY + c.waveX, y * c.aspectX + c.waveY});
            alpha *= fade;
        }
        if (close) p.push_back(p[0]);
        break;
    }
    default:
        p.assign(MILK_WAVE_SAMPLES, Point{0.0f, 0.0f});
        break;
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

// MilkDrop scales alpha by mode: modes 1, 12 and 15 gain a quarter, modes 2 and 5 are scaled down,
// and mode 3 replaces it with a fixed value, then scales it by the treble sum. The factors exist
// only for an image exactly 256, 512, 1024 or 2048 pixels wide; at any other width alpha is left
// as it is.
float modeAlpha(int mode, float alpha, int width, float trebSum) {
    auto pick = [&](float a, float b, float c, float d, float other) {
        switch (width) {
        case 256: return a;
        case 512: return b;
        case 1024: return c;
        case 2048: return d;
        default: return other;
        }
    };
    if (mode == MODE_XY_SPIRAL || mode == MODE_SKEWED_SPIRAL || mode == MODE_LASSO) return alpha * 1.25f;
    if (mode == MODE_SPIRO || mode == MODE_EXPLOSIVE_HASH) return alpha * pick(0.07f, 0.09f, 0.11f, 0.13f, 1.0f);
    if (mode == MODE_SPIRO_VOLUME) return pick(0.075f, 0.15f, 0.22f, 0.33f, alpha) * 1.3f * trebSum * trebSum;
    return alpha;
}

} // namespace

void buildMainWave(const MainWaveSettings& s, const MilkAudioFrame& audio, int width, int height, MainWave& out) {
    out = MainWave{};
    const int count = std::clamp(s.modeCount, 1, static_cast<int>(MODE_LIMIT));
    // MilkDrop's switch matches 0..count-1 and nothing else; any other mode keeps the zeroed buffer.
    auto known = [&](int m) { return m >= 0 && m < count ? m : static_cast<int>(MODE_NONE); };
    const int newMode = s.mode % count;
    const int mode = known(newMode);

    Context c;
    c.width = width;
    c.time = s.time;
    if (width > height) c.aspectY = static_cast<float>(height) / width;
    else c.aspectX = static_cast<float>(width) / height;
    c.waveX = 2.0f * s.x - 1.0f;
    c.waveY = 2.0f * s.y - 1.0f;
    c.mystery = s.mystery;
    c.bass = s.bass;
    c.mid = s.mid;
    c.spectrum = audio.spectrumL.data();

    // Each sample is scaled by fWaveScale / 128 and smoothed with a one-pole filter whose
    // feedback is fWaveSmoothing.
    std::copy(audio.waveL.begin(), audio.waveL.end(), c.l.begin());
    std::copy(audio.waveR.begin(), audio.waveR.end(), c.r.begin());
    const float scale = s.scale / 128.0f;
    const float mix2 = s.smoothing;
    const float mix1 = scale * (1.0f - mix2);
    c.l[0] *= scale;
    c.r[0] *= scale;
    for (int i = 1; i < MILK_FFT_INPUT; ++i) {
        c.l[i] = c.l[i] * mix1 + c.l[i - 1] * mix2;
        c.r[i] = c.r[i] * mix1 + c.r[i - 1] * mix2;
    }
    std::copy(c.r.begin(), c.r.begin() + PCM_TAIL, c.l.begin() + MILK_FFT_INPUT);
    std::copy(audio.spectrumL.begin(), audio.spectrumL.begin() + PCM_TAIL, c.r.begin() + MILK_FFT_INPUT);

    auto alphaFor = [&](int m) {
        if (m == MODE_NONE) return s.alpha;
        float alpha = modeAlpha(m, s.alpha, width, s.treb);
        if (s.modAlphaByVolume) {
            float ratio = (s.vol - s.modAlphaStart) / (s.modAlphaEnd - s.modAlphaStart);
            if (!std::isfinite(ratio)) ratio = s.vol > s.modAlphaStart ? 1e9f : 0.0f;
            alpha *= ratio;
        }
        return std::clamp(alpha, 0.0f, 1.0f);
    };

    Wave fresh;
    float alpha = alphaFor(mode);
    generate(mode, c, !s.blending, alpha, fresh);

    if (s.blending && s.oldMode != newMode) {
        // MilkDrop resamples the old mode's points onto the new one's count and mixes positions,
        // then mixes the two modes' alphas. Where the old mode breaks into two lines, the new
        // points that land on its last first-line point break there too.
        const int oldMode = known(s.oldMode);
        Wave previous;
        float oldAlpha = alphaFor(oldMode);
        generate(oldMode, c, false, oldAlpha, previous);
        Line& v1 = fresh.points;
        const Line& v2 = previous.points;
        const float mix = s.blendMix, mixOld = 1.0f - mix;
        if (!v1.empty() && v2.size() > 1) {
            const float step = static_cast<float>(v2.size() - 1) / static_cast<float>(v1.size());
            for (size_t i = 0; i < v1.size(); ++i) {
                const float at = i * step;
                const size_t n = std::min(static_cast<size_t>(at), v2.size() - 2);
                const float t = at - static_cast<float>(n);
                Point old;
                if (static_cast<int>(n) == previous.split - 1) {
                    old = v2[n];
                    fresh.split = static_cast<int>(i) + 1;
                } else {
                    for (int k = 0; k < 2; ++k) old[k] = v2[n][k] * (1.0f - t) + v2[n + 1][k] * t;
                }
                for (int k = 0; k < 2; ++k) v1[i][k] = v1[i][k] * mix + old[k] * mixOld;
            }
        }
        alpha = alpha * mix + oldAlpha * mixOld;
    }

    const Line& points = fresh.points;
    const size_t split = fresh.split < 0 ? points.size() : std::min(points.size(), static_cast<size_t>(fresh.split));
    smooth(Line(points.begin(), points.begin() + split), out.lines[0]);
    smooth(Line(points.begin() + split, points.end()), out.lines[1]);
    out.a = alpha;
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
    // MilkDrop passes the colour as bytes, truncated, and skips a wave whose alpha is below 0.004.
    // Alpha above 1 reaches this point only from an unmatched mode, and its byte wraps.
    auto byte = [](float v) { return std::floor(v * 255.0f) / 255.0f; };
    if (!(out.a >= 0.004f)) out.a = 0.0f;
    out.a = std::fmod(std::floor(out.a * 255.0f), 256.0f) / 255.0f;
    if (!std::isfinite(out.a)) out.a = 0.0f;
    out.r = byte(out.r);
    out.g = byte(out.g);
    out.b = byte(out.b);
    out.dots = s.dots;
    out.thick = s.thick;
    out.additive = s.additive;
}

} // namespace Milk

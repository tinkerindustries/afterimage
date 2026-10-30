#include "milk_blend.h"

#include <algorithm>
#include <cmath>

namespace Milk {

namespace {

// Midpoint displacement over the `c` values, as MilkDrop's GenPlasma does: the four corners are
// set first, edges take the mean of their ends plus noise, and the noise halves at each level.
void plasma(std::vector<float>& c, int stride, int x0, int x1, int y0, int y1, float dt, float aspectX, float aspectY,
            std::uniform_real_distribution<float>& unit, std::mt19937& rng) {
    const int midx = (x0 + x1) / 2;
    const int midy = (y0 + y1) / 2;
    auto at = [&](int x, int y) -> float& { return c[y * stride + x]; };
    float t00 = at(x0, y0), t01 = at(x1, y0), t10 = at(x0, y1), t11 = at(x1, y1);
    if (y1 - y0 >= 2) {
        if (x0 == 0) at(x0, midy) = 0.5f * (t00 + t10) + (unit(rng) * 2 - 1) * dt * aspectY;
        at(x1, midy) = 0.5f * (t01 + t11) + (unit(rng) * 2 - 1) * dt * aspectY;
    }
    if (x1 - x0 >= 2) {
        if (y0 == 0) at(midx, y0) = 0.5f * (t00 + t01) + (unit(rng) * 2 - 1) * dt * aspectX;
        at(midx, y1) = 0.5f * (t10 + t11) + (unit(rng) * 2 - 1) * dt * aspectX;
    }
    if (y1 - y0 >= 2 && x1 - x0 >= 2) {
        t00 = at(x0, midy);
        t01 = at(x1, midy);
        t10 = at(midx, y0);
        t11 = at(midx, y1);
        at(midx, midy) = 0.25f * (t10 + t11 + t00 + t01) + (unit(rng) * 2 - 1) * dt;
        plasma(c, stride, x0, midx, y0, midy, dt * 0.5f, aspectX, aspectY, unit, rng);
        plasma(c, stride, midx, x1, y0, midy, dt * 0.5f, aspectX, aspectY, unit, rng);
        plasma(c, stride, x0, midx, midy, y1, dt * 0.5f, aspectX, aspectY, unit, rng);
        plasma(c, stride, midx, x1, midy, y1, dt * 0.5f, aspectX, aspectY, unit, rng);
    }
}

// Milkwave 4.1's added patterns, from its plugin_blendpatterns.cpp, for ordinary presets. Each maps
// a value t to the same sweeping band as MilkDrop's own. Types 15 to 17 animate with a clock that,
// as in Milkwave, keeps running across blends.
template <typename Unit, typename Band>
void milkwavePattern(int type, int gridX, int gridY, float aspectX, float aspectY, std::mt19937& rng, Unit& unit, Band& setBand,
                     BlendPattern& out) {
    constexpr float TAU = 6.2831853f;
    auto frand = [&] { return unit(rng); };
    auto coin = [&] { return rng() % 2 == 0; };
    auto centred = [&](int x, int y, float& fx, float& fy) {
        fx = (x / static_cast<float>(gridX) - 0.5f) * aspectX;
        fy = (y / static_cast<float>(gridY) - 0.5f) * aspectY;
    };
    static float clock15 = 0.0f, clock16 = 0.0f, clock17 = 0.0f;
    constexpr float FPS = 60.0f;
    auto run = [&](float width, auto&& t) {
        size_t n = 0;
        for (int y = 0; y <= gridY; ++y) {
            for (int x = 0; x <= gridX; ++x) setBand(n++, t(x, y), width);
        }
    };

    if (type == 4) {
        // Clock wipe.
        const float w = 0.08f + 0.14f * frand();
        const float dir = coin() ? -1.0f : 1.0f;
        const float start = frand() * TAU;
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            float angle = std::atan2(fy, fx);
            const float dist = std::sqrt(fx * fx + fy * fy) * 1.41421356f;
            if (angle < 0) angle += TAU;
            angle = std::fmod(angle * dir + start + 10.0f * TAU, TAU);
            float t = angle / TAU;
            if (t < w) t += 1.0f;
            return t - dist * 0.1f;
        });
    } else if (type == 5) {
        // Spiral.
        const float w = 0.42f + 0.18f * frand();
        const float loops = 0.65f + 0.95f * frand();
        const float phase = frand() * TAU;
        const bool inward = coin();
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            const float angle = std::atan2(fy, fx);
            const float radius = std::sqrt(fx * fx + fy * fy) * 1.41421356f;
            float spiral = (angle + 3.14159265f) / TAU + loops * radius + phase / TAU;
            spiral -= std::floor(spiral);
            const float dist = std::min(std::fabs(spiral - 0.5f) * 2.0f, 1.0f);
            float t = dist * dist * (3.0f - 2.0f * dist);
            const float soften = 0.05f + 0.95f * (radius * radius * (3.0f - 2.0f * radius));
            t = 0.5f + (t - 0.5f) * soften;
            return inward ? 1.0f - t : t;
        });
    } else if (type == 6) {
        // Diamond.
        const float w = 0.07f + 0.12f * frand();
        const bool reverse = coin();
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            const float diamond = (std::fabs(fx) + std::fabs(fy)) * 0.5f;
            return reverse ? 1.0f - diamond : diamond;
        });
    } else if (type == 7) {
        // Three-armed clock wipe from a point near the centre.
        const float w = 0.05f + 0.15f * frand();
        const bool reverse = coin();
        const float glowIntensity = 0.5f + frand() * 1.5f;
        const float centreX = 0.5f + (frand() - 0.5f) * 0.1f;
        const float centreY = 0.5f + (frand() - 0.5f) * 0.1f;
        run(w, [&](int x, int y) {
            const float fx = (x / static_cast<float>(gridX) - centreX) * aspectX;
            const float fy = (y / static_cast<float>(gridY) - centreY) * aspectY;
            float angle = std::atan2(fy, fx);
            const float dist = std::sqrt(fx * fx + fy * fy) * 1.41421356f;
            if (angle < 0) angle += TAU;
            float clock = angle / TAU * 3.0f;
            if (reverse) clock = 3.0f - clock;
            clock -= std::floor(clock);
            return clock + (1.0f - dist) * glowIntensity * 0.3f;
        });
    } else if (type == 8) {
        // Cross or X.
        const float w = 0.08f + 0.12f * frand();
        const bool diagonal = coin();
        const float centreBias = 0.3f + frand() * 0.4f;
        const float softness = 0.1f + frand() * 0.2f;
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            float t;
            if (diagonal) {
                const float d1 = (fx + fy) * 0.7071f, d2 = (fx - fy) * 0.7071f;
                t = std::max(std::fabs(d1), std::fabs(d2));
            } else {
                t = std::max(std::fabs(fx), std::fabs(fy));
            }
            t = std::pow(t, centreBias);
            return std::clamp(t * (1.0f + softness) - softness * 0.5f, 0.0f, 1.0f);
        });
    } else if (type == 9) {
        // Checkerboard turned 45 degrees. Milkwave reads its .milk2 settings here, which are 0 for
        // an ordinary preset.
        const float w = 0.26f;
        const float phase = -0.04f;
        const float cr = std::cos(0.78539816f), sr = std::sin(0.78539816f);
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            const float u = (fx * cr - fy * sr) * 3.0f + 0.5f + phase;
            const float v = (fx * sr + fy * cr) * 3.0f + 0.5f - phase;
            const int cellX = static_cast<int>(std::floor(u)), cellY = static_cast<int>(std::floor(v));
            const float lx = u - std::floor(u), ly = v - std::floor(v);
            const float edge = std::min(std::min(lx, 1.0f - lx), std::min(ly, 1.0f - ly));
            const float t = (((cellX + 1024) + (cellY + 1024)) & 1) != 0 ? 1.0f : 0.0f;
            float e = std::clamp(edge / (w * 1.05f), 0.0f, 1.0f);
            e = e * e * (3.0f - 2.0f * e);
            return 0.5f + (t - 0.5f) * e;
        });
    } else if (type == 10) {
        // Curtains, in one to four stripes.
        const float w = 0.05f + 0.15f * frand();
        const bool opening = coin();
        const bool vertical = coin();
        const float wrinkles = 0.5f + frand() * 2.0f;
        const float gap = 0.05f + frand() * 0.15f;
        const bool reverse = coin();
        const int repeats = 1 + static_cast<int>(rng() % 4);
        const float variation = 0.3f * frand();
        const bool alternate = coin();
        run(w, [&](int x, int y) {
            const float fx = x / static_cast<float>(gridX), fy = y / static_cast<float>(gridY);
            const float pos = vertical ? fx : fy;
            const float across = vertical ? fy : fx;
            const float segmentPos = pos * repeats;
            const int segment = static_cast<int>(std::floor(segmentPos));
            const float local = segmentPos - segment;
            const float centreDist = std::max(std::fabs(local - 0.5f) - gap / 2, 0.0f);
            float side = local < 0.5f ? -1.0f : 1.0f;
            float t = centreDist * 2.0f + std::sin(segment * 1.618f) * variation;
            t += std::sin(across * 3.14159f * wrinkles) * 0.1f * (1.0f - t);
            if (opening) t = 1.0f - t;
            if (alternate && segment % 2 == 1) side = -side;
            if (reverse) return side > 0 ? t : 1.0f - t;
            return side > 0 ? 1.0f - t : t;
        });
    } else if (type == 11) {
        // Bubbles.
        const float w = 0.05f + 0.15f * frand();
        const int count = 10 + static_cast<int>(rng() % 30);
        const float sizeMin = 0.05f + frand() * 0.1f;
        const float sizeMax = 0.15f + frand() * 0.2f;
        const bool growing = coin();
        struct Bubble {
            float x, y, size;
        };
        std::vector<Bubble> bubbles(count);
        for (Bubble& b : bubbles) {
            b.x = frand();
            b.y = frand();
            b.size = sizeMin + frand() * (sizeMax - sizeMin);
            frand();  // Milkwave draws a speed it never uses.
        }
        run(w, [&](int x, int y) {
            const float fx = x / static_cast<float>(gridX), fy = y / static_cast<float>(gridY);
            float best = 0.0f;
            for (const Bubble& b : bubbles) {
                const float dx = (fx - b.x) * aspectX, dy = (fy - b.y) * aspectY;
                float influence = std::max(1.0f - std::sqrt(dx * dx + dy * dy) / b.size, 0.0f);
                influence = influence * influence * (3.0f - 2.0f * influence);
                best = std::max(best, influence);
            }
            return growing ? best : 1.0f - best;
        });
    } else if (type == 12) {
        // Kaleidoscope segments.
        const float w = 0.06f + 0.14f * frand();
        const int segments = 3 + static_cast<int>(rng() % 9);
        const float segmentAngle = TAU / segments;
        const float rotation = frand() * TAU;
        const bool mirror = coin();
        const float radial = 0.5f + frand();
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            float angle = std::atan2(fy, fx) + rotation;
            const float radius = std::sqrt(fx * fx + fy * fy) * 1.41421356f;
            if (angle < 0) angle += TAU;
            if (angle >= TAU) angle -= TAU;
            const int segment = static_cast<int>(angle / segmentAngle);
            float offset = angle - segment * segmentAngle;
            if (mirror && offset > segmentAngle * 0.5f) offset = segmentAngle - offset;
            return offset / segmentAngle * 0.7f + radius * 0.3f * radial;
        });
    } else if (type == 13) {
        // Moebius bands.
        const float w = 0.07f + 0.13f * frand();
        const float twist = 1.0f + frand() * 2.0f;
        const bool reverse = coin();
        const float stripWidth = 0.3f + frand() * 0.4f;
        const float offset = frand() * 0.5f;
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            const float angle = std::atan2(fy, fx);
            const float radius = std::sqrt(fx * fx + fy * fy) * 1.41421356f;
            float progress = ((angle + 3.14159265f) / TAU + offset) * twist;
            if (reverse) progress = -progress;
            const float value = radius + 0.3f * std::sin(progress * 3.14159265f);
            return 1.0f - std::fmod(value * (1.0f / stripWidth), 1.0f);
        });
    } else if (type == 14) {
        // Star sectors.
        const float w = 0.07f + 0.03f * frand();
        const float phase = frand() * TAU;
        const bool reverse = coin();
        const float segment = TAU / 10;
        const float edgeSoft = 0.16f + 0.08f * frand();
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            float angle = std::atan2(fy, fx) + phase;
            if (angle < 0.0f) angle += TAU;
            if (angle >= TAU) angle -= TAU;
            const float sectorPos = angle / segment;
            const int sector = static_cast<int>(std::floor(sectorPos));
            const float local = sectorPos - sector;
            float edge = std::min(std::min(local, 1.0f - local) / edgeSoft, 1.0f);
            edge = edge * edge * (3.0f - 2.0f * edge);
            const float colour = (sector + static_cast<int>(phase / TAU * 10.0f)) % 2 == 0 ? 1.0f : 0.0f;
            float t = std::clamp(0.5f + (colour - 0.5f) * edge * 0.5f * 2.0f, 0.0f, 1.0f);
            return reverse ? 1.0f - t : t;
        });
    } else if (type == 15) {
        // Animated tiles.
        const float w = 0.08f + 0.12f * frand();
        const int tile = 8 + static_cast<int>(rng() % 25);
        const float beatSync = 0.5f + frand() * 1.5f;
        const bool diagonal = coin();
        const bool cycling = coin();
        const float speed = 0.5f + frand() * 2.0f;
        clock15 += 1.0f / FPS;
        const float time = clock15 * speed;
        const float beat = std::pow(std::sin(time * 3.0f) * 0.5f + 0.5f, beatSync);
        run(w, [&](int x, int y) {
            const float fx = x / static_cast<float>(gridX), fy = y / static_cast<float>(gridY);
            const int tx = static_cast<int>(fx * gridX / tile), ty = static_cast<int>(fy * gridY / tile);
            const float pattern = diagonal ? ((tx + ty) % 2) * 0.8f + 0.1f : ((tx % 2) == (ty % 2)) * 0.8f + 0.1f;
            const float anim = std::sin(time * 2.0f + tx * 0.3f + ty * 0.7f) * 0.5f + 0.5f;
            float t = (pattern * 0.7f + anim * 0.3f) * beat;
            if (cycling) t = std::fmod(t + std::fmod(time * 0.2f + tx * 0.1f + ty * 0.15f, 1.0f) * 0.3f, 1.0f);
            return t;
        });
    } else if (type == 16) {
        // Flames rising from the bottom.
        const float w = 0.08f + 0.04f * frand();
        const float speed = 0.7f + frand() * 0.6f;
        const float seed1 = frand() * 10.0f, seed2 = frand() * 20.0f, seed3 = frand() * 30.0f;
        clock16 += 1.0f / FPS;
        const float time = clock16;
        run(w, [&](int x, int y) {
            const float fx = x / static_cast<float>(gridX), fy = y / static_cast<float>(gridY);
            const float flame = std::sin(fx * 15.0f + seed1 + time * 2.0f) * 0.4f + std::sin(fx * 30.0f + seed2 + time * 3.7f) * 0.2f +
                                std::sin(fx * 45.0f + seed3 + time * 5.3f) * 0.1f;
            const float shape = (1.0f - fy) * (0.3f + flame * 0.7f);
            const float front = std::fmod(time * speed, 1.5f);
            return std::clamp(1.0f - (fy - front + shape), 0.0f, 1.0f);
        });
    } else if (type == 17) {
        // Drain swirl.
        const float w = 0.05f + 0.15f * frand();
        const float intensity = 2.0f + frand() * 3.0f;
        const float drain = 0.5f + frand() * 1.5f;
        const bool clockwise = coin();
        const float pull = 0.7f + frand() * 0.6f;
        const bool invert = coin();
        clock17 += 1.0f / FPS;
        const float time = clock17 * drain;
        run(w, [&](int x, int y) {
            float fx, fy;
            centred(x, y, fx, fy);
            const float radius = std::sqrt(fx * fx + fy * fy) * 1.41421356f;
            float swirl = (1.0f - radius) * intensity;
            if (clockwise) swirl = -swirl;
            const float angle = std::atan2(fy, fx) + swirl + time * 2.0f;
            const float t = radius * pull + (1.0f - pull) * (0.5f + 0.5f * std::sin(angle * 2.0f + radius * 5.0f));
            return invert ? 1.0f - t : t;
        });
    } else {
        // Julia set, normalised over the mesh.
        const float w = 0.08f + 0.12f * frand();
        const float jr = -0.8f + frand() * 1.6f, ji = -0.8f + frand() * 1.6f;
        const int iterations = 20 + static_cast<int>(rng() % 20);
        const float zoom = 0.7f + frand() * 1.6f;
        const float rotation = frand() * TAU;
        const float edgeSoft = 0.3f + frand() * 0.5f;
        const float contrast = 0.7f + frand() * 0.6f;
        const float cr = std::cos(rotation), sr = std::sin(rotation);
        std::vector<float> values;
        for (int y = 0; y <= gridY; ++y) {
            for (int x = 0; x <= gridX; ++x) {
                float fx, fy;
                centred(x, y, fx, fy);
                float zx = (fx * cr - fy * sr) * zoom, zy = (fx * sr + fy * cr) * zoom;
                int i = 0;
                for (; i < iterations; ++i) {
                    const float tmp = zx * zx - zy * zy + jr;
                    zy = 2 * zx * zy + ji;
                    zx = tmp;
                    if (zx * zx + zy * zy > 4.0f) break;
                }
                float t = 1.0f;
                if (i < iterations) {
                    const float logZn = std::log(zx * zx + zy * zy) / 2.0f;
                    const float nu = std::log(logZn / std::log(2.0f)) / std::log(2.0f);
                    t = (i + 1 - nu) / iterations;
                }
                values.push_back(std::pow(t, contrast));
            }
        }
        const auto [lo, hi] = std::minmax_element(values.begin(), values.end());
        float range = *hi - *lo;
        if (range < 0.0001f) range = 1.0f;
        const float inv = 1.0f / w;
        for (size_t n = 0; n < values.size(); ++n) {
            float t = (values[n] - *lo) / range;
            t = t * t * (3.0f - 2.0f * t) * (1.0f - edgeSoft) + t * edgeSoft;
            out.a[n] = inv * (1.0f + w * 1.5f);
            out.c[n] = std::clamp(-inv + inv * t * 1.1f, -10.0f, 10.0f);
        }
    }
}

} // namespace

float cosineInterp(float x) { return 0.5f - 0.5f * std::cos(x * 3.1415926535f); }

void makeBlendPattern(int type, int gridX, int gridY, float aspectX, float aspectY, std::mt19937& rng, BlendPattern& out) {
    const int stride = gridX + 1;
    const size_t count = static_cast<size_t>(stride) * (gridY + 1);
    out.a.assign(count, 1.0f);
    out.c.assign(count, 0.0f);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    // Every pattern but the uniform one maps a value t in 0..1 to a band of this width that sweeps
    // from t = 0 to t = 1 as the blend runs.
    auto band = [&](size_t n, float t, float width) {
        const float inv = 1.0f / width;
        out.a[n] = inv * (1.0f + width);
        out.c[n] = -inv + inv * t;
    };

    if (type == 1) {
        const float ang = unit(rng) * 6.28f;
        const float width = 0.1f + 0.2f * unit(rng);
        const float vx = std::cos(ang), vy = std::sin(ang);
        size_t n = 0;
        for (int y = 0; y <= gridY; ++y) {
            const float fy = y / static_cast<float>(gridY) * aspectY;
            for (int x = 0; x <= gridX; ++x) {
                const float fx = x / static_cast<float>(gridX) * aspectX;
                float t = (fx - 0.5f) * vx + (fy - 0.5f) * vy + 0.5f;
                t = (t - 0.5f) / std::sqrt(2.0f) + 0.5f;
                band(n++, t, width);
            }
        }
    } else if (type == 2) {
        const float width = 0.12f + 0.13f * unit(rng);
        out.c[0] = unit(rng);
        out.c[gridX] = unit(rng);
        out.c[gridY * stride] = unit(rng);
        out.c[gridY * stride + gridX] = unit(rng);
        plasma(out.c, stride, 0, gridX, 0, gridY, 0.25f, aspectX, aspectY, unit, rng);
        const auto [lo, hi] = std::minmax_element(out.c.begin(), out.c.end());
        const float minc = *lo, mult = 1.0f / (*hi - *lo);
        for (size_t n = 0; n < count; ++n) band(n, (out.c[n] - minc) * mult, width);
    } else if (type >= 4 && type <= 18) {
        milkwavePattern(type, gridX, gridY, aspectX, aspectY, rng, unit, band, out);
    } else if (type == 3) {
        const float width = 0.025f + 0.14f * unit(rng) + 0.34f * unit(rng);
        const bool insideOut = rng() % 2 == 0;
        size_t n = 0;
        for (int y = 0; y <= gridY; ++y) {
            const float dy = (y / static_cast<float>(gridY) - 0.5f) * aspectY;
            for (int x = 0; x <= gridX; ++x) {
                const float dx = (x / static_cast<float>(gridX) - 0.5f) * aspectX;
                float t = std::sqrt(dx * dx + dy * dy) * 1.41421f;
                if (insideOut) t = 1.0f - t;
                band(n++, t, width);
            }
        }
    }
    // The patterns are made with row 0 at the bottom, as MilkDrop's mesh is. Ours has it at the top.
    for (int y = 0; y < (gridY + 1) / 2; ++y) {
        std::swap_ranges(out.a.begin() + y * stride, out.a.begin() + (y + 1) * stride, out.a.begin() + (gridY - y) * stride);
        std::swap_ranges(out.c.begin() + y * stride, out.c.begin() + (y + 1) * stride, out.c.begin() + (gridY - y) * stride);
    }
}

} // namespace Milk

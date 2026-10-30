#include "turret_game_sdl.h"

#include "audio_synth.h"
#include "feedback.h"
#include "preset.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace Milk {

namespace {

constexpr int WIDTH = 960;
constexpr int HEIGHT = 540;
constexpr float STEP_SECONDS = 1.0f / 60.0f;
constexpr int SAMPLES_PER_TICK = SAMPLE_RATE / 60;
constexpr int FLOW_EXAGGERATION = 6;

enum Brightness : int {
    BRIGHTNESS_NORMAL = 0,
    BRIGHTNESS_DARK = 1,
    BRIGHTNESS_SATURATED = 2,
};

const char* BRIGHTNESS_NAMES[] = {"normal", "dark", "saturated"};

struct World {
    Params params;
    SynthState synth;
    FrameVars frame;
    std::vector<float> audio = std::vector<float>(SAMPLES_PER_TICK, 0.0f);
    FeedbackBuffer buffer{WIDTH, HEIGHT};
    uint64_t tick = 0;
    float meanLuma = 0.0f;
    float litFraction = 0.0f;
    int brightness = BRIGHTNESS_DARK;
};

void measure(World& w) {
    const std::vector<float>& px = w.buffer.pixels();
    double sum = 0.0;
    int lit = 0;
    int n = 0;
    for (size_t i = 0; i < px.size(); i += 3 * 7) {
        const float luma = 0.299f * px[i] + 0.587f * px[i + 1] + 0.114f * px[i + 2];
        sum += luma;
        if (luma > 0.05f) ++lit;
        ++n;
    }
    w.meanLuma = static_cast<float>(sum / std::max(n, 1));
    w.litFraction = static_cast<float>(lit) / std::max(n, 1);
}

void build(World& w, uint64_t seed) {
    w.params = Params{};
    w.tick = 0;
    resetSynth(w.synth, static_cast<uint32_t>(seed), w.params.bpm);
    std::fill(w.audio.begin(), w.audio.end(), 0.0f);
    w.frame = frameFromParams(w.params, 0.0f);
    w.buffer.clear();
    w.meanLuma = 0.0f;
    w.litFraction = 0.0f;
    w.brightness = BRIGHTNESS_DARK;
}

void step(World& w, turret::Context& ctx) {
    w.tick = ctx.tick();
    const float t = w.tick * STEP_SECONDS;
    const int beats = synthesise(w.synth, w.params.bpm, SAMPLES_PER_TICK, w.audio.data());
    if (beats > 0) {
        turret::json::Value detail = turret::json::Value::MakeObject();
        detail["beat"] = static_cast<double>(w.synth.beatCount);
        detail["bass"] = w.synth.levels.bass;
        ctx.Emit("beat", "audio", detail);
    }

    FrameVars f = frameFromParams(w.params, t);
    applyMotion(w.params.motion, f, w.synth.levels, t);
    clampFrame(f);
    w.frame = f;

    w.buffer.warp(f, perVertexFor(w.params.motion), w.synth.levels, t);
    w.buffer.drawWave(f, w.audio.data(), SAMPLES_PER_TICK);

    measure(w);
    int brightness = BRIGHTNESS_NORMAL;
    if (w.meanLuma < 0.003f) brightness = BRIGHTNESS_DARK;
    else if (w.meanLuma > 0.6f) brightness = BRIGHTNESS_SATURATED;
    if (brightness != w.brightness) {
        turret::json::Value detail = turret::json::Value::MakeObject();
        detail["from"] = BRIGHTNESS_NAMES[w.brightness];
        detail["to"] = BRIGHTNESS_NAMES[brightness];
        detail["mean_luma"] = w.meanLuma;
        ctx.Emit("brightness", "image", detail);
        w.brightness = brightness;
    }
}

// Converts uv space (where cx and cy live) to frame pixels, undoing the aspect squeeze.
turret::Vec2 uvToPixels(float u, float v) {
    const float aspectY = static_cast<float>(HEIGHT) / WIDTH;
    return turret::Vec2{u * WIDTH, ((v - 0.5f) / aspectY + 0.5f) * HEIGHT};
}

void drawImage(const World& w, turret::Context& ctx, uint8_t shade) {
    SDL_Renderer* renderer = turret::backend::Renderer();
    static SDL_Texture* texture = nullptr;
    static std::vector<uint8_t> rgba(static_cast<size_t>(WIDTH) * HEIGHT * 4);
    if (texture == nullptr) {
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
        if (texture == nullptr) return;
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    }
    const std::vector<float>& px = w.buffer.pixels();
    const float gain = w.params.gammaAdj * 255.0f;
    for (size_t i = 0, o = 0; i < px.size(); i += 3, o += 4) {
        rgba[o] = static_cast<uint8_t>(std::min(255.0f, px[i] * gain));
        rgba[o + 1] = static_cast<uint8_t>(std::min(255.0f, px[i + 1] * gain));
        rgba[o + 2] = static_cast<uint8_t>(std::min(255.0f, px[i + 2] * gain));
        rgba[o + 3] = 255;
    }
    SDL_UpdateTexture(texture, nullptr, rgba.data(), WIDTH * 4);
    SDL_SetTextureColorMod(texture, shade, shade, shade);
    const turret::Rect screen = ctx.ToScreen(turret::Rect{0, 0, static_cast<float>(WIDTH), static_cast<float>(HEIGHT)});
    const SDL_FRect dst = turret::backend::ToFRect(screen);
    SDL_RenderTexture(renderer, texture, nullptr, &dst);
}

turret::Rect markerBox(turret::Vec2 p) { return turret::Rect{p.x - 12.0f, p.y - 12.0f, 24.0f, 24.0f}; }

void markEntities(const World& w, turret::Context& ctx) {
    ctx.MarkEntity("centre", markerBox(uvToPixels(w.frame.cx, w.frame.cy)));
    ctx.MarkEntity("wave", markerBox(turret::Vec2{w.frame.waveX * WIDTH, w.frame.waveY * HEIGHT}));
}

void draw(const World& w, turret::Context& ctx) {
    drawImage(w, ctx, 255);
    markEntities(w, ctx);
}

std::string fmt(const char* format, double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), format, value);
    return buf;
}

void drawSchematic(const World& w, turret::Context& ctx) {
    {
        turret::LayerScope image(ctx, "feedback");
        if (image.Drawing()) drawImage(w, ctx, 110);
    }
    {
        turret::LayerScope field(ctx, "warp_field");
        std::vector<MeshUv> mesh;
        computeMesh(w.frame, perVertexFor(w.params.motion), w.synth.levels, w.tick * STEP_SECONDS, WIDTH, HEIGHT, mesh);
        const turret::Color arrow{120, 220, 255, 255};
        for (int j = 3; j < MESH_Y; j += 6) {
            for (int i = 3; i < MESH_X; i += 6) {
                const float vx = static_cast<float>(i) / (MESH_X - 1) * WIDTH;
                const float vy = static_cast<float>(j) / (MESH_Y - 1) * HEIGHT;
                const MeshUv& uv = mesh[j * MESH_X + i];
                const float sx = uv.u * WIDTH;
                const float sy = uv.v * HEIGHT;
                const turret::Vec2 tip{vx, vy};
                const turret::Vec2 tail{vx - (vx - sx) * FLOW_EXAGGERATION, vy - (vy - sy) * FLOW_EXAGGERATION};
                ctx.SchematicArrow(tail, tip, arrow, 1.5f);
            }
        }
        ctx.SchematicLabel(turret::Vec2{12.0f, HEIGHT - 28.0f}, "flow x" + std::to_string(FLOW_EXAGGERATION), turret::kDarkGrey, 13);
    }
    {
        turret::LayerScope markers(ctx, "markers");
        const turret::Vec2 centre = uvToPixels(w.frame.cx, w.frame.cy);
        ctx.SchematicOutline(markerBox(centre), turret::kYellow, 3.0f);
        ctx.SchematicLabel(turret::Vec2{centre.x + 16.0f, centre.y - 8.0f}, "centre", turret::kYellow, 13);
        const turret::Vec2 wave{w.frame.waveX * WIDTH, w.frame.waveY * HEIGHT};
        ctx.DrawCircleOutline(wave, 10.0f, turret::kOrange, 2.0f);
        ctx.SchematicLabel(turret::Vec2{wave.x + 16.0f, wave.y + 8.0f}, "wave " + WAVE_MODE_NAMES[w.frame.waveMode], turret::kOrange, 13);
        markEntities(w, ctx);
    }
    {
        turret::LayerScope readout(ctx, "readout");
        const turret::Color text{230, 230, 230, 255};
        const FrameVars& f = w.frame;
        std::vector<std::string> lines = {
            "motion " + MOTION_NAMES[w.params.motion],
            fmt("zoom %.4f", f.zoom),
            fmt("rot %.4f", f.rot),
            fmt("warp %.3f", f.warp),
            fmt("decay %.3f", f.decay),
            fmt("mean luma %.3f", w.meanLuma),
        };
        float y = 10.0f;
        for (const std::string& line : lines) {
            ctx.DrawScreenText(turret::Vec2{10.0f, y}, line, text, 14.0f);
            y += 18.0f;
        }
        const float levels[3] = {w.synth.levels.bass, w.synth.levels.mid, w.synth.levels.treb};
        const char* names[3] = {"bass", "mid", "treb"};
        const turret::Color colours[3] = {turret::kRed, turret::kGreen, turret::kBlue};
        for (int b = 0; b < 3; ++b) {
            const float bx = 200.0f + b * 40.0f;
            const float bh = std::min(levels[b], 3.0f) * 30.0f;
            ctx.DrawScreenText(turret::Vec2{bx, 104.0f}, names[b], text, 12.0f);
            ctx.DrawRect(turret::Rect{bx, 100.0f - bh, 24.0f, bh}, colours[b]);
            ctx.DrawRectOutline(turret::Rect{bx, 70.0f, 24.0f, 30.0f}, text, 1.0f);
        }
    }
}

std::string hexColour(const uint8_t c[3]) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c[0], c[1], c[2]);
    return buf;
}

bool parseHex(const std::string& s, uint8_t out[3]) {
    if (s.size() != 7 || s[0] != '#') return false;
    unsigned r = 0, g = 0, b = 0;
    if (std::sscanf(s.c_str() + 1, "%02x%02x%02x", &r, &g, &b) != 3) return false;
    out[0] = static_cast<uint8_t>(r);
    out[1] = static_cast<uint8_t>(g);
    out[2] = static_cast<uint8_t>(b);
    return true;
}

int indexOf(const std::vector<std::string>& names, const std::string& name) {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) return static_cast<int>(i);
    }
    return -1;
}

void describe(const World& w, turret::json::Value& out) {
    out["phase"] = "running";
    const AudioLevels& a = w.synth.levels;
    turret::json::Value audio = turret::json::Value::MakeObject();
    audio["bass"] = a.bass;
    audio["mid"] = a.mid;
    audio["treb"] = a.treb;
    audio["bass_att"] = a.bassAtt;
    audio["mid_att"] = a.midAtt;
    audio["treb_att"] = a.trebAtt;
    audio["beat"] = static_cast<double>(w.synth.beatCount);
    out["audio"] = audio;

    const FrameVars& f = w.frame;
    turret::json::Value frame = turret::json::Value::MakeObject();
    frame["zoom"] = f.zoom;
    frame["rot"] = f.rot;
    frame["warp"] = f.warp;
    frame["cx"] = f.cx;
    frame["cy"] = f.cy;
    frame["dx"] = f.dx;
    frame["dy"] = f.dy;
    frame["sx"] = f.sx;
    frame["sy"] = f.sy;
    frame["decay"] = f.decay;
    frame["wave_a"] = f.waveA;
    out["frame"] = frame;

    turret::json::Value image = turret::json::Value::MakeObject();
    image["mean_luma"] = w.meanLuma;
    image["lit_fraction"] = w.litFraction;
    image["brightness"] = BRIGHTNESS_NAMES[w.brightness];
    out["image"] = image;

    turret::json::Value entities = turret::json::Value::MakeArray();
    const turret::Vec2 centre = uvToPixels(f.cx, f.cy);
    entities.push_back(turret::Context::Entity("centre", "warp_centre", centre.x, centre.y));
    entities.push_back(turret::Context::Entity("wave", "wave_origin", f.waveX * WIDTH, f.waveY * HEIGHT));
    out["entities"] = entities;

    const Params& p = w.params;
    turret::AddEnumParameter(out, "motion", "Motion (per-frame equations)", MOTION_NAMES, MOTION_NAMES[p.motion]);
    turret::AddScalarParameter(out, "zoom", "zoom", 0.8, 1.2, p.zoom);
    turret::AddScalarParameter(out, "zoomexp", "zoomexp", 0.25, 4.0, p.zoomExp);
    turret::AddScalarParameter(out, "rot", "rot (radians per frame)", -0.2, 0.2, p.rot);
    turret::AddScalarParameter(out, "warp", "warp", 0.0, 5.0, p.warp);
    turret::AddScalarParameter(out, "warpanimspeed", "warpanimspeed", 0.0, 5.0, p.warpSpeed);
    turret::AddScalarParameter(out, "warpscale", "warpscale", 0.1, 5.0, p.warpScale);
    turret::AddScalarParameter(out, "cx", "cx", 0.0, 1.0, p.cx);
    turret::AddScalarParameter(out, "cy", "cy", 0.0, 1.0, p.cy);
    turret::AddScalarParameter(out, "dx", "dx", -0.05, 0.05, p.dx);
    turret::AddScalarParameter(out, "dy", "dy", -0.05, 0.05, p.dy);
    turret::AddScalarParameter(out, "sx", "sx", 0.8, 1.2, p.sx);
    turret::AddScalarParameter(out, "sy", "sy", 0.8, 1.2, p.sy);
    turret::AddScalarParameter(out, "decay", "decay", 0.8, 1.0, p.decay);
    turret::AddEnumParameter(out, "wave_mode", "wave_mode", WAVE_MODE_NAMES, WAVE_MODE_NAMES[p.waveMode]);
    turret::AddScalarParameter(out, "wave_scale", "wave_scale", 0.0, 3.0, p.waveScale);
    turret::AddScalarParameter(out, "wave_x", "wave_x", 0.0, 1.0, p.waveX);
    turret::AddScalarParameter(out, "wave_y", "wave_y", 0.0, 1.0, p.waveY);
    turret::AddScalarParameter(out, "wave_a", "wave_a", 0.0, 1.0, p.waveAlpha);
    turret::AddColourParameter(out, "wave_colour", "Wave colour", hexColour(p.waveColour));
    turret::AddBoolParameter(out, "wave_thick", "wave_thick", p.waveThick);
    turret::AddBoolParameter(out, "colour_cycle", "Cycle wave colour", p.colourCycle);
    turret::AddBoolParameter(out, "wrap", "wrap", p.wrap);
    turret::AddScalarParameter(out, "gamma_adj", "gamma_adj (display brightness)", 0.5, 4.0, p.gammaAdj);
    turret::AddScalarParameter(out, "bpm", "Synth tempo (bpm)", 60.0, 180.0, p.bpm);

    turret::AddLayer(out, "feedback", "Feedback image");
    turret::AddLayer(out, "warp_field", "Warp flow arrows");
    turret::AddLayer(out, "markers", "Centre and wave markers");
    turret::AddLayer(out, "readout", "Variables and audio meters");
}

turret::ApplyResult setParameters(World& w, const turret::json::Value& values) {
    Params p = w.params;
    for (const auto& [id, value] : values.asObject()) {
        float* scalar = nullptr;
        if (id == "zoom") scalar = &p.zoom;
        else if (id == "zoomexp") scalar = &p.zoomExp;
        else if (id == "rot") scalar = &p.rot;
        else if (id == "warp") scalar = &p.warp;
        else if (id == "warpanimspeed") scalar = &p.warpSpeed;
        else if (id == "warpscale") scalar = &p.warpScale;
        else if (id == "cx") scalar = &p.cx;
        else if (id == "cy") scalar = &p.cy;
        else if (id == "dx") scalar = &p.dx;
        else if (id == "dy") scalar = &p.dy;
        else if (id == "sx") scalar = &p.sx;
        else if (id == "sy") scalar = &p.sy;
        else if (id == "decay") scalar = &p.decay;
        else if (id == "wave_scale") scalar = &p.waveScale;
        else if (id == "wave_x") scalar = &p.waveX;
        else if (id == "wave_y") scalar = &p.waveY;
        else if (id == "wave_a") scalar = &p.waveAlpha;
        else if (id == "gamma_adj") scalar = &p.gammaAdj;
        else if (id == "bpm") scalar = &p.bpm;
        if (scalar != nullptr) {
            *scalar = static_cast<float>(value.asNumber(*scalar));
            continue;
        }
        if (id == "motion" || id == "wave_mode") {
            const auto& names = id == "motion" ? MOTION_NAMES : WAVE_MODE_NAMES;
            const int index = indexOf(names, value.asString(""));
            if (index < 0) return turret::ApplyResult::Refused("bad_value", "'" + value.asString("") + "' is not an option of " + id);
            (id == "motion" ? p.motion : p.waveMode) = index;
        } else if (id == "wave_colour") {
            if (!parseHex(value.asString(""), p.waveColour)) {
                return turret::ApplyResult::Refused("bad_value", "wave_colour takes #rrggbb");
            }
        } else if (id == "wave_thick") {
            p.waveThick = value.asBool(p.waveThick);
        } else if (id == "colour_cycle") {
            p.colourCycle = value.asBool(p.colourCycle);
        } else if (id == "wrap") {
            p.wrap = value.asBool(p.wrap);
        } else {
            return turret::ApplyResult::Refused("unknown_parameter", "no declared parameter with id '" + id + "'");
        }
    }
    p.bpm = std::clamp(p.bpm, 1.0f, 400.0f);
    w.params = p;
    return turret::ApplyResult::Accepted();
}

template <typename T>
void appendBytes(std::vector<uint8_t>& out, const T* data, size_t count) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + sizeof(T) * count);
}

template <typename T>
bool readBytes(const std::vector<uint8_t>& in, size_t& offset, T* data, size_t count) {
    const size_t size = sizeof(T) * count;
    if (offset + size > in.size()) return false;
    std::memcpy(data, in.data() + offset, size);
    offset += size;
    return true;
}

turret::SaveResult saveState(const World& w) {
    std::vector<uint8_t> bytes;
    appendBytes(bytes, &w.params, 1);
    appendBytes(bytes, &w.synth, 1);
    appendBytes(bytes, &w.frame, 1);
    appendBytes(bytes, &w.tick, 1);
    appendBytes(bytes, &w.meanLuma, 1);
    appendBytes(bytes, &w.litFraction, 1);
    appendBytes(bytes, &w.brightness, 1);
    appendBytes(bytes, w.audio.data(), w.audio.size());
    appendBytes(bytes, w.buffer.pixels().data(), w.buffer.pixels().size());
    return turret::SaveResult::Accepted(std::move(bytes));
}

turret::ApplyResult loadState(World& w, const std::vector<uint8_t>& bytes) {
    World loaded;
    size_t offset = 0;
    bool ok = readBytes(bytes, offset, &loaded.params, 1) && readBytes(bytes, offset, &loaded.synth, 1) &&
              readBytes(bytes, offset, &loaded.frame, 1) && readBytes(bytes, offset, &loaded.tick, 1) &&
              readBytes(bytes, offset, &loaded.meanLuma, 1) && readBytes(bytes, offset, &loaded.litFraction, 1) &&
              readBytes(bytes, offset, &loaded.brightness, 1) &&
              readBytes(bytes, offset, loaded.audio.data(), loaded.audio.size()) &&
              readBytes(bytes, offset, loaded.buffer.pixels().data(), loaded.buffer.pixels().size());
    if (!ok || offset != bytes.size()) {
        return turret::ApplyResult::Refused("bad_snapshot", "snapshot size does not match this build");
    }
    w = std::move(loaded);
    return turret::ApplyResult::Accepted();
}

void mixAudio(const World& w, turret::AudioBlock& block) {
    for (int i = 0; i < block.frames; ++i) {
        const float s = w.audio[std::min(i, SAMPLES_PER_TICK - 1)];
        for (int c = 0; c < block.channels; ++c) block.samples[i * block.channels + c] = s;
    }
}

} // namespace

} // namespace Milk

turret::GameCallbacks TurretGame() {
    using namespace Milk;
    auto world = std::make_shared<World>();
    turret::GameCallbacks cb;
    cb.width = WIDTH;
    cb.height = HEIGHT;
    cb.title = "milk-experimental-effects";
    cb.projectType = "effect";
    cb.defaultView = "normal";
    cb.stepSeconds = STEP_SECONDS;
    cb.build = [world](turret::Context&, uint64_t seed) { build(*world, seed); };
    cb.step = [world](turret::Context& ctx, const turret::Input&) { step(*world, ctx); };
    cb.draw = [world](turret::Context& ctx, const turret::Framing&) { draw(*world, ctx); };
    cb.drawSchematic = [world](turret::Context& ctx, const turret::Framing&) { drawSchematic(*world, ctx); };
    cb.describe = [world](turret::Context&, turret::json::Value& out) { describe(*world, out); };
    cb.setParameters = [world](turret::Context&, const turret::json::Value& values) {
        return setParameters(*world, values);
    };
    cb.saveState = [world](turret::Context&) { return saveState(*world); };
    cb.loadState = [world](turret::Context&, const std::vector<uint8_t>& bytes) { return loadState(*world, bytes); };
    cb.mixAudio = [world](turret::Context&, turret::AudioBlock& block) { mixAudio(*world, block); };
    return cb;
}

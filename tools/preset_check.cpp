// Loads every .milk file under a directory, runs each preset's equations for a few seconds of
// synthesised audio, custom waves and shapes included, and reports code that fails to compile and
// output that goes non-finite.

#include "audio_synth.h"
#include "feedback.h"
#include "milk_audio.h"
#include "milk_preset.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace Milk;

namespace {

constexpr int FRAMES = 300;
constexpr int WIDTH = 1280;
constexpr int HEIGHT = 720;

bool finiteFrame(const FrameVars& f) {
    const float values[] = {f.zoom, f.zoomExp, f.rot, f.warp, f.cx, f.cy, f.dx, f.dy, f.sx, f.sy, f.decay,
                            f.waveX, f.waveY, f.waveR, f.waveG, f.waveB, f.waveA, f.waveScale};
    for (float v : values) {
        if (!std::isfinite(v)) return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <preset directory> [-v]\n", argv[0]);
        return 2;
    }
    const bool verbose = argc > 2 && std::string(argv[2]) == "-v";
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[1])) {
        if (entry.is_regular_file() && entry.path().extension() == ".milk") paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());

    int withErrors = 0;
    int nonFinite = 0;
    int withShaders = 0;
    int withPerPixel = 0;
    int withCustom = 0;
    long shapeInstances = 0;
    long wavePoints = 0;
    std::map<std::string, int> errorKinds;
    double slowestMs = 0.0;
    std::string slowest;
    const auto start = std::chrono::steady_clock::now();

    std::vector<float> audio(SAMPLE_RATE / 60);
    std::vector<MeshUv> mesh;
    MilkAudioFrame milkAudio;
    std::vector<CustomShapeInstance> shapes;
    std::vector<CustomWaveOutput> waves;
    for (const auto& path : paths) {
        PresetFile file;
        std::string error;
        if (!loadPresetFile(path.string(), file, error)) {
            std::printf("LOAD %s: %s\n", path.filename().c_str(), error.c_str());
            ++withErrors;
            continue;
        }
        PresetPlayer player(file);
        if (player.hasShaders()) ++withShaders;
        if (player.perVertex()) ++withPerPixel;
        bool custom = false;
        for (int i = 0; i < CUSTOM_COUNT; ++i) {
            custom |= file.waves[i].settings.count("enabled") && file.waves[i].settings.at("enabled") != 0.0;
            custom |= file.shapes[i].settings.count("enabled") && file.shapes[i].settings.at("enabled") != 0.0;
        }
        if (custom) ++withCustom;
        if (!player.errors().empty()) {
            ++withErrors;
            for (const std::string& e : player.errors()) {
                const size_t colon = e.find(": ");
                std::string kind = e.substr(colon + 2);
                const size_t quote = kind.find('\'');
                if (quote != std::string::npos) kind = kind.substr(0, quote);
                const size_t paren = kind.find(" takes");
                if (paren != std::string::npos) kind = kind.substr(0, paren);
                ++errorKinds[kind];
                if (verbose) std::printf("ERR  %s: %s\n", path.filename().c_str(), e.c_str());
            }
        }

        SynthState synth;
        resetSynth(synth, 7, 124.0f);
        bool finite = true;
        const auto t0 = std::chrono::steady_clock::now();
        for (int frame = 0; frame < FRAMES; ++frame) {
            synthesise(synth, 124.0f, static_cast<int>(audio.size()), audio.data());
            const float time = frame / 60.0f;
            const FrameVars f = player.runFrame(synth.levels, time, 60.0f, WIDTH, HEIGHT);
            computeMesh(f, player.perVertex(), synth.levels, time, WIDTH, HEIGHT, mesh);
            if (!finiteFrame(f)) finite = false;
            buildMilkAudioFrame(audio.data(), audio.data(), static_cast<int>(audio.size()), milkAudio);
            player.runCustomShapes(shapes);
            player.runCustomWaves(milkAudio, waves);
            shapeInstances += static_cast<long>(shapes.size());
            for (const CustomShapeInstance& c : shapes) {
                if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.rad) || !std::isfinite(c.ang)) finite = false;
            }
            for (const CustomWaveOutput& w : waves) {
                wavePoints += static_cast<long>(w.points.size());
                for (const CustomWavePoint& pt : w.points) {
                    if (!std::isfinite(pt.x) || !std::isfinite(pt.y)) finite = false;
                }
            }
            for (const MeshUv& uv : mesh) {
                if (!std::isfinite(uv.u) || !std::isfinite(uv.v)) finite = false;
            }
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / FRAMES;
        if (ms > slowestMs) {
            slowestMs = ms;
            slowest = path.filename().string();
        }
        if (!finite) {
            ++nonFinite;
            if (verbose) std::printf("NAN  %s\n", path.filename().c_str());
        }
    }

    const double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::printf("presets: %zu\n", paths.size());
    std::printf("with shaders: %d\n", withShaders);
    std::printf("with per_pixel code: %d\n", withPerPixel);
    std::printf("with custom waves or shapes: %d (%ld shape instances, %ld wave points drawn in total)\n", withCustom,
                shapeInstances, wavePoints);
    std::printf("with code that failed to compile: %d\n", withErrors);
    std::printf("with non-finite output: %d\n", nonFinite);
    std::printf("slowest: %.3f ms per frame (%s)\n", slowestMs, slowest.c_str());
    std::printf("total: %.1f s for %d frames each\n", total, FRAMES);
    for (const auto& [kind, count] : errorKinds) std::printf("  %4d  %s\n", count, kind.c_str());
    return 0;
}

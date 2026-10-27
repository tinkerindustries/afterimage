# milk-experimental-effects

Experiments with MilkDrop and Geiss style feedback visualisers, built as a Turret effect on the SDL3 backend.

## How it works

Each tick the effect:

1. Runs a small synthesiser that produces one tick of audio and derives MilkDrop's `bass`, `mid` and `treb` levels from it.
2. Starts the frame variables from the knob values and applies the selected motion, which plays the part of a preset's per-frame equations.
3. Computes a warp mesh with MilkDrop's zoom, stretch, warp, rotation and translation, and resamples the previous frame through it with bilinear filtering and `decay`.
4. Draws the waveform into the result, which feeds into the next tick.

The knob ids match the MilkDrop preset variable names where one exists (`zoom`, `zoomexp`, `rot`, `warp`, `cx`, `dy`, `decay`, `wave_mode` and so on). `describe` in `src/main.cpp` lists every knob.

## Layout

- `src/main.cpp`: the Turret callbacks, knobs, motions, drawing and snapshots.
- `src/feedback.*`: the warp mesh, the feedback buffer and the waveform drawing.
- `src/audio_synth.*`: the deterministic synthesiser and band levels.
- `harness/`: Turret's harness headers, vendored from the turret-game-setup skill.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The first configure fetches SDL3, SDL3_image and SDL3_ttf and needs network access. Load `build/milk_effect` in a Turret session with `effect_load`.

## Docs

- [docs/how-feedback-visualisers-work.md](docs/how-feedback-visualisers-work.md): how Geiss and MilkDrop produce their effects.
- [docs/online-resources.md](docs/online-resources.md): where to watch them and find preset collections.

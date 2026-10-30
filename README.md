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
- `src/preset.*`: the knobs, the motions and the per-frame equations, shared by both programs.
- `src/milk_expr.*`: a compiler and interpreter for MilkDrop's expression language, following projectm-eval, which Milkwave 4.1 uses.
- `src/milk_blend.*`: the blend patterns for changing between presets.
- `src/milk_preset.*`: reads `.milk` files and runs their `per_frame_init`, `per_frame` and `per_pixel` code.
- `src/milk_shader.*`: translates a preset's HLSL warp and comp shaders to Metal.
- `tools/`: command-line checks over a preset directory (see Presets below).
- `src/audio_synth.*`: the deterministic synthesiser and band levels.
- `mac/`: the standalone Metal app for macOS.
- `harness/`: Turret's harness headers, vendored from the turret-game-setup skill.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The first configure fetches SDL3, SDL3_image, SDL3_ttf, glslang and SPIRV-Cross and needs network access. Load `build/milk_effect` in a Turret session with `effect_load`.

## The Metal app

`build/Milk.app` is a standalone macOS app that runs the same effect on the GPU at up to 120 Hz. It needs macOS 14.2 or later. It listens to everything the Mac is playing through a Core Audio process tap, in stereo, so it follows Spotify or any other player. macOS asks for permission to record system audio on the first launch, and again after each rebuild, because the ad-hoc signature changes. If the tap cannot start, the app falls back to the synthesiser.

The CPU still computes the 64x48-cell warp mesh with `computeMesh`. The GPU resamples the previous frame through it, adds the waveform, and scales the result by `gamma_adj` for the screen. The built-in motions' equations are written for 60 Hz, so the app scales each frame's displacement and decay by the frame's length.

The feedback image is the window's size in points, not Retina pixels, which is what MilkDrop would have rendered in a window of the same physical size on a non-Retina screen. Presets measure offsets, blur widths and line widths in pixels, so this keeps them at the scale their authors saw. The feedback and blur images are 8 bits per channel, as MilkDrop's are, so increments below 1/255 vanish and some presets depend on that. Each frame ends with the image clamped to 0..1 (NaN becomes 0).

Launch it with `open build/Milk.app`. Keys:

- `N` and `P`: next and previous preset
- `Cmd+L`, or Presets > Preset List…: a window listing every preset under `presets/`. Type to filter, then double-click a row or press Return to play it. The playing preset is marked and the list follows it when it changes. Presets > Next Preset and Previous Preset (`Cmd+]` and `Cmd+[`) step through the same list.
- `1` to `5`: built-in motion (still, pulse, sway, storm, ripple)
- `W`: next wave mode
- `C`: colour cycling on or off
- `R`: wrap on or off
- `A`: switch between system audio and the synthesiser
- `L`: lock the preset, which stops automatic changes
- `H`: hard cuts on or off
- `Space`: clear the image
- `F`: full screen, and `Esc` to leave it

## Presets

The app reads `.milk` files from `presets/` and image textures from `presets/textures/`. Git ignores both. The textures come from projectM's [presets-milkdrop-texture-pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack). A texture name with no image gets a generated noise texture.

The app moves to the next preset after 62 s (Milkwave's shipped 60 s between presets plus its 2 s blend). With hard cuts on, it also changes when bass, mid and treb together pass a threshold that doubles at each cut and decays back over about a minute. The lock stops both. A preset's `progress` runs from 0 when it loads to 1 at the next automatic change.

Changing preset blends from the old one to the new over 1 s for a key press or the preset list, and 2 s for an automatic change; a hard cut does not blend. As in MilkDrop, both presets run their equations during a blend. Values that do not move pixels cross-fade, and switches such as `wrap` or `invert` change at a snap point. The warp meshes mix per vertex through a random pattern (a wipe, plasma or radial), and the old preset's warp and comp passes are drawn under the new one's by the same pattern. Shapes and waves of both presets fade, and the main waveform morphs from the old wave mode to the new. `milk_render --blend-from <preset>` renders a blend.

Each preset's base values and equations drive the same mesh warp as the built-in motions. Presets run at 60 Hz, one equation step per frame, as in MilkDrop. Equation lines are joined with nothing between them after each line's `//` comment is removed, as MilkDrop does. MilkDrop's editor broke some names across lines (`is_` then `beat`), and a line that ends without a `;` runs into the next. The block then fails to compile, and MilkDrop drops the whole block, so none of its code runs.

A frame runs in MilkDrop 2's order: motion vectors drawn onto the previous image, the warp (the preset's warp shader if it has one), custom shapes, custom waves, the main waveform, darken-centre, the outer and inner borders, then the composite. The three blur levels are made after the warp from the image the warp read, so the warp shader samples last frame's blur. The comp shader shows that same image and its blur, one frame behind MilkDrop 1's composite, which shows the new image. Custom waves read a 480-sample waveform and a 512-bin spectrum per channel, built by `src/milk_audio.cpp`. Each frame the waveform is shifted by up to 96 samples to line up with the previous frame's, as MilkDrop's `AlignWaves` does, and the samples past 480 are zeroed when it shifts. `src/milk_wave.cpp` draws the main waveform in MilkDrop's eight modes, with its scaling, smoothing and alpha rules. Samples are 8-bit, as MilkDrop's are. Lines are Direct3D 9's: one aliased pixel of the feedback image, drawn as Metal line primitives, with everything moved half a pixel so it lands on Direct3D 9's pixel centres. A thick custom wave or shape border is drawn four times, offset right and down by a pixel; the main wave does the same only on an image 512 or more pixels wide. Custom wave dots are one pixel below a 1024 pixel image and two from there up, plus one when thick. Custom shapes turn clockwise, and colours pack into bytes as MilkDrop's do, so values outside 0..1 wrap.

`bass`, `mid` and `treb` follow MilkDrop 2's sound analysis (`mac/band_levels.cpp`): the spectrum of the latest 576 left-channel samples is summed into bins 0-85, 85-170 and 170-256, and each sum is divided by its slow average. The `_att` values divide the fast average by the same slow average. The equations' `time` and `frame` run from the start of the session, and a shader's `time` counts from the preset's load. The blur follows MilkDrop's: a wide horizontal pass, a short vertical pass, textures a quarter, an eighth and a sixteenth of the image's width, and edge darkening on the first level only. MilkDrop's blur shaders offset their samples for Direct3D 9, which samples at whole pixel coordinates and flips the image on each pass. The Metal shaders start from the point Direct3D 9 samples, so the offsets put each level where MilkDrop puts it. `milk_blur_test` checks the GPU blur against a model of Direct3D 9's.

`src/milk_shader.cpp` wraps each shader in MilkDrop 2's header and entry point, compiles the HLSL to SPIR-V with glslang, and converts that to Metal with SPIRV-Cross. The header maps `sqrt`, `pow` and `log` to Direct3D 9's behaviour, which takes the absolute value of the input. A preset's top-level variables become writable statics, and their initialisers run inside the entry point, after the uniforms have their values. Constants that read no uniform keep their initialisers.

Direct3D 9 accepts some HLSL that glslang does not, so the translator rewrites it first: `float1` becomes `float`, a unary minus in front of a parenthesised comparison becomes `-1.0*`, a flat list of numbers initialising an array of vectors is regrouped per vector, `sampler_state` blocks are dropped, scalar `normalize` returns the sign, one-parameter float functions get vector overloads that read the first component, and variables named after C++ operator words (`or`, `and`, `not`) are renamed. glslang's HLSL front end is deprecated, so the build pins glslang to a release that still has it. The rotation matrices (`rot_s1` and the rest) and the `roam` values approximate MilkDrop's.

The comp shader draws MilkDrop's own 32 by 24 grid, bunched towards the centre with the centre row and column doubled, so its `rad` is 1 at the corners and its `ang` runs clockwise from 0 to 2 pi with fixed values on the centre lines. A preset with a PS version above 0 and no warp or comp code gets the shader MilkDrop writes for it from its decay, gamma, video echo, tint and filter values. The noise textures are made as MilkDrop makes them: random texels packed with the overflow it has, cubic interpolation between lattice points, and a mip chain for the 2D ones.

`src/milk_expr.cpp` follows projectm-eval's grammar: `|` and `&` bind looser than comparisons, `%` tighter than `*`, `^` is left associative and sits below unary minus, so `-2^2` is 4. Assignment binds looser than `?:`, so `c ? x = 1 : x = 2` assigns 2. Anything can be assigned to: a variable, a memory slot, or an `if`, `?:` or sequence that ends in one; any other target takes the value and drops it. Built-in function names cannot be variables, a number needs a digit (a lone `.` is an error), and only `$x` writes hex. Equality and truth tests are exact, `band` and `bor` allow 1e-5, division by less than 1e-300 gives 0, and `asin` and `acos` give 0 outside -1..1. It supports `gmem[i]`, `base[offset]`, `memcpy`, `memset`, `&=`, `|=` and `$'c'`. `rand(x)` returns a fraction in [0, floor(x)) from MilkDrop's Mersenne Twister and its fixed seed, which restarts when a preset loads.

The composite follows the preset's `PSVERSION_COMP` (MilkDrop 1 files count as 0). At 0 the app uses MilkDrop 1's composite: gamma, the drifting corner tint, video echo, then the brighten, darken, solarize and invert filters. Above 0 it runs the comp shader, or copies the image unchanged when the preset has no comp code. A `PSVERSION_WARP` of 0 likewise ignores any warp code.

The window title says when a preset has shader or equation code that failed to compile.

These tools work on presets:

- `build/milk_preset_check presets` runs every preset's equations for 300 frames and reports code that fails to compile and output that goes non-finite.
- `build/milk_shader_check presets` translates every shader, compiles it as Metal and links it against the app's vertex stages, then counts what got through each step.

- `build/milk_render [--audio <file> [--audio-start <seconds>]] [--black-start] [--frames <dir> --root <dir>] <sheet.png> <seconds> <size> <preset.milk>...` renders each preset offscreen on the synthesiser, or on the same stretch of an audio file when `--audio` is given (any format Core Audio reads; `tools/make_sweep.py` writes a 30 s sine sweep for it). `--black-start` begins from a black image instead of noise, and `--frames` also writes each frame as a PNG named after the preset's path under `--root`. It writes a contact sheet with MilkDrop's preview (the `.jpg` beside the `.milk`, where there is one) on the left and this renderer's frame on the right.

- `python3 tools/compare_frames.py <reference dir> <our dir> <out dir>` ranks how far this renderer's frames are from MilkDrop captures of the same presets and writes contact sheets of the biggest differences. `docs/capture-shortlist.txt` lists the presets chosen for capture, and `tools/make_sweep.py` writes the test audio.
- Against MilkDrop captures, set `MILK_MILKWAVE=1` when rendering. The captures on the `milkdrop-captures` branch come from Milkwave 4.1, and the switch follows its changes to MilkDrop 2: its audio analysis (both channels, bands in Hz), pixel shader version 3 or more for MilkDrop 2 presets, its defaults for keys a preset leaves out, its 19 blend patterns, and its additions for presets written for it: `bass_smooth`, `mid_smooth`, `treb_smooth` and the mouse variables in equations, and in shaders `_c14` to `_c17`, `sampler_fft` with `get_fft`, and `shiftHSV`. `MILK_BANDS=1` switches on the audio analysis alone. Compare a capture folder with a second run of the same presets to find how repeatable MilkDrop is, and read our scores relative to that.
`ctest --test-dir build` runs `milk_expr_test` (expression semantics), `milk_shader_test` (the HLSL rewrites), `milk_preset_list_test` (the preset list) and `milk_blur_test` (the blur against Direct3D 9's sampling).

Add `-v` to either check to list each failing preset. `milk_shader_check <file.milk> --dump` prints one preset's generated Metal, its uniform offsets and its texture bindings, and `--hlsl` prints the HLSL that goes to glslang.

## Docs

- [docs/how-feedback-visualisers-work.md](docs/how-feedback-visualisers-work.md): how Geiss and MilkDrop produce their effects.
- [docs/online-resources.md](docs/online-resources.md): where to watch them and find preset collections.
- [docs/remaining-presets.md](docs/remaining-presets.md): presets that still fail or render badly, from one scan, with the commands that regenerate it.

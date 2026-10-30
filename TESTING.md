# Testing

There is no CI. Everything here runs locally, and the Metal tests need a Mac with a GPU.

## Test layers

| Layer | Purpose here | Touches | Runner | Lives in |
|---|---|---|---|---|
| Unit | Expression semantics, HLSL rewrites | Nothing external | `ctest` | `tools/expr_test.cpp`, `tools/shader_test.cpp` |
| GPU | The blur against a model of Direct3D 9's sampling | Metal | `ctest` | `tools/blur_test.mm` |
| App component | The preset list and gallery | AppKit, Metal | `ctest` | `tools/preset_list_test.mm` |
| Preset sweep | Every preset's equations and shaders still work | A presets folder | `milk_preset_check`, `milk_shader_check` | `tools/preset_check.cpp`, `tools/shader_check.mm` |
| Visual | Frames against MilkDrop's, by eye and by score | A presets folder, reference captures | `milk_render`, `compare_frames.py` | `tools/milk_render.mm`, `tools/compare_frames.py` |

The unit, GPU and app tests are registered with CTest. The sweep and visual tools are run by hand.

## What to test where

### `src/milk_expr.*`
- **Write:** a case in `expr_test.cpp` for every precedence, assignment or built-in rule you change. `ARCHITECTURE.md` lists the rules the tests pin down.
- **Run:** `ctest --test-dir build -R expressions`

### `src/milk_shader.*`
- **Write:** a case in `shader_test.cpp` for every HLSL rewrite you add. A case translates a small shader that glslang rejects as written and checks it reaches Metal.
- **Don't bother:** unit tests for a single preset's shader. Use `milk_shader_check` on that preset.
- **Run:** `ctest --test-dir build -R shader_translation`

### The blur and composite passes
- **Write:** extend `blur_test.mm` when a blur offset or level changes.
- **Don't bother:** pixel tests for the other passes. The comparison against MilkDrop captures covers them.
- **Run:** `ctest --test-dir build -R blur`

### `mac/preset_list.*` and `mac/thumbnails.*`
- **Write:** extend `preset_list_test.mm` for list and filter behaviour.
- **Run:** `ctest --test-dir build -R preset_list`

## Running tests

| What | Command |
|---|---|
| Everything registered | `ctest --test-dir build` |
| One test | `ctest --test-dir build -R <name>` |
| Verbose output | `ctest --test-dir build -R <name> --output-on-failure` |

The registered names are `expressions`, `shader_translation`, `preset_list` and `blur`. The last two exist only on macOS.

### Checks over a presets folder

These need a folder of `.milk` presets. `presets/` in the checkout is git-ignored, so supply your own.

| What | Command |
|---|---|
| Equations compile and stay finite over 300 frames | `build/milk_preset_check <presets dir>` |
| Shaders translate, compile as Metal and link | `build/milk_shader_check <presets dir>` |
| List each failing preset | add `-v` to either check |
| One preset's generated Metal, uniform offsets and texture bindings | `build/milk_shader_check <file.milk> --dump` |
| The HLSL handed to glslang | `build/milk_shader_check <file.milk> --hlsl` |

### Rendering and comparing

`build/milk_render [--audio <file> [--audio-start <seconds>]] [--black-start] [--frames <dir> --root <dir>] [--blend-from <preset>] <sheet.png> <seconds> <size> <preset.milk>...` renders each preset offscreen. It uses the synthesiser, or the same stretch of an audio file when `--audio` is given (any format Core Audio reads). `--black-start` begins from a black image instead of noise. `--frames` also writes each frame as a PNG named after the preset's path under `--root`. `--blend-from` renders a blend from another preset. The output is a contact sheet with MilkDrop's preview (the `.jpg` beside the `.milk`, where there is one) on the left and this renderer's frame on the right.

`tools/make_sweep.py <out.wav>` writes a 30 second sine sweep to use as `--audio` input.

`python3 tools/compare_frames.py <reference dir> <our dir> <out dir>` ranks how far this renderer's frames are from MilkDrop captures of the same presets and writes contact sheets of the biggest differences. The captures come from Milkwave 4.1 and are not in this repository. Presets use random numbers, so compare a capture folder with a second run of the same presets to find how repeatable MilkDrop is, and read scores relative to that. `docs/capture-shortlist.txt` lists the presets chosen for capture.

### Environment switches

- `MILK_MILKWAVE=1` follows Milkwave's changes to MilkDrop 2 and adds its own features. Use it when rendering against Milkwave captures.
- `MILK_BANDS=1` switches on Milkwave's audio analysis alone.

## Conventions

- A test is a small program in `tools/` that exits non-zero on failure and is registered with `add_test` in `CMakeLists.txt`.
- Tests that need the renderer read `shaders.metal` from beside the executable. `CMakeLists.txt` copies it there after the build.

## Smoke test after a change

1. `cmake --build build` finishes without errors.
2. `ctest --test-dir build` passes. It takes seconds.
3. For a change to presets, shaders or expressions, run `build/milk_preset_check` and `build/milk_shader_check` on a presets folder before and after, and compare the failure counts.
4. For a change to rendering or the app, run `open build/Afterimage.app`, play audio, and check that the window title says "system audio" and that the visuals move.

## Known-awkward tests

- `blur` and `preset_list` need a Metal device. They cannot run on a machine without a GPU.
- Comparison scores vary between runs of the same preset, because presets use random numbers.

## Coverage

No enforced threshold.

> **TODO:** there is no CI. Nothing runs the registered tests on a push.

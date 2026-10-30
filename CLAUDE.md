# CLAUDE.md

Afterimage is a macOS music visualiser that runs MilkDrop `.milk` presets on the GPU with Metal. A C++ renderer library in `src/` is shared by the Mac app in `mac/` and by a Turret effect (`milk_effect`) built from `src/main.cpp`.

## Commands

| Task | Command |
|---|---|
| Configure and build | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build` |
| Test | `ctest --test-dir build` |
| One test | `ctest --test-dir build -R <name>` |
| Check a folder of presets | `build/milk_preset_check <dir>` and `build/milk_shader_check <dir>` |
| Run the app | `open build/Afterimage.app` |
| Release | `tools/release.sh` (see `RELEASE.md`) |

The first configure downloads SDL3, SDL3_image, SDL3_ttf, glslang and SPIRV-Cross and needs network access. A Turret session loads `build/milk_effect` with `effect_load`, never `load`.

## Conventions

- Code style follows the conventions in ../gpu-physics-simulator/CLAUDE.md: PascalCase types, camelCase functions, `m_` members, UPPER_SNAKE constants.
- `presets/` is git-ignored. Never commit presets, textures, or images rendered from them: their authors' licences are not ours to grant.
- `harness/` is vendored from Turret. Do not edit it.
- Copying code from another project means adding its licence notice to `THIRD_PARTY_NOTICES.md` in the same change.
- The Mac app's bundle identifier names its saved settings and thumbnail cache (see `mac/CLAUDE.md`).

## Before you finish

`cmake --build build && ctest --test-dir build`. `TESTING.md` lists the checks for preset, shader and rendering changes.

## Writing things down

A problem you are not fixing right now gets a file in `docs/issues/open/`, copied from `docs/issues/TEMPLATE.md`. Name it after the problem, not the fix. Closing it means moving it to `docs/issues/closed/` under the same name and filling in its resolution.

Work larger than a single session gets a plan. Read `docs/plans/README.md` first.

## Further reading

- `README.md`: what the app does and how to install it
- `ARCHITECTURE.md`: components, the frame pipeline and the invariants
- `TESTING.md`: the tests and checking tools
- `RELEASE.md`: how a release is cut
- `docs/README.md`: what each docs folder holds

# CLAUDE.md

See README.md for what this project is, its layout and the build commands.

- The binary is a Turret effect (`projectType = "effect"`). Load it with `effect_load`, never `load`.
- Include `turret_game_sdl.h`, never `turret_game.h` directly.
- `step` owns all state changes, including the feedback buffer. The draw callbacks only read the world.
- Time comes from the tick and audio comes from the synthesiser. Nothing reads the wall clock or a real audio device.
- `saveState` and `loadState` copy `Params`, `SynthState` and `FrameVars` as raw bytes, so those structs stay plain data.
- Code style follows the conventions in ../gpu-physics-simulator/CLAUDE.md: PascalCase types, camelCase functions, `m_` members, UPPER_SNAKE constants.

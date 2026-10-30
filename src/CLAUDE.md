# src/

The renderer library shared by the Mac app and the Turret effect, plus the effect itself (`main.cpp`, `preset.*`, `feedback.*`, `audio_synth.*`).

Rules for the Turret effect:

- Include `turret_game_sdl.h`, never `turret_game.h` directly.
- `step` owns all state changes, including the feedback buffer. The draw callbacks only read the world.
- Time comes from the tick and audio comes from the synthesiser. Nothing reads the wall clock or a real audio device.
- `saveState` and `loadState` copy `Params`, `SynthState` and `FrameVars` as raw bytes, so those structs stay plain data.

The `milk_*` files take no platform dependencies. Metal and Cocoa code goes in `mac/`.

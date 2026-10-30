# mac/

The Afterimage app: Objective-C++ compiled with ARC, built as the `MilkMetal` target that outputs `Afterimage.app`.

- `shaders.metal` is read from the main bundle at run time. A bare test executable finds it only because `CMakeLists.txt` copies it beside the executable.
- The renderer reads the presets folder once, when it is created. A change in Settings applies after a relaunch, which Settings offers.
- The audio device applies immediately: Settings calls `restartSystemAudio` on the renderer.
- The bundle identifier is the user-defaults domain and the name of the thumbnail cache folder. Changing it orphans the saved settings and the cache.
- Nothing deletes a thumbnail automatically. A thumbnail's file name hashes the preset path, its modification time and `VERSION` in `thumbnails.mm`. Raise `VERSION` when a rendering change should redraw them all.
- Each rebuild changes the ad-hoc signature, so macOS asks for system-audio permission again.
- The app is arm64 only. The deployment target is set in `CMakeLists.txt` and must match `LSMinimumSystemVersion` in `Info.plist.in`.

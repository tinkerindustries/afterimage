# Afterimage

Afterimage is a music visualiser for macOS. It listens to whatever the Mac is playing and draws feedback visuals that follow the music, in the style of Winamp's MilkDrop and Geiss. Each frame is the previous frame warped, faded and drawn over with a waveform, which gives the swirling, trailing look.

It reads standard MilkDrop `.milk` presets and runs their equations and HLSL shaders on the GPU with Metal. It also has a few built-in motions, so it draws something before you add any presets.

## What it does

- Listens to the system audio, or to one output device you choose. It plays no sound itself.
- Plays MilkDrop 2 presets, including per-frame and per-vertex equations, custom waves and shapes, warp and composite shaders, and textures.
- Blends from one preset to the next, and changes preset by itself every minute or so. It can also cut early on a loud beat.
- Shows every preset in a filterable list or a gallery of thumbnails, rendered by Afterimage.
- Runs at up to 120 Hz and goes full screen with a double-click.

## Requirements

- A Mac with Apple Silicon.
- macOS 14.2 or later.

## Install

1. Download `Afterimage-<version>.dmg` from the [latest release](https://github.com/tinkerindustries/afterimage/releases/latest).
2. Open the DMG and drag Afterimage to Applications.
3. Open Afterimage. macOS asks for permission to record system audio. Allow it. Without it, the app uses a built-in synthesiser instead of your music.
4. Add presets (below).

## Add presets

Afterimage ships with no presets. Presets are `.milk` files written by many different authors, each with their own licence terms.

1. Open Settings (`Cmd+,`) and choose **Open in Finder** to see the presets folder, or **Choose…** to pick a folder you already have. The default is `~/Library/Application Support/Afterimage/presets`.
2. Put your `.milk` files in that folder. Subfolders work.
3. Put image textures in a folder named `textures` inside it. [projectM's texture pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack) has the ones most presets use. A preset that names a missing texture gets generated noise instead.
4. Relaunch Afterimage. Settings offers to do this when you change the folder.

[docs/online-resources.md](docs/online-resources.md) lists where to find preset collections.

## Use it

Play music, then change presets with the keys below or from the preset list (`Cmd+L`). Type in the list to filter it, then double-click a preset or press Return to play it. The gallery button shows thumbnails. They render in the background the first time you open it and are cached.

| Key | Action |
|---|---|
| `N` / `P` | Next and previous preset |
| `Cmd+L` | Preset list and gallery |
| `Cmd+,` | Settings |
| `F`, or double-click | Full screen. `Esc` leaves it |
| `L` | Lock the preset, which stops automatic changes |
| `H` | Hard cuts on or off |
| `A` | Switch between system audio and the synthesiser |
| `W` | Next wave mode |
| `C` | Colour cycling on or off |
| `R` | Wrap on or off |
| `1` to `5` | Built-in motion: still, pulse, sway, storm, ripple |
| `Space` | Clear the image |

Settings holds the audio device, the presets folder, and buttons to clear the thumbnail cache or open its folder.

## Docs

- [ARCHITECTURE.md](ARCHITECTURE.md): how the app and the renderer are built, and the rules between the parts.
- [TESTING.md](TESTING.md): the tests and the tools for checking presets and rendering.
- [RELEASE.md](RELEASE.md): how a signed, notarized release is made.
- [docs/how-feedback-visualisers-work.md](docs/how-feedback-visualisers-work.md): how Geiss and MilkDrop produce their effects.
- [docs/online-resources.md](docs/online-resources.md): where to watch them and find preset collections.
- [docs/remaining-presets.md](docs/remaining-presets.md): presets that still fail or render badly, from one scan, with the commands that regenerate it.

## Licence

The code and documents are MIT licensed: see [LICENSE](LICENSE). [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) credits the projects this follows and the libraries it builds on. The licence does not cover MilkDrop presets or textures.

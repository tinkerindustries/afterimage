# Third-party notices

This project is released under the MIT licence in [LICENSE](LICENSE). It follows the behaviour of
the projects below and uses the libraries below. The licence of each project applies to that
project's own code and materials.

## Projects this renderer follows

### MilkDrop and Geiss

MilkDrop was written by Ryan Geiss (geisswerks.com). MilkDrop 2 is distributed with the Winamp
plugin source under the Nullsoft licence below. The warp mesh, blend patterns, preset format and
default values follow MilkDrop's source and behaviour.

```
Copyright 2005-2013 Nullsoft, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  * Neither the name of Nullsoft nor the names of its contributors may be used to
    endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

### Milkwave

[Milkwave](https://github.com/IkeC/Milkwave) by IkeC is a MilkDrop 2 visualiser forked from
BeatDrop. Its Visualizer is licensed under the BSD 3-Clause licence. This renderer follows its
changes to MilkDrop 2 behind `MILK_MILKWAVE`, and its captures are the reference for the
comparisons in this repository. Milkwave Remote is licensed separately under CC BY-NC 4.0, and
nothing from it is used here.

### projectm-eval

[projectm-eval](https://github.com/projectM-visualizer/projectm-eval) is the expression evaluator
that Milkwave 4.1 uses. `src/milk_expr.cpp` follows its grammar and semantics.

```
The MIT License (MIT)

Copyright (c) 2023 The projectM Team

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

### Other references

- [projectM](https://github.com/projectM-visualizer/projectm) (LGPL-2.1) is the reference for how
  MilkDrop presets behave.
- [Butterchurn](https://github.com/jberg/butterchurn) (MIT) is a WebGL port of MilkDrop used as a
  reference.
- [projectM's texture pack](https://github.com/projectM-visualizer/presets-milkdrop-texture-pack)
  supplies the textures that `presets/textures/` expects. It is not included in this repository.

## Libraries fetched at build time

CMake downloads these. None of their code is stored in this repository. A binary built from this
repository includes them, so a distributed binary must carry their licence texts, which are in
each library's source tree under `build/_deps/<name>-src`.

| Library | Licence |
|---|---|
| [SDL3](https://github.com/libsdl-org/SDL) | zlib |
| [SDL3_image](https://github.com/libsdl-org/SDL_image) | zlib, with the licences of the image libraries it bundles |
| [SDL3_ttf](https://github.com/libsdl-org/SDL_ttf) | zlib, with the licences of FreeType and HarfBuzz |
| [glslang](https://github.com/KhronosGroup/glslang) | BSD-3-Clause, BSD-2-Clause, Apache-2.0 and MIT; see its `LICENSE.txt` |
| [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Apache-2.0 |

## Presets, textures and captures

The MIT licence covers this repository's code and documents. It does not cover MilkDrop presets,
textures or the images rendered from them. Each preset has its own author and licence terms, or
none. `presets/` is not tracked, and the `milkdrop-captures` branch holds images rendered from
other people's presets.

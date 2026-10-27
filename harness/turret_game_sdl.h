// Turret's game harness — the SDL3 backend.
//
// A game includes this header from exactly one .cpp file and defines
// `turret::GameCallbacks TurretGame()`. It supplies the geometry types and the drawing
// primitives `turret_game.h` declares, then includes it; that half owns the control socket, the
// frame format, the JSON, `Context`, the command loop and `main`, and knows nothing about SDL.
//
// The renderer is built on a GPU device this backend creates, rather than letting
// `SDL_CreateRenderer` pick a backend of its own, so a game running pipelines of its own and the
// harness's 2D kit draw through one device and can share a frame. `backend::GpuDevice()` hands
// it out.
//
// SDL's renderer installs no camera transform, so `BeginGameCamera` does nothing here: a game
// drawing its own sprites puts world coordinates through `Context::ToScreen` first.
#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#define TURRET_BACKEND "sdl"

namespace turret {

// ---------------------------------------------------------------------------------------------
// Geometry and colour. These are the harness's own types rather than SDL's, so a game's
// callbacks keep the same signatures whichever renderer sits underneath. They convert to the
// SDL equivalents in one place, at the bottom of this file.
// ---------------------------------------------------------------------------------------------

struct Vec2 {
  float x = 0.0f, y = 0.0f;
};

struct Rect {
  float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

struct Color {
  uint8_t r = 0, g = 0, b = 0, a = 255;
};

inline constexpr Color kBackground{245, 245, 245, 255};
inline constexpr Color kBlack{0, 0, 0, 255};
inline constexpr Color kWhite{255, 255, 255, 255};
inline constexpr Color kRed{230, 41, 55, 255};
inline constexpr Color kGreen{0, 228, 48, 255};
inline constexpr Color kBlue{0, 121, 241, 255};
inline constexpr Color kYellow{253, 249, 0, 255};
inline constexpr Color kOrange{255, 161, 0, 255};
inline constexpr Color kPurple{200, 122, 255, 255};
inline constexpr Color kGrey{130, 130, 130, 255};
inline constexpr Color kDarkGrey{80, 80, 80, 255};

inline Color WithAlpha(Color c, float alpha) {
  c.a = static_cast<uint8_t>(std::max(0.0f, std::min(1.0f, alpha)) * 255.0f);
  return c;
}

}  // namespace turret

#include "turret_game.h"

namespace turret {
namespace backend {

// ---------------------------------------------------------------------------------------------
// What the harness holds of SDL: the window, the renderer and the device it was built on. Every
// call into SDL the kit makes goes through this file, so the one place to look when porting to
// another renderer is this namespace.
// ---------------------------------------------------------------------------------------------

inline SDL_Window*& WindowPtr() {
  static SDL_Window* window = nullptr;
  return window;
}

inline SDL_Renderer*& RendererPtr() {
  static SDL_Renderer* renderer = nullptr;
  return renderer;
}

/** The GPU device the renderer draws through. The harness creates it up front rather than
 *  letting SDL pick a backend of its own behind SDL_CreateRenderer, so a game's own pipelines
 *  and the kit's 2D calls share one device and can share a frame. */
inline SDL_GPUDevice*& GpuDevicePtr() {
  static SDL_GPUDevice* device = nullptr;
  return device;
}

/** The renderer the current draw pass is targeting, for a game drawing its own sprites. Draw in
 *  output-pixel space and put world coordinates through `Context::ToScreen` first. */
inline SDL_Renderer* Renderer() { return RendererPtr(); }
/** The GPU device that renderer was built on, for a game running pipelines of its own. The
 *  harness creates it and destroys it; a game draws through it and owns none of it. */
inline SDL_GPUDevice* GpuDevice() { return GpuDevicePtr(); }

/** Creates the device and the renderer on it, and stores both. Every shader format is asked for
 *  so SDL picks whichever backend this machine has. Returns false with SDL_GetError() set and
 *  nothing stored if either step fails. A binary supplying its own `main` under
 *  TURRET_GAME_NO_MAIN calls this rather than SDL_CreateRenderer, so a game reads the same
 *  device whichever entry point opened the window. */
inline bool OpenRenderer(SDL_Window* window) {
  SDL_GPUDevice* device = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL |
          SDL_GPU_SHADERFORMAT_METALLIB,
      false, nullptr);
  if (device == nullptr) return false;
  SDL_Renderer* renderer = SDL_CreateGPURenderer(device, window);
  if (renderer == nullptr) {
    SDL_DestroyGPUDevice(device);
    return false;
  }
  RendererPtr() = renderer;
  GpuDevicePtr() = device;
  return true;
}

/** Undoes OpenRenderer, renderer first: it holds resources on the device, and SDL never destroys
 *  a device it was handed rather than one it made. The window is the caller's own. */
inline void CloseRenderer() {
  if (RendererPtr() != nullptr) SDL_DestroyRenderer(RendererPtr());
  if (GpuDevicePtr() != nullptr) SDL_DestroyGPUDevice(GpuDevicePtr());
  RendererPtr() = nullptr;
  GpuDevicePtr() = nullptr;
}

inline SDL_FColor ToFColor(Color c) {
  return SDL_FColor{c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

inline SDL_FRect ToFRect(Rect r) { return SDL_FRect{r.x, r.y, r.w, r.h}; }

inline void SetDrawColor(Color c) {
  SDL_SetRenderDrawColor(RendererPtr(), c.r, c.g, c.b, c.a);
}

inline void FillRect(Rect r, Color c) {
  SetDrawColor(c);
  SDL_FRect fr = ToFRect(r);
  SDL_RenderFillRect(RendererPtr(), &fr);
}

/** A filled convex triangle. SDL has no primitive for one, so it goes through the geometry
 *  path, which is also what a thick line uses. */
inline void FillTriangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
  SDL_FColor fc = ToFColor(color);
  SDL_Vertex verts[3];
  verts[0].position = SDL_FPoint{a.x, a.y};
  verts[1].position = SDL_FPoint{b.x, b.y};
  verts[2].position = SDL_FPoint{c.x, c.y};
  for (int i = 0; i < 3; ++i) {
    verts[i].color = fc;
    verts[i].tex_coord = SDL_FPoint{0.0f, 0.0f};
  }
  SDL_RenderGeometry(RendererPtr(), nullptr, verts, 3, nullptr, 0);
}

/** A line with width, drawn as a quad. SDL_RenderLine is one pixel wide whatever the scale, and
 *  a schematic's whole point is that its strokes read at a glance. */
inline void ThickLine(Vec2 from, Vec2 to, Color color, float thickness) {
  float dx = to.x - from.x;
  float dy = to.y - from.y;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len <= 0.0001f) return;
  float half = std::max(thickness, 1.0f) * 0.5f;
  float nx = -dy / len * half;
  float ny = dx / len * half;
  Vec2 p0{from.x + nx, from.y + ny};
  Vec2 p1{to.x + nx, to.y + ny};
  Vec2 p2{to.x - nx, to.y - ny};
  Vec2 p3{from.x - nx, from.y - ny};
  FillTriangle(p0, p1, p2, color);
  FillTriangle(p0, p2, p3, color);
}

/** A ring, drawn as a fan of quads. SDL has no circle primitive of either kind, and an outlined
 *  disc is what shows two overlapping things as two things rather than one blob. */
inline void CircleOutline(Vec2 centre, float radius, Color color, float thickness, int segments) {
  if (radius <= 0.0f || segments < 3) return;
  const float half = std::max(thickness, 1.0f) * 0.5f;
  const float inner = std::max(0.0f, radius - half);
  const float outer = radius + half;
  float previousCos = 1.0f;
  float previousSin = 0.0f;
  for (int i = 1; i <= segments; ++i) {
    const float angle = static_cast<float>(i) / segments * 6.28318530718f;
    const float nextCos = std::cos(angle);
    const float nextSin = std::sin(angle);
    const Vec2 a{centre.x + previousCos * inner, centre.y + previousSin * inner};
    const Vec2 b{centre.x + previousCos * outer, centre.y + previousSin * outer};
    const Vec2 c{centre.x + nextCos * outer, centre.y + nextSin * outer};
    const Vec2 d{centre.x + nextCos * inner, centre.y + nextSin * inner};
    FillTriangle(a, b, c, color);
    FillTriangle(a, c, d, color);
    previousCos = nextCos;
    previousSin = nextSin;
  }
}

/** Four filled bars rather than a stroked path, so the corners meet cleanly at any thickness. */
inline void RectOutline(Rect r, Color color, float thickness) {
  float t = std::max(thickness, 1.0f);
  FillRect(Rect{r.x, r.y, r.w, t}, color);
  FillRect(Rect{r.x, r.y + r.h - t, r.w, t}, color);
  FillRect(Rect{r.x, r.y + t, t, r.h - t * 2.0f}, color);
  FillRect(Rect{r.x + r.w - t, r.y + t, t, r.h - t * 2.0f}, color);
}

/** The face every label is drawn in. SDL_ttf ships no built-in font, so the kit opens a face the
 *  desktop already has, tried in order. A machine carrying none of them gets no label text —
 *  outlines and arrows still draw, and `LabelFont` says so once on stderr rather than per call.
 *  Opened once and resized per draw with TTF_SetFontSize, which is what keeps a label smooth at
 *  any size instead of scaling one bitmap. */
inline TTF_Font* LabelFont(float pointSize) {
  static bool warned = false;
  static TTF_Font* font = [] () -> TTF_Font* {
    static const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Verdana.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (const char* path : candidates) {
      TTF_Font* loaded = TTF_OpenFont(path, 32.0f);
      if (loaded != nullptr) return loaded;
    }
    return nullptr;
  }();
  if (font == nullptr) {
    if (!warned) {
      warned = true;
      std::cerr << "turret: no system font found; schematic labels will not be drawn" << std::endl;
    }
    return nullptr;
  }
  TTF_SetFontSize(font, pointSize);
  return font;
}

inline Vec2 MeasureText(const std::string& text, float pointSize) {
  TTF_Font* font = LabelFont(pointSize);
  if (font == nullptr || text.empty()) return Vec2{0.0f, 0.0f};
  int w = 0, h = 0;
  if (!TTF_GetStringSize(font, text.c_str(), text.size(), &w, &h)) return Vec2{0.0f, 0.0f};
  return Vec2{static_cast<float>(w), static_cast<float>(h)};
}

/** Rasterises `text` and blits it with its top-left at `pos`, in output-pixel space. The glyph
 *  texture is built and thrown away per call: a schematic draws tens of labels, not thousands,
 *  and a cache keyed by string, size and colour would outlive the frame it was correct for. */
inline void DrawText(Vec2 pos, const std::string& text, Color color, float pointSize) {
  TTF_Font* font = LabelFont(pointSize);
  if (font == nullptr || text.empty()) return;
  SDL_Color fg{color.r, color.g, color.b, color.a};
  SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.size(), fg);
  if (surface == nullptr) return;
  SDL_Texture* texture = SDL_CreateTextureFromSurface(RendererPtr(), surface);
  if (texture != nullptr) {
    SDL_FRect dst{pos.x, pos.y, static_cast<float>(surface->w), static_cast<float>(surface->h)};
    SDL_RenderTexture(RendererPtr(), texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
  }
  SDL_DestroySurface(surface);
}


// -- The window, the frames and the game's own transform. -----------------------------------

inline bool DisplayAvailable() {
  // SDL answers for itself: `SDL_Init(SDL_INIT_VIDEO)` and `SDL_CreateWindow` both return a
  // failure and set `SDL_GetError()` when there is no display to open one on, and `OpenWindow`
  // below checks each of them. Nothing is gained by initialising the video subsystem early to
  // ask the same question twice — see `backend::DisplayAvailable`'s declaration for why the
  // raylib backend cannot take this answer.
  return true;
}

inline bool OpenWindow(const std::string& title, int width, int height, bool hidden) {
  // A hidden window is not a hidden app on macOS: SDL makes the process a regular UI
  // application during video init, which puts a Dock icon up for a window nobody can see.
  // The hint is read by SDL_Init and ignored on every other platform.
  if (hidden) SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
  // SDL turns SIGTERM and SIGINT into a quit event the game reads between steps. A game stuck
  // inside a step never reads it, so Turret's SIGTERM would not end it. Read by SDL_Init.
  SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
    return false;
  }
  if (!TTF_Init()) {
    std::cerr << "TTF_Init failed: " << SDL_GetError() << std::endl;
    return false;
  }
  SDL_WindowFlags flags = SDL_WINDOW_BORDERLESS;
  if (hidden) flags |= SDL_WINDOW_HIDDEN;
  SDL_Window* window = SDL_CreateWindow(title.c_str(), width, height, flags);
  if (window == nullptr) {
    std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
    return false;
  }
  if (!OpenRenderer(window)) {
    std::cerr << "could not open a GPU renderer: " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    return false;
  }
  WindowPtr() = window;
  return true;
}

inline void CloseWindow() {
  CloseRenderer();
  if (WindowPtr() != nullptr) SDL_DestroyWindow(WindowPtr());
  WindowPtr() = nullptr;
  TTF_Quit();
  SDL_Quit();
}

inline void WindowSize(int& width, int& height) {
  width = 0;
  height = 0;
  if (WindowPtr() != nullptr) SDL_GetWindowSize(WindowPtr(), &width, &height);
}

inline void SetWindowSize(int width, int height) {
  if (WindowPtr() != nullptr) SDL_SetWindowSize(WindowPtr(), width, height);
}

inline void SetWindowPosition(int x, int y) {
  if (WindowPtr() != nullptr) SDL_SetWindowPosition(WindowPtr(), x, y);
}

inline void WindowPosition(int& x, int& y) {
  x = 0;
  y = 0;
  if (WindowPtr() != nullptr) SDL_GetWindowPosition(WindowPtr(), &x, &y);
}

inline void WorkAreaContaining(int x, int y, int& boundsX, int& boundsY, int& boundsWidth,
                               int& boundsHeight) {
  SDL_Rect bounds{0, 0, 1920, 1080};
  SDL_DisplayID display = WindowPtr() != nullptr ? SDL_GetDisplayForWindow(WindowPtr()) : 0;
  int count = 0;
  SDL_DisplayID* displays = SDL_GetDisplays(&count);
  if (displays != nullptr) {
    if (display == 0 && count > 0) display = displays[0];
    for (int i = 0; i < count; ++i) {
      SDL_Rect candidate{};
      if (!SDL_GetDisplayBounds(displays[i], &candidate)) continue;
      if (x >= candidate.x && x < candidate.x + candidate.w && y >= candidate.y &&
          y < candidate.y + candidate.h) {
        display = displays[i];
        break;
      }
    }
    SDL_free(displays);
  }
  if (display != 0 && !SDL_GetDisplayUsableBounds(display, &bounds)) SDL_GetDisplayBounds(display, &bounds);
  boundsX = bounds.x;
  boundsY = bounds.y;
  boundsWidth = bounds.w;
  boundsHeight = bounds.h;
}

inline bool& CloseRequestedRef() {
  static bool requested = false;
  return requested;
}

/** Real keyboard and mouse events reach this window and are drained right here. During live play
 *  the game's `liveInput` reads the keyboard state SDL keeps, and every key and button that went
 *  down is latched for `LivePressed()`, by scancode and by `SDL_BUTTON_*` number. Draining the
 *  queue is what stops the OS marking the window unresponsive. A request to close the window is
 *  kept for `WindowCloseRequested`. */
inline void PumpEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) CloseRequestedRef() = true;
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) detail::LatchLiveKey(static_cast<int>(event.key.scancode));
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) detail::LatchLiveButton(static_cast<int>(event.button.button));
  }
}

inline void ShowWindowForPlay(bool shown) {
  SDL_Window* window = WindowPtr();
  if (window == nullptr) return;
  if (shown) {
    CloseRequestedRef() = false;
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
  } else {
    SDL_HideWindow(window);
  }
}

inline bool WindowCloseRequested() {
  const bool requested = CloseRequestedRef();
  CloseRequestedRef() = false;
  return requested;
}

inline void BeginWindowFrame() {
  SDL_Renderer* renderer = RendererPtr();
  SDL_SetRenderTarget(renderer, nullptr);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SetDrawColor(kBackground);
  SDL_RenderClear(renderer);
}

inline void EndWindowFrame() { SDL_RenderPresent(RendererPtr()); }

/** The off-screen target, kept between captures and rebuilt when the size asked for changes. */
inline SDL_Texture*& OffscreenPtr() {
  static SDL_Texture* target = nullptr;
  return target;
}

inline bool BeginOffscreenFrame(int width, int height) {
  SDL_Renderer* renderer = RendererPtr();
  if (renderer == nullptr) return false;
  static int targetWidth = -1;
  static int targetHeight = -1;
  if (targetWidth != width || targetHeight != height) {
    if (OffscreenPtr() != nullptr) SDL_DestroyTexture(OffscreenPtr());
    OffscreenPtr() = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                                       width, height);
    targetWidth = width;
    targetHeight = height;
  }
  if (OffscreenPtr() == nullptr) return false;
  SDL_SetRenderTarget(renderer, OffscreenPtr());
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SetDrawColor(kBackground);
  SDL_RenderClear(renderer);
  return true;
}

/** Whether every pixel of `surface` is the frame's own clear colour — the honest way to say a
 *  schematic held nothing, cheaper and simpler than tracking geometry: if the game's draw call
 *  painted nothing beyond the background, there was nothing there to paint. A game that always
 *  draws chrome (a grid, a border) will never read as empty by this test, which is a real limit
 *  of a pixel-only check. */
inline bool SurfaceIsBlank(SDL_Surface* surface, Color background) {
  if (surface == nullptr) return false;
  SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  if (rgba == nullptr) return false;
  bool blank = true;
  for (int y = 0; y < rgba->h && blank; ++y) {
    const uint8_t* row = static_cast<const uint8_t*>(rgba->pixels) + static_cast<size_t>(y) * rgba->pitch;
    for (int x = 0; x < rgba->w; ++x) {
      const uint8_t* px = row + static_cast<size_t>(x) * 4;
      if (px[0] != background.r || px[1] != background.g || px[2] != background.b || px[3] != background.a) {
        blank = false;
        break;
      }
    }
  }
  SDL_DestroySurface(rgba);
  return blank;
}

/** Encodes `surface` as PNG into memory. SDL_image writes to an `SDL_IOStream`, and a dynamic
 *  one grows a buffer the harness then copies out of, so nothing touches the filesystem. */
inline bool EncodePng(SDL_Surface* surface, std::vector<unsigned char>& out) {
  SDL_IOStream* stream = SDL_IOFromDynamicMem();
  if (stream == nullptr) return false;
  bool ok = IMG_SavePNG_IO(surface, stream, false);
  if (ok) {
    Sint64 size = SDL_TellIO(stream);
    void* data = SDL_GetPointerProperty(SDL_GetIOProperties(stream),
                                        SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr);
    if (data != nullptr && size > 0) {
      const unsigned char* bytes = static_cast<const unsigned char*>(data);
      out.assign(bytes, bytes + size);
    } else {
      ok = false;
    }
  }
  SDL_CloseIO(stream);
  return ok;
}

/** Copies `surface` out as 8-bit RGBA, top row first, with no padding between rows. */
inline bool CopyRgba(SDL_Surface* surface, std::vector<unsigned char>& out) {
  SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
  if (rgba == nullptr) return false;
  const size_t rowBytes = static_cast<size_t>(rgba->w) * 4;
  out.resize(rowBytes * rgba->h);
  for (int y = 0; y < rgba->h; ++y) {
    const uint8_t* row = static_cast<const uint8_t*>(rgba->pixels) + static_cast<size_t>(y) * rgba->pitch;
    std::copy(row, row + rowBytes, out.data() + rowBytes * y);
  }
  SDL_DestroySurface(rgba);
  return true;
}

inline bool EndOffscreenFrame(std::vector<unsigned char>& bytes, bool& blank, Color background,
                              int& width, int& height, bool png) {
  SDL_Renderer* renderer = RendererPtr();
  SDL_Surface* surface = SDL_RenderReadPixels(renderer, nullptr);
  SDL_SetRenderTarget(renderer, nullptr);
  if (surface == nullptr) return false;
  blank = SurfaceIsBlank(surface, background);
  width = surface->w;
  height = surface->h;
  bool ok = png ? EncodePng(surface, bytes) : CopyRgba(surface, bytes);
  SDL_DestroySurface(surface);
  return ok;
}

/** SDL's renderer has no camera transform that does not also clip, so the kit converts to
 *  output-pixel space itself and there is nothing to install here. A game drawing its own
 *  sprites puts world coordinates through `Context::ToScreen`. */
inline void BeginGameCamera(const Camera&) {}
inline void EndGameCamera() {}

}  // namespace backend
}  // namespace turret

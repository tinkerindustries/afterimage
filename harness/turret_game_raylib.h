// Turret's game harness — the raylib backend.
//
// A game includes this header from exactly one .cpp file and defines
// `turret::GameCallbacks TurretGame()`. It supplies the geometry types and the drawing
// primitives `turret_game.h` declares, then includes it; that half owns the control socket, the
// frame format, the JSON, `Context`, the command loop and `main`, and knows nothing about
// raylib.
//
// `Vec2` and `Color` are raylib's own `Vector2` and `Color`, so a game already drawing with
// raylib passes what it has to the schematic kit and back. `Rect` is the harness's own, because
// raylib's `Rectangle` names its size `width`/`height` where the rest of the harness says
// `w`/`h`; it converts to and from a `Rectangle` implicitly, so a call site that has one still
// compiles.
//
// raylib installs a camera transform through `BeginMode2D`, and the game's own draw callback
// runs inside one — a game draws in world coordinates with raw raylib calls and the transform
// is the harness's doing. The kit does not want it: `Context` has already converted to
// output-pixel space by the time it calls a primitive here, so each primitive steps outside the
// transform for the duration of its own call.
#pragma once

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#if defined(__APPLE__)
// Included ahead of `namespace turret` so its declarations land in the global namespace, where
// `DisplayAvailable` below expects to find them with `::`.
#include <CoreGraphics/CoreGraphics.h>
#endif

#define TURRET_BACKEND "raylib"

namespace turret {

// ---------------------------------------------------------------------------------------------
// Geometry and colour. `Vec2` and `Color` are raylib's, which is what keeps a game's existing
// draw code and the schematic kit speaking the same types. `Rect` is the harness's own and
// converts both ways.
// ---------------------------------------------------------------------------------------------

using Vec2 = ::Vector2;
using Color = ::Color;

struct Rect {
  float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;

  Rect() = default;
  constexpr Rect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
  Rect(const ::Rectangle& r) : x(r.x), y(r.y), w(r.width), h(r.height) {}
  operator ::Rectangle() const { return ::Rectangle{x, y, w, h}; }
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
// Every call into raylib the harness makes lives here. raylib puts its whole API in the global
// namespace, and several of its names — `DrawText`, `MeasureText`, `CloseWindow`,
// `SetWindowPosition` — are also names in this one, so every call below is written `::` first.
// ---------------------------------------------------------------------------------------------

/** The transform the game's own drawing is running under, and whether one is installed at all.
 *  The kit draws in output-pixel space, so a primitive steps outside this for its own call and
 *  puts it back afterwards. */
inline ::Camera2D& GameCameraRef() {
  static ::Camera2D camera{{0, 0}, {0, 0}, 0.0f, 1.0f};
  return camera;
}

inline bool& GameCameraActive() {
  static bool active = false;
  return active;
}

/** Steps outside the game's camera for the life of the object. raylib's `BeginMode2D` pushes a
 *  matrix and flushes the batch; a schematic draws tens of shapes, so paying that per primitive
 *  costs nothing worth an alternative. */
struct OutsideGameCamera {
  bool restore;
  OutsideGameCamera() : restore(GameCameraActive()) {
    if (restore) ::EndMode2D();
  }
  ~OutsideGameCamera() {
    if (restore) ::BeginMode2D(GameCameraRef());
  }
};

/** The face every label is drawn in. raylib's built-in font is a 10-pixel bitmap that turns to
 *  mush at any size that is not a whole multiple of it, and a schematic is read at a glance, so
 *  the kit loads a real system face once and keeps it for the process's life. Nothing is
 *  vendored: these are the faces a desktop already has, tried in order, and the built-in font is
 *  what a machine carrying none of them falls back to. Loaded at a large size and filtered, so a
 *  label drawn at any size stays smooth. Called on the render thread after the window exists,
 *  which is the only place a texture can be uploaded. */
inline const ::Font& LabelFont() {
  /** The pixel size the label face is rasterised at. Labels are drawn far smaller than this and
   *  scaled down, which is what keeps them smooth rather than chunky. */
  static constexpr int kLabelFontAtlasSize = 64;
  static ::Font font = [] {
    static const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Verdana.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSans.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    for (const char* path : candidates) {
      if (!::FileExists(path)) continue;
      ::Font loaded = ::LoadFontEx(path, kLabelFontAtlasSize, nullptr, 0);
      if (loaded.texture.id != 0 && loaded.glyphCount > 0) {
        ::SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
        return loaded;
      }
    }
    return ::GetFontDefault();
  }();
  return font;
}

/** Letter spacing, as a fraction of the font size. */
inline constexpr float kLabelSpacingRatio = 0.05f;

// -- Primitives, all in output-pixel space. ---------------------------------------------------

inline void FillRect(Rect r, Color c) {
  OutsideGameCamera outside;
  ::DrawRectangleRec(::Rectangle{r.x, r.y, r.w, r.h}, c);
}

inline void RectOutline(Rect r, Color color, float thickness) {
  OutsideGameCamera outside;
  ::DrawRectangleLinesEx(::Rectangle{r.x, r.y, r.w, r.h}, thickness, color);
}

inline void ThickLine(Vec2 from, Vec2 to, Color color, float thickness) {
  OutsideGameCamera outside;
  ::DrawLineEx(from, to, thickness, color);
}

inline void FillTriangle(Vec2 a, Vec2 b, Vec2 c, Color color) {
  OutsideGameCamera outside;
  // raylib culls a triangle wound the wrong way and draws nothing at all. The kit hands them
  // over in either winding — a disc's fan runs one way, an arrow head the other — so the
  // winding is fixed here rather than at each call site. Drawing it twice instead would
  // double-blend anything translucent.
  float cross = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
  if (cross > 0.0f) {
    ::DrawTriangle(a, c, b, color);
  } else {
    ::DrawTriangle(a, b, c, color);
  }
}

inline void CircleOutline(Vec2 centre, float radius, Color color, float thickness, int segments) {
  OutsideGameCamera outside;
  if (radius <= 0.0f || segments < 3) return;
  float previousX = centre.x + radius;
  float previousY = centre.y;
  for (int i = 1; i <= segments; ++i) {
    float angle = static_cast<float>(i) / segments * 6.28318530718f;
    float nextX = centre.x + std::cos(angle) * radius;
    float nextY = centre.y + std::sin(angle) * radius;
    ::DrawLineEx(Vec2{previousX, previousY}, Vec2{nextX, nextY}, thickness, color);
    previousX = nextX;
    previousY = nextY;
  }
}

inline Vec2 MeasureText(const std::string& text, float size) {
  return ::MeasureTextEx(LabelFont(), text.c_str(), size, size * kLabelSpacingRatio);
}

inline void DrawText(Vec2 pos, const std::string& text, Color color, float size) {
  OutsideGameCamera outside;
  ::DrawTextEx(LabelFont(), text.c_str(), pos, size, size * kLabelSpacingRatio, color);
}

// -- The window, the frames and the game's own transform. -------------------------------------

#if defined(__APPLE__)
// A hidden window is not a hidden app on macOS. GLFW's Cocoa init ends with
// `[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular]`, gated on the menu-bar
// init hint alone, so `InitWindow` puts a Dock icon up for a window nobody can see. Turning
// the hint off leaves the process a background app. glfw is linked into raylib on every
// desktop target and its headers are not installed beside raylib's, so the symbol and the
// hint value are declared here instead of included.
extern "C" void glfwInitHint(int hint, int value);
constexpr int kGlfwCocoaMenubar = 0x00051002;
#endif

#if !defined(__APPLE__)
// GLFW is what raylib's desktop platform opens a window through, so GLFW's own monitor list is
// the only answer that agrees with what `InitWindow` is about to do. Win32 is not: it still
// reports a monitor for a desktop session whose display has gone — a disconnected RDP session,
// where `GetSystemMetrics(SM_CMONITORS)` is 1 — while GLFW, which counts only an active display
// device under an active adapter, reports none. raylib 5.5's `InitWindow` calls `rlglInit`
// whether its platform init succeeded or not, so zero monitors there is an access violation
// rather than a false, and `IsWindowReady()` below is never reached to catch it.
//
// Declared rather than included, the same way the Cocoa hint above is and for the same reason:
// glfw is linked into raylib on every desktop target and its headers are not installed beside
// raylib's. `glfwInit` returns true immediately once it has run, so asking here leaves raylib's
// own call a no-op.
extern "C" {
struct GLFWmonitor;
int glfwInit(void);
GLFWmonitor** glfwGetMonitors(int* count);
}
#endif

// raylib reports a monitor's whole bounds only. Declared for the reason the block above gives.
extern "C" {
struct GLFWmonitor;
GLFWmonitor** glfwGetMonitors(int* count);
void glfwGetMonitorWorkarea(GLFWmonitor* monitor, int* xpos, int* ypos, int* width, int* height);
}

inline bool DisplayAvailable() {
#if defined(__APPLE__)
  // Asked through CoreGraphics rather than GLFW: `glfwInit` here would run before `OpenWindow`'s
  // `glfwInitHint` below, and a hint set after GLFW has initialised is ignored — the Dock icon
  // that hint exists to suppress would come back for every hidden window. macOS has a display
  // wherever it has a window server, but a sleeping display is where that stops holding: raylib
  // centres a new window on `glfwGetPrimaryMonitor()`, which returns null once every online
  // display is asleep, and raylib 5.5 reads that monitor's video mode without checking for null
  // first, taking a SIGSEGV before `IsWindowReady` is ever reached. CoreGraphics can tell a
  // sleeping display from an absent one without starting GLFW.
  CGDirectDisplayID displays[16];
  uint32_t count = 0;
  if (::CGGetOnlineDisplayList(16, displays, &count) != kCGErrorSuccess || count == 0) {
    return true;  // nothing to ask; keep the prior "a window server is always a display" answer
  }
  for (uint32_t i = 0; i < count; ++i) {
    if (!::CGDisplayIsAsleep(displays[i])) return true;
  }
  return false;  // every online display is asleep: the case GLFW's monitor lookup dies on
#else
  if (!glfwInit()) return false;
  int count = 0;
  GLFWmonitor** monitors = glfwGetMonitors(&count);
  // A Linux box with no panel attached still lists one monitor, a 0x0 screen from Xwayland, that
  // no window can be shown on.
  for (int i = 0; monitors != nullptr && i < count; ++i) {
    int x = 0, y = 0, width = 0, height = 0;
    glfwGetMonitorWorkarea(monitors[i], &x, &y, &width, &height);
    if (width > 0 && height > 0) return true;
  }
  return false;
#endif
}

inline bool OpenWindow(const std::string& title, int width, int height, bool hidden) {
  ::SetTraceLogLevel(LOG_WARNING);
#if defined(__APPLE__)
  if (hidden) glfwInitHint(kGlfwCocoaMenubar, 0);
#endif
  unsigned int configFlags = FLAG_WINDOW_UNDECORATED;
  if (hidden) configFlags |= FLAG_WINDOW_HIDDEN;
  ::SetConfigFlags(configFlags);
  ::InitWindow(width, height, title.c_str());
  if (!::IsWindowReady()) {
    std::cerr << "InitWindow failed" << std::endl;
    return false;
  }
  // raylib's default exit key is Escape, which would read a game's own pause key during live
  // play as the window's close button.
  ::SetExitKey(KEY_NULL);
  return true;
}

inline void CloseWindow() { ::CloseWindow(); }

inline void WindowSize(int& width, int& height) {
  width = ::GetScreenWidth();
  height = ::GetScreenHeight();
}

inline void SetWindowSize(int width, int height) { ::SetWindowSize(width, height); }

inline void SetWindowPosition(int x, int y) { ::SetWindowPosition(x, y); }

inline void WindowPosition(int& x, int& y) {
  ::Vector2 position = ::GetWindowPosition();
  x = static_cast<int>(position.x);
  y = static_cast<int>(position.y);
}

inline void WorkAreaContaining(int x, int y, int& boundsX, int& boundsY, int& boundsWidth,
                               int& boundsHeight) {
  int monitor = ::GetCurrentMonitor();
  int count = ::GetMonitorCount();
  for (int i = 0; i < count; ++i) {
    ::Vector2 origin = ::GetMonitorPosition(i);
    int left = static_cast<int>(origin.x);
    int top = static_cast<int>(origin.y);
    if (x >= left && x < left + ::GetMonitorWidth(i) && y >= top && y < top + ::GetMonitorHeight(i)) {
      monitor = i;
      break;
    }
  }
  // raylib numbers its monitors in GLFW's own order.
  GLFWmonitor** monitors = glfwGetMonitors(&count);
  if (monitors != nullptr && monitor >= 0 && monitor < count) {
    glfwGetMonitorWorkarea(monitors[monitor], &boundsX, &boundsY, &boundsWidth, &boundsHeight);
    if (boundsWidth > 0 && boundsHeight > 0) return;
  }
  ::Vector2 origin = ::GetMonitorPosition(monitor);
  boundsX = static_cast<int>(origin.x);
  boundsY = static_cast<int>(origin.y);
  boundsWidth = ::GetMonitorWidth(monitor);
  boundsHeight = ::GetMonitorHeight(monitor);
}

/** raylib services the window's event queue inside `EndDrawing`, and live play draws a frame
 *  every time round the idle loop, so this only reads what the last `EndDrawing` gathered. Every
 *  key and mouse button that went down is latched for `LivePressed()`. Keys are drained from
 *  raylib's own queue, which keeps a tap that went down and came up inside one frame, so a game's
 *  `GetKeyPressed` finds that queue empty. */
inline void PumpEvents() {
  for (int key = ::GetKeyPressed(); key != 0; key = ::GetKeyPressed()) detail::LatchLiveKey(key);
  for (int button = MOUSE_BUTTON_LEFT; button <= MOUSE_BUTTON_BACK; ++button) {
    if (::IsMouseButtonPressed(button)) detail::LatchLiveButton(button);
  }
}

inline void ShowWindowForPlay(bool shown) {
  if (shown) {
    ::ClearWindowState(FLAG_WINDOW_HIDDEN);
    ::SetWindowFocused();
  } else {
    ::SetWindowState(FLAG_WINDOW_HIDDEN);
  }
}

/** raylib reads the close button inside `EndDrawing`, and clears it for the next frame. */
inline bool WindowCloseRequested() { return ::WindowShouldClose(); }

inline void BeginWindowFrame() {
  ::BeginDrawing();
  ::ClearBackground(kBackground);
}

inline void EndWindowFrame() { ::EndDrawing(); }

/** The off-screen target, kept between captures and rebuilt when the size asked for changes. */
inline ::RenderTexture2D& OffscreenRef() {
  static ::RenderTexture2D target{};
  return target;
}

inline bool BeginOffscreenFrame(int width, int height) {
  static int targetWidth = -1;
  static int targetHeight = -1;
  if (targetWidth != width || targetHeight != height) {
    if (targetWidth > 0) ::UnloadRenderTexture(OffscreenRef());
    OffscreenRef() = ::LoadRenderTexture(width, height);
    targetWidth = width;
    targetHeight = height;
  }
  if (OffscreenRef().texture.id == 0) return false;
  ::BeginTextureMode(OffscreenRef());
  ::ClearBackground(kBackground);
  return true;
}

/** Whether every pixel of `img` is the frame's own clear colour — the honest way to say a
 *  schematic held nothing, cheaper and simpler than tracking geometry: if the game's draw call
 *  painted nothing beyond the background, there was nothing there to paint. A game that always
 *  draws chrome (a grid, a border) will never read as empty by this test, which is a real limit
 *  of a pixel-only check. */
inline bool ImageIsBlank(const ::Image& img, Color background) {
  Color* pixels = ::LoadImageColors(img);
  if (pixels == nullptr) return false;
  bool blank = true;
  int count = img.width * img.height;
  for (int i = 0; i < count; ++i) {
    Color p = pixels[i];
    if (p.r != background.r || p.g != background.g || p.b != background.b || p.a != background.a) {
      blank = false;
      break;
    }
  }
  ::UnloadImageColors(pixels);
  return blank;
}

inline bool EndOffscreenFrame(std::vector<unsigned char>& bytes, bool& blank, Color background,
                              int& width, int& height, bool png) {
  ::EndTextureMode();

  ::Image img = ::LoadImageFromTexture(OffscreenRef().texture);
  ::ImageFlipVertical(&img);  // a render texture's origin is bottom-left; the window's is top-left
  blank = ImageIsBlank(img, background);
  width = img.width;
  height = img.height;
  if (!png) {
    ::ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    const unsigned char* pixels = static_cast<const unsigned char*>(img.data);
    bool ok = pixels != nullptr && img.width > 0 && img.height > 0;
    if (ok) bytes.assign(pixels, pixels + static_cast<size_t>(img.width) * img.height * 4);
    ::UnloadImage(img);
    return ok;
  }
  int fileSize = 0;
  unsigned char* data = ::ExportImageToMemory(img, ".png", &fileSize);
  ::UnloadImage(img);
  if (data == nullptr || fileSize <= 0) return false;
  bytes.assign(data, data + fileSize);
  ::MemFree(data);
  return true;
}

/** Installs the transform the game's own raw raylib drawing runs under. The kit does not use it
 *  — `Context` has already converted to output-pixel space — so every primitive above steps back
 *  outside it. */
inline void BeginGameCamera(const Camera& camera) {
  ::Camera2D native{};
  native.target = ::Vector2{camera.target.x, camera.target.y};
  native.offset = ::Vector2{camera.offset.x, camera.offset.y};
  native.zoom = camera.zoom;
  native.rotation = 0.0f;
  GameCameraRef() = native;
  GameCameraActive() = true;
  ::BeginMode2D(native);
}

inline void EndGameCamera() {
  if (!GameCameraActive()) return;
  ::EndMode2D();
  GameCameraActive() = false;
}

}  // namespace backend
}  // namespace turret

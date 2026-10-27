// Turret's game harness — the protocol half.
//
// A game vendors this header plus one backend header beside it and includes the backend:
// `turret_game_raylib.h` or `turret_game_sdl.h`. The backend supplies the geometry types and
// the drawing primitives, then includes this file, which owns everything a renderer has no
// opinion about — the control socket, the frame format, the JSON, `Context`, `GameCallbacks`,
// the command loop and `main`. Including this file on its own is an error: it has no backend
// to draw through.
//
// A game includes one backend header from exactly one .cpp file and defines
// `turret::GameCallbacks TurretGame()`. This header owns `main`: it opens the game's window,
// connects the control socket named in argv[1], and idles waiting for commands. The game has no
// loop of its own — Step advances the world one fixed tick, Draw and DrawSchematic only read it,
// and nothing in between happens except in response to a command.
//
// argv[2], if present, places the window: "x,y" or "x,y,width,height", comma-separated
// integers. Absent, the window opens wherever the OS drops it. This is placement only, done
// once at startup — no raising, no focus stealing, nothing on a timer.
//
// The window is a real OS window, and TURRET_GAME_HIDDEN hides it. Turret sets it on every
// binary it spawns, so a session's picture arrives in the pane and no window opens over the
// top of the app; this repo's own automated tests set it for the same reason. It hides the
// application as well as the window: each backend asks its windowing library to stay a
// background app, so a hidden binary leaves no Dock icon on macOS. Run the binary
// yourself with the variable unset and the window is there to watch. A screenshot never reads
// the window either way (see CaptureScreenshot), so hiding it costs no picture. Live play
// (`play_start`) shows the window and ticks the game in real time on the game's own `liveInput`,
// and `play_stop` hides it again; docs/decisions/game-live-play.md in Turret is why. A frame
// bigger than the display gets a window shrunk to fit it, and the window's frames are scaled
// down to match (see ApplyWindowPlacement).
//
// See docs/plans/archive/memory/game-mode.md for the protocol this implements and the invariants it
// holds: game time only moves inside a command, drawing never mutates the world, and idling
// still pumps the window's event queue so the OS does not mark it unresponsive.
#pragma once

#ifndef TURRET_BACKEND
#error "Include turret_game_raylib.h or turret_game_sdl.h rather than turret_game.h — this half has no renderer."
#endif

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <chrono>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

// The control socket is a real unix domain socket on POSIX. It was one on Windows too, once —
// Windows 10 1803+ supports AF_UNIX at the OS level, and an earlier version of this file spoke
// it over Winsock's afunix.h. That branch was correct and a raw .NET or C client can still bind
// one at the OS level today. It is dead the moment Turret is the other end, though: Node's `net`
// module — what `src/main/game/game-process.ts` listens with — implements a local-domain
// `.listen()` as a Windows named pipe unconditionally, on every version checked, and gives no
// documented way to opt into a real AF_UNIX socket instead (confirmed three independent ways;
// see docs/plans/archive/memory/windows-support.md's phase 4 finding). Carrying the AF_UNIX branch
// forward as untested dead code, on the chance a future Node version adds the option back, was
// considered and rejected — an `#ifdef` nobody compiles rots quietly, and git history already
// keeps the old branch if that day comes. So the win32 branch below is a named pipe client, not
// an AF_UNIX one: it connects to the `\\.\pipe\<name>` path Turret listens on with
// `net.createServer().listen()`, using CreateFileW/ReadFile/WriteFile/PeekNamedPipe rather than
// sockets at all — no Winsock, no WSAStartup. This still needs windows.h, which declares
// CloseWindow, ShowCursor, DrawText, LoadImage and PlaySound, every one of which collides with a
// renderer function of the same name, so the two sections that declare them are dropped before
// it is pulled in.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOGDI
#define NOGDI
#endif
#ifndef NOUSER
#define NOUSER
#endif
#ifndef NOMINMAX
#define NOMINMAX  // or windows.h's min and max macros eat every std::min and std::max below
#endif
#include <windows.h>
#undef near
#undef far
// The idle loop's own poll()-shaped check (see "Idling is pumping" below) tests these three
// flags by name. windows.h carries no poll() for a pipe handle — WSAPoll only takes sockets, and
// this file no longer opens one — so detail::PollOne sets these itself from PeekNamedPipe rather
// than delegating to a real poll syscall; the values only have to be distinct from each other.
#ifndef POLLIN
#define POLLIN 0x0001
#endif
#ifndef POLLHUP
#define POLLHUP 0x0002
#endif
#ifndef POLLERR
#define POLLERR 0x0004
#endif
#else
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace turret {

/** Maps the game's own logical coordinate space onto the render target. The harness carries the
 *  transform rather than relying on the backend to install one, and the drawing kit applies it
 *  before it calls a primitive. A game still works in world coordinates and never touches this.
 *  A backend whose renderer does install a transform of its own uses it for the game's own raw
 *  drawing, through `BeginGameCamera` — never for the kit, which has already done the work. */
struct Camera {
  Vec2 target{0.0f, 0.0f};  // the world point that lands on `offset`
  Vec2 offset{0.0f, 0.0f};  // where on the target that world point lands, in output pixels
  float zoom = 1.0f;

  Vec2 ToScreen(Vec2 w) const {
    return Vec2{(w.x - target.x) * zoom + offset.x, (w.y - target.y) * zoom + offset.y};
  }
  Rect ToScreen(Rect r) const {
    Vec2 p = ToScreen(Vec2{r.x, r.y});
    return Rect{p.x, p.y, r.w * zoom, r.h * zoom};
  }
  Vec2 ToWorld(Vec2 s) const {
    float z = zoom > 0.0f ? zoom : 1.0f;
    return Vec2{(s.x - offset.x) / z + target.x, (s.y - offset.y) / z + target.y};
  }
};


// ---------------------------------------------------------------------------------------------
// The backend seam. Every one of these is declared here and defined by the backend header that
// included this file — the whole of what a renderer has to supply, and the whole of what this
// half is allowed to know about one. Adding a renderer means writing these and nothing else.
//
// Every drawing primitive takes output-pixel coordinates. `Context` applies the camera before
// it calls one, so a backend never reasons about framing. The two exceptions are
// `BeginGameCamera`/`EndGameCamera`, which wrap the game's own draw callback: a game drawing
// with raw backend calls expects the transform its renderer installs, and a backend that has no
// such thing leaves them empty.
//
// The backend also supplies, before including this file: `Vec2`, `Rect`, `Color`, the colour
// constants and `WithAlpha`.
// ---------------------------------------------------------------------------------------------
namespace backend {

// -- The window. --------------------------------------------------------------------------
/** Whether this desktop session has a display the renderer will open a window on, answered
 *  without opening anything. `main` asks before `OpenWindow`, because a renderer's own answer to
 *  "no display" is not always a return value: raylib 5.5's `InitWindow` ignores its platform
 *  init's failure and calls OpenGL with no context, so a session whose display has gone takes an
 *  access violation rather than a false. A backend that cannot tell without initialising answers
 *  true and leaves it to `OpenWindow`. */
bool DisplayAvailable();
/** Opens the game's window and whatever draws into it. False if either could not be made, with
 *  the backend's own error already on stderr. */
bool OpenWindow(const std::string& title, int width, int height, bool hidden);
void CloseWindow();
void WindowSize(int& width, int& height);
void SetWindowSize(int width, int height);
void SetWindowPosition(int x, int y);
void WindowPosition(int& x, int& y);
/** The usable bounds of the display `x, y` falls on, less the menu bar, dock or taskbar, or of
 *  the display the window is on when it falls on none. In the same units as `WindowSize`. */
void WorkAreaContaining(int x, int y, int& boundsX, int& boundsY, int& boundsWidth,
                        int& boundsHeight);
/** Drains the OS event queue. Called while idling between commands, so the window does not read
 *  as unresponsive. The harness never acts on real input; during live play the game's
 *  `liveInput` reads the state this leaves behind. Every key and mouse button that went down is
 *  handed to `detail::LatchLiveKey` and `detail::LatchLiveButton`, which keep it for
 *  `LivePressed()` while a live run ticks. */
void PumpEvents();
/** Shows the window, raises it and gives it the keyboard for live play, or hides it again. */
void ShowWindowForPlay(bool shown);
/** Whether the person asked to close the window since the last frame. */
bool WindowCloseRequested();

// -- Frames. ------------------------------------------------------------------------------
/** Targets the window and clears it to `kBackground`. */
void BeginWindowFrame();
void EndWindowFrame();
/** Targets an off-screen surface of exactly this size, creating or resizing one as needed, and
 *  clears it. A screenshot is taken from here rather than from the window: a window is double-
 *  or triple-buffered and its swap timing is the compositor's business, so reading it can come
 *  back mid-swap or reliably behind. */
bool BeginOffscreenFrame(int width, int height);
/** Reads the off-screen surface back, says whether every pixel of it is `background`, reports
 *  the size actually captured, and re-targets the window. `bytes` is a PNG when `png` is true, and
 *  otherwise the raw pixels: 8-bit RGBA, top row first, `width * height * 4` bytes. */
bool EndOffscreenFrame(std::vector<unsigned char>& bytes, bool& blank, Color background, int& width,
                       int& height, bool png);

// -- The game's own transform. ------------------------------------------------------------
void BeginGameCamera(const Camera& camera);
void EndGameCamera();

// -- Primitives, all in output-pixel space. -----------------------------------------------
void FillRect(Rect rect, Color color);
void RectOutline(Rect rect, Color color, float thickness);
void ThickLine(Vec2 from, Vec2 to, Color color, float thickness);
void FillTriangle(Vec2 a, Vec2 b, Vec2 c, Color color);
void CircleOutline(Vec2 centre, float radius, Color color, float thickness, int segments);
Vec2 MeasureText(const std::string& text, float size);
void DrawText(Vec2 pos, const std::string& text, Color color, float size);

}  // namespace backend

// ---------------------------------------------------------------------------------------------
// A minimal JSON value. Hand-rolled so the header adds no dependency beyond the renderer the
// backend links, which any game using this harness already has. Objects are `std::map` so a
// document with the same content serialises the same way every time, which is what a "run it
// twice, compare" test needs.
// ---------------------------------------------------------------------------------------------
namespace json {

class Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

enum class Type { Null, Bool, Number, String, Array, Object };

class Value {
 public:
  Value() : type_(Type::Null) {}
  Value(std::nullptr_t) : type_(Type::Null) {}
  Value(bool b) : type_(Type::Bool), bool_(b) {}
  Value(int n) : type_(Type::Number), num_(n) {}
  Value(unsigned n) : type_(Type::Number), num_(n) {}
  Value(long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(unsigned long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(long long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(unsigned long long n) : type_(Type::Number), num_(static_cast<double>(n)) {}
  Value(double n) : type_(Type::Number), num_(n) {}
  Value(const char* s) : type_(Type::String), str_(s) {}
  Value(std::string s) : type_(Type::String), str_(std::move(s)) {}
  Value(Array a) : type_(Type::Array), arr_(std::move(a)) {}
  Value(Object o) : type_(Type::Object), obj_(std::move(o)) {}

  static Value MakeArray() {
    Value v;
    v.type_ = Type::Array;
    return v;
  }
  static Value MakeObject() {
    Value v;
    v.type_ = Type::Object;
    return v;
  }

  Type type() const { return type_; }
  bool isNull() const { return type_ == Type::Null; }
  bool isObject() const { return type_ == Type::Object; }
  bool isArray() const { return type_ == Type::Array; }

  Value& operator[](const std::string& key) {
    if (type_ != Type::Object) {
      type_ = Type::Object;
      obj_.clear();
    }
    return obj_[key];
  }

  const Value* find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    auto it = obj_.find(key);
    return it == obj_.end() ? nullptr : &it->second;
  }

  void push_back(Value v) {
    if (type_ != Type::Array) {
      type_ = Type::Array;
      arr_.clear();
    }
    arr_.push_back(std::move(v));
  }

  double asNumber(double def = 0) const { return type_ == Type::Number ? num_ : def; }
  int asInt(int def = 0) const { return type_ == Type::Number ? static_cast<int>(num_) : def; }
  uint64_t asUint64(uint64_t def = 0) const {
    return type_ == Type::Number ? static_cast<uint64_t>(num_) : def;
  }
  bool asBool(bool def = false) const { return type_ == Type::Bool ? bool_ : def; }
  std::string asString(const std::string& def = "") const {
    return type_ == Type::String ? str_ : def;
  }
  const Array& asArray() const {
    static const Array empty;
    return type_ == Type::Array ? arr_ : empty;
  }
  const Object& asObject() const {
    static const Object empty;
    return type_ == Type::Object ? obj_ : empty;
  }

  std::string dump() const {
    std::string out;
    DumpTo(out);
    return out;
  }

  static Value parse(const std::string& text);

 private:
  Type type_;
  bool bool_ = false;
  double num_ = 0;
  std::string str_;
  Array arr_;
  Object obj_;

  // JSON has no NaN or infinity. Written as the strings "NaN", "Infinity" and "-Infinity", so a
  // physics bug that produces one reaches the model as a readable value instead of a document
  // Turret cannot parse.
  static std::string FormatNumber(double v) {
    if (std::isnan(v)) return "\"NaN\"";
    if (std::isinf(v)) return v > 0 ? "\"Infinity\"" : "\"-Infinity\"";
    if (v == std::floor(v) && std::fabs(v) < 1e15) {
      return std::to_string(static_cast<long long>(v));
    }
    std::ostringstream oss;
    oss.precision(9);
    oss << v;
    return oss.str();
  }

  static void DumpString(const std::string& s, std::string& out) {
    out += '"';
    for (unsigned char c : s) {
      switch (c) {
        case '"':
          out += "\\\"";
          break;
        case '\\':
          out += "\\\\";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
          } else {
            out += static_cast<char>(c);
          }
      }
    }
    out += '"';
  }

  void DumpTo(std::string& out) const {
    switch (type_) {
      case Type::Null:
        out += "null";
        break;
      case Type::Bool:
        out += bool_ ? "true" : "false";
        break;
      case Type::Number:
        out += FormatNumber(num_);
        break;
      case Type::String:
        DumpString(str_, out);
        break;
      case Type::Array: {
        out += '[';
        bool first = true;
        for (const auto& v : arr_) {
          if (!first) out += ',';
          first = false;
          v.DumpTo(out);
        }
        out += ']';
        break;
      }
      case Type::Object: {
        out += '{';
        bool first = true;
        for (const auto& [k, v] : obj_) {
          if (!first) out += ',';
          first = false;
          DumpString(k, out);
          out += ':';
          v.DumpTo(out);
        }
        out += '}';
        break;
      }
    }
  }
};

class Parser {
 public:
  explicit Parser(const std::string& s) : s_(s) {}

  Value Parse() {
    SkipWs();
    Value v = ParseValue();
    return v;
  }

 private:
  const std::string& s_;
  size_t i_ = 0;

  char Peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
  char Next() { return i_ < s_.size() ? s_[i_++] : '\0'; }
  void SkipWs() {
    while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) ++i_;
  }
  [[noreturn]] void Fail(const std::string& msg) {
    throw std::runtime_error("json parse error: " + msg + " at offset " + std::to_string(i_));
  }
  void Expect(char c) {
    if (Next() != c) Fail(std::string("expected '") + c + "'");
  }

  Value ParseValue() {
    SkipWs();
    char c = Peek();
    if (c == '{') return ParseObject();
    if (c == '[') return ParseArray();
    if (c == '"') return Value(ParseString());
    if (c == 't' || c == 'f') return ParseBool();
    if (c == 'n') return ParseNull();
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return ParseNumber();
    Fail("unexpected character");
  }

  Value ParseObject() {
    Expect('{');
    Value v = Value::MakeObject();
    SkipWs();
    if (Peek() == '}') {
      Next();
      return v;
    }
    while (true) {
      SkipWs();
      if (Peek() != '"') Fail("expected string key");
      std::string key = ParseString();
      SkipWs();
      Expect(':');
      v[key] = ParseValue();
      SkipWs();
      char c = Next();
      if (c == ',') continue;
      if (c == '}') break;
      Fail("expected ',' or '}'");
    }
    return v;
  }

  Value ParseArray() {
    Expect('[');
    Value v = Value::MakeArray();
    SkipWs();
    if (Peek() == ']') {
      Next();
      return v;
    }
    while (true) {
      v.push_back(ParseValue());
      SkipWs();
      char c = Next();
      if (c == ',') continue;
      if (c == ']') break;
      Fail("expected ',' or ']'");
    }
    return v;
  }

  std::string ParseString() {
    Expect('"');
    std::string out;
    while (true) {
      if (i_ >= s_.size()) Fail("unterminated string");
      char c = Next();
      if (c == '"') break;
      if (c != '\\') {
        out += c;
        continue;
      }
      char e = Next();
      switch (e) {
        case '"':
          out += '"';
          break;
        case '\\':
          out += '\\';
          break;
        case '/':
          out += '/';
          break;
        case 'n':
          out += '\n';
          break;
        case 't':
          out += '\t';
          break;
        case 'r':
          out += '\r';
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        case 'u': {
          if (i_ + 4 > s_.size()) Fail("bad unicode escape");
          unsigned code = 0;
          for (int k = 0; k < 4; ++k) {
            char h = Next();
            code <<= 4;
            if (h >= '0' && h <= '9')
              code |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f')
              code |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
              code |= static_cast<unsigned>(h - 'A' + 10);
            else
              Fail("bad hex digit");
          }
          if (code < 0x80) {
            out += static_cast<char>(code);
          } else if (code < 0x800) {
            out += static_cast<char>(0xC0 | (code >> 6));
            out += static_cast<char>(0x80 | (code & 0x3F));
          } else {
            out += static_cast<char>(0xE0 | (code >> 12));
            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (code & 0x3F));
          }
          break;
        }
        default:
          Fail("bad escape");
      }
    }
    return out;
  }

  Value ParseNumber() {
    size_t start = i_;
    if (Peek() == '-') Next();
    while (std::isdigit(static_cast<unsigned char>(Peek()))) Next();
    if (Peek() == '.') {
      Next();
      while (std::isdigit(static_cast<unsigned char>(Peek()))) Next();
    }
    if (Peek() == 'e' || Peek() == 'E') {
      Next();
      if (Peek() == '+' || Peek() == '-') Next();
      while (std::isdigit(static_cast<unsigned char>(Peek()))) Next();
    }
    return Value(std::stod(s_.substr(start, i_ - start)));
  }

  Value ParseBool() {
    if (s_.compare(i_, 4, "true") == 0) {
      i_ += 4;
      return Value(true);
    }
    if (s_.compare(i_, 5, "false") == 0) {
      i_ += 5;
      return Value(false);
    }
    Fail("bad literal");
  }

  Value ParseNull() {
    if (s_.compare(i_, 4, "null") == 0) {
      i_ += 4;
      return Value();
    }
    Fail("bad literal");
  }
};

inline Value Value::parse(const std::string& text) { return Parser(text).Parse(); }

}  // namespace json

// ---------------------------------------------------------------------------------------------
// The game-facing surface.
// ---------------------------------------------------------------------------------------------

enum class RenderMode { Normal, Schematic };

inline const char* RenderModeName(RenderMode m) {
  return m == RenderMode::Schematic ? "schematic" : "normal";
}

inline bool RenderModeFromName(const std::string& name, RenderMode& out) {
  if (name == "normal") {
    out = RenderMode::Normal;
    return true;
  }
  if (name == "schematic") {
    out = RenderMode::Schematic;
    return true;
  }
  return false;
}

/** What `apply_state` returns. A game accepts a state document or refuses it by name. */
struct ApplyResult {
  bool ok = false;
  std::string code;
  std::string message;

  static ApplyResult Accepted() { return ApplyResult{true, "", ""}; }
  static ApplyResult Refused(std::string code, std::string message) {
    return ApplyResult{false, std::move(code), std::move(message)};
  }
};

/** What `save_state` returns: the world as bytes only this game can interpret, or a refusal by
 *  name — the same shape `ApplyResult` gives `apply_state`, so a game implementing one is not
 *  surprised by the other. */
struct SaveResult {
  bool ok = false;
  std::string code;
  std::string message;
  std::vector<uint8_t> bytes;

  static SaveResult Accepted(std::vector<uint8_t> bytes) { return SaveResult{true, "", "", std::move(bytes)}; }
  static SaveResult Refused(std::string code, std::string message) {
    return SaveResult{false, std::move(code), std::move(message), {}};
  }
};

/** Where a mouse or touch pointer is, in the game's own logical frame — the frame's own pixels,
 *  `GameCallbacks::width` by `height`, top-left origin — and whether its primary button is
 *  held. `present` is false when the step named no pointer. The harness keeps
 *  no state of its own between ticks, so a game that wants a press or a release compares `down`
 *  with the tick before in its own world, where a snapshot and a replay carry it. */
struct Pointer {
  bool present = false;
  float x = 0.0f;
  float y = 0.0f;
  bool down = false;
};

/** The input held across a `step` command's ticks. Buttons named by the model, held constant
 *  for every tick the command advances — see docs/plans/archive/memory/game-mode.md. */
struct Input {
  std::set<std::string> buttons;
  Pointer pointer;
  bool Held(const std::string& name) const { return buttons.count(name) != 0; }
};

/** The keys and mouse buttons that went down during live play since the last `liveInput` call,
 *  in the order they went down, with key repeat left out. Codes are the backend's own: an
 *  `SDL_Scancode` and an `SDL_BUTTON_*` number on SDL, a `KeyboardKey` and a `MouseButton` on
 *  raylib. A tap that goes down and comes up between two ticks never shows in a held-state read
 *  such as `SDL_GetKeyboardState` or `IsKeyDown`, and it shows here. Read it inside `liveInput`:
 *  the harness empties it after every `liveInput` call, and gathers nothing while play is paused
 *  or stopped. Version 13. */
struct LivePresses {
  std::vector<int> keys;
  std::vector<int> buttons;
  bool key(int code) const { return std::find(keys.begin(), keys.end(), code) != keys.end(); }
  bool button(int code) const { return std::find(buttons.begin(), buttons.end(), code) != buttons.end(); }
};

namespace detail {
inline LivePresses& LivePressLatch() {
  static LivePresses presses;
  return presses;
}
}  // namespace detail

/** What `liveInput` reads for the presses since the tick before; see `LivePresses`. */
inline const LivePresses& LivePressed() { return detail::LivePressLatch(); }

/** One packet queued during `Step`, addressed to another peer by the id Turret assigned it at
 *  `init`. Tagged with the tick it was sent on so the answer can say when it left, the same way
 *  an event carries the tick it happened at. A game never constructs this directly — see
 *  `Context::SendPacket`. */
struct OutgoingPacket {
  uint64_t tick = 0;
  std::string to;
  std::vector<uint8_t> data;
  bool reliable = false;
};

/** What a draw callback is drawing to. `region` is the requested crop, as a fraction of the
 *  game's own full logical frame (0,0)-(1,1), top-left origin. `width`/`height` are the output
 *  image's pixel size. A game may ignore this entirely and always draw the whole world at its
 *  own logical coordinates — the harness reframes and scales the result to match `region` and
 *  `size` on its own, via a camera transform wrapped around the callback. A game only needs to
 *  read `Framing` if it wants to do less work, or more detail, for a specific crop. */
struct Framing {
  double x0 = 0.0, y0 = 0.0, x1 = 1.0, y1 = 1.0;
  int width = 0, height = 0;
};

/** Passed to every callback. Owns the tick counter, the input in force, the render mode, and
 *  the event queue — a game reads these and calls Emit; it never advances the tick itself. The
 *  `_`-prefixed methods are the harness's own bookkeeping; a game does not call them. */
class Context {
 public:
  /** Labels are rasterised at the size they are drawn at rather than scaled from one atlas, so
   *  a label stays smooth however far the camera is zoomed in or out. */
  /** How small a label is ever allowed to come out, in output pixels. A game names its label
   *  sizes in its own coordinate space, which a zoomed-out crop or a small requested size can
   *  shrink to a smear; a label under this is drawn larger instead, since a label nobody can
   *  read is worse than one taking up more room than the game intended. */
  static constexpr float kMinLabelOutputPx = 12.0f;

  uint64_t tick() const { return tick_; }
  const Input& input() const { return input_; }
  RenderMode renderMode() const { return renderMode_; }

  /** This process's own id — the same string `load` assigned it and the model types back to
   *  `set_state`/`observe`/`reset`. `"1"` in a single-peer session. Set once, from `init`. */
  const std::string& PeerId() const { return peerId_; }
  /** Every peer id in this session, in load order, including this process's own — what
   *  `SendPacket`'s `to` names. A single-peer session's list holds only `PeerId()`. */
  const std::vector<std::string>& Peers() const { return peers_; }

  /** Queues a packet addressed to another peer, tagged with the tick this is called on.
   *  Turret owns delivery entirely: this only ever reaches the queue Turret drains from this
   *  command's answer — the harness never opens a socket of its own. See
   *  docs/plans/archive/memory/game-mode.md ("Peers") and this phase's report for how Turret decides
   *  when it arrives.
   *
   *  `reliable` asks for the delivery a reliable transport gives: Turret never drops the packet
   *  and never reorders it, so reliable packets from one peer to another arrive in the order they
   *  were sent. The latency dial still delays it. An unreliable packet (the default) is subject
   *  to all three dials. Reliable and unreliable packets do not order against each other. */
  void SendPacket(const std::string& to, const std::vector<uint8_t>& data, bool reliable = false) {
    outgoing_.push_back(OutgoingPacket{tick_, to, data, reliable});
  }
  void SendPacket(const std::string& to, const std::string& data, bool reliable = false) {
    SendPacket(to, std::vector<uint8_t>(data.begin(), data.end()), reliable);
  }

  /** As cheap to write as a printf: what happened, optionally naming an entity, optionally
   *  carrying detail. Queued and drained the next time the harness builds a describe document. */
  void Emit(const std::string& kind) { Emit(kind, "", json::Value()); }
  void Emit(const std::string& kind, const std::string& subject) {
    Emit(kind, subject, json::Value());
  }
  void Emit(const std::string& kind, const std::string& subject, const json::Value& detail) {
    json::Value ev = json::Value::MakeObject();
    ev["tick"] = tick_;
    ev["kind"] = kind;
    if (!subject.empty()) ev["subject"] = subject;
    if (!detail.isNull()) ev["detail"] = detail;
    events_.push_back(std::move(ev));
  }

  /** A ready-made entity object for a describe document: id, kind, position, and whatever else
   *  the caller adds afterwards. */
  static json::Value Entity(const std::string& id, const std::string& kind, double x, double y) {
    json::Value e = json::Value::MakeObject();
    e["id"] = id;
    e["kind"] = kind;
    e["x"] = x;
    e["y"] = y;
    return e;
  }

  // -- Layers. --------------------------------------------------------------------------------

  /** Whether a layer is drawn in the pass now running. An id nothing has declared reads as on,
   *  so a game that names a layer it forgot to declare still draws it. */
  bool LayerOn(const std::string& id) const {
    auto it = layers_.find(id);
    return it == layers_.end() ? true : it->second;
  }

  /** Opens a layer over the drawing that follows. Every drawing call between this and the
   *  matching `PopLayer` is dropped while the layer is off, so a game names a layer once rather
   *  than testing it around each call. A layer opened inside a closed one stays closed however
   *  it was declared. `LayerScope` pops on the way out of a block and is what a game normally
   *  uses. */
  void PushLayer(const std::string& id) const {
    layerStack_.push_back(OpenLayer{id, _DrawingSuppressed() || !LayerOn(id)});
  }
  void PopLayer() const {
    if (!layerStack_.empty()) layerStack_.pop_back();
  }

  // -- The camera, and drawing in world space. -------------------------------------------------

  /** The transform in force for the draw pass now running. Every helper below applies it, so a
   *  game only needs this to do something the kit has no call for — see `renderer()`. */
  const Camera& camera() const { return camera_; }
  /** Output pixels per unit of the game's own coordinate space. */
  float zoom() const { return camera_.zoom; }
  Vec2 ToScreen(Vec2 world) const { return camera_.ToScreen(world); }
  Rect ToScreen(Rect world) const { return camera_.ToScreen(world); }

  /** All in the game's own coordinate space, camera applied. Thicknesses and text sizes are in
   *  that space too, so they scale with a zoomed crop the way the shapes they annotate do. */
  void DrawRect(Rect rect, Color color) const {
    if (!_AllowDraw()) return;
    backend::FillRect(camera_.ToScreen(rect), color);
  }
  void DrawRectOutline(Rect rect, Color color, float thickness = 2.0f) const {
    if (!_AllowDraw()) return;
    backend::RectOutline(camera_.ToScreen(rect), color, thickness * camera_.zoom);
  }
  void DrawLine(Vec2 from, Vec2 to, Color color, float thickness = 2.0f) const {
    if (!_AllowDraw()) return;
    backend::ThickLine(camera_.ToScreen(from), camera_.ToScreen(to), color, thickness * camera_.zoom);
  }
  void DrawTriangle(Vec2 a, Vec2 b, Vec2 c, Color color) const {
    if (!_AllowDraw()) return;
    backend::FillTriangle(camera_.ToScreen(a), camera_.ToScreen(b), camera_.ToScreen(c), color);
  }
  /** A filled disc, approximated by a fan. Mobs and projectiles are round in most top-down
   *  games, and a filled triangle is the one primitive every backend has. */
  void DrawCircle(Vec2 centre, float radius, Color color, int segments = 16) const {
    if (!_AllowDraw()) return;
    Vec2 mid = camera_.ToScreen(centre);
    float r = radius * camera_.zoom;
    if (r <= 0.0f || segments < 3) return;
    float previousX = mid.x + r;
    float previousY = mid.y;
    for (int i = 1; i <= segments; ++i) {
      float angle = static_cast<float>(i) / segments * 6.28318530718f;
      float nextX = mid.x + std::cos(angle) * r;
      float nextY = mid.y + std::sin(angle) * r;
      backend::FillTriangle(mid, Vec2{previousX, previousY}, Vec2{nextX, nextY}, color);
      previousX = nextX;
      previousY = nextY;
    }
  }

  /** A ring at `radius` in the game's own coordinate space. Thickness is in that space too, so it
   *  scales with a zoomed crop the way the shape it outlines does. */
  void DrawCircleOutline(Vec2 centre, float radius, Color color, float thickness = 1.5f,
                         int segments = 20) const {
    if (!_AllowDraw()) return;
    backend::CircleOutline(camera_.ToScreen(centre), radius * camera_.zoom, color,
                           thickness * camera_.zoom, segments);
  }

  /** Text in output-pixel space, unaffected by the camera. A game's own on-screen text belongs
   *  here: world-space text magnifies with everything else the moment a region is cropped and
   *  zoomed into, which is exactly when it most needs to stay legible. */
  void DrawScreenText(Vec2 pos, const std::string& text, Color color, float size = 16.0f) const {
    if (!_AllowDraw()) return;
    backend::DrawText(pos, text, color, size);
  }
  Vec2 MeasureScreenText(const std::string& text, float size = 16.0f) const {
    return backend::MeasureText(text, size);
  }

  // -- Where an entity landed in the picture. -------------------------------------------------

  /** Says where an entity was drawn in the picture this pass is making, as a box in the game's
   *  own coordinate space. The screenshot answer reports every box in output pixels, so a model
   *  finds an entity in the picture without measuring pixels itself. Call it from either draw
   *  callback for each entity worth finding, with the id the describe document gives it. */
  void MarkEntity(const std::string& id, Rect world) const { MarkEntityOnScreen(id, camera_.ToScreen(world)); }
  /** The same box in output pixels, for a renderer that projects the world itself: a 3D view's
   *  projected bounds. */
  void MarkEntityOnScreen(const std::string& id, Rect screen) const { marks_.push_back(EntityMark{id, screen}); }

  // -- The schematic drawing kit: a thick outline, an arrow, a label. -------------------------

  void SchematicOutline(Rect rect, Color color, float thickness = 3.0f) const {
    if (!_AllowDraw()) return;
    backend::RectOutline(camera_.ToScreen(rect), color, thickness);
  }

  void SchematicArrow(Vec2 from, Vec2 to, Color color, float thickness = 3.0f) const {
    if (!_AllowDraw()) return;
    Vec2 tail = camera_.ToScreen(from);
    Vec2 tip = camera_.ToScreen(to);
    backend::ThickLine(tail, tip, color, thickness);
    double angle = std::atan2(static_cast<double>(tip.y - tail.y), static_cast<double>(tip.x - tail.x));
    constexpr double kHeadLen = 10.0;
    constexpr double kHeadAngle = 0.5;  // radians
    Vec2 left{static_cast<float>(tip.x - kHeadLen * std::cos(angle - kHeadAngle)),
              static_cast<float>(tip.y - kHeadLen * std::sin(angle - kHeadAngle))};
    Vec2 right{static_cast<float>(tip.x - kHeadLen * std::cos(angle + kHeadAngle)),
               static_cast<float>(tip.y - kHeadLen * std::sin(angle + kHeadAngle))};
    backend::FillTriangle(tip, right, left, color);
  }

  /** Draws text that never lands on top of text already placed this frame — every label the
   *  kit has drawn shares one registry, cleared per draw pass. A candidate spot that overlaps
   *  an earlier label is nudged downward until it is clear, or until it runs out of room, so
   *  two labels are never left to fight over the same pixels.
   *
   *  `pos` is in the game's own coordinate space. Sizing and collision both happen in
   *  output-pixel space, so a label keeps its size and its spacing however the crop is framed —
   *  which is the opposite of how everything else in the kit behaves, and deliberate. */
  void SchematicLabel(Vec2 pos, const std::string& text, Color color, int fontSize = 16) const {
    if (!_AllowDraw()) return;
    float size = std::max(static_cast<float>(fontSize), kMinLabelOutputPx);
    Vec2 measured = backend::MeasureText(text, size);
    if (measured.x <= 0.0f) return;
    Vec2 drawPos = camera_.ToScreen(pos);
    constexpr int kMaxAttempts = 8;
    Rect screenRect{};
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
      screenRect = Rect{drawPos.x, drawPos.y, measured.x, measured.y};
      bool clear = true;
      for (const auto& placed : labelRects_) {
        if (screenRect.x < placed.x + placed.w && placed.x < screenRect.x + screenRect.w &&
            screenRect.y < placed.y + placed.h && placed.y < screenRect.y + screenRect.h) {
          clear = false;
          break;
        }
      }
      if (clear || attempt == kMaxAttempts - 1) break;
      drawPos.y += measured.y + 2.0f;
    }
    labelRects_.push_back(screenRect);
    // A pale plate under the text, so a label over a filled shape or a grid line still reads.
    float pad = size * 0.15f;
    backend::FillRect(Rect{drawPos.x - pad, drawPos.y - pad, measured.x + pad * 2, measured.y + pad * 2},
                      WithAlpha(kBackground, 0.78f));
    backend::DrawText(drawPos, text, color, size);
  }

  // -- Harness-only bookkeeping. A game does not call these. -----------------------------------

  void _ResetForNewRun() {
    tick_ = 0;
    events_.clear();
    outgoing_.clear();
  }
  /** Set by a successful `load_state`, once the game's own `loadState` callback has accepted the
   *  payload — never by a game directly. Puts the tick where the snapshot was taken, unlike
   *  `_ResetForNewRun`, which always goes to 0; clears the event and outgoing-packet queues the
   *  same way, since neither belongs to a world a snapshot is about to replace. */
  void _RestoreTick(uint64_t tick) {
    tick_ = tick;
    events_.clear();
    outgoing_.clear();
  }
  void _AdvanceTick() { ++tick_; }
  /** Records a declared layer and its starting state. The default is honoured the first time an
   *  id is seen and ignored afterwards, so a toggle a person or the model made is not undone by
   *  the next describe. A group holds at most one layer on: a second member declaring itself on
   *  is seeded off instead. */
  void _SeedLayer(const std::string& id, bool on, const std::string& group) {
    if (layers_.count(id) != 0) return;
    if (on && !group.empty()) {
      for (const auto& other : layerOrder_) {
        auto g = layerGroups_.find(other);
        if (g != layerGroups_.end() && g->second == group && layers_[other]) {
          on = false;
          break;
        }
      }
    }
    layerOrder_.push_back(id);
    layerGroups_[id] = group;
    layers_[id] = on;
  }
  /** Turns exactly the named layers on and every other declared layer off. Refuses, naming the
   *  id, when one of them was never declared — a typo would otherwise come back as a picture
   *  quietly missing the overlay that was asked for. */
  bool _SetLayers(const std::vector<std::string>& on, std::string& unknownId) {
    for (const auto& id : on) {
      if (layers_.count(id) == 0) {
        unknownId = id;
        return false;
      }
    }
    for (auto& entry : layers_) entry.second = false;
    for (const auto& id : on) layers_[id] = true;
    return true;
  }
  const std::vector<std::string>& _LayerOrder() const { return layerOrder_; }
  /** Whether a layer drew anything at all in the pass that just ran — a layer that is on and
   *  drew nothing is what a screenshot answer reports back as empty. */
  bool _LayerDrew(const std::string& id) const { return drewLayers_.count(id) != 0; }
  bool _DrawingSuppressed() const { return !layerStack_.empty() && layerStack_.back().closed; }
  /** Every drawing helper's first line: false while the layer in force is off, and otherwise a
   *  note that the layers now open drew something. */
  bool _AllowDraw() const {
    if (_DrawingSuppressed()) return false;
    for (const auto& open : layerStack_) drewLayers_.insert(open.id);
    return true;
  }
  void _SetInput(Input in) { input_ = std::move(in); }
  void _SetRenderMode(RenderMode m) { renderMode_ = m; }
  std::vector<json::Value> _DrainEvents() {
    std::vector<json::Value> drained;
    drained.swap(events_);
    return drained;
  }

  /** Set once from `init`'s own request fields — see `HandleFrame`. Left untouched by
   *  `_ResetForNewRun`, since a peer's identity does not change across `set_state`/`reset`;
   *  only ever called again if a later `init` names it afresh. */
  void _SetPeerIdentity(std::string id, std::vector<std::string> peers) {
    peerId_ = std::move(id);
    peers_ = std::move(peers);
  }
  std::vector<OutgoingPacket> _DrainOutgoingPackets() {
    std::vector<OutgoingPacket> drained;
    drained.swap(outgoing_);
    return drained;
  }
  /** Clears what one draw pass accumulates: the label-collision registry, which layers drew
   *  something, and any layer a game left open by pushing without popping. Called once before
   *  each pass. */
  void _BeginDrawPass() const {
    labelRects_.clear();
    drewLayers_.clear();
    layerStack_.clear();
    marks_.clear();
  }
  struct EntityMark {
    std::string id;
    Rect box;
  };
  /** Every box `MarkEntity` and `MarkEntityOnScreen` recorded in the pass just drawn. */
  const std::vector<EntityMark>& _Marks() const { return marks_; }
  /** Installs the transform for the draw pass about to happen. Every drawing helper reads it,
   *  and SchematicLabel uses it to place labels in output-pixel space. */
  void _SetCamera(const Camera& camera) const { camera_ = camera; }

 private:
  uint64_t tick_ = 0;
  Input input_;
  RenderMode renderMode_ = RenderMode::Normal;
  std::vector<json::Value> events_;
  std::string peerId_;
  std::vector<std::string> peers_;
  std::vector<OutgoingPacket> outgoing_;
  mutable std::vector<Rect> labelRects_;
  mutable Camera camera_{};
  struct OpenLayer {
    std::string id;
    bool closed = false;
  };
  std::vector<std::string> layerOrder_;
  std::map<std::string, bool> layers_;
  std::map<std::string, std::string> layerGroups_;
  mutable std::vector<OpenLayer> layerStack_;
  mutable std::set<std::string> drewLayers_;
  mutable std::vector<EntityMark> marks_;
};

/** Opens a layer for as long as the block lives. Every drawing call inside is dropped while the
 *  layer is off:
 *
 *      { turret::LayerScope hitboxes(ctx, "hitboxes");
 *        for (const auto& e : world.entities) ctx.SchematicOutline(HitboxOf(e), RED); }
 *
 *  Nesting is allowed, and a scope inside a closed one stays closed. */
class LayerScope {
 public:
  LayerScope(const Context& ctx, const std::string& id) : ctx_(ctx) { ctx_.PushLayer(id); }
  ~LayerScope() { ctx_.PopLayer(); }
  LayerScope(const LayerScope&) = delete;
  LayerScope& operator=(const LayerScope&) = delete;

  /** Whether drawing under this scope is drawn now, for a game drawing outside `Context`: raw
   *  raylib or SDL calls, or a renderer of its own, which the scope cannot drop by itself. A
   *  true answer also records that the layer drew, so the screenshot answer does not report it
   *  as empty. */
  bool Drawing() const { return ctx_._AllowDraw(); }

 private:
  const Context& ctx_;
};

// ---------------------------------------------------------------------------------------------
// Parameter helpers. A `describe` callback that wants a knob in the panel calls one of these on
// its own `out` document, one line per parameter, rather than hand-building the row's JSON. Each
// appends one well-formed row to `doc["parameters"]`, creating the array on first use — the same
// way `entities` is already an array a game pushes onto. `BuildDescribe` leaves `parameters` out
// of the answer entirely when a game never calls one of these, the way it already leaves out
// empty `events`. See docs/plans/archive/memory/vfx-project-type.md ("The parameter row") for the exact
// shape each type carries and what a `set_parameters` write for it looks like.
//
// `id` is the game's own name for the knob, opaque to Turret — the only thing a `set_parameters`
// write ever names, never a position in this array.
// ---------------------------------------------------------------------------------------------

inline void AddScalarParameter(json::Value& doc, const std::string& id, const std::string& label, double min,
                                double max, double value) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["label"] = label;
  row["type"] = "scalar";
  row["min"] = min;
  row["max"] = max;
  row["value"] = value;
  doc["parameters"].push_back(std::move(row));
}

/** `low`/`high` write back as `value`'s own `[low, high]` pair. */
inline void AddRangeParameter(json::Value& doc, const std::string& id, const std::string& label, double min,
                               double max, double low, double high) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["label"] = label;
  row["type"] = "range";
  row["min"] = min;
  row["max"] = max;
  json::Value value = json::Value::MakeArray();
  value.push_back(low);
  value.push_back(high);
  row["value"] = std::move(value);
  doc["parameters"].push_back(std::move(row));
}

/** `points` is `[t, v]` pairs, `t` over 0-1, oldest-first order is the game's own — Turret
 *  neither sorts nor validates it. */
inline void AddCurveParameter(json::Value& doc, const std::string& id, const std::string& label,
                               const std::vector<std::pair<double, double>>& points) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["label"] = label;
  row["type"] = "curve";
  json::Value pts = json::Value::MakeArray();
  for (const auto& [t, v] : points) {
    json::Value pt = json::Value::MakeArray();
    pt.push_back(t);
    pt.push_back(v);
    pts.push_back(std::move(pt));
  }
  row["points"] = std::move(pts);
  doc["parameters"].push_back(std::move(row));
}

/** `hex` is `#rrggbb`. Turret does not validate the string's shape; a game that writes something
 *  else is a game bug the panel will show verbatim. */
inline void AddColourParameter(json::Value& doc, const std::string& id, const std::string& label,
                                const std::string& hex) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["label"] = label;
  row["type"] = "colour";
  row["value"] = hex;
  doc["parameters"].push_back(std::move(row));
}

/** `stops` is `[t, "#rrggbb"]` pairs, `t` over 0-1. */
inline void AddRampParameter(json::Value& doc, const std::string& id, const std::string& label,
                              const std::vector<std::pair<double, std::string>>& stops) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["label"] = label;
  row["type"] = "ramp";
  json::Value arr = json::Value::MakeArray();
  for (const auto& [t, hex] : stops) {
    json::Value stop = json::Value::MakeArray();
    stop.push_back(t);
    stop.push_back(hex);
    arr.push_back(std::move(stop));
  }
  row["stops"] = std::move(arr);
  doc["parameters"].push_back(std::move(row));
}

inline void AddEnumParameter(json::Value& doc, const std::string& id, const std::string& label,
                              const std::vector<std::string>& options, const std::string& value) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["label"] = label;
  row["type"] = "enum";
  json::Value opts = json::Value::MakeArray();
  for (const auto& option : options) opts.push_back(option);
  row["options"] = std::move(opts);
  row["value"] = value;
  doc["parameters"].push_back(std::move(row));
}

inline void AddBoolParameter(json::Value& doc, const std::string& id, const std::string& label, bool value) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["label"] = label;
  row["type"] = "bool";
  row["value"] = value;
  doc["parameters"].push_back(std::move(row));
}

// ---------------------------------------------------------------------------------------------
// Layers. A `describe` callback declares one row per layer the same way it declares a
// parameter, and `draw` and `drawSchematic` name the same ids through `LayerScope`. Turret draws a toggle
// per row and writes the set back by id; what a layer contains is entirely the game's own, the
// same way a parameter's meaning is.
// ---------------------------------------------------------------------------------------------

/** Declares one layer: the id the drawing code names, the label a person reads, whether it
 *  starts on, and an optional group. Layers sharing a group behave as a radio set — turning one
 *  on turns the rest of the group off — and an empty group means an independent toggle. The
 *  default is honoured the first time an id is seen and ignored on every describe after that, so
 *  a toggle already made is not undone; `BuildDescribe` rewrites each row's `on` to the layer's
 *  live state before the answer leaves. */
inline void AddLayer(json::Value& doc, const std::string& id, const std::string& label, bool defaultOn = true,
                     const std::string& group = "") {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["label"] = label;
  row["on"] = defaultOn;
  if (!group.empty()) row["group"] = group;
  doc["layers"].push_back(std::move(row));
}

/** Seeds the context from the rows `AddLayer` just wrote and then rewrites each row's `on` to
 *  the live value. A describe document says what the layers are now, never what the game asked
 *  for the first time it declared them. */
inline void SyncLayers(Context& ctx, json::Value& doc) {
  const json::Value* declared = doc.find("layers");
  if (declared == nullptr) return;
  json::Value rebuilt = json::Value::MakeArray();
  for (const auto& row : declared->asArray()) {
    const json::Value* idField = row.find("id");
    if (idField == nullptr) continue;
    std::string id = idField->asString();
    if (id.empty()) continue;
    const json::Value* groupField = row.find("group");
    ctx._SeedLayer(id, row.find("on") ? row.find("on")->asBool(true) : true,
                   groupField ? groupField->asString() : "");
    json::Value out = row;
    out["on"] = ctx.LayerOn(id);
    rebuilt.push_back(std::move(out));
  }
  doc["layers"] = std::move(rebuilt);
}

// ---------------------------------------------------------------------------------------------
// Level editing (version 9). A `describe` callback that loaded its level from a file names that
// file and what in it a person may drag in Turret's Level tab. Each item names the id the draw
// callbacks mark it with, a JSON pointer into the file, and a shape: "rect" is four numbers
// [x, y, w, h] in world units, "point" two. Turret writes the dragged numbers into the file at the
// pointer and loads the game again; nothing here touches the world.
// ---------------------------------------------------------------------------------------------

/** Names the level file the edit block is about, relative to the game's working directory. */
inline void SetEditFile(json::Value& doc, const std::string& file) { doc["edit"]["file"] = file; }

/** The state document that loads another of this game's level files, for the Level tab's Load
 *  button. Every "@stem" string in it becomes the file's name without its extension, and every
 *  "@file" its path relative to the working directory. Applying it starts that level the way the
 *  game itself enters one: the file's own entities, and the party where the level puts it. A
 *  document that only swaps the geometry leaves the old level's entities standing in the new one. */
inline void SetEditLoadState(json::Value& doc, const json::Value& state) { doc["edit"]["load"] = state; }

/** Declares one draggable thing. `layer` is the edit layer whose pictures show its handle, and
 *  `snap` the world units a dragged value lands on, or 0 for whole units. */
inline void AddEditItem(json::Value& doc, const std::string& id, const std::string& pointer, const std::string& shape,
                        const std::string& layer = "", double snap = 0.0) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["path"] = pointer;
  row["shape"] = shape;
  if (!layer.empty()) row["layer"] = layer;
  if (snap > 0.0) row["snap"] = snap;
  doc["edit"]["items"].push_back(std::move(row));
}

/** Declares one scalar from 0 to 1 that slides along the line from `from` to `to`, in world
 *  units: where a door sits on the wall it is cut in. `fallback`, when 0 or more, is the value the
 *  game takes when the file leaves the field out, so the handle starts where the thing is. */
inline void AddEditAlongItem(json::Value& doc, const std::string& id, const std::string& pointer, Vec2 from, Vec2 to,
                             const std::string& layer = "", double fallback = -1.0) {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["path"] = pointer;
  row["shape"] = "along";
  json::Value track = json::Value::MakeArray();
  track.push_back(static_cast<double>(from.x));
  track.push_back(static_cast<double>(from.y));
  track.push_back(static_cast<double>(to.x));
  track.push_back(static_cast<double>(to.y));
  row["track"] = std::move(track);
  if (!layer.empty()) row["layer"] = layer;
  if (fallback >= 0.0) row["fallback"] = fallback;
  doc["edit"]["items"].push_back(std::move(row));
}

/** Declares values under `pointer` that have no position, such as a count and a kind, edited in
 *  the Level tab's form. Build `fields` with the parameter helpers — AddScalarParameter,
 *  AddEnumParameter and the rest — on a document of its own; each row's id is the key under
 *  `pointer` it writes. */
inline void AddEditFormItem(json::Value& doc, const std::string& id, const std::string& pointer, json::Value& fields,
                            const std::string& layer = "") {
  json::Value row = json::Value::MakeObject();
  row["id"] = id;
  row["path"] = pointer;
  row["fields"] = fields["parameters"];
  if (!layer.empty()) row["layer"] = layer;
  doc["edit"]["items"].push_back(std::move(row));
}

/** Offers one element a person can add: `templateRow` is inserted into the array at `pointer`,
 *  with every "@point" string in it replaced by the [x, y] where it was dropped. */
inline void AddEditPalette(json::Value& doc, const std::string& label, const std::string& pointer,
                           const json::Value& templateRow, const std::string& layer = "") {
  json::Value row = json::Value::MakeObject();
  row["label"] = label;
  row["path"] = pointer;
  row["template"] = templateRow;
  if (!layer.empty()) row["layer"] = layer;
  doc["edit"]["palette"].push_back(std::move(row));
}

inline ApplyResult DefaultApplyState(Context&, const json::Value&) {
  return ApplyResult::Refused("unimplemented", "this game has no apply_state callback");
}

inline ApplyResult DefaultSetParameters(Context&, const json::Value&) {
  return ApplyResult::Refused("unimplemented", "this game has no set_parameters callback");
}

inline SaveResult DefaultSaveState(Context&) {
  return SaveResult::Refused("unimplemented", "this game has no save_state callback");
}

inline ApplyResult DefaultLoadState(Context&, const std::vector<uint8_t>&) {
  return ApplyResult::Refused("unimplemented", "this game has no load_state callback");
}

/** One tick's worth of sound for `GameCallbacks::mixAudio` to fill: `frames` frames of
 *  `channels` interleaved samples, each from -1 to 1, zeroed before the call. */
struct AudioBlock {
  int sampleRate = 48000;
  int channels = 2;
  int frames = 0;
  std::vector<float> samples;
};

/** The callbacks a game author writes, returned from `TurretGame()`. Every field but
 *  `applyState` is required — the harness calls whatever is set and nothing else. */
struct GameCallbacks {
  int width = 640;
  int height = 480;
  std::string title = "Turret Game";
  /** What this binary is: `"game"` (the default) or `"effect"`. Answered by `init` as
   *  `project_type`. A harness older than this field answers without it, and Turret reads the
   *  absence as `"game"`. See docs/plans/archive/memory/vfx-project-type.md. */
  std::string projectType = "game";
  /** How much game time one `step` covers, in seconds. The harness reads no clock of its own —
   *  this is what `advancePresentation` is handed as `dt`, and nothing else. */
  float stepSeconds = 1.0f / 60.0f;

  /** Creates the world at tick 0 from a seed. */
  std::function<void(Context&, uint64_t seed)> build;
  /** Advances exactly one fixed timestep given the input in force. Draws nothing. */
  std::function<void(Context&, const Input&)> step;
  /** Advances whatever the presentation owns and the simulation does not — particles, trails,
   *  screen shake, anything a pure draw callback cannot move on its own. Called once per tick,
   *  immediately after `step`, with `stepSeconds` as `dt`. This is what keeps drawing pure:
   *  two screenshots at the same tick still produce the same bytes, and a filmstrip still shows
   *  an effect evolving tick by tick. The playable client calls it on its own clock at render
   *  rate instead. Optional. */
  std::function<void(Context&, float dt)> advancePresentation;
  /** Renders the game as a player sees it, framed and scaled per `Framing`. Reads the world,
   *  changes nothing. */
  std::function<void(Context&, const Framing&)> draw;
  /** Renders the same world for a model to read: bold outlines, arrows, names. */
  std::function<void(Context&, const Framing&)> drawSchematic;
  /** Fills a describe document's `phase`, `entities`, and optional `notes`. `notes` is an
   *  array of strings; a lone string is read as one note, and anything Turret cannot read as
   *  text is replaced by a note saying what arrived instead. The harness adds `tick` and drains
   *  `events` itself. */
  std::function<void(Context&, json::Value&)> describe;
  /** Puts the world into a given state document, or refuses it. The one callback a game may
   *  omit — the default refuses, naming the omission, so an author who has not written one yet
   *  still has a game that runs. */
  std::function<ApplyResult(Context&, const json::Value&)> applyState = DefaultApplyState;
  /** Turns one or more declared parameters by id — `{ "<id>": <value>, … }`, the shape
   *  `AddScalarParameter` and the rest write into a describe document's own `parameters` array.
   *  Never resets the run: the tick before and after the call is the same, which is the whole
   *  reason this is a separate callback from `applyState` rather than folded into it. Optional,
   *  the same way `applyState` is: the default refuses, naming the omission. */
  std::function<ApplyResult(Context&, const json::Value&)> setParameters = DefaultSetParameters;
  /** Captures the world as bytes only this game can interpret — exact by construction, unlike a
   *  state document's own vocabulary, which is only ever as faithful as `applyState` makes it.
   *  The tick these bytes were captured at travels separately; this returns the game's own
   *  payload alone. Optional alongside `applyState`: the default refuses, naming the omission,
   *  so a restore that cannot get a snapshot falls back to replaying a checkpoint instead of
   *  failing outright. */
  std::function<SaveResult(Context&)> saveState = DefaultSaveState;
  /** Restores the world from bytes this same game's own `saveState` produced earlier in this
   *  process's life — never bytes from another game, another version of this one, or written by
   *  hand. Optional, the same way `saveState` is; the default refuses. */
  std::function<ApplyResult(Context&, const std::vector<uint8_t>&)> loadState = DefaultLoadState;
  /** Called once per incoming packet, immediately before the `Step` call for the tick it
   *  arrives on — see docs/plans/archive/memory/game-mode.md ("Peers") for how Turret decides which
   *  tick that is. Optional: a game that never calls `Context::SendPacket`, or runs with only
   *  one peer, needs none of this. */
  std::function<void(Context&, const std::string& from, const std::vector<uint8_t>& data)> onPacket;
  /** Whether the peers of one session are expected to hold the same world. `true` (the default)
   *  is a lockstep game: Turret compares every peer's entities by id and position after each
   *  round and reports the first disagreement. A client/server game whose clients predict, or
   *  whose peers each see only their own view, sets this `false`; Turret then skips the
   *  comparison for the load and says so wherever it would have reported one. Answered by `init`
   *  as `peers_expected_to_agree`; a harness older than version 3 answers without it, and Turret
   *  reads the absence as `true`. */
  bool peersExpectedToAgree = true;
  /** The view Turret draws pictures in until the model names one: `"schematic"` (the default) or
   *  `"normal"`. Answered by `init` as `default_view`. Version 8. */
  std::string defaultView = "schematic";
  /** The sample rate and channel count `mixAudio` is handed. Version 8. */
  int audioSampleRate = 48000;
  int audioChannels = 2;
  /** Fills one tick's sound from the game's own mixer, for a `step` that asked for audio. Called
   *  once per tick, after `step` and `advancePresentation`, with a block sized to one
   *  `stepSeconds` at `audioSampleRate`. Mixes what the tick just played and reads the world
   *  only. Optional; a game without it answers `audio` with a refusal. Version 8. */
  std::function<void(Context&, AudioBlock&)> mixAudio;
  /** The input for the tick about to run during live play, read from the OS through the game's
   *  own renderer library: the keys its keyboard build reads, and the mouse in the window's own
   *  pixels, which the harness turns into the frame's before `step` reads it. The one place a
   *  game may read the OS's input, because what it returns is logged tick by tick and replays
   *  without it. `LivePressed()` holds the presses since the last
   *  call, for a tap too short to be held when the tick reads. Optional; without it `play_start`
   *  is refused. Version 8. */
  std::function<Input(Context&)> liveInput;
  /** Reads the named data files again, paths relative to the working directory, and rebuilds
   *  whatever was built from them in place, keeping the run: the tick, and every entity by id, the
   *  way applyState keeps them. Turret's Level tab calls it after it writes a level file, so an
   *  edit shows in milliseconds and in the middle of live play. Refuse a file that no longer
   *  checks out, leaving the world as it was. Optional; without it Turret loads the binary again
   *  with the run put back instead. Version 10. */
  std::function<ApplyResult(Context&, const std::vector<std::string>& paths)> reloadAssets;
  /** Reads a level file the run has not loaded, path relative to the working directory, writes its
   *  edit block into `out` with the same helpers `describe` uses, and holds what `drawDocument`
   *  needs to draw its plan. Touches nothing in the run: not its world, its tick or its random
   *  state. Turret's Level tab calls it to show any level file with handles on it without loading
   *  it. Optional, together with `drawDocument`. Version 11. */
  std::function<ApplyResult(Context&, const std::string& path, json::Value& out)> describeDocument;
  /** Draws the plan of the file `describeDocument` last read, in the schematic's own style, and
   *  marks its editable things with the ids its edit block gives them. Version 11. */
  std::function<void(Context&, const Framing&)> drawDocument;
};

}  // namespace turret

/** A game defines this in the one .cpp file that includes this header. The harness calls it
 *  once at startup. */
turret::GameCallbacks TurretGame();

// ---------------------------------------------------------------------------------------------
// The harness itself: the control socket, the frame format, and the idle loop. Everything past
// this point is internal to the harness; a game does not call into it directly.
// ---------------------------------------------------------------------------------------------
namespace turret {
namespace detail {

constexpr const char* kHarnessVersion = "14";

// The one place the two platforms' control channels differ. POSIX speaks a real AF_UNIX socket;
// win32 speaks a named pipe (see the include-block comment above `namespace turret` for why).
// Everything past this block — ReadFrame, WriteFrame, HandleFrame, the idle loop in main() — is
// written once against these names and neither platform's branch leaks past it.
#ifdef _WIN32
using Socket = HANDLE;
// Not constexpr: INVALID_HANDLE_VALUE is `(HANDLE)(LONG_PTR)-1`, an integer-to-pointer cast MSVC
// (correctly) refuses to treat as a core constant expression. A plain inline const still gives
// every translation unit the same one definition, which is all `kNoSocket == kNoSocket` needs.
inline const Socket kNoSocket = INVALID_HANDLE_VALUE;
/** Shaped like `pollfd`/`WSAPOLLFD` on purpose, field names included, so the idle loop below
 *  reads identically on both platforms even though nothing here is a real poll(). */
struct PollFd {
  Socket fd = kNoSocket;
  short events = 0;
  short revents = 0;
};
inline void CloseSocket(Socket fd) { ::CloseHandle(fd); }
inline bool StartSockets() { return true; }  // nothing to start: no Winsock on this path

/** Polls one pipe handle for readability, close enough to poll()'s shape for the idle loop above
 *  to stay platform-agnostic: >0 means `revents` is worth reading, 0 means the timeout elapsed
 *  with nothing to report. `PeekNamedPipe` never blocks, so the wait a real `poll()` does in the
 *  kernel is a series of one-millisecond waits with a peek after each, up to the idle loop's own
 *  short timeout (10ms), which keeps the window pumping between checks, per "Idling is pumping"
 *  (docs/plans/archive/memory/game-mode.md). A plain `Sleep` would round every wait up to the
 *  scheduler's tick, about 15.6ms, and a command arriving while the game slept would wait out the
 *  rest of it: a client that sends its next command the moment it reads an answer paid most of a
 *  tick per command. The high-resolution timer wakes on the millisecond without changing the
 *  machine's timer resolution; where Windows has none (before 10 1803) `Sleep(1)` stands in, and
 *  the wait is what it always was. The deadline is read from `steady_clock` because
 *  `GetTickCount64` itself only advances once a scheduler tick. A failed peek means the other end
 *  is gone. */
inline int PollOne(PollFd* fd, int timeoutMs) {
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
  static HANDLE timer = ::CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(0, timeoutMs));
  for (;;) {
    DWORD bytesAvailable = 0;
    if (!::PeekNamedPipe(fd->fd, nullptr, 0, nullptr, &bytesAvailable, nullptr)) {
      fd->revents = POLLHUP;
      return 1;
    }
    if (bytesAvailable > 0) {
      fd->revents = POLLIN;
      return 1;
    }
    if (std::chrono::steady_clock::now() >= deadline) break;
    LARGE_INTEGER due;
    due.QuadPart = -10000;  // one millisecond, relative, in 100ns units
    if (timer != nullptr && ::SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
      ::WaitForSingleObject(timer, INFINITE);
    } else {
      ::Sleep(1);
    }
  }
  fd->revents = 0;
  return 0;
}

inline bool ReadExact(Socket fd, void* buf, size_t n) {
  char* p = static_cast<char*>(buf);
  size_t got = 0;
  while (got < n) {
    DWORD r = 0;
    if (!::ReadFile(fd, p + got, static_cast<DWORD>(n - got), &r, nullptr) || r == 0) return false;
    got += r;
  }
  return true;
}

inline bool WriteExact(Socket fd, const void* buf, size_t n) {
  const char* p = static_cast<const char*>(buf);
  size_t sent = 0;
  while (sent < n) {
    DWORD w = 0;
    if (!::WriteFile(fd, p + sent, static_cast<DWORD>(n - sent), &w, nullptr)) return false;
    sent += w;
  }
  return true;
}
#else
using Socket = int;
inline constexpr Socket kNoSocket = -1;
using PollFd = pollfd;
inline void CloseSocket(Socket fd) { ::close(fd); }
inline bool SocketInterrupted() { return errno == EINTR; }
inline int PollOne(PollFd* fd, int timeoutMs) { return ::poll(fd, 1, timeoutMs); }
inline bool StartSockets() { return true; }

inline bool ReadExact(Socket fd, void* buf, size_t n) {
  char* p = static_cast<char*>(buf);
  size_t got = 0;
  while (got < n) {
    const long long r = ::recv(fd, p + got, static_cast<int>(n - got), 0);
    if (r == 0) return false;
    if (r < 0) {
      if (SocketInterrupted()) continue;
      return false;
    }
    got += static_cast<size_t>(r);
  }
  return true;
}

inline bool WriteExact(Socket fd, const void* buf, size_t n) {
  const char* p = static_cast<const char*>(buf);
  size_t sent = 0;
  while (sent < n) {
    const long long w = ::send(fd, p + sent, static_cast<int>(n - sent), 0);
    if (w < 0) {
      if (SocketInterrupted()) continue;
      return false;
    }
    sent += static_cast<size_t>(w);
  }
  return true;
}
#endif

// A frame is a length-prefixed JSON header followed by an optional length-prefixed binary body
// (a screenshot's bytes are why the body exists — they must not travel as base64 text inside
// the header). Both lengths are 4-byte native-endian unsigned integers; the harness only ever
// runs on the same machine that spawned it, so this is a same-machine format like the rest of
// the protocol's determinism promises.

inline bool ReadFrame(Socket fd, std::string& header, std::vector<unsigned char>& body) {
  uint32_t headerLen = 0;
  if (!ReadExact(fd, &headerLen, 4)) return false;
  header.resize(headerLen);
  if (headerLen > 0 && !ReadExact(fd, header.data(), headerLen)) return false;
  uint32_t bodyLen = 0;
  if (!ReadExact(fd, &bodyLen, 4)) return false;
  body.resize(bodyLen);
  if (bodyLen > 0 && !ReadExact(fd, body.data(), bodyLen)) return false;
  return true;
}

inline bool WriteFrame(Socket fd, const std::string& header, const unsigned char* bodyData, size_t bodyLen) {
  // A pipe is fully buffered, so anything the game printed while answering would otherwise sit
  // here until the buffer filled. Flushed first so it reaches Turret ahead of the answer, and
  // Turret can hand it to the model in the same tool result.
  std::cout.flush();
  std::fflush(stdout);
  std::fflush(stderr);
  uint32_t headerLen = static_cast<uint32_t>(header.size());
  if (!WriteExact(fd, &headerLen, 4)) return false;
  if (headerLen > 0 && !WriteExact(fd, header.data(), headerLen)) return false;
  uint32_t bLen = static_cast<uint32_t>(bodyLen);
  if (!WriteExact(fd, &bLen, 4)) return false;
  if (bLen > 0 && !WriteExact(fd, bodyData, bLen)) return false;
  return true;
}

inline bool WriteFrame(Socket fd, const std::string& header) { return WriteFrame(fd, header, nullptr, 0); }

/** Where to put the game's window, parsed from an optional third argv entry: "x,y" or
 *  "x,y,width,height", comma-separated integers, no spaces. Absent or unparseable, the harness
 *  places nothing and the window opens wherever the OS drops it. */
struct WindowPlacement {
  bool hasPosition = false;
  int x = 0, y = 0;
  bool hasSize = false;
  int width = 0, height = 0;
};

inline WindowPlacement ParseWindowPlacement(const std::string& text) {
  WindowPlacement placement;
  std::vector<int> parts;
  size_t start = 0;
  while (start <= text.size()) {
    size_t comma = text.find(',', start);
    std::string token = text.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    try {
      parts.push_back(std::stoi(token));
    } catch (const std::exception&) {
      return WindowPlacement{};  // unparseable: place nothing
    }
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  if (parts.size() == 2 || parts.size() == 4) {
    placement.hasPosition = true;
    placement.x = parts[0];
    placement.y = parts[1];
  }
  if (parts.size() == 4) {
    placement.hasSize = true;
    placement.width = parts[2];
    placement.height = parts[3];
  }
  return placement;
}

/** What the process exits with when this desktop session has no display to open a window on —
 *  distinct from the `1` every other startup failure uses, so a caller can tell "nowhere to run
 *  this" from "it tried and failed". Above every exit code a shell reserves, and below 126. */
inline constexpr int kNoDisplayExit = 70;

/** Places the window where Turret asked, held inside the usable area of the display the request
 *  lands on, and shrinks a window bigger than that area until it fits, keeping its shape. A
 *  window opens at the game's frame size, which can be bigger than any screen; the window's
 *  frames are drawn scaled to whatever size it ends up (`WindowFraming`). Turret computes the
 *  position from its own window's bounds and does not know how big this window is; the harness
 *  does, so the clamp belongs here. An overlapping window beats one off the side of the screen.
 *  A window that needed shrinking and was given no position is centred on its display. Call
 *  after the window exists. */
inline void ApplyWindowPlacement(const WindowPlacement& placement) {
  if (placement.hasSize) backend::SetWindowSize(placement.width, placement.height);
  int windowX = 0, windowY = 0, windowWidth = 0, windowHeight = 0;
  backend::WindowPosition(windowX, windowY);
  backend::WindowSize(windowWidth, windowHeight);
  if (windowWidth <= 0 || windowHeight <= 0) return;
  int boundsX = 0, boundsY = 0, boundsWidth = 0, boundsHeight = 0;
  backend::WorkAreaContaining(placement.hasPosition ? placement.x : windowX + windowWidth / 2,
                              placement.hasPosition ? placement.y : windowY + windowHeight / 2,
                              boundsX, boundsY, boundsWidth, boundsHeight);
  bool shrunk = false;
  if (boundsWidth > 0 && boundsHeight > 0) {
    const double scale = std::min({1.0, static_cast<double>(boundsWidth) / windowWidth,
                                   static_cast<double>(boundsHeight) / windowHeight});
    if (scale < 1.0) {
      backend::SetWindowSize(std::max(1, static_cast<int>(std::floor(windowWidth * scale))),
                             std::max(1, static_cast<int>(std::floor(windowHeight * scale))));
      backend::WindowSize(windowWidth, windowHeight);
      shrunk = true;
    }
  }
  if (!placement.hasPosition) {
    if (shrunk) {
      backend::SetWindowPosition(boundsX + (boundsWidth - windowWidth) / 2,
                                 boundsY + (boundsHeight - windowHeight) / 2);
    }
    return;
  }
  int right = boundsX + boundsWidth - windowWidth;
  int bottom = boundsY + boundsHeight - windowHeight;
  int x = std::max(boundsX, std::min(placement.x, right));
  int y = std::max(boundsY, std::min(placement.y, bottom));
  backend::SetWindowPosition(x, y);
}

/** Connects to Turret's control channel at `path`, retrying briefly — Turret listens before it
 *  spawns the game, so this should succeed on the first or second try, but a slow scheduler is
 *  not a protocol failure. POSIX speaks AF_UNIX; win32 speaks a named pipe (see the include-block
 *  comment above `namespace turret` for why there are two). */
#ifdef _WIN32
inline Socket ConnectControlSocket(const std::string& path) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    Socket h = ::CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != kNoSocket) return h;
    // ERROR_PIPE_BUSY (every instance already taken) and ERROR_FILE_NOT_FOUND (Turret hasn't
    // called listen() yet) read the same way here: not open yet, worth another try. This harness
    // never shares a pipe with another client, so busy is transient by construction.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return kNoSocket;
}
#else
inline Socket ConnectControlSocket(const std::string& path) {
  if (!StartSockets()) return kNoSocket;
  for (int attempt = 0; attempt < 200; ++attempt) {
    Socket fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == kNoSocket) return kNoSocket;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
      return fd;
    }
    CloseSocket(fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return kNoSocket;
}
#endif

/** Builds a describe document from the game's callback plus the harness's own tick and drained
 *  events. `events` and `notes` are left out when empty, matching the memory's "optional". */
inline json::Value BuildDescribe(Context& ctx, const GameCallbacks& game) {
  json::Value doc = json::Value::MakeObject();
  if (game.describe) game.describe(ctx, doc);
  SyncLayers(ctx, doc);
  doc["tick"] = ctx.tick();
  if (doc.find("phase") == nullptr) doc["phase"] = "playing";
  if (doc.find("entities") == nullptr) doc["entities"] = json::Value::MakeArray();
  auto events = ctx._DrainEvents();
  if (!events.empty()) {
    json::Value arr = json::Value::MakeArray();
    for (auto& e : events) arr.push_back(std::move(e));
    doc["events"] = std::move(arr);
  }
  return doc;
}

/** One trace row: the same document-building step as `BuildDescribe`, minus the event drain —
 *  draining belongs to the single describe the answer already carries, and a `step` that traces
 *  N ticks must not drain the queue N times before that describe gets to it. See
 *  docs/plans/archive/memory/game-speculation.md ("BuildDescribe drains the event queue"). */
inline json::Value BuildTraceRow(Context& ctx, const GameCallbacks& game) {
  json::Value doc = json::Value::MakeObject();
  if (game.describe) game.describe(ctx, doc);
  SyncLayers(ctx, doc);
  doc["tick"] = ctx.tick();
  if (doc.find("phase") == nullptr) doc["phase"] = "playing";
  if (doc.find("entities") == nullptr) doc["entities"] = json::Value::MakeArray();
  return doc;
}

/** A trace row cut down to the paths Turret asked for, in the describe document's own shape:
 *  `<entity id>.<field>` keeps that field on that entity, split at the first dot, and a path with
 *  no dot keeps that top-level field. Without this every row carries the whole world, and a few
 *  hundred ticks of a large one overrun the answer's header cap. */
inline json::Value FilterTraceRow(const json::Value& full, const std::vector<std::string>& paths) {
  json::Value row = json::Value::MakeObject();
  json::Value entities = json::Value::MakeArray();
  std::map<std::string, std::vector<std::string>> fieldsById;
  for (const auto& path : paths) {
    size_t dot = path.find('.');
    if (dot == std::string::npos) {
      if (const json::Value* v = full.find(path)) row[path] = *v;
    } else {
      fieldsById[path.substr(0, dot)].push_back(path.substr(dot + 1));
    }
  }
  if (const json::Value* all = full.find("entities")) {
    for (const auto& entity : all->asArray()) {
      const json::Value* idField = entity.find("id");
      if (idField == nullptr) continue;
      auto wanted = fieldsById.find(idField->asString());
      if (wanted == fieldsById.end()) continue;
      json::Value kept = json::Value::MakeObject();
      kept["id"] = *idField;
      for (const auto& field : wanted->second) {
        if (const json::Value* v = entity.find(field)) kept[field] = *v;
      }
      entities.push_back(std::move(kept));
    }
  }
  row["entities"] = std::move(entities);
  return row;
}

/** The whole logical frame at the frame's own size: a screenshot's default framing when a
 *  request names no region or size. */
inline Framing FullFrame(const GameCallbacks& game) {
  return Framing{0.0, 0.0, 1.0, 1.0, game.width, game.height};
}

/** Shrinks a size nobody asked for to what a model reads at full resolution. Claude's vision
 *  input scales an image down past 1568 pixels on its long edge or about 1.15 megapixels, so a
 *  bigger default costs encoding and transfer only to be thrown away, and shrinks the labels the
 *  game drew at its own pixel sizes along with it. Drawn at this size, a label keeps the size
 *  the game asked for. A size the request names is never touched. */
inline void FitDefaultSizeToModel(Framing& framing) {
  constexpr double kMaxLongEdge = 1568.0;
  constexpr double kMaxPixels = 1150000.0;
  const double w = framing.width, h = framing.height;
  if (w <= 0 || h <= 0) return;
  const double scale = std::min({1.0, kMaxLongEdge / std::max(w, h), std::sqrt(kMaxPixels / (w * h))});
  if (scale >= 1.0) return;
  framing.width = static_cast<int>(std::floor(w * scale));
  framing.height = static_cast<int>(std::floor(h * scale));
}

/** A camera that maps `framing`'s region of the game's own logical coordinate space onto the
 *  full render target, uniformly scaled (never stretched) and centered — a region narrower or
 *  shorter than its target leaves a margin rather than distorting the game's own drawing. */
inline Camera CameraForFraming(const GameCallbacks& game, const Framing& framing) {
  float regionWidthPx = static_cast<float>((framing.x1 - framing.x0) * game.width);
  float regionHeightPx = static_cast<float>((framing.y1 - framing.y0) * game.height);
  Camera camera{};
  camera.target = {static_cast<float>(framing.x0 * game.width) + regionWidthPx / 2.0f,
                   static_cast<float>(framing.y0 * game.height) + regionHeightPx / 2.0f};
  camera.offset = {framing.width / 2.0f, framing.height / 2.0f};
  float zoomX = regionWidthPx > 0 ? framing.width / regionWidthPx : 1.0f;
  float zoomY = regionHeightPx > 0 ? framing.height / regionHeightPx : 1.0f;
  camera.zoom = std::min(zoomX, zoomY);
  return camera;
}

/** The whole logical frame at the window's own size, which is smaller than the frame when
 *  `ApplyWindowPlacement` shrank the window to fit its display. */
inline Framing WindowFraming(const GameCallbacks& game) {
  Framing framing = FullFrame(game);
  int width = 0, height = 0;
  backend::WindowSize(width, height);
  if (width > 0 && height > 0) {
    framing.width = width;
    framing.height = height;
  }
  return framing;
}

/** Draws the whole frame into the window, scaled to fit it. `schematic` picks the schematic view
 *  over the normal one. */
inline void DrawWindowFrame(Context& ctx, const GameCallbacks& game, bool schematic) {
  backend::BeginWindowFrame();
  ctx._BeginDrawPass();
  Framing framing = WindowFraming(game);
  Camera camera = CameraForFraming(game, framing);
  ctx._SetCamera(camera);
  backend::BeginGameCamera(camera);
  if (schematic) {
    if (game.drawSchematic) game.drawSchematic(ctx, framing);
  } else {
    if (game.draw) game.draw(ctx, framing);
  }
  backend::EndGameCamera();
  backend::EndWindowFrame();
}

inline void DrawCurrentFrame(Context& ctx, const GameCallbacks& game) {
  DrawWindowFrame(ctx, game, ctx.renderMode() == RenderMode::Schematic);
}

/** Turns a pointer `liveInput` read in the window's own pixels into the frame's, through the
 *  camera the window is drawn with, so `step` and the logged segments read it the way they read
 *  a model's pointer. */
inline void PointerFromWindow(const GameCallbacks& game, Pointer& pointer) {
  if (!pointer.present) return;
  Vec2 frame = CameraForFraming(game, WindowFraming(game)).ToWorld(Vec2{pointer.x, pointer.y});
  pointer.x = frame.x;
  pointer.y = frame.y;
}


/** What a screenshot capture produced: the picture's bytes (a PNG, or raw RGBA when the capture
 *  asked for it), the framing actually used (region and size may have been clamped from what was
 *  asked for), and whether the frame held nothing. */
struct ScreenshotResult {
  bool ok = false;
  std::vector<unsigned char> bytes;
  Framing framing;
  bool empty = false;
  /** Output pixels per unit of the game's own logical coordinate space, after cropping and
   *  scaling — what a burnt-in scale bar used to say, reported as a number instead. */
  float scale = 1.0f;
  /** Each entity the draw pass marked, as [x0, y0, x1, y1] in the PNG's own pixels, clipped to
   *  it. A box wholly outside the picture is left out. */
  struct Box {
    std::string id;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  };
  std::vector<Box> marks;
};

/** Captures the current render mode as a PNG, or as raw RGBA when `png` is false, cropped and scaled to `requested` and drawn into
 *  an off-screen surface rather than read from the window's own front buffer.
 *
 *  A region re-renders rather than crops the already-drawn frame: the camera reframes the
 *  game's own draw call onto the requested rectangle, so a close-in region is drawn at full
 *  detail rather than blown up from a handful of source pixels.
 *
 *  Nothing but the game's own drawing reaches the pixels. The tick and the scale travel as
 *  numbers in the screenshot's own answer, where a small crop cannot crowd them out. */
inline ScreenshotResult CaptureScreenshot(Context& ctx, const GameCallbacks& game, const Framing& requested,
                                          bool document, bool png) {
  ScreenshotResult result;
  result.framing = requested;
  if (!backend::BeginOffscreenFrame(requested.width, requested.height)) return result;

  Camera camera = CameraForFraming(game, requested);
  bool schematic = ctx.renderMode() == RenderMode::Schematic;
  result.scale = camera.zoom;

  ctx._BeginDrawPass();
  ctx._SetCamera(camera);
  backend::BeginGameCamera(camera);
  if (document) {
    game.drawDocument(ctx, requested);
  } else if (schematic) {
    if (game.drawSchematic) game.drawSchematic(ctx, requested);
  } else {
    if (game.draw) game.draw(ctx, requested);
  }
  backend::EndGameCamera();

  int width = requested.width;
  int height = requested.height;
  result.ok = backend::EndOffscreenFrame(result.bytes, result.empty, kBackground, width, height, png);
  result.framing.width = width;
  result.framing.height = height;
  // The backend may hand back a smaller image than it drew into; the boxes follow it.
  const double sx = requested.width > 0 ? static_cast<double>(width) / requested.width : 1.0;
  const double sy = requested.height > 0 ? static_cast<double>(height) / requested.height : 1.0;
  for (const auto& mark : ctx._Marks()) {
    const double left = std::max(0.0, mark.box.x * sx);
    const double top = std::max(0.0, mark.box.y * sy);
    const double right = std::min(static_cast<double>(width), (mark.box.x + mark.box.w) * sx);
    const double bottom = std::min(static_cast<double>(height), (mark.box.y + mark.box.h) * sy);
    if (right <= left || bottom <= top) continue;
    result.marks.push_back(ScreenshotResult::Box{mark.id, static_cast<int>(std::floor(left)), static_cast<int>(std::floor(top)),
                                                 static_cast<int>(std::ceil(right)), static_cast<int>(std::ceil(bottom))});
  }
  return result;
}

/** A packet's bytes travel as base64 text inside the JSON header, not as the frame's binary
 *  body — that body is reserved for a screenshot's own bytes (see the memory's "The wire,
 *  concretely"), and a packet a game sends over `SendPacket` is small enough that base64's
 *  overhead costs nothing worth a second body slot. */
inline std::string Base64Encode(const std::vector<uint8_t>& data) {
  static const char* kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((data.size() + 2) / 3) * 4);
  size_t i = 0;
  for (; i + 3 <= data.size(); i += 3) {
    uint32_t n = (static_cast<uint32_t>(data[i]) << 16) | (static_cast<uint32_t>(data[i + 1]) << 8) | data[i + 2];
    out += kAlphabet[(n >> 18) & 0x3F];
    out += kAlphabet[(n >> 12) & 0x3F];
    out += kAlphabet[(n >> 6) & 0x3F];
    out += kAlphabet[n & 0x3F];
  }
  size_t remaining = data.size() - i;
  if (remaining == 1) {
    uint32_t n = static_cast<uint32_t>(data[i]) << 16;
    out += kAlphabet[(n >> 18) & 0x3F];
    out += kAlphabet[(n >> 12) & 0x3F];
    out += "==";
  } else if (remaining == 2) {
    uint32_t n = (static_cast<uint32_t>(data[i]) << 16) | (static_cast<uint32_t>(data[i + 1]) << 8);
    out += kAlphabet[(n >> 18) & 0x3F];
    out += kAlphabet[(n >> 12) & 0x3F];
    out += kAlphabet[(n >> 6) & 0x3F];
    out += '=';
  }
  return out;
}

/** `save_state`/`load_state`'s own bytes carry the tick they were taken at as an 8-byte
 *  native-endian prefix ahead of the game's own opaque payload — the tick is the harness's own
 *  bookkeeping, never something a game's `saveState`/`loadState` callbacks read or write
 *  themselves. Same-machine, like the rest of this format's determinism promises. */
inline std::vector<uint8_t> PrefixTick(uint64_t tick, const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> out(sizeof(tick) + payload.size());
  std::memcpy(out.data(), &tick, sizeof(tick));
  if (!payload.empty()) std::memcpy(out.data() + sizeof(tick), payload.data(), payload.size());
  return out;
}

inline bool SplitTickPrefix(const std::vector<uint8_t>& bytes, uint64_t& tick, std::vector<uint8_t>& payload) {
  if (bytes.size() < sizeof(tick)) return false;
  std::memcpy(&tick, bytes.data(), sizeof(tick));
  payload.assign(bytes.begin() + sizeof(tick), bytes.end());
  return true;
}

inline std::vector<uint8_t> Base64Decode(const std::string& text) {
  auto value = [](unsigned char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  std::vector<uint8_t> out;
  out.reserve((text.size() / 4) * 3);
  int bits = 0, nbits = 0;
  for (unsigned char c : text) {
    int v = value(c);
    if (v < 0) continue;  // padding ('=') or whitespace: skipped rather than rejected
    bits = (bits << 6) | v;
    nbits += 6;
    if (nbits >= 8) {
      nbits -= 8;
      out.push_back(static_cast<uint8_t>((bits >> nbits) & 0xFF));
    }
  }
  return out;
}

inline double MillisecondsSince(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

/** Makes the app a regular, frontmost one for live play, or an accessory one again afterwards.
 *  A hidden binary starts as a background app (see `OpenWindow` in each backend), and macOS
 *  gives a background app's window no keyboard. Goes through the Objective-C runtime so neither
 *  backend needs a framework it does not already link. Does nothing elsewhere. */
inline void SetAppForeground(bool foreground) {
#if defined(__APPLE__)
  id app = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(reinterpret_cast<id>(objc_getClass("NSApplication")),
                                                           sel_registerName("sharedApplication"));
  if (app == nullptr) return;
  // NSApplicationActivationPolicyRegular is 0, NSApplicationActivationPolicyAccessory 1.
  reinterpret_cast<void (*)(id, SEL, long)>(objc_msgSend)(app, sel_registerName("setActivationPolicy:"), foreground ? 0 : 1);
  if (foreground) reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(app, sel_registerName("activateIgnoringOtherApps:"), YES);
#else
  (void)foreground;
#endif
}

/** The sound `step` commands with `audio` set have mixed and `audio` has not yet collected. */
struct AudioCapture {
  std::vector<int16_t> pcm;
  int sampleRate = 0;
  int channels = 0;
  uint64_t firstTick = 0;
  int ticks = 0;
  /** The fraction of a frame the last tick's block rounded away, carried so a long capture's
   *  length matches its tick count. */
  double carry = 0.0;
  bool truncated = false;
};

inline AudioCapture& Audio() {
  static AudioCapture capture;
  return capture;
}

/** The most sound one capture holds, so a long step cannot exhaust memory. */
constexpr int kMaxAudioSeconds = 120;

/** Mixes the tick just run into the capture. A tick that does not follow the last one captured
 *  starts the capture again, so `first_tick` and `ticks` always describe contiguous sound. */
inline void MixTick(Context& ctx, const GameCallbacks& game) {
  if (!game.mixAudio) return;
  AudioCapture& capture = Audio();
  const int rate = std::max(1, game.audioSampleRate);
  const int channels = std::max(1, game.audioChannels);
  if (capture.ticks > 0 && (capture.firstTick + capture.ticks != ctx.tick() || capture.sampleRate != rate || capture.channels != channels)) {
    capture = AudioCapture{};
  }
  if (capture.ticks == 0) {
    capture.firstTick = ctx.tick();
    capture.sampleRate = rate;
    capture.channels = channels;
  }
  const double exact = rate * static_cast<double>(game.stepSeconds) + capture.carry;
  const int frames = static_cast<int>(std::floor(exact));
  capture.carry = exact - frames;
  AudioBlock block;
  block.sampleRate = rate;
  block.channels = channels;
  block.frames = frames;
  block.samples.assign(static_cast<size_t>(frames) * channels, 0.0f);
  game.mixAudio(ctx, block);
  ++capture.ticks;
  if (capture.pcm.size() + block.samples.size() > static_cast<size_t>(kMaxAudioSeconds) * rate * channels) {
    capture.truncated = true;
    return;
  }
  for (float sample : block.samples) {
    const float clamped = std::max(-1.0f, std::min(1.0f, std::isfinite(sample) ? sample : 0.0f));
    capture.pcm.push_back(static_cast<int16_t>(std::lround(clamped * 32767.0f)));
  }
}

/** Ticks of live play that held one input. */
struct LiveSegment {
  int ticks = 0;
  Input input;
};

/** A live run: the person playing the game in real time through its own window. */
struct LiveRun {
  bool playing = false;
  bool paused = false;
  /** The person closed the window, which stopped the run; `play_stop` still has to collect it. */
  bool stoppedByWindow = false;
  /** Whether the window was hidden before play, so stopping hides it again. */
  bool hiddenAtRest = true;
  /** `TURRET_GAME_PLAY_HIDDEN` keeps the window hidden and the app in the background through
   *  live play, so a test can play on a machine someone is working at. */
  bool showForPlay = true;
  uint64_t startTick = 0;
  std::vector<LiveSegment> segments;
  std::chrono::steady_clock::time_point nextTick;
};

inline LiveRun& Live() {
  static LiveRun run;
  return run;
}

/** Called by the backend for every key or mouse button that goes down. Kept only while a live
 *  run is ticking, so a press made while paused or between runs never reaches a tick. */
inline void LatchLiveKey(int code) {
  if (Live().playing && !Live().paused) LivePressLatch().keys.push_back(code);
}
inline void LatchLiveButton(int code) {
  if (Live().playing && !Live().paused) LivePressLatch().buttons.push_back(code);
}

inline bool SameInput(const Input& a, const Input& b) {
  return a.buttons == b.buttons && a.pointer.present == b.pointer.present &&
         (!a.pointer.present || (a.pointer.x == b.pointer.x && a.pointer.y == b.pointer.y && a.pointer.down == b.pointer.down));
}

/** One tick of live play: the game's own `liveInput`, then the same `step` and
 *  `advancePresentation` a `step` command runs, logged so the tick replays without the OS. */
inline void LiveTick(Context& ctx, const GameCallbacks& game) {
  Input in = game.liveInput(ctx);
  PointerFromWindow(game, in.pointer);
  LivePressLatch() = LivePresses{};
  LiveRun& run = Live();
  if (!run.segments.empty() && SameInput(run.segments.back().input, in)) {
    ++run.segments.back().ticks;
  } else {
    run.segments.push_back(LiveSegment{1, in});
  }
  ctx._SetInput(std::move(in));
  ctx._AdvanceTick();
  if (game.step) game.step(ctx, ctx.input());
  if (game.advancePresentation) game.advancePresentation(ctx, game.stepSeconds);
}

/** Draws the window as a player sees it, whatever view the pane's pictures use. */
inline void DrawLiveFrame(Context& ctx, const GameCallbacks& game) { DrawWindowFrame(ctx, game, false); }

/** Ends play and hides the window again if it was hidden before; the segments wait for
 *  `play_stop`. */
inline void EndLivePlay(bool byWindow) {
  LiveRun& run = Live();
  if (!run.playing) return;
  run.playing = false;
  run.paused = false;
  run.stoppedByWindow = byWindow;
  if (run.hiddenAtRest && run.showForPlay) {
    backend::ShowWindowForPlay(false);
    SetAppForeground(false);
  }
}

/** Runs every live tick that has come due, at most a few at once so a stall does not turn into a
 *  burst, and answers how long until the next is due. */
inline int RunDueLiveTicks(Context& ctx, const GameCallbacks& game) {
  LiveRun& run = Live();
  const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::chrono::duration<double>(std::max(0.001f, game.stepSeconds)));
  auto now = std::chrono::steady_clock::now();
  for (int caughtUp = 0; caughtUp < 4 && now >= run.nextTick; ++caughtUp) {
    LiveTick(ctx, game);
    run.nextTick += period;
  }
  if (now >= run.nextTick) run.nextTick = now + period;
  return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(run.nextTick - now).count());
}

/** The segments of a live run as `play_stop` answers them, each pointer given as fractions of the
 *  frame the way a `step` takes one. */
inline json::Value LiveSegmentsJson(const GameCallbacks& game) {
  json::Value out = json::Value::MakeArray();
  for (const auto& segment : Live().segments) {
    json::Value row = json::Value::MakeObject();
    row["ticks"] = segment.ticks;
    json::Value buttons = json::Value::MakeArray();
    for (const auto& button : segment.input.buttons) buttons.push_back(button);
    row["buttons"] = std::move(buttons);
    if (segment.input.pointer.present) {
      json::Value pointer = json::Value::MakeObject();
      pointer["x"] = game.width > 0 ? segment.input.pointer.x / game.width : 0.0;
      pointer["y"] = game.height > 0 ? segment.input.pointer.y / game.height : 0.0;
      pointer["down"] = segment.input.pointer.down;
      row["pointer"] = std::move(pointer);
    }
    out.push_back(std::move(row));
  }
  return out;
}

inline bool HandleCommand(Socket fd, Context& ctx, const GameCallbacks& game, const json::Value& req, double id,
                          const std::string& cmd, const std::vector<unsigned char>& body);

/** One request/answer round. Returns false only when the connection itself should close —
 *  a malformed or unknown command still gets an answer naming the problem, and so does a
 *  command whose callback threw: the game keeps running, and the model reads what was thrown. */
inline bool HandleFrame(Socket fd, Context& ctx, const GameCallbacks& game, const std::string& headerText,
                        const std::vector<unsigned char>& body) {
  json::Value req;
  try {
    req = json::Value::parse(headerText);
  } catch (const std::exception& e) {
    json::Value err = json::Value::MakeObject();
    err["id"] = -1;
    err["ok"] = false;
    err["answers"] = "unknown";
    err["code"] = "bad_request";
    err["message"] = std::string("could not parse request: ") + e.what();
    return WriteFrame(fd, err.dump());
  }

  const json::Value* idField = req.find("id");
  double id = idField ? idField->asNumber(-1) : -1;
  std::string cmd = req.find("cmd") ? req.find("cmd")->asString() : "";

  std::string thrown;
  try {
    return HandleCommand(fd, ctx, game, req, id, cmd, body);
  } catch (const std::exception& e) {
    thrown = e.what();
  } catch (...) {
    thrown = "an exception that is not a std::exception";
  }
  json::Value err = json::Value::MakeObject();
  err["id"] = id;
  err["ok"] = false;
  err["answers"] = cmd;
  err["code"] = "exception";
  err["message"] = "the game threw while handling '" + cmd + "': " + thrown +
                   ". The world may be part-way through the change; reset or set_state before trusting it.";
  return WriteFrame(fd, err.dump());
}

inline bool HandleCommand(Socket fd, Context& ctx, const GameCallbacks& game, const json::Value& req, double id,
                          const std::string& cmd, const std::vector<unsigned char>& body) {
  json::Value answer = json::Value::MakeObject();
  answer["id"] = id;
  answer["answers"] = cmd;

  // A live run, or one the window's close button stopped, owns the tick until `play_stop` hands
  // its input back: a step taken in between would sit in the middle of the person's run.
  LiveRun& live = Live();
  // reload_assets is a data file changing under the run, which a person playing should see at once.
  const bool liveCommand = cmd == "play_pause" || cmd == "play_status" || cmd == "play_stop" || cmd == "reload_assets";
  if ((live.playing || live.stoppedByWindow) && !liveCommand) {
    answer["ok"] = false;
    answer["code"] = "playing";
    answer["message"] = live.playing ? "a live run is going; stop it with play_stop first"
                                     : "the person closed the window, which stopped the live run; collect it with play_stop first";
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "init") {
    ctx._ResetForNewRun();
    if (const json::Value* peerIdField = req.find("peer_id")) {
      std::vector<std::string> peers;
      if (const json::Value* peersField = req.find("peers")) {
        for (const auto& p : peersField->asArray()) peers.push_back(p.asString());
      }
      ctx._SetPeerIdentity(peerIdField->asString(), std::move(peers));
    }
    uint64_t seed = req.find("seed") ? req.find("seed")->asUint64(0) : 0;
    if (game.build) game.build(ctx, seed);
    answer["ok"] = true;
    answer["version"] = kHarnessVersion;
    answer["tick"] = ctx.tick();
    answer["project_type"] = game.projectType;
    answer["peers_expected_to_agree"] = game.peersExpectedToAgree;
    answer["default_view"] = game.defaultView == "normal" ? "normal" : "schematic";
    Audio() = AudioCapture{};
    answer["describe"] = BuildDescribe(ctx, game);
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "set_state") {
    const json::Value* state = req.find("state");
    ApplyResult result = game.applyState(ctx, state ? *state : json::Value::MakeObject());
    if (result.ok) {
      ctx._ResetForNewRun();
      answer["ok"] = true;
      answer["tick"] = ctx.tick();
      answer["describe"] = BuildDescribe(ctx, game);
    } else {
      answer["ok"] = false;
      answer["code"] = result.code;
      answer["message"] = result.message;
    }
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "reload_assets") {
    if (!game.reloadAssets) {
      answer["ok"] = false;
      answer["code"] = "unimplemented";
      answer["message"] = "this game has no reload_assets callback";
      return WriteFrame(fd, answer.dump());
    }
    std::vector<std::string> paths;
    if (const json::Value* pathsField = req.find("paths")) {
      for (const auto& entry : pathsField->asArray()) paths.push_back(entry.asString());
    }
    ApplyResult result = game.reloadAssets(ctx, paths);
    answer["ok"] = result.ok;
    answer["tick"] = ctx.tick();
    if (!result.ok) {
      answer["code"] = result.code;
      answer["message"] = result.message;
    }
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "set_parameters") {
    // Deliberately no _ResetForNewRun() here — the whole reason this is not `set_state`: a knob
    // turned mid-run takes effect from the next tick, and the tick before and after this command
    // is the same.
    const json::Value* values = req.find("values");
    ApplyResult result = game.setParameters(ctx, values ? *values : json::Value::MakeObject());
    if (result.ok) {
      answer["ok"] = true;
      answer["tick"] = ctx.tick();
      answer["describe"] = BuildDescribe(ctx, game);
    } else {
      answer["ok"] = false;
      answer["code"] = result.code;
      answer["message"] = result.message;
    }
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "step") {
    if (const json::Value* input = req.find("input")) {
      Input in;
      for (const auto& b : input->find("buttons") ? input->find("buttons")->asArray() : json::Array{}) {
        in.buttons.insert(b.asString());
      }
      // Turret sends a fraction of the whole frame, the way a region is given; the game reads
      // the frame's own pixels.
      if (const json::Value* pointer = input->find("pointer")) {
        const json::Value* x = pointer->find("x");
        const json::Value* y = pointer->find("y");
        const json::Value* down = pointer->find("down");
        in.pointer.present = true;
        in.pointer.x = static_cast<float>((x ? x->asNumber(0.0) : 0.0) * game.width);
        in.pointer.y = static_cast<float>((y ? y->asNumber(0.0) : 0.0) * game.height);
        in.pointer.down = down ? down->asBool(false) : false;
      }
      ctx._SetInput(std::move(in));
    }

    // Incoming packets Turret has already decided are due during this call, each tagged with
    // the exact tick it arrives on — Turret computed that tick from its own delivery queue
    // (latency, loss, reordering) before ever sending this command; the harness itself makes
    // no scheduling decision, only delivers on the tick it is told.
    struct IncomingPacket {
      uint64_t tick = 0;
      std::string from;
      std::vector<uint8_t> data;
    };
    std::vector<IncomingPacket> incoming;
    if (const json::Value* packetsField = req.find("packets")) {
      for (const auto& p : packetsField->asArray()) {
        IncomingPacket ip;
        ip.tick = p.find("tick") ? p.find("tick")->asUint64(0) : 0;
        ip.from = p.find("from") ? p.find("from")->asString() : "";
        ip.data = Base64Decode(p.find("data") ? p.find("data")->asString() : "");
        incoming.push_back(std::move(ip));
      }
    }

    // `trace` is either `true`, for whole rows, or the list of paths each row is cut down to.
    bool wantTrace = false;
    std::vector<std::string> tracePaths;
    if (const json::Value* traceField = req.find("trace")) {
      if (traceField->isArray()) {
        wantTrace = true;
        for (const auto& path : traceField->asArray()) tracePaths.push_back(path.asString());
      } else {
        wantTrace = traceField->asBool(false);
      }
    }
    json::Value trace = json::Value::MakeArray();

    // Wall-clock cost of the game's own callbacks, measured here because the harness is the only
    // place that sees them run. Serialising and drawing are not counted.
    double stepMsTotal = 0.0;
    double stepMsMax = 0.0;

    const bool wantAudio = req.find("audio") != nullptr && req.find("audio")->asBool(false);

    int ticks = req.find("ticks") ? req.find("ticks")->asInt(1) : 1;
    for (int i = 0; i < ticks; ++i) {
      auto tickStart = std::chrono::steady_clock::now();
      // The tick advances before Step runs, so an event Step emits carries the tick it just
      // reached (tick 1 for the first step out of tick 0) rather than the tick it left. A
      // packet due this tick is delivered the same way, right before Step, so a game can react
      // to it within the very tick it arrives.
      ctx._AdvanceTick();
      if (game.onPacket) {
        for (const auto& p : incoming) {
          if (p.tick == ctx.tick()) game.onPacket(ctx, p.from, p.data);
        }
      }
      if (game.step) game.step(ctx, ctx.input());
      if (game.advancePresentation) game.advancePresentation(ctx, game.stepSeconds);
      double tickMs = MillisecondsSince(tickStart);
      // Mixed after the timing is read, so `step_ms` stays the game's simulation cost alone.
      if (wantAudio) MixTick(ctx, game);
      stepMsTotal += tickMs;
      stepMsMax = std::max(stepMsMax, tickMs);
      // Never BuildDescribe here — that drains the event queue, and the answer's own `describe`
      // below would come back with `events` empty. BuildTraceRow reads the world the same way
      // without touching the queue.
      if (wantTrace) {
        json::Value row = BuildTraceRow(ctx, game);
        trace.push_back(tracePaths.empty() ? std::move(row) : FilterTraceRow(row, tracePaths));
      }
    }
    answer["ok"] = true;
    answer["tick"] = ctx.tick();
    answer["describe"] = BuildDescribe(ctx, game);
    if (wantTrace) answer["trace"] = std::move(trace);
    json::Value timing = json::Value::MakeObject();
    timing["ticks"] = ticks;
    timing["step_ms"] = stepMsTotal;
    timing["step_ms_max"] = stepMsMax;
    answer["timing"] = std::move(timing);
    auto outgoing = ctx._DrainOutgoingPackets();
    if (!outgoing.empty()) {
      json::Value arr = json::Value::MakeArray();
      for (auto& p : outgoing) {
        json::Value pv = json::Value::MakeObject();
        pv["tick"] = p.tick;
        pv["to"] = p.to;
        pv["data"] = Base64Encode(p.data);
        if (p.reliable) pv["reliable"] = true;
        arr.push_back(std::move(pv));
      }
      answer["packets_out"] = std::move(arr);
    }
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "observe") {
    answer["ok"] = true;
    answer["tick"] = ctx.tick();
    answer["describe"] = BuildDescribe(ctx, game);
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "set_render_mode") {
    std::string modeName = req.find("mode") ? req.find("mode")->asString() : "";
    RenderMode mode;
    if (!RenderModeFromName(modeName, mode)) {
      answer["ok"] = false;
      answer["code"] = "unknown_render_mode";
      answer["message"] = "render mode must be 'normal' or 'schematic', got '" + modeName + "'";
      return WriteFrame(fd, answer.dump());
    }
    ctx._SetRenderMode(mode);
    answer["ok"] = true;
    answer["mode"] = RenderModeName(mode);
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "screenshot") {
    if (const json::Value* modeField = req.find("mode")) {
      RenderMode mode;
      std::string modeName = modeField->asString();
      if (!RenderModeFromName(modeName, mode)) {
        answer["ok"] = false;
        answer["code"] = "unknown_render_mode";
        answer["message"] = "render mode must be 'normal' or 'schematic', got '" + modeName + "'";
        return WriteFrame(fd, answer.dump());
      }
      ctx._SetRenderMode(mode);
    }

    // `layers_once`: the layers named apply to this picture alone. The set in force before it is
    // back when the answer has been written, on every path out, so the run's next describe reports
    // its own set and nobody has to draw a second picture to put it back.
    struct LayerRestore {
      Context& ctx;
      std::vector<std::string> on;
      bool armed = false;
      ~LayerRestore() {
        if (!armed) return;
        std::string unknownId;
        ctx._SetLayers(on, unknownId);
      }
    } layerRestore{ctx};
    if (const json::Value* layersField = req.find("layers")) {
      const json::Value* onceField = req.find("layers_once");
      if (onceField != nullptr && onceField->asBool(false)) {
        for (const auto& id : ctx._LayerOrder()) {
          if (ctx.LayerOn(id)) layerRestore.on.push_back(id);
        }
        layerRestore.armed = true;
      }
      std::vector<std::string> on;
      for (const auto& entry : layersField->asArray()) {
        std::string id = entry.asString();
        if (!id.empty()) on.push_back(id);
      }
      std::string unknownId;
      if (!ctx._SetLayers(on, unknownId)) {
        answer["ok"] = false;
        answer["code"] = "unknown_layer";
        answer["message"] = "this game declares no layer '" + unknownId + "'";
        return WriteFrame(fd, answer.dump());
      }
    }

    // A level file the run has not loaded: its plan is drawn in place of the world, and its edit
    // block answers beside the picture. The run itself is not touched.
    bool drawingDocument = false;
    json::Value documentEdit;
    if (const json::Value* documentField = req.find("document")) {
      if (!game.describeDocument || !game.drawDocument) {
        answer["ok"] = false;
        answer["code"] = "unimplemented";
        answer["message"] = "this game has no describeDocument callback";
        return WriteFrame(fd, answer.dump());
      }
      json::Value described = json::Value::MakeObject();
      ApplyResult result = game.describeDocument(ctx, documentField->asString(), described);
      if (!result.ok) {
        answer["ok"] = false;
        answer["code"] = result.code;
        answer["message"] = result.message;
        return WriteFrame(fd, answer.dump());
      }
      if (const json::Value* edit = described.find("edit")) documentEdit = *edit;
      drawingDocument = true;
    }

    Framing framing = FullFrame(game);
    if (const json::Value* regionField = req.find("region")) {
      const json::Array& r = regionField->asArray();
      if (r.size() != 4) {
        answer["ok"] = false;
        answer["code"] = "bad_region";
        answer["message"] = "region must be [x0, y0, x1, y1], fractions of the frame";
        return WriteFrame(fd, answer.dump());
      }
      framing.x0 = std::clamp(r[0].asNumber(0.0), 0.0, 1.0);
      framing.y0 = std::clamp(r[1].asNumber(0.0), 0.0, 1.0);
      framing.x1 = std::clamp(r[2].asNumber(1.0), 0.0, 1.0);
      framing.y1 = std::clamp(r[3].asNumber(1.0), 0.0, 1.0);
      if (framing.x1 <= framing.x0) framing.x1 = std::min(1.0, framing.x0 + 0.01);
      if (framing.y1 <= framing.y0) framing.y1 = std::min(1.0, framing.y0 + 0.01);
      // A region named without a size is drawn at the whole frame's own pixel count, in the
      // region's shape, so a smaller region comes back magnified rather than smaller. Every
      // picture taken at no named size then has one resolution whatever it is framed on, and a
      // region Turret keeps in force across calls does not shrink the pictures after it.
      const double regionWidth = (framing.x1 - framing.x0) * game.width;
      const double regionHeight = (framing.y1 - framing.y0) * game.height;
      const double magnify = std::sqrt((static_cast<double>(game.width) * game.height) / (regionWidth * regionHeight));
      framing.width = static_cast<int>(std::lround(regionWidth * magnify));
      framing.height = static_cast<int>(std::lround(regionHeight * magnify));
    }
    const json::Value* sizeField = req.find("size");
    if (sizeField == nullptr) FitDefaultSizeToModel(framing);
    if (sizeField != nullptr) {
      const json::Array& s = sizeField->asArray();
      if (s.size() != 2) {
        answer["ok"] = false;
        answer["code"] = "bad_size";
        answer["message"] = "size must be [width, height], in pixels";
        return WriteFrame(fd, answer.dump());
      }
      framing.width = s[0].asInt(framing.width);
      framing.height = s[1].asInt(framing.height);
    }
    constexpr int kMinDimension = 16;
    constexpr int kMaxDimension = 2048;
    framing.width = std::clamp(framing.width, kMinDimension, kMaxDimension);
    framing.height = std::clamp(framing.height, kMinDimension, kMaxDimension);

    // `format: "rgba"` skips the PNG encode: the body is the raw pixels, for a caller that draws
    // them straight to the screen and would only decode a PNG again.
    const json::Value* formatField = req.find("format");
    const bool png = formatField == nullptr || formatField->asString() != "rgba";

    auto renderStart = std::chrono::steady_clock::now();
    ScreenshotResult shot = CaptureScreenshot(ctx, game, framing, drawingDocument, png);
    double renderMs = MillisecondsSince(renderStart);
    if (!shot.ok) {
      answer["ok"] = false;
      answer["code"] = "capture_failed";
      answer["message"] = "the frame could not be captured";
      return WriteFrame(fd, answer.dump());
    }
    if (drawingDocument) answer["edit"] = documentEdit;
    answer["ok"] = true;
    answer["mode"] = RenderModeName(ctx.renderMode());
    answer["tick"] = ctx.tick();
    answer["format"] = png ? "png" : "rgba";
    answer["width"] = shot.framing.width;
    answer["height"] = shot.framing.height;
    answer["empty"] = shot.empty;
    answer["scale"] = shot.scale;
    // Drawing, reading the pixels back and encoding any PNG, so an upper bound on a frame's cost.
    answer["render_ms"] = renderMs;
    json::Value regionOut = json::Value::MakeArray();
    regionOut.push_back(shot.framing.x0);
    regionOut.push_back(shot.framing.y0);
    regionOut.push_back(shot.framing.x1);
    regionOut.push_back(shot.framing.y1);
    json::Value sizeOut = json::Value::MakeArray();
    sizeOut.push_back(shot.framing.width);
    sizeOut.push_back(shot.framing.height);
    json::Value framingOut = json::Value::MakeObject();
    framingOut["region"] = regionOut;
    framingOut["size"] = sizeOut;
    answer["framing"] = framingOut;
    if (!ctx._LayerOrder().empty()) {
      json::Value on = json::Value::MakeArray();
      json::Value off = json::Value::MakeArray();
      json::Value empty = json::Value::MakeArray();
      for (const auto& id : ctx._LayerOrder()) {
        if (!ctx.LayerOn(id)) {
          off.push_back(id);
          continue;
        }
        on.push_back(id);
        if (!ctx._LayerDrew(id)) empty.push_back(id);
      }
      json::Value layersOut = json::Value::MakeObject();
      layersOut["on"] = std::move(on);
      layersOut["off"] = std::move(off);
      layersOut["empty"] = std::move(empty);
      answer["layers"] = std::move(layersOut);
    }
    if (!shot.marks.empty()) {
      json::Value marksOut = json::Value::MakeArray();
      for (const auto& mark : shot.marks) {
        json::Value row = json::Value::MakeObject();
        row["id"] = mark.id;
        json::Value box = json::Value::MakeArray();
        box.push_back(mark.x0);
        box.push_back(mark.y0);
        box.push_back(mark.x1);
        box.push_back(mark.y1);
        row["box"] = std::move(box);
        marksOut.push_back(std::move(row));
      }
      answer["marks"] = std::move(marksOut);
    }
    return WriteFrame(fd, answer.dump(), shot.bytes.data(), shot.bytes.size());
  }

  if (cmd == "save_state") {
    SaveResult result = game.saveState(ctx);
    if (!result.ok) {
      answer["ok"] = false;
      answer["code"] = result.code;
      answer["message"] = result.message;
      return WriteFrame(fd, answer.dump());
    }
    // In the body, not the header: a snapshot of a large world overruns the header's cap as
    // base64 text.
    answer["ok"] = true;
    std::vector<uint8_t> snapshot = PrefixTick(ctx.tick(), result.bytes);
    return WriteFrame(fd, answer.dump(), snapshot.data(), snapshot.size());
  }

  if (cmd == "load_state") {
    // The body, as `save_state` answers it. `state_bytes` in the header is what a Turret
    // predating harness version 2 sends.
    std::vector<uint8_t> raw;
    if (!body.empty()) {
      raw.assign(body.begin(), body.end());
    } else {
      const json::Value* bytesField = req.find("state_bytes");
      raw = Base64Decode(bytesField ? bytesField->asString() : "");
    }
    uint64_t tick = 0;
    std::vector<uint8_t> payload;
    if (!SplitTickPrefix(raw, tick, payload)) {
      answer["ok"] = false;
      answer["code"] = "bad_state_bytes";
      answer["message"] = "state_bytes is too short to hold a snapshot";
      return WriteFrame(fd, answer.dump());
    }
    ApplyResult result = game.loadState(ctx, payload);
    if (!result.ok) {
      answer["ok"] = false;
      answer["code"] = result.code;
      answer["message"] = result.message;
      return WriteFrame(fd, answer.dump());
    }
    ctx._RestoreTick(tick);
    answer["ok"] = true;
    answer["tick"] = ctx.tick();
    answer["describe"] = BuildDescribe(ctx, game);
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "audio") {
    if (!game.mixAudio) {
      answer["ok"] = false;
      answer["code"] = "unimplemented";
      answer["message"] = "this game has no mixAudio callback";
      return WriteFrame(fd, answer.dump());
    }
    AudioCapture capture = std::move(Audio());
    Audio() = AudioCapture{};
    answer["ok"] = true;
    answer["format"] = "s16";
    answer["sample_rate"] = capture.ticks > 0 ? capture.sampleRate : std::max(1, game.audioSampleRate);
    answer["channels"] = capture.ticks > 0 ? capture.channels : std::max(1, game.audioChannels);
    answer["frames"] = static_cast<double>(capture.pcm.size() / static_cast<size_t>(std::max(1, capture.channels)));
    answer["first_tick"] = capture.firstTick;
    answer["ticks"] = capture.ticks;
    if (capture.truncated) answer["truncated"] = true;
    // Native-endian, like the frame lengths: the harness and Turret share a machine.
    return WriteFrame(fd, answer.dump(), reinterpret_cast<const unsigned char*>(capture.pcm.data()),
                      capture.pcm.size() * sizeof(int16_t));
  }

  if (cmd == "play_start") {
    if (!game.liveInput) {
      answer["ok"] = false;
      answer["code"] = "unimplemented";
      answer["message"] = "this game has no liveInput callback";
      return WriteFrame(fd, answer.dump());
    }
    live.playing = true;
    live.paused = false;
    live.stoppedByWindow = false;
    live.startTick = ctx.tick();
    live.segments.clear();
    LivePressLatch() = LivePresses{};
    live.nextTick = std::chrono::steady_clock::now();
    if (live.showForPlay) {
      SetAppForeground(true);
      backend::ShowWindowForPlay(true);
    }
    answer["ok"] = true;
    answer["tick"] = ctx.tick();
    // Rounded to a thousandth: `stepSeconds` is a float, and 1/60 does not survive one exactly.
    answer["ticks_per_second"] = std::round(1000.0 / std::max(0.001, static_cast<double>(game.stepSeconds))) / 1000.0;
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "play_pause") {
    if (!live.playing) {
      answer["ok"] = false;
      answer["code"] = "not_playing";
      answer["message"] = "no live run is going";
      return WriteFrame(fd, answer.dump());
    }
    const json::Value* pausedField = req.find("paused");
    const bool paused = pausedField ? pausedField->asBool(true) : true;
    if (live.paused && !paused) live.nextTick = std::chrono::steady_clock::now();
    live.paused = paused;
    answer["ok"] = true;
    answer["paused"] = live.paused;
    answer["tick"] = ctx.tick();
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "play_status") {
    answer["ok"] = true;
    answer["playing"] = live.playing;
    answer["paused"] = live.paused;
    answer["tick"] = ctx.tick();
    if (live.playing || live.stoppedByWindow) answer["start_tick"] = live.startTick;
    if (live.stoppedByWindow) answer["stopped_by"] = "window";
    return WriteFrame(fd, answer.dump());
  }

  if (cmd == "play_stop") {
    if (!live.playing && !live.stoppedByWindow) {
      answer["ok"] = false;
      answer["code"] = "not_playing";
      answer["message"] = "no live run is going";
      return WriteFrame(fd, answer.dump());
    }
    const bool byWindow = live.stoppedByWindow;
    EndLivePlay(false);
    answer["ok"] = true;
    answer["tick"] = ctx.tick();
    answer["start_tick"] = live.startTick;
    answer["stopped_by"] = byWindow ? "window" : "turret";
    answer["inputs"] = LiveSegmentsJson(game);
    answer["describe"] = BuildDescribe(ctx, game);
    live.segments.clear();
    live.stoppedByWindow = false;
    return WriteFrame(fd, answer.dump());
  }

  answer["ok"] = false;
  answer["code"] = "unknown_command";
  answer["message"] = "no such command: '" + cmd + "'";
  return WriteFrame(fd, answer.dump());
}

}  // namespace detail
}  // namespace turret

// The keyboard build supplies its own main and its own loop, but wants everything above: the
// drawing kit, the camera and the backend. Defining TURRET_GAME_NO_MAIN takes the harness's own
// entry point and command loop out, leaving the rest.
#ifndef TURRET_GAME_NO_MAIN

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: " << (argc > 0 ? argv[0] : "game")
               << " <control-socket-path> [x,y[,width,height]]" << std::endl;
    std::cerr << "       " << (argc > 0 ? argv[0] : "game") << " --probe-display" << std::endl;
    return 1;
  }
  // Answers the one question a caller has to settle before a run is worth starting, without
  // opening a window or connecting to anything: can a window be opened on this desktop session
  // at all. `e2e/game.spec.ts` skips the whole spec on `kNoDisplayExit` — a Windows session with
  // no monitor attached (a disconnected RDP session is the commonest way) is not somewhere a
  // real game window can be tested.
  if (std::string(argv[1]) == "--probe-display") {
    bool available = turret::backend::DisplayAvailable();
    std::cout << (available ? "display: yes" : "display: no") << std::endl;
    return available ? 0 : turret::detail::kNoDisplayExit;
  }
  // Checked before the window rather than after: see `backend::DisplayAvailable`'s own comment
  // for why "the renderer will say so" is not true of every renderer.
  if (!turret::backend::DisplayAvailable()) {
    std::cerr << "no display: this desktop session has no monitor attached, so no window can be opened"
              << std::endl;
    return turret::detail::kNoDisplayExit;
  }
  std::string socketPath = argv[1];
  // A third argv entry is where to put the window: "x,y" or "x,y,width,height". Turret passes
  // it when it wants the game beside its own window rather than wherever the OS drops it;
  // absent, this placement step does nothing. Placement only — no raising, no focus stealing.
  turret::detail::WindowPlacement placement =
      argc >= 3 ? turret::detail::ParseWindowPlacement(argv[2]) : turret::detail::WindowPlacement{};

  turret::GameCallbacks game = TurretGame();

  // The game's window is a real OS window, hidden when TURRET_GAME_HIDDEN is set — which Turret
  // sets on every binary it spawns, and the tests set to keep a window off a CI machine.
  // Screenshots come off an off-screen surface, so a hidden window still draws every picture
  // Turret asks for.
  const char* hiddenForTests = std::getenv("TURRET_GAME_HIDDEN");
  bool hidden = hiddenForTests != nullptr && std::string(hiddenForTests) != "0";
  if (!turret::backend::OpenWindow(game.title, game.width, game.height, hidden)) return 1;
  turret::detail::ApplyWindowPlacement(placement);

  turret::Context ctx;

  // The window's swap chain is not ready the instant it exists, so the very first frame the idle
  // loop draws can be a visible black flash. A screenshot never reads the window, so this is
  // cosmetic only, for whoever is watching.
  turret::detail::DrawCurrentFrame(ctx, game);
  turret::detail::DrawCurrentFrame(ctx, game);

  turret::detail::Socket fd = turret::detail::ConnectControlSocket(socketPath);
  if (fd == turret::detail::kNoSocket) {
    std::cerr << "could not connect control socket at " << socketPath << std::endl;
    turret::backend::CloseWindow();
    return 1;
  }

  // Idling is pumping: a frame frozen between commands still redraws and still services the
  // window's event queue, or the OS marks it unresponsive. poll() with a short timeout means
  // the loop never blocks on the socket alone. Real keyboard and mouse input reaches the OS
  // window and is dropped there, except during live play, when the game's `liveInput` reads it.
  turret::detail::PollFd pfd{};
  pfd.fd = fd;
  pfd.events = POLLIN;

  turret::detail::Live().hiddenAtRest = hidden;
  const char* playHidden = std::getenv("TURRET_GAME_PLAY_HIDDEN");
  turret::detail::Live().showForPlay = playHidden == nullptr || std::string(playHidden) == "0";

  bool running = true;
  while (running) {
    turret::detail::LiveRun& live = turret::detail::Live();
    int waitMs = 10;
    if (live.playing && !live.paused) {
      waitMs = std::max(0, std::min(waitMs, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 live.nextTick - std::chrono::steady_clock::now()).count())));
    }
    int rc = turret::detail::PollOne(&pfd, waitMs);
    if (rc > 0 && (pfd.revents & (POLLIN | POLLHUP | POLLERR))) {
      std::string header;
      std::vector<unsigned char> body;
      if (!turret::detail::ReadFrame(fd, header, body)) {
        running = false;
        break;
      }
      if (!turret::detail::HandleFrame(fd, ctx, game, header, body)) {
        running = false;
        break;
      }
    } else {
      turret::backend::PumpEvents();
      if (live.playing) {
        // Live play draws every frame, which is also what services the window's input on raylib.
        if (!live.paused) turret::detail::RunDueLiveTicks(ctx, game);
        turret::detail::DrawLiveFrame(ctx, game);
        if (turret::backend::WindowCloseRequested()) turret::detail::EndLivePlay(true);
      } else if (!hidden) {
        // A hidden window shows nobody anything, and redrawing it a hundred times a second costs a
        // core per game process. Screenshots draw their own frame off screen.
        turret::detail::DrawCurrentFrame(ctx, game);
      }
    }
  }

  turret::detail::CloseSocket(fd);
  turret::backend::CloseWindow();
  return 0;
}

#endif  // TURRET_GAME_NO_MAIN

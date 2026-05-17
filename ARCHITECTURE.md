# `gpu_lib_mock_dumbfb` — Architecture Design Document

**PixelGPU Project — Phase 1 / Phase 2 Foundation**
**Status:** Phase 1 implementation complete

---

## 1. Purpose and Scope

`gpu_lib_mock_dumbfb` is the foundational display-preview library for
PixelGPU. It owns one job: take a caller-managed block of memory that
represents a framebuffer and make it visible in a desktop window.

It does **not** own the framebuffer memory. It does **not** perform
drawing. It does not handle keyboard or mouse input beyond detecting
program exit (Esc key, window close button). It exposes no framebuffer
readback. All of those concerns belong to higher layers.

The library is intentionally thin at this phase. Its design choices are
made to support later curriculum phases without requiring a rewrite of
the public interface.

---

## 2. Pixel Format

All framebuffer memory in Phase 1 and Phase 2 uses a single fixed
format:

```
XBGR8888  (32 bits per pixel, little-endian)

  Byte 0 (LSB): Blue
  Byte 1:       Green
  Byte 2:       Red
  Byte 3 (MSB): X  (unused or alpha — callers may use either convention)
```

Rationale: 32-bit stride guarantees natural word alignment on all
supported platforms. BGR byte order matches little-endian host memory
layout used by most x86/ARM systems so no per-pixel byte swapping is
needed. The X channel is reserved for future alpha blending experiments
or simply left as 0xFF by convention.

No other pixel formats are supported at this time. Format negotiation
will be added in a later phase when it is pedagogically appropriate.

---

## 3. Resolution System

### 3.1 The `Resolution` Type

```cpp
// pixelgpu/resolution.h  (C++ header)
namespace pixelgpu {

struct Resolution {
    uint32_t width;
    uint32_t height;

    // Construct from explicit dimensions
    constexpr Resolution(uint32_t w, uint32_t h) noexcept;

    // Construct from a named preset
    explicit Resolution(ResolutionPreset preset) noexcept;

    // Convenience
    uint32_t stride_bytes() const noexcept;   // width * 4 (XBGR8888)
    uint64_t frame_bytes() const noexcept;    // stride * height
    float    aspect_ratio() const noexcept;   // (float)width / height
};

} // namespace pixelgpu
```

The C API exposes this as:

```c
typedef struct { uint32_t width; uint32_t height; } pgpu_resolution_t;
```

### 3.2 Named Resolution Presets

A hand-rolled header file `pixelgpu/resolution_presets.h` defines an
enum of canonical preset names. The source for names and dimensions is
the Wikipedia article *"Graphics display resolution"* plus the IBM and
VESA naming conventions it documents. Refresh rate is **not** part of
the resolution; it is a separate display parameter (see Section 5.3).

```cpp
enum class ResolutionPreset : uint32_t {
    // ── IBM / legacy text and CGA-era ────────────────────────────────
    CGA         = 0,   // 320 × 200
    EGA,               // 640 × 350
    VGA,               // 640 × 480
    SVGA,              // 800 × 600

    // ── XGA family ───────────────────────────────────────────────────
    XGA,               // 1024 × 768
    SXGA,              // 1280 × 1024
    UXGA,              // 1600 × 1200

    // ── Widescreen 16:9 ──────────────────────────────────────────────
    HD720,             // 1280 × 720    (720p)
    HD1080,            // 1920 × 1080   (1080p / Full HD)
    QHD,               // 2560 × 1440   (1440p / WQHD)
    UHD4K,             // 3840 × 2160   (4K UHD)

    // ── Widescreen 16:10 ─────────────────────────────────────────────
    WXGA,              // 1280 × 800
    WSXGA_PLUS,        // 1680 × 1050
    WUXGA,             // 1920 × 1200

    // ── Handheld / retro-console approximate sizes ───────────────────
    GAMEBOY,           // 160 × 144     (original Game Boy LCD)
    GAMEBOY_COLOR,     // 160 × 144     (same physical; same preset)
    GAMEBOY_ADVANCE,   // 240 × 160
    NDS,               // 256 × 192     (one screen of Nintendo DS)
    PSP,               // 480 × 272

    // ── Square / specialty ───────────────────────────────────────────
    QVGA,              // 320 × 240
    HVGA,              // 480 × 320

    _COUNT
};

// Table lookup: ResolutionPreset → (width, height)
constexpr Resolution resolution_from_preset(ResolutionPreset p) noexcept;
```

The table implementation lives in a `.cpp` file, not inline, to keep
compile times predictable as the table grows.

---

## 4. Backend Architecture

### 4.1 Design Overview

The library supports multiple **display backends**. Each backend wraps
a different windowing or remoting technology (SDL3, GLFW, VNC/RDP,
etc.). Exactly one backend is selected at runtime to service a given
display handle.

The selection mechanism is a **runtime registry** populated by
**static initializers** in each compiled-in backend translation unit.
No RTTI is required. No virtual dispatch table at registration time.

```
┌──────────────────────────────────────────────────────────┐
│                    Public C API                          │
│   pgpu_display_create() / pgpu_display_destroy() / …    │
└──────────────────────┬───────────────────────────────────┘
                       │  calls
┌──────────────────────▼───────────────────────────────────┐
│              C++ DisplayFactory                          │
│  - holds BackendRegistry (static, process-lifetime)      │
│  - selects backend by priority / name filter             │
│  - allocates IDisplay via chosen backend's factory fn    │
└──────────────────────┬───────────────────────────────────┘
                       │  creates
┌──────────────────────▼───────────────────────────────────┐
│           IDisplay  (abstract base class)                │
│   present()  register_vsync_callback()                   │
│   wait_vsync()  destroy()                                │
│   …                                                      │
└──────┬───────────────┬──────────────────┬────────────────┘
       │               │                  │
  SDL3Display     GLFWDisplay        VNCDisplay
  (libSDL3)       (libglfw3)         (libvncclient)
```

### 4.2 Backend Registration

Each backend defines a file-scope `BackendRegistrar` object. Its
constructor calls `BackendRegistry::register_backend()` before
`main()` runs.

```cpp
// Internal — not part of public API
namespace pixelgpu::internal {

struct BackendDescriptor {
    std::string_view  name;           // "sdl3", "glfw", "vnc", …
    int               preference;     // Higher = more preferred. No fixed scale.
    bool              supports_bezel; // Phase 1: always false; reserved.

    // Factory: given a DisplayParams, try to create a display.
    // Returns nullptr if this backend cannot satisfy the params on
    // this platform / configuration.  MUST NOT throw.
    std::unique_ptr<IDisplay> (*create)(const DisplayParams&) noexcept;
};

class BackendRegistry {
public:
    static BackendRegistry& instance() noexcept;
    void register_backend(const BackendDescriptor& desc);

    // Returns all descriptors sorted by preference (descending).
    std::vector<BackendDescriptor> sorted_by_preference() const;

    // Returns a descriptor for the named backend, or nullptr if not found.
    const BackendDescriptor* find_by_name(std::string_view name) const;

    // Called by factory functions to record a failure reason.
    void set_last_error(std::string msg);
    std::string last_error() const;

    // Human-readable summary, e.g. "sdl3(100), glfw(100), vnc(10)"
    std::string registered_summary() const;
};

// Convenience RAII registrar — instantiate once per backend TU
struct BackendRegistrar {
    explicit BackendRegistrar(const BackendDescriptor& desc) noexcept {
        BackendRegistry::instance().register_backend(desc);
    }
};

} // namespace pixelgpu::internal
```

Backend TU registration pattern:

```cpp
// sdl3_backend.cpp
namespace {
    pixelgpu::internal::BackendRegistrar g_sdl3_reg {
        pixelgpu::internal::BackendDescriptor {
            .name           = "sdl3",
            .preference     = 100,
            .supports_bezel = false,       // Phase 1 stub
            .create         = SDL3Display::try_create,
        }
    };
} // anonymous namespace
```

### 4.3 `IDisplay` Interface

```cpp
namespace pixelgpu {

// C-compatible vsync callback type.
// Invoked on the backend's internal timer thread once per vsync tick.
extern "C" {
    using VsyncCallbackFn = void (*)(void* user_data);
}

class IDisplay {
public:
    virtual ~IDisplay() = default;

    // Non-copyable, non-movable — displays are unique resources.
    IDisplay(const IDisplay&)            = delete;
    IDisplay& operator=(const IDisplay&) = delete;
    IDisplay(IDisplay&&)                 = delete;
    IDisplay& operator=(IDisplay&&)      = delete;

    // ── Event pump ──────────────────────────────────────────────────────

    // Process pending window events (close, keyboard, etc.).
    //
    // MUST be called from the thread that created the display.  On Windows,
    // Win32 delivers messages only to the thread that owns the HWND; calling
    // this from any other thread silently does nothing and leaves the window
    // frozen.
    //
    // The C API calls this automatically from pgpu_wait_vsync(), which the
    // render loop already calls once per frame from the main thread.  Direct
    // C++ callers must call it themselves once per frame.
    //
    // Default: no-op (headless / remote backends have no window event queue).
    virtual void pump_events() noexcept {}

    // ── Presentation ────────────────────────────────────────────────────

    // Blit the current framebuffer to the window immediately.
    // Does not affect the vsync timer phase or counter.
    virtual void present_now() = 0;

    // ── Vsync ───────────────────────────────────────────────────────────

    // Register a vsync callback fired on each tick.  Pass nullptr to unregister.
    // The callback is invoked on the backend's internal timer thread.
    virtual void set_vsync_callback(VsyncCallbackFn cb,
                                    void* user_data) noexcept = 0;

    // Block until the next vsync tick or until timeout_ms elapses.
    // Returns true if vsync fired, false if timeout.
    virtual bool wait_vsync(uint32_t timeout_ms) = 0;

    // ── Status ──────────────────────────────────────────────────────────

    // True while the window is open and the backend pump is healthy.
    virtual bool is_alive() const noexcept = 0;

    // ── Metadata ────────────────────────────────────────────────────────

    virtual Resolution       resolution()   const noexcept = 0;
    virtual uint32_t         refresh_hz()   const noexcept = 0;
    // Short lowercase identifier, e.g. "sdl3", "glfw", "vnc".
    virtual std::string_view backend_name() const noexcept = 0;

protected:
    IDisplay() = default;
};

} // namespace pixelgpu
```

### 4.4 `DisplayParams` — Factory Input

```cpp
namespace pixelgpu {

struct DisplayParams {
    // Required
    Resolution     resolution;
    void*          framebuffer_ptr;    // Caller-owned memory, XBGR8888

    // Optional
    uint32_t       refresh_hz       = 60;
    std::string    window_title     = "PixelGPU";

    // Backend selection (empty = use preference scores)
    // Comma-separated priority list: "sdl3,glfw"
    // If non-empty, only named backends are considered, in listed order.
    std::string    backend_priority;

    // Bezel (Phase 1: ignored; plumbing present)
    std::string    text_arg;     // e.g. "gameboy" — empty = no bezel
};

} // namespace pixelgpu
```

### 4.5 `DisplayFactory`

```cpp
namespace pixelgpu {

class DisplayFactory {
public:
    // Returns a live display, or nullptr on failure.
    // On failure, call last_error_string() on the calling thread for details.
    static std::unique_ptr<IDisplay> create(const DisplayParams& params) noexcept;

    // Returns a description of why the last create() on this thread failed.
    // Empty if the last create() succeeded.
    static std::string_view last_error_string() noexcept;

    // Returns a human-readable summary of all registered backends and their
    // preference scores, e.g. "sdl3(100), glfw(100), vnc(10)".
    static std::string registered_backends_summary();
};

} // namespace pixelgpu
```

Selection logic (in priority order):

1. If `params.backend_priority` is non-empty, parse the comma-separated
   list. Try each named backend in listed order. The first one whose
   `create()` returns non-null wins.
2. If `params.backend_priority` is empty, try all registered backends
   in descending preference order. The first successful `create()` wins.
3. If all attempts return nullptr, `DisplayFactory::create()` returns
   nullptr and logs an explanation string.

If a bezel is requested (`text_arg_` non-empty) but no backend
with `supports_bezel == true` is available (or none was selected), the
factory fails immediately with a descriptive error rather than silently
opening a window without the bezel.

---

## 5. Vsync and Timing Model

### 5.1 Internal Timer Thread

Each backend that supports vsync runs an **internal timer thread**
started when the display is created. The thread fires at
`1.0 / refresh_hz` intervals using a high-resolution monotonic clock.

On each tick the thread:
1. Posts a notification to all `wait_vsync()` callers (via a
   condition variable or platform semaphore).
2. Invokes the registered vsync callback (if any) from the timer thread.

**Thread safety contract for the public API:**

- `set_vsync_callback()` is safe to call from any thread.
- `wait_vsync()` is safe to call from any thread.
- `present_now()` is safe to call from any thread.
- The framebuffer memory itself has **no** thread-safety guarantee.
  This is deliberate — tearing and race conditions on the framebuffer
  are a teaching moment about real hardware behavior.

### 5.2 Vsync Callback

```c
// C API type
typedef void (*pgpu_vsync_callback_fn)(void* user_data);
```

- Invoked on the backend's internal timer thread.
- `user_data` is the pointer registered by the caller; the library
  does not inspect it.
- There is no in-callback cancel mechanism. To cancel callbacks, call
  `pgpu_display_set_vsync_callback(handle, NULL, NULL)` from any thread.

### 5.3 `wait_vsync()`

```c
// C API
pgpu_status_t pgpu_wait_vsync(pgpu_display_t handle, uint32_t timeout_ms);
```

Blocks the calling thread until the next vsync fires or `timeout_ms`
milliseconds elapse. Returns `PGPU_OK` on vsync, `PGPU_TIMEOUT` on
timeout, `PGPU_ERROR_INVALID_HANDLE` for a bad handle.

Multiple threads may call `wait_vsync()` concurrently; all are woken
on the same vsync tick.

### 5.4 `present_now()`

Copies (or signals) the current framebuffer contents to the window
immediately. Does not affect the vsync timer counter or phase.
Intended for callers who do not want to think about timing yet.

---

## 6. Error Handling

### 6.1 Status Enum

```c
typedef enum {
    PGPU_OK                     =  0,
    PGPU_ERROR_INVALID_HANDLE   = -1,
    PGPU_ERROR_BACKEND_FAILED   = -2,
    PGPU_ERROR_BEZEL_REQUIRED   = -3,   // bezel requested but unavailable
    PGPU_ERROR_BEZEL_ASPECT     = -4,   // bezel aspect does not match resolution
    PGPU_ERROR_OUT_OF_MEMORY    = -5,
    PGPU_ERROR_UNKNOWN          = -99,
} pgpu_status_t;
```

### 6.2 Error String API

All C API functions catch C++ exceptions internally and translate them
to a status code. The most recent error message is stored in a
pre-allocated thread-local string buffer.

```c
// Returns the status code of the last failed call on this thread.
pgpu_status_t   pgpu_last_error_code(void);

// Returns a null-terminated human-readable string describing the error.
// The pointer is valid until the next pgpu_* call on this thread.
const char*     pgpu_last_error_string(void);
```

---

## 7. Public C API Surface

```c
// ── Handle types ────────────────────────────────────────────────────
typedef struct pgpu_display_opaque* pgpu_display_t;

// ── Resolution ──────────────────────────────────────────────────────
typedef struct { uint32_t width; uint32_t height; } pgpu_resolution_t;

// Named preset IDs match ResolutionPreset enum values
pgpu_resolution_t pgpu_resolution_from_preset(uint32_t preset_id);
pgpu_resolution_t pgpu_resolution_make(uint32_t width, uint32_t height);

// ── Display lifecycle ────────────────────────────────────────────────
pgpu_display_t  pgpu_display_create(
    pgpu_resolution_t  resolution,
    void*              framebuffer_ptr,
    uint32_t           refresh_hz,           // 0 = default 60
    const char*        window_title,         // NULL = "PixelGPU"
    const char*        backend_priority,     // NULL = auto
    const char*        text_arg        // NULL = no bezel
);

void            pgpu_display_destroy(pgpu_display_t handle);

// ── Presentation ─────────────────────────────────────────────────────
void            pgpu_display_present_now(pgpu_display_t handle);

// ── Vsync ────────────────────────────────────────────────────────────
void            pgpu_display_set_vsync_callback(
    pgpu_display_t          handle,
    pgpu_vsync_callback_fn  callback,   // NULL = unregister
    void*                   user_data
);

pgpu_status_t   pgpu_wait_vsync(pgpu_display_t handle, uint32_t timeout_ms);

// ── Status ───────────────────────────────────────────────────────────
int             pgpu_display_is_alive(pgpu_display_t handle);  // 1 / 0

// ── Diagnostics ──────────────────────────────────────────────────────
pgpu_status_t   pgpu_last_error_code(void);
const char*     pgpu_last_error_string(void);

// Returns comma-separated list of registered backend names in
// preference order, e.g. "sdl3(100),glfw(90),vnc(10)"
const char*     pgpu_registered_backends(void);
```

The C++ API is the `DisplayFactory` / `IDisplay` surface described in
Section 4. The C API is a thin shim that allocates `IDisplay` objects
on the heap and exposes them as opaque `pgpu_display_t` handles. There
is a single shared ownership table (a locked `std::unordered_map`) that
maps `pgpu_display_t` → `std::unique_ptr<IDisplay>` for handle
validation.

---

## 8. Bezel System (Phase 1: Stubbed)

The full bezel pipeline is deferred to Phase 2. The following describes
the **complete intended design**; only the skeletal hooks are wired in
Phase 1.

### 8.1 File Discovery

The library searches for bezel image files in this order:

1. The path named by the environment variable `PIXELGPU_BEZEL_PATH`
   (if set and non-empty).
2. A `monitor-bezels/` subdirectory relative to the current working
   directory.

### 8.2 Filename Convention

```
<basename>_<aspect>.png

<aspect> encoding:
  4:3   → "4x3"
  16:9  → "16x9"
  16:10 → "16x10"
  3:2   → "3x2"
  1:1   → "1x1"
  etc.

Examples:
  gameboy_4x3.png
  crt_monitor_4x3.png
  widescreen_tv_16x9.png
```

The factory computes the framebuffer's aspect ratio, derives the
`<aspect>` token, and appends it to the basename to form the filename.
If no matching file exists, the factory fails.

### 8.3 Aspect Validation

The bezel's transparent region must have the same aspect ratio (within
a small epsilon) as the requested framebuffer. If not, the factory
returns `PGPU_ERROR_BEZEL_ASPECT`. The bezel image may be isometrically
scaled (uniform scale) to match any resolution of the correct aspect
ratio.

### 8.4 Transparent Region Detection

The largest axis-aligned rectangle where **all corner and interior
pixels have alpha < 255** is detected using the classic histogram
maximal rectangle algorithm (O(W × H)). "Largest" means largest area
in pixels *after scaling to match the framebuffer*. Smaller transparent
regions (button holes, lens flare cutouts, etc.) are ignored because
only the single largest qualifying rectangle is used.

Note that interior pixels need not be fully transparent — the definition
is that alpha < 255 is the threshold for "transparent enough to define
the screen aperture," allowing decal overlays (specular highlights,
screen reflections, insects, smudges) to exist within the screen region.

### 8.5 Window Sizing

The framebuffer is rendered at **pixel-accurate** size (no scaling).
The window is enlarged beyond the framebuffer dimensions to accommodate
the bezel border:

```
window_width  = framebuffer_width  + left_bezel_margin + right_bezel_margin
window_height = framebuffer_height + top_bezel_margin  + bottom_bezel_margin
```

Where margins are derived from the detected transparent rectangle
position in the (scaled) bezel image.

### 8.6 Backend Capability Flag

Each backend declares `supports_bezel` in its descriptor. Only SDL3
and GLFW backends will set this to `true` (in a future commit). Headless
or remoting backends leave it `false`.

---

## 9. CMake / Build System Design

### 9.1 Backend Feature Flags

Backends are controlled by CMake options in `Features.cmake`:

```cmake
option(FEATURE_BACKEND_SDL3  "Enable SDL3 display backend"      ON)
option(FEATURE_BACKEND_GLFW  "Enable GLFW display backend"      ON)
option(FEATURE_BACKEND_VNC   "Enable VNC/libvncserver backend"   OFF)
```

`ON` means *"enable if the dependency is found."* If the vcpkg package
or system library is missing, the backend is silently disabled and a
CMake status message is emitted. It does **not** cause a configure
error.

This preserves the "best available" auto-selection behavior while
allowing users to explicitly disable backends they do not want (to avoid
pulling in unwanted dependencies):

```cmake
cmake --preset default -DFEATURE_BACKEND_VNC=OFF
```

### 9.2 Sub-library Structure

Each backend is a separate CMake `OBJECT` library target (or a thin
static lib) linked into `gpu_lib_mock_dumbfb`:

```
libs/
  gpu_lib_mock_dumbfb/
    CMakeLists.txt           ← main lib; links backends
    include/
      pixelgpu/
        display_factory.h    ← C++ factory
        idisplay.h           ← C++ interface
        resolution.h         ← Resolution type
        resolution_presets.h ← ResolutionPreset enum + table
    include_c/
      pgpu/
        pgpu.h               ← C-only public header (extern "C")
    src/
      display_factory.cpp
      backend_registry.cpp
      backend_registry.h     ← Internal header (not installed)
      c_api.cpp              ← All extern "C" wrappers + thread-local error store
    backends/
      sdl3/
        CMakeLists.txt       ← find_package(SDL3), build if found
        sdl3_display.h
        sdl3_display.cpp
        sdl3_registrar.cpp   ← Static init registration
      glfw/
        CMakeLists.txt
        glfw_display.h
        glfw_display.cpp
        glfw_registrar.cpp
      vnc/                   ← Not yet implemented (Phase 2)
```

The main `CMakeLists.txt` conditionally includes each backend
subdirectory and adds its object files to `gpu_lib_mock_dumbfb`:

```cmake
add_library(gpu_lib_mock_dumbfb STATIC
    src/display_factory.cpp
    src/backend_registry.cpp
    src/c_api.cpp
    src/error_store.cpp
)

if(FEATURE_BACKEND_SDL3)
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        add_subdirectory(backends/sdl3)
        target_link_libraries(gpu_lib_mock_dumbfb PRIVATE gpu_backend_sdl3)
        target_compile_definitions(gpu_lib_mock_dumbfb PRIVATE PGPU_HAS_BACKEND_SDL3)
        message(STATUS "PixelGPU: SDL3 backend enabled")
    else()
        message(STATUS "PixelGPU: SDL3 not found, SDL3 backend disabled")
    endif()
endif()
# … repeat for GLFW, VNC, etc.
```

### 9.3 vcpkg Dependencies

Current `vcpkg.json`:

```json
"dependencies": [
    { "name": "catch2", "version>=": "3.8.0" },
    { "name": "fmt",    "version>=": "11.0.2" },
    { "name": "sdl3"  },
    { "name": "glfw3" }
],
"features": {
    "tests": {
        "description": "Build the tests of the package's tests",
        "dependencies": [
            { "name": "catch2", "version>=": "3.8.0" }
        ]
    }
}
```

SDL3 and GLFW are unconditional baseline dependencies because they are
the primary backends on all three tier-1 platforms (Windows, Linux, macOS).

**Pending for Phase 2:** `libspng` will be added when bezel PNG loading is
implemented (see Section 8). The VNC backend will be gated behind a vcpkg
feature (`backend-vnc`) so it is omittable by users who do not pass
`--x-feature=backend-vnc`.

### 9.4 PNG Loading Library

**Recommendation: `libspng`**

Comparison at this scope:

| Library    | Type         | Alpha access | API simplicity | vcpkg support | Notes |
|------------|--------------|--------------|----------------|---------------|-------|
| stb_image  | Header-only  | Yes          | Very high      | Yes           | No alpha-only decode; pulls entire image as RGBA |
| libpng     | C library    | Full control | Low (verbose)  | Yes           | Requires manual struct management |
| lodepng    | Single file  | Yes          | High           | Partial       | No vcpkg port maintained |
| **libspng**| C library    | Full control | Medium-high    | **Yes**       | Modern API, safe defaults, fast |

`libspng` provides direct access to the raw alpha channel needed by the
transparent-region detector, has a clean modern C API, is actively
maintained, and has a first-class vcpkg port. `stb_image` is simpler
but forces an RGBA decode that wastes memory; the bezel detector only
needs the alpha channel. `libpng` works but the API is needlessly
complex for this use case.

---

## 10. Namespace and Naming Conventions

| Context         | Convention              | Example                          |
|-----------------|-------------------------|----------------------------------|
| C++ namespace   | `pixelgpu::`            | `pixelgpu::DisplayFactory`       |
| C++ internals   | `pixelgpu::internal::`  | `pixelgpu::internal::BackendRegistry` |
| C API prefix    | `pgpu_`                 | `pgpu_display_create()`          |
| C handle types  | `pgpu_*_t`              | `pgpu_display_t`                 |
| C enum values   | `PGPU_`                 | `PGPU_OK`, `PGPU_ERROR_TIMEOUT`  |
| C callback type | `pgpu_*_fn`             | `pgpu_vsync_callback_fn`         |
| CMake options   | `FEATURE_BACKEND_*`     | `FEATURE_BACKEND_SDL3`           |
| CMake targets   | `gpu_backend_*`         | `gpu_backend_sdl3`               |
| Env variables   | `PIXELGPU_*`            | `PIXELGPU_BEZEL_PATH`            |
| Header include  | `pixelgpu/*.h` (C++)    | `#include <pixelgpu/resolution.h>` |
|                 | `pgpu/pgpu.h` (C)       | `#include <pgpu/pgpu.h>`         |

---

## 11. Phase 1 Implementation Checklist

This section defines the minimum viable scope for the first working
commit.

### Must Have

- [X] `Resolution` struct + `ResolutionPreset` enum + preset table
- [X] `DisplayParams` struct
- [X] `IDisplay` abstract interface
- [X] `BackendRegistry` + `BackendRegistrar` (static init registration)
- [X] `DisplayFactory::create()` with preference-sorted selection
- [X] `DisplayFactory::create()` with named priority-list selection
- [X] Thread-local error string + `pgpu_last_error_*` functions
- [X] `pgpu_display_create()` / `pgpu_display_destroy()` C wrappers
- [X] `pgpu_display_present_now()` C wrapper
- [X] `pgpu_wait_vsync()` C wrapper
- [X] `pgpu_display_set_vsync_callback()` C wrapper
- [X] `pgpu_display_is_alive()` C wrapper
- [X] SDL3 backend: window, XBGR8888 texture, present, vsync timer thread
- [X] GLFW backend: window, OpenGL texture upload, present, vsync timer
- [X] `pgpu_registered_backends()` diagnostic function

### Should Have (Phase 1, but not blocking)

- [X] CMake auto-disable when dependency not found
- [X] Catch2 unit tests: factory selection, C API error paths, resolution presets
- [ ] Catch2 integration test: vsync callback fires (requires a live display; intentionally
      excluded from the unit suite — deferred to a headless-capable integration harness)

### Deferred to Phase 2

- [ ] Bezel PNG loading and region detection
- [ ] Bezel window enlargement
- [ ] VNC backend
- [ ] Any keyboard/mouse input beyond exit detection

---

## 12. Key Design Decisions Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Pixel format | XBGR8888 fixed | Alignment, endianness, simplicity |
| Backend registration | Runtime static init | Extensible, no RTTI required |
| Preference model | Scalar integer | Simple enough for this phase |
| Factory return on failure | `nullptr` + error string | C-compatible, no exceptions |
| Vsync model | Internal timer thread | Clean caller model; tearing is a feature |
| C/C++ boundary | Single `pgpu.h` with `extern "C"` | One header story |
| Handle model | Opaque `pgpu_display_t` over `unique_ptr` | C-safe, no leaks |
| PNG library | `libspng` | Modern C API, alpha access, vcpkg support |
| CMake backends | OBJECT sub-libs, auto-disable if dep missing | Clean, user-controllable |
| SDL version | SDL3 | Long-term investment; SDL2 backend optional later |
| Bezel in Phase 1 | Hooks present, implementation deferred | API stability without blocking delivery |
| Namespace | `pixelgpu::` / `pgpu_` | Consistent, project-branded |

---

*End of architecture document.*

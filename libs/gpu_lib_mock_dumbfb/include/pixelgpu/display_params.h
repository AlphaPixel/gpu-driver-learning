#pragma once

#include "pixelgpu/resolution.h"

#include <cstdint>
#include <string>

// Opaque buffer handle from the C API — forward-declared here so C++ callers
// can store one in DisplayParams without pulling in pgpu.h.
struct pgpu_buffer_opaque;

namespace pixelgpu {

// ---------------------------------------------------------------------------
// DisplayParams
//
// Passed to DisplayFactory::create() to describe the desired display.
// All pointer fields must remain valid for the lifetime of the IDisplay
// object; the library does NOT copy them.
// ---------------------------------------------------------------------------
struct DisplayParams {
    // ── Required ───────────────────────────────────────────────────────────

    // Size of the virtual framebuffer in pixels.
    Resolution resolution;

    // Caller-owned framebuffer memory.  Must be at least
    //   resolution.frame_bytes()  bytes.
    // Format: XBGR8888 (32 bits per pixel, little-endian).
    // The library reads this memory on every present.  It does NOT write
    // into it and does NOT manage its lifetime.
    void* framebuffer_ptr = nullptr;

    // Phase 4 forward hook: handle to a VRAM-backed buffer registered with
    // the device.  NULL (default) = framebuffer_ptr is plain caller-owned
    // heap memory (current Phase 1 behaviour).  Non-NULL = VRAM-backed path
    // introduced in Phase 4 — the backend will ignore framebuffer_ptr and
    // read from the VRAM allocation instead.
    pgpu_buffer_opaque* framebuffer_handle = nullptr;

    // ── Optional — display behaviour ───────────────────────────────────────

    // Target vsync rate in Hz.  0 → use backend default (60 Hz).
    uint32_t refresh_hz = 60;

    // Window title bar text.  Empty string → "PixelGPU".
    std::string window_title = "PixelGPU";

    // ── Optional — backend selection ───────────────────────────────────────

    // Comma-separated list of backend names to try, in priority order.
    // Examples: "sdl3"  or  "sdl3,glfw"  or  "vnc".
    //
    // Empty (default): the factory ranks all registered backends by their
    //   preference score and picks the highest-scoring one that succeeds.
    //
    // Non-empty: ONLY the named backends are considered, in listed order.
    //   If none succeed, create() returns nullptr regardless of what other
    //   backends are registered.
    std::string backend_priority;

    // ── Optional — bezel (Phase 1: stubbed; ignored by all backends) ───────

    // Base filename of a monitor-bezel PNG, WITHOUT aspect suffix or
    // extension.  E.g. "gameboy" → the factory will look for
    // "gameboy_4x3.png" (or the appropriate aspect variant).
    //
    // Empty (default): no bezel.
    //
    // If non-empty and no bezel-capable backend is available, or if
    // the bezel file's aspect ratio does not match the framebuffer,
    // create() returns nullptr.
    std::string text_arg;
};

} // namespace pixelgpu

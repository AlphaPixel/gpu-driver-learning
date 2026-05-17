#pragma once

#include "pixelgpu/resolution.h"

#include <cstdint>
#include <string_view>

namespace pixelgpu {

// ---------------------------------------------------------------------------
// C-compatible vsync callback type.
//
// Invoked on the backend's internal timer thread once per vsync tick.
// user_data is whatever the caller registered; the library does not
// inspect it.  There is no in-callback cancel — to cancel, call
// IDisplay::set_vsync_callback(nullptr, nullptr) from any thread.
// ---------------------------------------------------------------------------
extern "C" {
    using VsyncCallbackFn = void (*)(void* user_data);
}

// ---------------------------------------------------------------------------
// IDisplay
//
// Abstract interface implemented by every display backend.
// Obtained exclusively through DisplayFactory::create(); the caller
// owns the returned std::unique_ptr<IDisplay>.
//
// Thread-safety contract:
//   present_now()           — safe to call from any thread
//   set_vsync_callback()    — safe to call from any thread
//   wait_vsync()            — safe to call from any thread (multiple
//                             callers are all woken on the same tick)
//   is_alive()              — safe to call from any thread
//
//   The framebuffer memory itself has NO thread-safety guarantee from
//   this library.  Tearing and race conditions are intentional teaching
//   material for later curriculum phases.
// ---------------------------------------------------------------------------
class IDisplay {
public:
    virtual ~IDisplay() = default;

    // Copy/move disabled — displays are unique resources.
    IDisplay(const IDisplay&)            = delete;
    IDisplay& operator=(const IDisplay&) = delete;
    IDisplay(IDisplay&&)                 = delete;
    IDisplay& operator=(IDisplay&&)      = delete;

    // ── Event pump ─────────────────────────────────────────────────────────

    // Process pending window events (close button, keyboard, resize, etc.).
    //
    // MUST be called from the same thread that created the display (i.e. the
    // thread that called DisplayFactory::create() / pgpu_display_create()).
    // On Windows this is a hard requirement: Win32 delivers messages to the
    // thread that owns the HWND, and only that thread can drain them.
    //
    // The C API calls this automatically from pgpu_wait_vsync(), which is
    // already invoked from the application's main thread in the render loop.
    // Direct C++ callers should call it themselves once per frame.
    //
    // Default implementation is a no-op (for headless / remote backends that
    // have no window-system event queue).
    virtual void pump_events() noexcept {}

    // ── Presentation ───────────────────────────────────────────────────────

    // Blit the current framebuffer contents to the window immediately.
    // Does NOT affect the vsync timer phase or counter.
    // For callers who do not yet care about regulated timing.
    virtual void present_now() = 0;

    // ── Vsync ──────────────────────────────────────────────────────────────

    // Register a vsync callback.  Pass nullptr for cb to unregister.
    // The callback is invoked on the backend's internal timer thread.
    // Thread-safe; last call wins.
    virtual void set_vsync_callback(VsyncCallbackFn cb,
                                    void*           user_data) noexcept = 0;

    // Block the calling thread until the next vsync tick fires, or until
    // timeout_ms milliseconds elapse.
    // Returns true  if a vsync tick fired.
    // Returns false if timeout elapsed before a tick.
    virtual bool wait_vsync(uint32_t timeout_ms) = 0;

    // ── Status ─────────────────────────────────────────────────────────────

    // Returns true while the display window is open and the backend's
    // internal pump is healthy.  Becomes false when:
    //   - the user closes the window
    //   - the user presses Escape
    //   - the backend encounters an unrecoverable error
    virtual bool is_alive() const noexcept = 0;

    // ── Metadata ───────────────────────────────────────────────────────────

    virtual Resolution       resolution()   const noexcept = 0;
    virtual uint32_t         refresh_hz()   const noexcept = 0;

    // Short lowercase identifier, e.g. "sdl3", "glfw", "vnc".
    virtual std::string_view backend_name() const noexcept = 0;

protected:
    IDisplay() = default;
};

} // namespace pixelgpu

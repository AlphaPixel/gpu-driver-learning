#include "pgpu/pgpu.h"

#include "pixelgpu/display_factory.h"
#include "pixelgpu/display_params.h"
#include "pixelgpu/idisplay.h"
#include "pixelgpu/resolution.h"
#include "backend_registry.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Handle table
//
// Maps opaque pgpu_display_t → owned IDisplay.
// pgpu_display_t is typed as pgpu_display_opaque*, but the actual pointer
// value is a uintptr_t key into this table — we never dereference it as
// a real pointer.  This lets the C side hold an opaque, non-dereferenceable
// token while the C++ side owns the real object.
//
// Using a map of unique_ptr means destroy() is the only place lifetimes
// are managed, making leaks and double-frees straightforward to reason about.
// ---------------------------------------------------------------------------

// The opaque struct is declared in the header but never defined.
// We use its pointer purely as a numeric token.  The actual storage is
// in the handle table below.

namespace {

struct HandleEntry {
    // owned_fb must be declared BEFORE display so that its destructor runs AFTER
    // display's destructor.  ~IDisplay() joins the render thread, which must
    // complete before the framebuffer memory it points into is freed.
    // (C++ destroys members in reverse-declaration order.)
    std::unique_ptr<uint8_t[]>          owned_fb;  // null when caller owns the buffer
    std::unique_ptr<pixelgpu::IDisplay> display;
    void*                               fb_ptr = nullptr;  // always the live framebuffer
};

struct HandleTable {
    std::mutex m;
    std::unordered_map<uintptr_t, HandleEntry> table;
    uintptr_t next_key = 1; // 0 is reserved for "null handle"

    pgpu_display_t insert(std::unique_ptr<pixelgpu::IDisplay> display,
                          std::unique_ptr<uint8_t[]>           owned_fb,
                          void*                                fb_ptr) {
        std::lock_guard lock(m);
        const uintptr_t key = next_key++;
        table[key] = HandleEntry{ std::move(owned_fb), std::move(display), fb_ptr };
        return reinterpret_cast<pgpu_display_t>(key); // NOLINT
    }

    pixelgpu::IDisplay* get(pgpu_display_t h) {
        if (!h) return nullptr;
        std::lock_guard lock(m);
        const auto key = reinterpret_cast<uintptr_t>(h); // NOLINT
        const auto it  = table.find(key);
        return (it != table.end()) ? it->second.display.get() : nullptr;
    }

    void* get_fb(pgpu_display_t h) {
        if (!h) return nullptr;
        std::lock_guard lock(m);
        const auto key = reinterpret_cast<uintptr_t>(h); // NOLINT
        const auto it  = table.find(key);
        return (it != table.end()) ? it->second.fb_ptr : nullptr;
    }

    bool erase(pgpu_display_t h) {
        if (!h) return false;
        std::lock_guard lock(m);
        const auto key = reinterpret_cast<uintptr_t>(h); // NOLINT
        return table.erase(key) > 0;
    }
};

HandleTable& handles() noexcept {
    static HandleTable s;
    return s;
}

// ── Thread-local error store ────────────────────────────────────────────────

thread_local pgpu_status_t tl_error_code   = PGPU_OK;
thread_local std::string   tl_error_string = "";

void set_error(pgpu_status_t code, std::string msg) noexcept {
    tl_error_code   = code;
    tl_error_string = std::move(msg);
}

void clear_error() noexcept {
    tl_error_code   = PGPU_OK;
    tl_error_string.clear();
}

// Diagnostic backend summary — stored in a mutex-protected string so it
// survives across thread boundaries long enough for the caller to read it.
std::mutex          g_summary_mutex;
std::string         g_summary_buf;

} // anonymous namespace

// ---------------------------------------------------------------------------
extern "C" {
// ---------------------------------------------------------------------------

pgpu_resolution_t pgpu_resolution_from_preset(pgpu_resolution_preset_t preset) {
    const auto res = pixelgpu::resolution_from_preset(
        static_cast<pixelgpu::ResolutionPreset>(preset));
    return pgpu_resolution_t{ res.width, res.height };
}

pgpu_resolution_t pgpu_resolution_make(uint32_t width, uint32_t height) {
    return pgpu_resolution_t{ width, height };
}

// ---------------------------------------------------------------------------
pgpu_display_t pgpu_display_create(
    pgpu_resolution_t  resolution,
    void*              framebuffer_ptr,
    uint32_t           refresh_hz,
    const char*        window_title,
    const char*        backend_priority,
    const char*        text_arg)
{
    clear_error();

    // When no caller-provided framebuffer, allocate and own one internally.
    std::unique_ptr<uint8_t[]> owned_fb;
    if (!framebuffer_ptr) {
        const std::size_t fb_size = static_cast<std::size_t>(
            static_cast<uint64_t>(resolution.width) * resolution.height * 4u);
        if (fb_size == 0) {
            set_error(PGPU_ERROR_BAD_PARAMS,
                      "pgpu_display_create: zero-size resolution.");
            return nullptr;
        }
        owned_fb        = std::make_unique<uint8_t[]>(fb_size); // value-init → zeroed
        framebuffer_ptr = owned_fb.get();
    }

    pixelgpu::DisplayParams params;
    params.resolution       = pixelgpu::Resolution{ resolution.width, resolution.height };
    params.framebuffer_ptr  = framebuffer_ptr;
    params.refresh_hz       = (refresh_hz == 0) ? 60u : refresh_hz;
    params.window_title     = (window_title && *window_title) ? window_title : "PixelGPU";
    params.backend_priority = (backend_priority && *backend_priority) ? backend_priority : "";
    params.text_arg         = (text_arg  && *text_arg)  ? text_arg  : "";

    auto display = pixelgpu::DisplayFactory::create(params);
    if (!display) {
        const auto err = std::string(pixelgpu::DisplayFactory::last_error_string());
        set_error(PGPU_ERROR_BACKEND_FAILED,
                  err.empty() ? "DisplayFactory::create() returned null." : err);
        return nullptr; // owned_fb freed here if we allocated it
    }

    return handles().insert(std::move(display), std::move(owned_fb), framebuffer_ptr);
}

// ---------------------------------------------------------------------------
void pgpu_display_destroy(pgpu_display_t handle) {
    // Erasing the entry drops both the IDisplay and any owned framebuffer.
    handles().erase(handle);
}

// ---------------------------------------------------------------------------
void* pgpu_display_get_framebuffer(pgpu_display_t handle) {
    return handles().get_fb(handle);
}

// ---------------------------------------------------------------------------
void pgpu_display_present_now(pgpu_display_t handle) {
    clear_error();
    auto* d = handles().get(handle);
    if (!d) {
        set_error(PGPU_ERROR_INVALID_HANDLE, "pgpu_display_present_now: invalid handle.");
        return;
    }
    d->present_now();
}

// ---------------------------------------------------------------------------
void pgpu_display_set_vsync_callback(
    pgpu_display_t         handle,
    pgpu_vsync_callback_fn callback,
    void*                  user_data)
{
    clear_error();
    auto* d = handles().get(handle);
    if (!d) {
        set_error(PGPU_ERROR_INVALID_HANDLE, "pgpu_display_set_vsync_callback: invalid handle.");
        return;
    }
    d->set_vsync_callback(callback, user_data);
}

// ---------------------------------------------------------------------------
pgpu_status_t pgpu_wait_vsync(pgpu_display_t handle, uint32_t timeout_ms) {
    clear_error();
    auto* d = handles().get(handle);
    if (!d) {
        set_error(PGPU_ERROR_INVALID_HANDLE, "pgpu_wait_vsync: invalid handle.");
        return PGPU_ERROR_INVALID_HANDLE;
    }
    // Pump window events on the calling thread (must be the main thread).
    // This keeps the window responsive — close button, Escape key, drag, etc.
    d->pump_events();
    const bool fired = d->wait_vsync(timeout_ms);
    return fired ? PGPU_OK : PGPU_TIMEOUT;
}

// ---------------------------------------------------------------------------
int pgpu_display_is_alive(pgpu_display_t handle) {
    auto* d = handles().get(handle);
    if (!d) return 0;
    return d->is_alive() ? 1 : 0;
}

// ---------------------------------------------------------------------------
pgpu_status_t pgpu_last_error_code(void) {
    return tl_error_code;
}

const char* pgpu_last_error_string(void) {
    return tl_error_string.c_str();
}

// ---------------------------------------------------------------------------
const char* pgpu_registered_backends(void) {
    std::lock_guard lock(g_summary_mutex);
    g_summary_buf = pixelgpu::DisplayFactory::registered_backends_summary();
    return g_summary_buf.c_str();
}

// ---------------------------------------------------------------------------
const char* pgpu_display_backend_name(pgpu_display_t handle) {
    auto* d = handles().get(handle);
    if (!d) return "";
    return d->backend_name().data();
}

// ---------------------------------------------------------------------------
} // extern "C"

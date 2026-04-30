#pragma once

#include "pixelgpu/idisplay.h"
#include "pixelgpu/display_params.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>

namespace pixelgpu::backend {

// ---------------------------------------------------------------------------
// SDL3Display
//
// IDisplay implementation backed by an SDL3 window and streaming texture.
//
// The framebuffer (XBGR8888) is copied into an SDL_Texture on every
// present.  SDL3's SDL_PIXELFORMAT_XBGR8888 matches our layout directly,
// so no per-pixel conversion is needed.
//
// Vsync is driven by an internal std::thread that sleeps on a high-resolution
// monotonic clock and fires once per nominal refresh period.  This is a
// software timer — it does not lock to the display's hardware vsync signal.
// That is intentional: students observe timing imprecision and
// understand why real drivers use hardware vsync events.
//
// Event pumping (SDL_PollEvent) must run on the thread that created the SDL
// window (the main thread).  It is invoked via pump_events(), which the C API
// calls automatically from pgpu_wait_vsync() once per frame.
// ---------------------------------------------------------------------------
class SDL3Display final : public IDisplay {
public:
    // Factory — called by the backend registry.
    // Returns nullptr if SDL3 cannot be initialised on this platform,
    // or if a window/renderer cannot be created at the requested resolution.
    static std::unique_ptr<IDisplay> try_create(const DisplayParams& params) noexcept;

    ~SDL3Display() override;

    // IDisplay interface
    void         pump_events()  noexcept override;
    void         present_now()  override;
    void         set_vsync_callback(VsyncCallbackFn cb, void* user_data) noexcept override;
    bool         wait_vsync(uint32_t timeout_ms) override;
    bool         is_alive()     const noexcept override;
    Resolution   resolution()   const noexcept override;
    uint32_t     refresh_hz()   const noexcept override;
    std::string_view backend_name() const noexcept override;

private:
    // Private constructor — use try_create().
    explicit SDL3Display(const DisplayParams& params);

    // Initialise SDL3 subsystems and create window + renderer + texture.
    // Returns false on any failure; leaves a message in m_init_error.
    bool init(const DisplayParams& params);

    // Background thread body
    void vsync_thread_body();    // fires vsync ticks

    // Notify all wait_vsync() callers and fire the callback (if any).
    void fire_vsync_tick();

    // ── SDL3 resources (main thread ownership) ────────────────────────────
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture*  m_texture  = nullptr;

    // ── Framebuffer reference (not owned) ─────────────────────────────────
    void*    m_framebuffer_ptr = nullptr;
    uint32_t m_width           = 0;
    uint32_t m_height          = 0;
    uint32_t m_refresh_hz      = 60;

    // ── Liveness ──────────────────────────────────────────────────────────
    std::atomic<bool> m_alive{ true };

    // ── Vsync machinery ───────────────────────────────────────────────────
    std::thread m_vsync_thread;

    // Protects m_vsync_cb and m_vsync_user_data.
    mutable std::mutex  m_cb_mutex;
    VsyncCallbackFn     m_vsync_cb        = nullptr;
    void*               m_vsync_user_data = nullptr;

    // Condition variable + counter for wait_vsync().
    mutable std::mutex              m_tick_mutex;
    std::condition_variable         m_tick_cv;
    uint64_t                        m_tick_count = 0;

    // Guards SDL rendering calls between present_now() and the vsync
    // thread (if both are blitting simultaneously).
    mutable std::mutex m_render_mutex;

    std::string m_init_error;
};

} // namespace pixelgpu::backend

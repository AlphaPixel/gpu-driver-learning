#include "sdl3_display.h"
#include "backend_registry.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstring>
#include <string>

namespace pixelgpu::backend {

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// try_create  (static factory)
// ---------------------------------------------------------------------------
std::unique_ptr<IDisplay> SDL3Display::try_create(const DisplayParams& params) noexcept {
    try {
        auto d = std::unique_ptr<SDL3Display>(new SDL3Display(params));
        if (!d->m_init_error.empty()) {
            pixelgpu::internal::BackendRegistry::instance()
                .set_last_error(d->m_init_error);
            return nullptr;
        }
        return d;
    } catch (const std::exception& ex) {
        pixelgpu::internal::BackendRegistry::instance()
            .set_last_error(std::string("SDL3Display exception: ") + ex.what());
        return nullptr;
    } catch (...) {
        pixelgpu::internal::BackendRegistry::instance()
            .set_last_error("SDL3Display: unknown exception during construction.");
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Constructor / init
// ---------------------------------------------------------------------------
SDL3Display::SDL3Display(const DisplayParams& params) {
    m_framebuffer_ptr = params.framebuffer_ptr;
    m_width           = params.resolution.width;
    m_height          = params.resolution.height;
    m_refresh_hz      = (params.refresh_hz == 0) ? 60u : params.refresh_hz;

    if (!init(params)) return; // m_init_error set by init()

    // Start the vsync timer thread.  Event pumping happens on the calling
    // thread via pump_events() — no separate event thread needed.
    m_vsync_thread = std::thread(&SDL3Display::vsync_thread_body, this);
}

bool SDL3Display::init(const DisplayParams& params) {
    // SDL3 uses SDL_Init flags differently from SDL2.
    // SDL_INIT_VIDEO is sufficient for a window + renderer.
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        m_init_error = std::string("SDL_InitSubSystem(VIDEO) failed: ")
                       + SDL_GetError();
        return false;
    }

    const std::string title =
        params.window_title.empty() ? "PixelGPU" : params.window_title;

    m_window = SDL_CreateWindow(
        title.c_str(),
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        0 // flags: no fullscreen, no resizable
    );
    if (!m_window) {
        m_init_error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    // Use the default renderer (hardware-accelerated when available,
    // software fallback otherwise).  We do not request vsync from SDL3
    // because we drive our own software timer.
    m_renderer = SDL_CreateRenderer(m_window, nullptr);
    if (!m_renderer) {
        m_init_error = std::string("SDL_CreateRenderer failed: ") + SDL_GetError();
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    // SDL3 pixel format names list components MSB→LSB in the 32-bit value,
    // which is the reverse of byte order on little-endian systems.
    // Our framebuffer is [B][G][R][X] in memory → XRGB8888 in SDL3 naming.
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_XRGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        static_cast<int>(m_width),
        static_cast<int>(m_height)
    );
    if (!m_texture) {
        m_init_error = std::string("SDL_CreateTexture failed: ") + SDL_GetError();
        SDL_DestroyRenderer(m_renderer);
        SDL_DestroyWindow(m_window);
        m_renderer = nullptr;
        m_window   = nullptr;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
SDL3Display::~SDL3Display() {
    // Signal threads to stop.
    m_alive.store(false, std::memory_order_release);

    // Unblock any wait_vsync() callers.
    {
        std::lock_guard lock(m_tick_mutex);
        ++m_tick_count;
    }
    m_tick_cv.notify_all();

    if (m_vsync_thread.joinable()) m_vsync_thread.join();

    // Release SDL resources on whichever thread destructs us.
    // SDL itself is not strictly single-threaded for resource destruction
    // (resources can be freed from any thread in SDL3).
    if (m_texture)  { SDL_DestroyTexture(m_texture);   m_texture  = nullptr; }
    if (m_renderer) { SDL_DestroyRenderer(m_renderer); m_renderer = nullptr; }
    if (m_window)   { SDL_DestroyWindow(m_window);     m_window   = nullptr; }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

// ---------------------------------------------------------------------------
// present_now
// ---------------------------------------------------------------------------
void SDL3Display::present_now() {
    if (!m_alive.load(std::memory_order_acquire)) return;

    std::lock_guard lock(m_render_mutex);

    // Upload framebuffer into the streaming texture.
    const int pitch = static_cast<int>(m_width * 4);
    if (!SDL_UpdateTexture(m_texture, nullptr, m_framebuffer_ptr, pitch)) {
        // Non-fatal; skip this frame.
        return;
    }

    SDL_RenderClear(m_renderer);
    SDL_RenderTexture(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);
}

// ---------------------------------------------------------------------------
// Vsync callback registration
// ---------------------------------------------------------------------------
void SDL3Display::set_vsync_callback(VsyncCallbackFn cb, void* user_data) noexcept {
    std::lock_guard lock(m_cb_mutex);
    m_vsync_cb        = cb;
    m_vsync_user_data = user_data;
}

// ---------------------------------------------------------------------------
// wait_vsync
// ---------------------------------------------------------------------------
bool SDL3Display::wait_vsync(uint32_t timeout_ms) {
    const uint64_t tick_before = [&] {
        std::lock_guard lock(m_tick_mutex);
        return m_tick_count;
    }();

    std::unique_lock lock(m_tick_mutex);
    return m_tick_cv.wait_for(lock,
        std::chrono::milliseconds(timeout_ms),
        [&] { return m_tick_count != tick_before; }
    );
}

// ---------------------------------------------------------------------------
// fire_vsync_tick  (called from vsync_thread_body)
// ---------------------------------------------------------------------------
void SDL3Display::fire_vsync_tick() {
    // 1. Wake all wait_vsync() callers.
    {
        std::lock_guard lock(m_tick_mutex);
        ++m_tick_count;
    }
    m_tick_cv.notify_all();

    // 2. Invoke the registered callback (if any).
    VsyncCallbackFn cb   = nullptr;
    void*           data = nullptr;
    {
        std::lock_guard lock(m_cb_mutex);
        cb   = m_vsync_cb;
        data = m_vsync_user_data;
    }
    if (cb) cb(data);
}

// ---------------------------------------------------------------------------
// vsync_thread_body
//
// Software timer.  Sleeps for one refresh period then fires a tick.
// Uses a steady_clock to accumulate phase — this prevents drift from
// compounding sleep imprecision over many frames.
// ---------------------------------------------------------------------------
void SDL3Display::vsync_thread_body() {
    using Clock  = std::chrono::steady_clock;
    using Period = std::chrono::duration<double>;

    const Period frame_period{ 1.0 / static_cast<double>(m_refresh_hz) };
    auto next_tick = Clock::now() + frame_period;

    while (m_alive.load(std::memory_order_acquire)) {
        std::this_thread::sleep_until(next_tick);
        if (!m_alive.load(std::memory_order_acquire)) break;

        fire_vsync_tick();
        next_tick += frame_period;
    }
}

// ---------------------------------------------------------------------------
// pump_events  (must be called from the thread that created the window)
// ---------------------------------------------------------------------------
void SDL3Display::pump_events() noexcept {
    // SDL_PollEvent must run on the thread that created the SDL window.
    // On Windows, SDL windows are backed by Win32 HWNDs whose message queue
    // is thread-affine; pumping from a different thread leaves the window
    // frozen (busy cursor, no drag, no key events).
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            m_alive.store(false, std::memory_order_release);
        } else if (ev.type == SDL_EVENT_KEY_DOWN) {
            if (ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                m_alive.store(false, std::memory_order_release);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------
bool         SDL3Display::is_alive()     const noexcept { return m_alive.load(std::memory_order_acquire); }
Resolution   SDL3Display::resolution()   const noexcept { return Resolution{ m_width, m_height }; }
uint32_t     SDL3Display::refresh_hz()   const noexcept { return m_refresh_hz; }
std::string_view SDL3Display::backend_name() const noexcept { return "sdl3"; }

} // namespace pixelgpu::backend

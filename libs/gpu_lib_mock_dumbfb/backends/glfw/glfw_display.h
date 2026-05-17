#pragma once

#include "pixelgpu/idisplay.h"
#include "pixelgpu/display_params.h"

// GLFW requires an OpenGL context for rendering.  We use a minimal
// OpenGL 2.1-compatible path (glTexImage2D / glDrawPixels alternative)
// so the backend works on the widest range of hardware without needing
// modern GLSL.  A GL 2.1 core texture-blit path is used:
//   1. Upload XBGR8888 framebuffer to a GL_RGBA texture.
//   2. Draw a full-screen quad with that texture.
//
// GLFW3 is included before any GL header so it can define its own
// GL-version guards.

// GLFW_INCLUDE_NONE prevents GLFW from pulling in the system GL header
// — we include it explicitly after.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Minimal GL declarations we need (subset of gl.h / gl2.h).
// Including the full system GL header here would create platform-specific
// guard ordering issues.  We forward-declare only what we call.
#if defined(_WIN32)
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif

// GL 1.2+ constants not present in Windows' GL/gl.h (which only covers 1.1).
#ifndef GL_CLAMP_TO_EDGE
#  define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_BGRA
#  define GL_BGRA 0x80E1
#endif

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>

namespace pixelgpu::backend {

// ---------------------------------------------------------------------------
// GLFWDisplay
//
// IDisplay implementation backed by a GLFW window and an OpenGL texture.
//
// Like SDL3Display, vsync is driven by a software timer thread rather than
// hardware vsync.  The GL texture upload and blit happen on a dedicated
// render thread because GLFW requires that all GL calls occur on the
// thread that holds the context current.
//
// The GLFW event pump (glfwPollEvents) MUST run on the main thread on
// macOS.  To support both macOS and other platforms from a library context
// (where we cannot guarantee which thread is "main"), we run the event pump
// on the thread that constructs the GLFWDisplay — i.e. the thread that
// called DisplayFactory::create().  This is documented as a constraint.
//
// Callers on macOS must call pgpu_display_create() from the main thread.
// ---------------------------------------------------------------------------
class GLFWDisplay final : public IDisplay {
public:
    static std::unique_ptr<IDisplay> try_create(const DisplayParams& params) noexcept;

    ~GLFWDisplay() override;

    void         pump_events()   noexcept override;
    void         present_now()   override;
    void         set_vsync_callback(VsyncCallbackFn cb, void* user_data) noexcept override;
    bool         wait_vsync(uint32_t timeout_ms) override;
    bool         is_alive()    const noexcept override;
    Resolution   resolution()  const noexcept override;
    uint32_t     refresh_hz()  const noexcept override;
    std::string_view backend_name() const noexcept override;

private:
    explicit GLFWDisplay(const DisplayParams& params);
    bool init(const DisplayParams& params);

    void vsync_thread_body();
    void render_thread_body();   // owns the GL context
    void fire_vsync_tick();

    // GLFW key callback — registered in init(); invoked during glfwPollEvents()
    static void key_callback(GLFWwindow* window, int key, int scancode,
                             int action, int mods) noexcept;

    // Upload the framebuffer to the GL texture and present.
    // Called from render_thread_body or, if the render thread isn't up yet,
    // directly from the calling thread.
    void upload_and_blit();

    GLFWwindow* m_window = nullptr;
    GLuint      m_texture_id = 0;

    void*    m_framebuffer_ptr = nullptr;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    uint32_t m_refresh_hz = 60;

    std::atomic<bool> m_alive{ true };

    // Signal the render thread to blit (set by present_now())
    std::mutex              m_present_mutex;
    std::condition_variable m_present_cv;
    bool                    m_present_requested = false;

    std::thread m_vsync_thread;
    std::thread m_render_thread;

    mutable std::mutex m_cb_mutex;
    VsyncCallbackFn    m_vsync_cb        = nullptr;
    void*              m_vsync_user_data = nullptr;

    mutable std::mutex      m_tick_mutex;
    std::condition_variable m_tick_cv;
    uint64_t                m_tick_count = 0;

    // Render thread signals this once its GL init is complete.
    std::mutex              m_render_ready_mutex;
    std::condition_variable m_render_ready_cv;
    bool                    m_render_ready = false;
    std::string             m_render_error;

    std::string m_init_error;
};

} // namespace pixelgpu::backend

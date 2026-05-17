#include "glfw_display.h"
#include "backend_registry.h"

#include <chrono>
#include <stdexcept>
#include <string>

namespace pixelgpu::backend {

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// GLFW error callback (global; only one can be registered)
// ---------------------------------------------------------------------------
namespace {
    void glfw_error_cb(int /*code*/, const char* desc) {
        // Stored in the registry for retrieval by try_create().
        pixelgpu::internal::BackendRegistry::instance()
            .set_last_error(std::string("GLFW error: ") + desc);
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// try_create
// ---------------------------------------------------------------------------
std::unique_ptr<IDisplay> GLFWDisplay::try_create(const DisplayParams& params) noexcept {
    try {
        auto d = std::unique_ptr<GLFWDisplay>(new GLFWDisplay(params));
        if (!d->m_init_error.empty()) {
            pixelgpu::internal::BackendRegistry::instance()
                .set_last_error(d->m_init_error);
            return nullptr;
        }
        return d;
    } catch (const std::exception& ex) {
        pixelgpu::internal::BackendRegistry::instance()
            .set_last_error(std::string("GLFWDisplay exception: ") + ex.what());
        return nullptr;
    } catch (...) {
        pixelgpu::internal::BackendRegistry::instance()
            .set_last_error("GLFWDisplay: unknown exception during construction.");
        return nullptr;
    }
}

// ---------------------------------------------------------------------------
// Constructor / init
// ---------------------------------------------------------------------------
GLFWDisplay::GLFWDisplay(const DisplayParams& params) {
    m_framebuffer_ptr = params.framebuffer_ptr;
    m_width           = params.resolution.width;
    m_height          = params.resolution.height;
    m_refresh_hz      = (params.refresh_hz == 0) ? 60u : params.refresh_hz;

    if (!init(params)) return;

    // Start the render thread first; it creates the GL context and texture.
    m_render_thread = std::thread(&GLFWDisplay::render_thread_body, this);

    // Wait until the render thread finishes GL initialisation (or fails).
    {
        std::unique_lock lock(m_render_ready_mutex);
        m_render_ready_cv.wait(lock, [this] { return m_render_ready; });
    }
    if (!m_render_error.empty()) {
        m_init_error = m_render_error;
        m_alive.store(false, std::memory_order_release);
        if (m_render_thread.joinable()) m_render_thread.join();
        return;
    }

    m_vsync_thread = std::thread(&GLFWDisplay::vsync_thread_body, this);
}

bool GLFWDisplay::init(const DisplayParams& params) {
    glfwSetErrorCallback(glfw_error_cb);

    if (!glfwInit()) {
        m_init_error = "glfwInit() failed. "
                       "Check GLFW error via pgpu_last_error_string().";
        return false;
    }

    // Request a compatibility context — we only need GL 2.1 for texture blit.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    const std::string title =
        params.window_title.empty() ? "PixelGPU" : params.window_title;

    m_window = glfwCreateWindow(
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        title.c_str(),
        nullptr,  // not fullscreen
        nullptr   // no shared context
    );
    if (!m_window) {
        m_init_error = "glfwCreateWindow() failed.";
        glfwTerminate();
        return false;
    }

    // Detach the GL context from this thread immediately; the render thread
    // will make it current there.
    glfwMakeContextCurrent(nullptr);

    // Store this pointer so key_callback can reach us, then register.
    // Both calls must happen on the main thread — we're in init(), which is
    // always called from the thread that called pgpu_display_create().
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, key_callback);

    return true;
}

// ---------------------------------------------------------------------------
// key_callback  (static; invoked by glfwPollEvents on whichever thread calls it)
// ---------------------------------------------------------------------------
void GLFWDisplay::key_callback(GLFWwindow* window, int key, int /*scancode*/,
                               int action, int /*mods*/) noexcept {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        auto* self = static_cast<GLFWDisplay*>(glfwGetWindowUserPointer(window));
        if (self) self->m_alive.store(false, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------
GLFWDisplay::~GLFWDisplay() {
    m_alive.store(false, std::memory_order_release);

    // Unblock wait_vsync() callers.
    { std::lock_guard lock(m_tick_mutex); ++m_tick_count; }
    m_tick_cv.notify_all();

    // Unblock the render thread if it is waiting for a present signal.
    { std::lock_guard lock(m_present_mutex); m_present_requested = true; }
    m_present_cv.notify_all();

    if (m_vsync_thread.joinable()) m_vsync_thread.join();
    if (m_render_thread.joinable()) m_render_thread.join();

    // GLFW window destruction and termination must happen after the render
    // thread (which holds the GL context) has exited.
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

// ---------------------------------------------------------------------------
// render_thread_body
//
// Owns the GL context for the lifetime of the display.  Waits for present
// requests and executes them.
// ---------------------------------------------------------------------------
void GLFWDisplay::render_thread_body() {
    // Make the GL context current on this thread.
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(0); // We drive our own vsync; disable driver-level sync.

    // Allocate a GL texture for framebuffer upload.
    glGenTextures(1, &m_texture_id);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    // Allocate texture storage (initial data = nullptr).
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(m_width),
                 static_cast<GLsizei>(m_height),
                 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    // Signal that GL init is complete.
    {
        std::lock_guard lock(m_render_ready_mutex);
        m_render_ready = true;
    }
    m_render_ready_cv.notify_one();

    // Render loop: wait for present requests or shutdown.
    while (m_alive.load(std::memory_order_acquire)) {
        {
            std::unique_lock lock(m_present_mutex);
            m_present_cv.wait(lock, [this] {
                return m_present_requested || !m_alive.load(std::memory_order_acquire);
            });
            m_present_requested = false;
        }

        if (!m_alive.load(std::memory_order_acquire)) break;
        upload_and_blit();
    }

    // Cleanup GL resources before releasing the context.
    if (m_texture_id) {
        glDeleteTextures(1, &m_texture_id);
        m_texture_id = 0;
    }
    glfwMakeContextCurrent(nullptr);
}

// ---------------------------------------------------------------------------
// upload_and_blit  (called from render_thread_body; GL context is current)
// ---------------------------------------------------------------------------
void GLFWDisplay::upload_and_blit() {
    // NOTE: glfwPollEvents() is intentionally NOT called here.
    // This function runs on the render thread, which does not own the HWND.
    // Event pumping happens via pump_events(), which is called on the main
    // thread from pgpu_wait_vsync().

    // Upload XBGR8888 framebuffer. GL_BGRA / GL_UNSIGNED_BYTE matches our
    // in-memory layout on little-endian systems (XBGR8888 == BGRA in GL).
    glBindTexture(GL_TEXTURE_2D, m_texture_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    0, 0,
                    static_cast<GLsizei>(m_width),
                    static_cast<GLsizei>(m_height),
                    GL_BGRA, GL_UNSIGNED_BYTE,
                    m_framebuffer_ptr);

    // Full-screen textured quad using fixed-function GL 2.1.
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture_id);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 0, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 1.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glfwSwapBuffers(m_window);
}

// ---------------------------------------------------------------------------
// pump_events  (must be called from the thread that created the window)
// ---------------------------------------------------------------------------
void GLFWDisplay::pump_events() noexcept {
    // glfwPollEvents() must run on the thread that created the GLFW window.
    // On Windows the Win32 message queue is thread-affine: messages for an
    // HWND are only delivered to the thread that owns it.  Calling this from
    // a background thread (as was done in upload_and_blit) silently does
    // nothing useful and leaves the window frozen.
    glfwPollEvents();
    if (m_window && glfwWindowShouldClose(m_window)) {
        m_alive.store(false, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// present_now
// ---------------------------------------------------------------------------
void GLFWDisplay::present_now() {
    if (!m_alive.load(std::memory_order_acquire)) return;
    {
        std::lock_guard lock(m_present_mutex);
        m_present_requested = true;
    }
    m_present_cv.notify_one();
}

// ---------------------------------------------------------------------------
// vsync_thread_body
// ---------------------------------------------------------------------------
void GLFWDisplay::vsync_thread_body() {
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
// fire_vsync_tick
// ---------------------------------------------------------------------------
void GLFWDisplay::fire_vsync_tick() {
    { std::lock_guard lock(m_tick_mutex); ++m_tick_count; }
    m_tick_cv.notify_all();

    VsyncCallbackFn cb   = nullptr;
    void*           data = nullptr;
    { std::lock_guard lock(m_cb_mutex); cb = m_vsync_cb; data = m_vsync_user_data; }
    if (cb) cb(data);
}

// ---------------------------------------------------------------------------
// wait_vsync
// ---------------------------------------------------------------------------
bool GLFWDisplay::wait_vsync(uint32_t timeout_ms) {
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
// set_vsync_callback
// ---------------------------------------------------------------------------
void GLFWDisplay::set_vsync_callback(VsyncCallbackFn cb, void* user_data) noexcept {
    std::lock_guard lock(m_cb_mutex);
    m_vsync_cb        = cb;
    m_vsync_user_data = user_data;
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------
bool         GLFWDisplay::is_alive()    const noexcept { return m_alive.load(std::memory_order_acquire); }
Resolution   GLFWDisplay::resolution()  const noexcept { return Resolution{ m_width, m_height }; }
uint32_t     GLFWDisplay::refresh_hz()  const noexcept { return m_refresh_hz; }
std::string_view GLFWDisplay::backend_name() const noexcept { return "glfw"; }

} // namespace pixelgpu::backend

/*
 * gpu_app_mock_dumbfb.cpp
 *
 * PixelGPU demo application.
 *
 * Demonstrates:
 *   - Allocating a framebuffer in plain heap memory
 *   - Opening a display window via the C API
 *   - Drawing simple animated test patterns directly into the framebuffer [student-implemented]
 *   - Syncing to the display with pgpu_wait_vsync()
 *   - Receiving a vsync callback from the backend timer thread
 *   - Graceful shutdown when the window is closed or Esc is pressed
 *
 * The framebuffer pixel format is XBGR8888:
 *   Byte 0 = Blue, Byte 1 = Green, Byte 2 = Red, Byte 3 = X (ignored)
 *
 * All drawing is done with raw pointer arithmetic — no drawing library.
 * This is intentional: students should feel the framebuffer as memory.
 */

#include <pgpu/pgpu.h>

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>

// ---------------------------------------------------------------------------
// XBGR8888 pixel packing helper (you can rewrite this yourself from scratch)
// ---------------------------------------------------------------------------
static inline uint32_t xbgr(uint8_t r, uint8_t g, uint8_t b) noexcept {
    // Byte layout in memory: [B][G][R][X]
    // As a uint32_t on little-endian: X=byte3, R=byte2, G=byte1, B=byte0
    return (static_cast<uint32_t>(0xFF) << 24)  // X = 0xFF (unused)
         | (static_cast<uint32_t>(r)    << 16)
         | (static_cast<uint32_t>(g)    <<  8)
         | (static_cast<uint32_t>(b));
}

// ---------------------------------------------------------------------------
// Plot a single pixel into the framebuffer (no bounds check — caller beware!)
// ---------------------------------------------------------------------------
static inline void plot(uint32_t* fb, uint32_t stride_px,
                        uint32_t x, uint32_t y, uint32_t color) noexcept {
    fb[y * stride_px + x] = color;
}

// ---------------------------------------------------------------------------
// Fill the entire framebuffer with one colour (can re-implement for practice)
// ---------------------------------------------------------------------------
static void fill(uint32_t* fb, uint32_t npixels, uint32_t color) noexcept {
    for (uint32_t i = 0; i < npixels; ++i)
        fb[i] = color;
//}


// ---------------------------------------------------------------------------
// SIGINT (Ctrl-C) handler — sets a flag; render loop checks it each frame
// ---------------------------------------------------------------------------
static std::atomic<bool> g_interrupted{ false };

static void sigint_handler(int) noexcept {
    g_interrupted.store(true, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Vsync callback — counts ticks, used to demonstrate callback mechanism
// ---------------------------------------------------------------------------
static std::atomic<uint64_t> g_vsync_count{ 0 };

static void vsync_callback(void* /*user_data*/) noexcept {
    g_vsync_count.fetch_add(1, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    // ── Choose resolution ────────────────────────────────────────────────────
    const pgpu_resolution_t res = pgpu_resolution_from_preset(PGPU_RES_VGA); // 640 x 480

    std::printf("PixelGPU Demo\n");
    std::printf("  Resolution : %u x %u\n", res.width, res.height);
    std::printf("  Frame size : %llu bytes\n",
                static_cast<unsigned long long>(
                    (uint64_t)res.width * res.height * 4));
    std::printf("  Backends   : %s\n", pgpu_registered_backends());
    std::printf("\n");
    std::printf("Press Esc, close the window, or Ctrl-C to exit early.\n");

    // ── Open display ─────────────────────────────────────────────────────────
    // Pass nullptr: the library allocates and owns the framebuffer internally.
    pgpu_display_t display = pgpu_display_create(
        res,
        nullptr,  // library-managed framebuffer
        60,
        "PixelGPU - Demo",
        nullptr, // backend_priority: auto-select
        nullptr   // text_arg: none
    );
    if (!display) {
        std::fprintf(stderr, "ERROR: Could not open display: %s\n",
                     pgpu_last_error_string());
        return 1;
    }

    std::printf("Backend in use : %s\n\n", pgpu_display_backend_name(display));

    // ── Framebuffer pointer ───────────────────────────────────────────────────
    // The library owns this memory; valid until pgpu_display_destroy().
    uint32_t* framebuffer =
        static_cast<uint32_t*>(pgpu_display_get_framebuffer(display));

    // ── SIGINT handler ────────────────────────────────────────────────────────
    std::signal(SIGINT, sigint_handler);

    // ── Register vsync callback ───────────────────────────────────────────────
    pgpu_display_set_vsync_callback(display, vsync_callback, nullptr);

    // ── Render loop ──────────────────────────────────────────────────────────
    constexpr uint32_t TOTAL_FRAMES = 600;   // probably around 10 seconds at 60 Hz; adjust as needed
    uint32_t frame = 0;

    while (pgpu_display_is_alive(display)
           && !g_interrupted.load(std::memory_order_relaxed)
           && frame < TOTAL_FRAMES) {

        // <<<>>> Add your drawing code implementation here!  Draw something interesting into the framebuffer

        pgpu_wait_vsync(display, 100);
        pgpu_display_present_now(display);
        ++frame;

    }

    if (frame >= TOTAL_FRAMES) {
        std::printf("\nDemo framecounter completed. Demo complete.\n");
    } else if (g_interrupted.load(std::memory_order_relaxed)) {
        std::printf("\nInterrupted.\n");
    } else {
        std::printf("\nWindow closed.\n");
    }
    std::printf("Total frames rendered      : %u\n", frame);
    std::printf("Total vsync callbacks      : %llu\n",
                static_cast<unsigned long long>(
                    g_vsync_count.load(std::memory_order_relaxed)));

    pgpu_display_destroy(display);
    return 0;
}

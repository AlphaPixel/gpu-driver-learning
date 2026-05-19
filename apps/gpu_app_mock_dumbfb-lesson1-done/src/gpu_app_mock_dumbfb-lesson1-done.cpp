/*
 * gpu_app_mock_dumbfb-lesson1-done.cpp
 *
 * PixelGPU — Lesson 1 reference implementation.
 *
 * Usage:  gpu_app_mock_dumbfb-lesson1-done <challenge-number>
 *   1: Single pixel in the center
 *   2: Flood fill
 *   3: Single-axis gradient
 *   4: Two-axis gradient
 *   5: Checkerboard
 *   6: Horizontal color stripes
 *   7: Random scatter
 *   8: Solid filled rectangle
 *   9: Random rectangles
 *   10: Animated gradient scroll
 *   11: Bouncing rectangle
 *   12: Line between two points
 *   13: Filled circle
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
#include <cstdlib>

// ---------------------------------------------------------------------------
// XBGR8888 pixel packing helper
// ---------------------------------------------------------------------------
static inline uint32_t xbgr(uint8_t r, uint8_t g, uint8_t b) noexcept {
    // Byte layout in memory: [B][G][R][X]
    // As a uint32_t on little-endian: X=byte3, R=byte2, G=byte1, B=byte0
    return (static_cast<uint32_t>(0xFF) << 24)
         | (static_cast<uint32_t>(r)    << 16)
         | (static_cast<uint32_t>(g)    <<  8)
         | (static_cast<uint32_t>(b));
}

// ---------------------------------------------------------------------------
// Plot a single pixel (no bounds check — caller ensures validity)
// ---------------------------------------------------------------------------
static inline void plot(uint32_t* fb, uint32_t stride_px,
                        uint32_t x, uint32_t y, uint32_t color) noexcept {
    fb[y * stride_px + x] = color;
}

// ---------------------------------------------------------------------------
// Fill entire framebuffer with one colour
// ---------------------------------------------------------------------------
static void fill(uint32_t* fb, uint32_t npixels, uint32_t color) noexcept {
    for (uint32_t i = 0; i < npixels; ++i)
        fb[i] = color;
}

// ---------------------------------------------------------------------------
// Challenge 1 — Single pixel in the centre
//   Write one white pixel at the exact centre of the 640×480 framebuffer.
// ---------------------------------------------------------------------------
static void challenge_01_single_pixel(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    fill(fb, w * h, xbgr(0, 0, 0));
    plot(fb, w, w / 2u, h / 2u, xbgr(255, 255, 255));
}

// ---------------------------------------------------------------------------
// Challenge 2 — Flood fill
//   Fill every pixel with the same solid colour (pure green).
// ---------------------------------------------------------------------------
static void challenge_02_flood_fill(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    fill(fb, w * h, xbgr(0, 255, 0));
}

// ---------------------------------------------------------------------------
// Challenge 3 — Single-axis gradient
//   Red increases left to right (0..255); green and blue stay at 0.
// ---------------------------------------------------------------------------
static void challenge_03_single_axis_gradient(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            plot(fb, w, x, y,
                 xbgr(static_cast<uint8_t>((x * 255u) / (w - 1u)), 0, 0));
}

// ---------------------------------------------------------------------------
// Challenge 4 — Two-axis gradient
//   Red increases left to right; blue increases top to bottom; green = 0.
//   Corners: black (TL), red (TR), blue (BL), magenta (BR).
// ---------------------------------------------------------------------------
static void challenge_04_two_axis_gradient(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t b = static_cast<uint8_t>((y * 255u) / (h - 1u));
        for (uint32_t x = 0; x < w; ++x) {
            const uint8_t r = static_cast<uint8_t>((x * 255u) / (w - 1u));
            plot(fb, w, x, y, xbgr(r, 0, b));
        }
    }
}

// ---------------------------------------------------------------------------
// Challenge 5 — Checkerboard
//   8×8 grid of 80×60-pixel tiles, alternating black and white.
// ---------------------------------------------------------------------------
static void challenge_05_checkerboard(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    constexpr uint32_t TILE_W = 80u;
    constexpr uint32_t TILE_H = 60u;
    for (uint32_t y = 0; y < h; ++y) {
        const uint32_t tile_row = y / TILE_H;
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t tile_col = x / TILE_W;
            const uint32_t color = ((tile_col + tile_row) % 2u == 0u)
                                 ? xbgr(255, 255, 255)
                                 : xbgr(0, 0, 0);
            plot(fb, w, x, y, color);
        }
    }
}

// ---------------------------------------------------------------------------
// Challenge 6 — Horizontal colour stripes
//   8 equal horizontal bands, each a different solid colour.
// ---------------------------------------------------------------------------
static void challenge_06_horizontal_stripes(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    static const uint32_t kStripes[8] = {
        xbgr(255,   0,   0), // red
        xbgr(255, 128,   0), // orange
        xbgr(255, 255,   0), // yellow
        xbgr(  0, 255,   0), // green
        xbgr(  0, 255, 255), // cyan
        xbgr(  0,   0, 255), // blue
        xbgr(128,   0, 255), // violet
        xbgr(255, 255, 255), // white
    };
    const uint32_t band_h = h / 8u;
    for (uint32_t y = 0; y < h; ++y) {
        const uint32_t band = (y < band_h * 7u) ? (y / band_h) : 7u;
        for (uint32_t x = 0; x < w; ++x)
            plot(fb, w, x, y, kStripes[band]);
    }
}

// ---------------------------------------------------------------------------
// Challenge 7 — Random scatter (accumulating, no clear each frame)
//   10,000 random-coloured pixels per frame; dots accumulate on the black bg.
// ---------------------------------------------------------------------------
static void challenge_07_random_scatter(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    for (int i = 0; i < 10000; ++i) {
        const uint32_t x = static_cast<uint32_t>(rand()) % w;
        const uint32_t y = static_cast<uint32_t>(rand()) % h;
        const uint8_t  r = static_cast<uint8_t>(rand() % 256);
        const uint8_t  g = static_cast<uint8_t>(rand() % 256);
        const uint8_t  b = static_cast<uint8_t>(rand() % 256);
        plot(fb, w, x, y, xbgr(r, g, b));
    }
}

// ---------------------------------------------------------------------------
// Challenge 8 — Solid filled rectangle
//   A 200×100 blue rectangle with its top-left corner at (220, 190).
// ---------------------------------------------------------------------------
static void challenge_08_solid_rectangle(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    fill(fb, w * h, xbgr(0, 0, 0));
    constexpr uint32_t RX = 220u, RY = 190u, RW = 200u, RH = 100u;
    for (uint32_t y = RY; y < RY + RH; ++y)
        for (uint32_t x = RX; x < RX + RW; ++x)
            plot(fb, w, x, y, xbgr(0, 0, 255));
}

// ---------------------------------------------------------------------------
// Challenge 9 — Random rectangles
//   50 random-sized, random-coloured rectangles per frame on a black bg.
//   Framebuffer is cleared to black at the start of each frame.
// ---------------------------------------------------------------------------
static void challenge_09_random_rectangles(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    fill(fb, w * h, xbgr(0, 0, 0));
    for (int i = 0; i < 50; ++i) {
        const uint32_t x0 = static_cast<uint32_t>(rand()) % w;
        const uint32_t y0 = static_cast<uint32_t>(rand()) % h;
        const uint32_t rw = 10u + static_cast<uint32_t>(rand()) % 150u;
        const uint32_t rh = 10u + static_cast<uint32_t>(rand()) % 100u;
        const uint32_t x1 = (x0 + rw < w) ? x0 + rw : w;
        const uint32_t y1 = (y0 + rh < h) ? y0 + rh : h;
        const uint32_t color = xbgr(static_cast<uint8_t>(rand() % 256),
                                    static_cast<uint8_t>(rand() % 256),
                                    static_cast<uint8_t>(rand() % 256));
        for (uint32_t y = y0; y < y1; ++y)
            for (uint32_t x = x0; x < x1; ++x)
                plot(fb, w, x, y, color);
    }
}

// ---------------------------------------------------------------------------
// Challenge 10 — Animated gradient scroll
//   Red gradient that scrolls leftward one pixel per frame, wrapping around.
// ---------------------------------------------------------------------------
static void challenge_10_gradient_scroll(uint32_t* fb, uint32_t w, uint32_t h,
                                         uint32_t frame) noexcept {
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const uint8_t r = static_cast<uint8_t>(((x + frame) % w) * 255u / (w - 1u));
            plot(fb, w, x, y, xbgr(r, 0, 0));
        }
}

// ---------------------------------------------------------------------------
// Challenge 11 — Bouncing rectangle
//   White 60×40 rect moving at 1 px/frame in each axis, bouncing off walls.
// ---------------------------------------------------------------------------
static void challenge_11_bouncing_rectangle(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    constexpr int32_t RW = 60;
    constexpr int32_t RH = 40;
    static int32_t rx = 0, ry = 0, vx = 1, vy = 1;

    fill(fb, w * h, xbgr(0, 0, 0));

    rx += vx;
    ry += vy;

    if (rx + RW >= static_cast<int32_t>(w)) { rx = static_cast<int32_t>(w) - RW; vx = -vx; }
    if (rx < 0)                              { rx = 0;                             vx = -vx; }
    if (ry + RH >= static_cast<int32_t>(h)) { ry = static_cast<int32_t>(h) - RH; vy = -vy; }
    if (ry < 0)                              { ry = 0;                             vy = -vy; }

    for (int32_t y = ry; y < ry + RH; ++y)
        for (int32_t x = rx; x < rx + RW; ++x)
            plot(fb, w, static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                 xbgr(255, 255, 255));
}

// ---------------------------------------------------------------------------
// Bresenham's line algorithm — integer-only, all octants, with bounds check
// ---------------------------------------------------------------------------
static void bresenham_line(uint32_t* fb, uint32_t w, uint32_t h,
                            int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                            uint32_t color) noexcept {
    const int32_t dx =  (x1 > x0 ? x1 - x0 : x0 - x1);
    const int32_t dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    const int32_t sx = (x0 < x1) ? 1 : -1;
    const int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    for (;;) {
        if (x0 >= 0 && y0 >= 0 && static_cast<uint32_t>(x0) < w && static_cast<uint32_t>(y0) < h)
            plot(fb, w, static_cast<uint32_t>(x0), static_cast<uint32_t>(y0), color);
        if (x0 == x1 && y0 == y1) break;
        const int32_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// ---------------------------------------------------------------------------
// Challenge 12 — Line between two points (Bresenham)
//   Yellow line from near the top-left corner to near the bottom-right.
// ---------------------------------------------------------------------------
static void challenge_12_line(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    fill(fb, w * h, xbgr(0, 0, 0));
    bresenham_line(fb, w, h,
                   50, 50,
                   static_cast<int32_t>(w) - 50, static_cast<int32_t>(h) - 50,
                   xbgr(255, 255, 0));
}

// ---------------------------------------------------------------------------
// Challenge 13 — Filled circle
//   Solid cyan circle centered in the window, radius 100 pixels.
//   Uses the circle equation dx²+dy² ≤ r² — no sqrt needed.
// ---------------------------------------------------------------------------
static void challenge_13_filled_circle(uint32_t* fb, uint32_t w, uint32_t h) noexcept {
    fill(fb, w * h, xbgr(0, 0, 0));
    const int32_t cx = static_cast<int32_t>(w / 2u);
    const int32_t cy = static_cast<int32_t>(h / 2u);
    constexpr int32_t R  = 100;
    constexpr int32_t R2 = R * R;

    for (int32_t y = cy - R; y <= cy + R; ++y) {
        if (y < 0 || static_cast<uint32_t>(y) >= h) continue;
        const int32_t dy = y - cy;
        for (int32_t x = cx - R; x <= cx + R; ++x) {
            if (x < 0 || static_cast<uint32_t>(x) >= w) continue;
            const int32_t dx = x - cx;
            if (dx * dx + dy * dy <= R2)
                plot(fb, w, static_cast<uint32_t>(x), static_cast<uint32_t>(y),
                     xbgr(0, 255, 255));
        }
    }
}

// ---------------------------------------------------------------------------
// SIGINT (Ctrl-C) handler
// ---------------------------------------------------------------------------
static std::atomic<bool> g_interrupted{ false };

static void sigint_handler(int) noexcept {
    g_interrupted.store(true, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Vsync callback — counts ticks to demonstrate the callback mechanism
// ---------------------------------------------------------------------------
static std::atomic<uint64_t> g_vsync_count{ 0 };

static void vsync_callback(void* /*user_data*/) noexcept {
    g_vsync_count.fetch_add(1, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
static const char* const kChallengeNames[14] = {
    "",
    "Challenge 1: Single pixel in the centre",
    "Challenge 2: Flood fill",
    "Challenge 3: Single-axis gradient",
    "Challenge 4: Two-axis gradient",
    "Challenge 5: Checkerboard",
    "Challenge 6: Horizontal colour stripes",
    "Challenge 7: Random scatter",
    "Challenge 8: Solid filled rectangle",
    "Challenge 9: Random rectangles",
    "Challenge 10: Animated gradient scroll",
    "Challenge 11: Bouncing rectangle",
    "Challenge 12: Line between two points",
    "Challenge 13: Filled circle",
};

int main(int argc, char* argv[]) {
    // ── Parse challenge number ───────────────────────────────────────────────
    if (argc < 2) {
        std::fprintf(stderr, "Usage: %s <challenge-number>\n\n", argv[0]);
        std::fprintf(stderr, "Available challenges:\n");
        for (int i = 1; i <= 13; ++i)
            std::fprintf(stderr, "  %2d  %s\n", i, kChallengeNames[i]);
        return 1;
    }

    const unsigned int challenge = static_cast<unsigned int>(std::strtoul(argv[1], nullptr, 10));
    if (challenge < 1u || challenge > 13u) {
        std::fprintf(stderr, "Error: challenge must be 1–13 (got \"%s\")\n", argv[1]);
        return 1;
    }

    // ── Choose resolution ────────────────────────────────────────────────────
    const pgpu_resolution_t res = pgpu_resolution_from_preset(PGPU_RES_VGA); // 640 x 480

    std::printf("PixelGPU: Lesson 1 Reference\n");
    std::printf("  %s\n", kChallengeNames[challenge]);
    std::printf("  Resolution : %u x %u\n", res.width, res.height);
    std::printf("  Backends   : %s\n", pgpu_registered_backends());
    std::printf("\nPress Esc, close the window, or Ctrl-C to exit.\n\n");

    // ── Build window title ───────────────────────────────────────────────────
    char title[128];
    std::snprintf(title, sizeof(title), "Lesson 1 — %s", kChallengeNames[challenge]);

    // ── Open display ─────────────────────────────────────────────────────────
    pgpu_display_t display = pgpu_display_create(
        res,
        nullptr,  // library-managed framebuffer
        60,
        title,
        "sdl3",
        nullptr
    );
    if (!display) {
        std::fprintf(stderr, "ERROR: Could not open display: %s\n",
                     pgpu_last_error_string());
        return 1;
    }

    std::printf("Backend in use : %s\n\n", pgpu_display_backend_name(display));

    // ── Framebuffer pointer ───────────────────────────────────────────────────
    uint32_t* framebuffer =
        static_cast<uint32_t*>(pgpu_display_get_framebuffer(display));

    // ── SIGINT handler ────────────────────────────────────────────────────────
    std::signal(SIGINT, sigint_handler);

    // ── Register vsync callback ───────────────────────────────────────────────
    pgpu_display_set_vsync_callback(display, vsync_callback, nullptr);

    // ── Render loop ──────────────────────────────────────────────────────────
    uint32_t frame = 0;

    while (pgpu_display_is_alive(display)
           && !g_interrupted.load(std::memory_order_relaxed)) {

        switch (challenge) {
            case  1: challenge_01_single_pixel(       framebuffer, res.width, res.height);         break;
            case  2: challenge_02_flood_fill(          framebuffer, res.width, res.height);         break;
            case  3: challenge_03_single_axis_gradient(framebuffer, res.width, res.height);         break;
            case  4: challenge_04_two_axis_gradient(   framebuffer, res.width, res.height);         break;
            case  5: challenge_05_checkerboard(        framebuffer, res.width, res.height);         break;
            case  6: challenge_06_horizontal_stripes(  framebuffer, res.width, res.height);         break;
            case  7: challenge_07_random_scatter(      framebuffer, res.width, res.height);         break;
            case  8: challenge_08_solid_rectangle(     framebuffer, res.width, res.height);         break;
            case  9: challenge_09_random_rectangles(   framebuffer, res.width, res.height);         break;
            case 10: challenge_10_gradient_scroll(     framebuffer, res.width, res.height, frame);  break;
            case 11: challenge_11_bouncing_rectangle(  framebuffer, res.width, res.height);         break;
            case 12: challenge_12_line(                framebuffer, res.width, res.height);         break;
            case 13: challenge_13_filled_circle(       framebuffer, res.width, res.height);         break;
            default: break;
        }

        pgpu_wait_vsync(display, 100);
        pgpu_display_present_now(display);
        ++frame;
    }

    // ── Shutdown report ───────────────────────────────────────────────────────
    if (g_interrupted.load(std::memory_order_relaxed))
        std::printf("\nInterrupted.\n");
    else
        std::printf("\nWindow closed.\n");

    std::printf("Total frames rendered : %u\n", frame);
    std::printf("Total vsync callbacks : %llu\n",
                static_cast<unsigned long long>(
                    g_vsync_count.load(std::memory_order_relaxed)));

    pgpu_display_destroy(display);
    return 0;
}

#pragma once

/*
 * pgpu.h  —  PixelGPU public C API
 *
 * This is the ONLY header that C translation units need to include.
 * C++ callers should prefer the native C++ headers in <pixelgpu/>.
 *
 * All functions use C linkage and C calling conventions.
 * No C++ types appear in this header.
 *
 * Pixel format: XBGR8888  (32 bits/pixel, little-endian)
 *   Byte 0 = Blue, Byte 1 = Green, Byte 2 = Red, Byte 3 = X (unused/alpha)
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Status codes ─────────────────────────────────────────────────────────── */

typedef enum pgpu_status {
    PGPU_OK                   =   0,
    PGPU_TIMEOUT              =   1,   /* wait_vsync timed out — not an error */
    PGPU_ERROR_INVALID_HANDLE =  -1,
    PGPU_ERROR_BACKEND_FAILED =  -2,
    PGPU_ERROR_BEZEL_REQUIRED =  -3,   /* bezel requested but no capable backend */
    PGPU_ERROR_BEZEL_ASPECT   =  -4,   /* bezel aspect ratio mismatch */
    PGPU_ERROR_BAD_PARAMS     =  -5,   /* null framebuffer, zero resolution, etc. */
    PGPU_ERROR_OUT_OF_MEMORY  =  -6,
    PGPU_ERROR_UNKNOWN        = -99,
} pgpu_status_t;

/* ── Opaque handles ───────────────────────────────────────────────────────── */

/* Forward-declared; never dereference directly. */
struct pgpu_display_opaque;
typedef struct pgpu_display_opaque* pgpu_display_t;

/*
 * Phase 4 forward hook: opaque handle to a VRAM-backed buffer.
 * NULL means caller-owned heap memory.
 * Non-NULL will mean a VRAM allocation registered with the device.
 */
struct pgpu_buffer_opaque;
typedef struct pgpu_buffer_opaque* pgpu_buffer_handle_t;

/* ── Resolution ───────────────────────────────────────────────────────────── */

typedef struct {
    uint32_t width;
    uint32_t height;
} pgpu_resolution_t;

/*
 * Named resolution preset IDs.
 * Values match pixelgpu::ResolutionPreset — cast freely between them.
 */
typedef enum pgpu_resolution_preset {
    PGPU_RES_CGA             = 0,
    PGPU_RES_EGA,
    PGPU_RES_VGA,
    PGPU_RES_SVGA,
    PGPU_RES_XGA,
    PGPU_RES_SXGA,
    PGPU_RES_UXGA,
    PGPU_RES_QVGA,
    PGPU_RES_HVGA,
    PGPU_RES_HD720,
    PGPU_RES_HD1080,
    PGPU_RES_QHD,
    PGPU_RES_UHD4K,
    PGPU_RES_WXGA,
    PGPU_RES_WSXGA_PLUS,
    PGPU_RES_WUXGA,
    PGPU_RES_GAMEBOY,
    PGPU_RES_GAMEBOY_ADVANCE,
    PGPU_RES_NDS,
    PGPU_RES_PSP,
} pgpu_resolution_preset_t;

/* Return the Resolution for a named preset.
   Returns {0,0} for unknown preset IDs. */
pgpu_resolution_t pgpu_resolution_from_preset(pgpu_resolution_preset_t preset);

/* Convenience constructor from explicit dimensions. */
pgpu_resolution_t pgpu_resolution_make(uint32_t width, uint32_t height);

/* ── Vsync callback ───────────────────────────────────────────────────────── */

/*
 * Invoked on the backend's internal timer thread once per vsync tick.
 * user_data is the pointer registered by the caller.
 * To cancel callbacks, call pgpu_display_set_vsync_callback(h, NULL, NULL).
 */
typedef void (*pgpu_vsync_callback_fn)(void* user_data);

/* ── Display lifecycle ────────────────────────────────────────────────────── */

/*
 * pgpu_display_create()
 *
 * Opens a display window.  The library manages framebuffer memory.
 *
 * Parameters:
 *   resolution       — framebuffer width × height in pixels
 *   framebuffer_ptr  — caller-owned memory, XBGR8888, at least
 *                      (resolution.width * 4 * resolution.height) bytes;
 *                      pass NULL to let the library allocate and own the
 *                      framebuffer (retrieve the pointer afterwards with
 *                      pgpu_display_get_framebuffer()).
 *   refresh_hz       — vsync rate in Hz; pass 0 for the default (60 Hz)
 *   window_title     — window title bar text; NULL → "PixelGPU"
 *   backend_priority — comma-separated backend name list, e.g. "sdl3,glfw";
 *                      NULL or "" → auto-select by preference score
 *   text_arg         — text argument, (monitor bezel base name, e.g. "gameboy";
 *                      NULL or "" → no bezel)
 *
 * Returns a live handle on success, or NULL on failure.
 * On failure call pgpu_last_error_code() / pgpu_last_error_string().
 */
pgpu_display_t pgpu_display_create(
    pgpu_resolution_t  resolution,
    void*              framebuffer_ptr,
    uint32_t           refresh_hz,
    const char*        window_title,
    const char*        backend_priority,
    const char*        text_arg
);

/*
 * pgpu_display_get_framebuffer()
 *
 * Returns a pointer to the display's XBGR8888 framebuffer.
 *
 * When the display was created with framebuffer_ptr = NULL the library
 * owns this memory; the pointer is valid until pgpu_display_destroy().
 * When the display was created with a caller-provided pointer this returns
 * that same pointer unchanged.
 *
 * Returns NULL for a NULL or invalid handle.
 */
void* pgpu_display_get_framebuffer(pgpu_display_t handle);

/*
 * pgpu_display_destroy()
 *
 * Closes the window, stops the vsync timer, and releases all resources.
 * Passing NULL is a no-op.  After this call the handle is invalid.
 */
void pgpu_display_destroy(pgpu_display_t handle);

/* ── Presentation ─────────────────────────────────────────────────────────── */

/*
 * Blit the current framebuffer to the window immediately.
 * Does not affect the vsync timer phase.
 * Safe to call from any thread.
 */
void pgpu_display_present_now(pgpu_display_t handle);

/* ── Vsync ────────────────────────────────────────────────────────────────── */

/*
 * Register (or replace) the vsync callback.
 * Pass cb = NULL to unregister.
 * Thread-safe; last call wins.
 */
void pgpu_display_set_vsync_callback(
    pgpu_display_t         handle,
    pgpu_vsync_callback_fn callback,
    void*                  user_data
);

/*
 * Block the calling thread until the next vsync tick or timeout.
 *
 * Returns PGPU_OK      — vsync fired
 *         PGPU_TIMEOUT — timeout elapsed without a tick (not an error)
 *         PGPU_ERROR_* — handle invalid or other problem
 */
pgpu_status_t pgpu_wait_vsync(pgpu_display_t handle, uint32_t timeout_ms);

/* ── Status ───────────────────────────────────────────────────────────────── */

/*
 * Returns 1 while the display window is open and the backend is healthy.
 * Returns 0 once the user closes the window or presses Escape.
 * Returns 0 for a NULL handle.
 */
int pgpu_display_is_alive(pgpu_display_t handle);

/* ── Error reporting ──────────────────────────────────────────────────────── */

/*
 * Return the status code and human-readable description of the last
 * failed pgpu_* call on the calling thread.
 *
 * pgpu_last_error_string() returns a pointer into a thread-local buffer;
 * valid until the next pgpu_* call on this thread.
 */
pgpu_status_t pgpu_last_error_code(void);
const char*   pgpu_last_error_string(void);

/* ── Diagnostics ──────────────────────────────────────────────────────────── */

/*
 * Returns a null-terminated string listing registered backends and their
 * preference scores, e.g. "sdl3(100), glfw(100), vnc(10)".
 * The pointer is valid until the next call to this function on any thread.
 */
const char* pgpu_registered_backends(void);

/*
 * Returns the name of the backend driving this display handle
 * (e.g. "sdl3" or "glfw").  The string has static storage duration.
 * Returns "" for a NULL or invalid handle.
 */
const char* pgpu_display_backend_name(pgpu_display_t handle);

#ifdef __cplusplus
} /* extern "C" */
#endif

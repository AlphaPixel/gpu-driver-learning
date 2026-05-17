#pragma once

#include <cstdint>

namespace pixelgpu {

// ---------------------------------------------------------------------------
// ResolutionPreset
//
// Canonical named display resolutions drawn from the IBM/VESA naming
// conventions documented in the Wikipedia article "Graphics display
// resolution" and the VESA DMT standard.
//
// Refresh rate is NOT part of the resolution. The display subsystem
// accepts refresh_hz as a separate parameter and defaults to 60 Hz.
// ---------------------------------------------------------------------------
enum class ResolutionPreset : uint32_t {
    // ── IBM / CGA-era ──────────────────────────────────────────────────────
    CGA = 0,        //  320 ×  200  (IBM Color Graphics Adapter)
    EGA,            //  640 ×  350  (IBM Enhanced Graphics Adapter)
    VGA,            //  640 ×  480  (IBM Video Graphics Array)
    SVGA,           //  800 ×  600  (VESA Super VGA)

    // ── XGA family ────────────────────────────────────────────────────────
    XGA,            // 1024 ×  768  (IBM Extended Graphics Array)
    SXGA,           // 1280 × 1024  (Super XGA, 5:4 aspect)
    UXGA,           // 1600 × 1200  (Ultra XGA, 4:3 aspect)

    // ── QVGA / HVGA (quarter / half VGA) ──────────────────────────────────
    QVGA,           //  320 ×  240  (Quarter VGA)
    HVGA,           //  480 ×  320  (Half VGA)

    // ── Widescreen 16:9 ───────────────────────────────────────────────────
    HD720,          // 1280 ×  720  (720p / HD)
    HD1080,         // 1920 × 1080  (1080p / Full HD)
    QHD,            // 2560 × 1440  (1440p / WQHD / Quad HD)
    UHD4K,          // 3840 × 2160  (4K UHD)

    // ── Widescreen 16:10 ──────────────────────────────────────────────────
    WXGA,           // 1280 ×  800
    WSXGA_PLUS,     // 1680 × 1050
    WUXGA,          // 1920 × 1200

    // ── Handheld / retro-console ──────────────────────────────────────────
    GAMEBOY,        //  160 ×  144  (original Game Boy, Game Boy Color)
    GAMEBOY_ADVANCE,//  240 ×  160  (Game Boy Advance)
    NDS,            //  256 ×  192  (Nintendo DS — one screen)
    PSP,            //  480 ×  272  (PlayStation Portable)

    _COUNT
};

// ---------------------------------------------------------------------------
// Resolution
//
// Represents a framebuffer width × height in pixels.
//
// Pixel format is always XBGR8888 (32 bits per pixel):
//   Byte 0 = Blue, Byte 1 = Green, Byte 2 = Red, Byte 3 = X (unused/alpha)
//
// This is a plain data type — safe to copy, store in arrays, use as
// constexpr values, and pass across C/C++ boundaries via pgpu_resolution_t.
// ---------------------------------------------------------------------------
struct Resolution {
    uint32_t width;
    uint32_t height;

    // Default: zero (invalid — use a preset or explicit dimensions)
    constexpr Resolution() noexcept : width(0), height(0) {}

    // Explicit dimensions
    constexpr Resolution(uint32_t w, uint32_t h) noexcept
        : width(w), height(h) {}

    // Construct from a named preset (see resolution_presets.h for the table)
    explicit Resolution(ResolutionPreset preset) noexcept;

    // ── Computed properties ────────────────────────────────────────────────

    // Bytes per row. Always width * 4 for XBGR8888.
    constexpr uint32_t stride_bytes() const noexcept {
        return width * 4u;
    }

    // Total bytes for one frame.
    constexpr uint64_t frame_bytes() const noexcept {
        return static_cast<uint64_t>(stride_bytes()) * height;
    }

    // Aspect ratio as a float, e.g. 1.3333… for 4:3, 1.7777… for 16:9.
    // Returns 0.0f if height is zero.
    constexpr float aspect_ratio() const noexcept {
        if (height == 0) return 0.0f;
        return static_cast<float>(width) / static_cast<float>(height);
    }

    // Validity check
    constexpr bool is_valid() const noexcept {
        return width > 0 && height > 0;
    }

    // Equality
    constexpr bool operator==(const Resolution& o) const noexcept {
        return width == o.width && height == o.height;
    }
    constexpr bool operator!=(const Resolution& o) const noexcept {
        return !(*this == o);
    }
};

// ---------------------------------------------------------------------------
// resolution_from_preset()
//
// Returns the Resolution for a named preset.  Returns a zero-size
// Resolution (is_valid() == false) for unknown/invalid values.
// ---------------------------------------------------------------------------
Resolution resolution_from_preset(ResolutionPreset preset) noexcept;

} // namespace pixelgpu

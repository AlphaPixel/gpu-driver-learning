#include "pixelgpu/resolution.h"

#include <array>
#include <cstdint>

namespace pixelgpu {

namespace {

struct PresetEntry {
    ResolutionPreset preset;
    uint32_t         width;
    uint32_t         height;
};

// ---------------------------------------------------------------------------
// Canonical resolution table.
//
// Source: Wikipedia "Graphics display resolution", IBM hardware specs, and
// VESA Display Monitor Timing (DMT) standard naming conventions.
//
// Keep entries in the same order as the ResolutionPreset enum.
// ---------------------------------------------------------------------------
constexpr std::array<PresetEntry, static_cast<std::size_t>(ResolutionPreset::_COUNT)>
k_preset_table = {{
    // IBM / CGA-era
    { ResolutionPreset::CGA,             320,  200 },
    { ResolutionPreset::EGA,             640,  350 },
    { ResolutionPreset::VGA,             640,  480 },
    { ResolutionPreset::SVGA,            800,  600 },

    // XGA family
    { ResolutionPreset::XGA,            1024,  768 },
    { ResolutionPreset::SXGA,           1280, 1024 },
    { ResolutionPreset::UXGA,           1600, 1200 },

    // QVGA / HVGA
    { ResolutionPreset::QVGA,            320,  240 },
    { ResolutionPreset::HVGA,            480,  320 },

    // Widescreen 16:9
    { ResolutionPreset::HD720,          1280,  720 },
    { ResolutionPreset::HD1080,         1920, 1080 },
    { ResolutionPreset::QHD,            2560, 1440 },
    { ResolutionPreset::UHD4K,          3840, 2160 },

    // Widescreen 16:10
    { ResolutionPreset::WXGA,           1280,  800 },
    { ResolutionPreset::WSXGA_PLUS,     1680, 1050 },
    { ResolutionPreset::WUXGA,          1920, 1200 },

    // Handheld / retro-console
    { ResolutionPreset::GAMEBOY,         160,  144 },
    { ResolutionPreset::GAMEBOY_ADVANCE, 240,  160 },
    { ResolutionPreset::NDS,             256,  192 },
    { ResolutionPreset::PSP,             480,  272 },
}};

} // anonymous namespace

// ---------------------------------------------------------------------------
Resolution resolution_from_preset(ResolutionPreset preset) noexcept {
    const auto idx = static_cast<std::size_t>(preset);
    if (idx >= k_preset_table.size()) {
        return Resolution{}; // invalid
    }
    const auto& e = k_preset_table[idx];
    return Resolution{ e.width, e.height };
}

// ---------------------------------------------------------------------------
// Resolution constructor from preset (defined here to keep the header clean)
Resolution::Resolution(ResolutionPreset preset) noexcept {
    *this = resolution_from_preset(preset);
}

} // namespace pixelgpu

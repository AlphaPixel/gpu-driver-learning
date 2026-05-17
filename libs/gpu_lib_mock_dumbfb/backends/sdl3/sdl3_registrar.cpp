#include "sdl3_display.h"
#include "backend_registry.h"

// ---------------------------------------------------------------------------
// SDL3 backend registration
//
// This translation unit's sole job is to register the SDL3 backend with the
// BackendRegistry before main() runs.  It is always compiled when
// PGPU_HAS_BACKEND_SDL3 is defined; the CMake build system controls that.
//
// Preference = 100 — first-class windowed desktop backend.
// supports_bezel = false — Phase 1 stub; will become true when bezel
//   rendering is implemented in a later phase.
// ---------------------------------------------------------------------------

namespace {

const pixelgpu::internal::BackendDescriptor k_sdl3_descriptor {
    .name           = "sdl3",
    .preference     = 100,
    .supports_bezel = false,
    .create         = pixelgpu::backend::SDL3Display::try_create,
};

// Constructed before main(); registers k_sdl3_descriptor with the registry.
pixelgpu::internal::BackendRegistrar g_sdl3_registrar{ k_sdl3_descriptor };

} // anonymous namespace

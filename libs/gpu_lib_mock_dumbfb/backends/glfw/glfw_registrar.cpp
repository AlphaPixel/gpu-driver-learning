#include "glfw_display.h"
#include "backend_registry.h"

// ---------------------------------------------------------------------------
// GLFW backend registration
//
// Preference = 100 — first-class windowed desktop backend, equal to SDL3.
// When both are available, stable_sort preserves registration order, so
// whichever TU links first wins.  Users who care can specify
// backend_priority = "sdl3" or "glfw" explicitly.
//
// supports_bezel = false — Phase 1 stub; will become true once bezel
// rendering is implemented alongside SDL3 in a later phase.
// ---------------------------------------------------------------------------

namespace {

const pixelgpu::internal::BackendDescriptor k_glfw_descriptor {
    .name           = "glfw",
    .preference     = 100,
    .supports_bezel = false,
    .create         = pixelgpu::backend::GLFWDisplay::try_create,
};

pixelgpu::internal::BackendRegistrar g_glfw_registrar{ k_glfw_descriptor };

} // anonymous namespace

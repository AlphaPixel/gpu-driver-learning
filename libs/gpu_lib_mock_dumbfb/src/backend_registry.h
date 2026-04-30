#pragma once

// Internal header — not part of the public API.
// Included only by backend translation units and display_factory.cpp.

#include "pixelgpu/display_params.h"
#include "pixelgpu/idisplay.h"

#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace pixelgpu::internal {

// ---------------------------------------------------------------------------
// BackendDescriptor
//
// Self-description that every backend registers exactly once at static
// initialisation time (before main() runs).
// ---------------------------------------------------------------------------
struct BackendDescriptor {
    // Short lowercase name, e.g. "sdl3", "glfw", "vnc".
    // Must be a string literal (static storage); the registry stores the
    // string_view directly.
    std::string_view name;

    // Higher value = more preferred.  No fixed scale; the only invariant
    // is that higher wins when multiple backends succeed.
    // Suggested tiers:
    //   100  — first-class windowed (SDL3, GLFW)
    //    50  — capable but secondary (e.g. platform-specific fallbacks)
    //    10  — headless / remoting (VNC, RDP)
    int preference;

    // True if this backend can render a monitor-bezel overlay.
    // Phase 1: always false.  SDL3 and GLFW will set this to true in a
    // later phase once bezel support is implemented.
    bool supports_bezel;

    // Factory function.
    //
    // Called by DisplayFactory::create() with the caller-supplied params.
    // Should return a fully-initialised, live IDisplay on success, or
    // nullptr if this backend cannot satisfy the request (library missing,
    // platform unsupported, capabilities insufficient, etc.).
    //
    // MUST NOT throw.  Catch all exceptions internally and return nullptr;
    // store a diagnostic message via BackendRegistry::set_last_error().
    std::unique_ptr<IDisplay> (*create)(const DisplayParams&) noexcept;
};

// ---------------------------------------------------------------------------
// BackendRegistry
//
// Process-lifetime singleton that accumulates BackendDescriptors as each
// backend's static initialiser runs, then serves them to DisplayFactory.
//
// All methods are thread-safe (guarded by an internal mutex).
// ---------------------------------------------------------------------------
class BackendRegistry {
public:
    // Meyer's singleton — safe on all supported platforms for single-process
    // use.  DLL-boundary considerations are deferred to a later phase.
    static BackendRegistry& instance() noexcept;

    // Called by BackendRegistrar constructors (static init time).
    // Duplicate names are silently ignored (last-registered wins).
    void register_backend(const BackendDescriptor& desc);

    // Returns all registered descriptors sorted by preference (descending).
    // The returned vector is a snapshot; subsequent registrations do not
    // affect it.
    std::vector<BackendDescriptor> sorted_by_preference() const;

    // Returns a descriptor for the named backend, or nullopt if not found.
    // Name comparison is case-sensitive.
    const BackendDescriptor* find_by_name(std::string_view name) const;

    // ── Diagnostic error string ────────────────────────────────────────────

    // Called by factory functions to record a human-readable failure reason.
    // Thread-safe; stored per-registry (last write wins — factory is
    // single-threaded at selection time).
    void set_last_error(std::string msg);
    std::string last_error() const;

    // ── Diagnostics ────────────────────────────────────────────────────────

    // Returns a human-readable summary of registered backends for debugging,
    // e.g.  "sdl3(100), glfw(100), vnc(10)"
    std::string registered_summary() const;

private:
    BackendRegistry() = default;

    mutable std::mutex          m_mutex;
    std::vector<BackendDescriptor> m_backends;
    std::string                 m_last_error;
};

// ---------------------------------------------------------------------------
// BackendRegistrar
//
// RAII helper.  Declare one static instance per backend TU:
//
//   namespace {
//       pixelgpu::internal::BackendRegistrar g_reg { k_my_descriptor };
//   }
//
// Its constructor calls BackendRegistry::instance().register_backend()
// before main() runs.
// ---------------------------------------------------------------------------
struct BackendRegistrar {
    explicit BackendRegistrar(const BackendDescriptor& desc) noexcept {
        BackendRegistry::instance().register_backend(desc);
    }
};

} // namespace pixelgpu::internal

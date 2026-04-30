#pragma once

#include "pixelgpu/display_params.h"
#include "pixelgpu/idisplay.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace pixelgpu {

// ---------------------------------------------------------------------------
// DisplayFactory
//
// Creates IDisplay instances by selecting among registered backends.
//
// Selection rules:
//
//   1. If params.backend_priority is non-empty, parse the comma-separated
//      list of backend names.  Try each in listed order.  The first one
//      whose create() returns non-null wins.
//
//   2. If params.backend_priority is empty, try all registered backends in
//      descending preference order.  The first successful create() wins.
//
//   3. If a text_arg is specified but no selected backend declares
//      supports_bezel == true, the factory fails immediately.
//
//   4. On any failure, create() returns nullptr.  Call last_error_string()
//      on the calling thread to retrieve a description.
//
// Thread-safety: create() is NOT thread-safe with respect to itself (do
// not call from multiple threads simultaneously).  The registry mutation
// (register_backend) IS thread-safe and safe to interleave with create().
// ---------------------------------------------------------------------------
class DisplayFactory {
public:
    // Create a display from the given params.
    // Returns nullptr on failure; see last_error_code() / last_error_string().
    static std::unique_ptr<IDisplay> create(const DisplayParams& params) noexcept;

    // Human-readable description of why the last create() on this thread
    // failed.  Empty if the last create() succeeded.
    static std::string_view last_error_string() noexcept;

    // Returns a human-readable summary of all registered backends and their
    // preference scores, e.g. "sdl3(100), glfw(100), vnc(10)".
    // Useful for diagnostics / --list-backends style CLI flags.
    static std::string registered_backends_summary();
};

} // namespace pixelgpu

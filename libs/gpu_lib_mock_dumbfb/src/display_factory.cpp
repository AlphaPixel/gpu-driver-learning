#include "pixelgpu/display_factory.h"
#include "backend_registry.h"

#include <sstream>
#include <string>

namespace pixelgpu {

// ---------------------------------------------------------------------------
// Thread-local error store
// ---------------------------------------------------------------------------
namespace {
    thread_local std::string tl_last_error;
} // anonymous namespace

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

// Split a comma-separated string into tokens, trimming ASCII whitespace.
std::vector<std::string> split_priority(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) {
        // Trim leading/trailing spaces
        const auto first = token.find_first_not_of(" \t\r\n");
        const auto last  = token.find_last_not_of(" \t\r\n");
        if (first != std::string::npos) {
            out.push_back(token.substr(first, last - first + 1));
        }
    }
    return out;
}

// Attempt to create an IDisplay using one specific descriptor.
// Returns the display on success, nullptr on failure.
// Records failure reason into tl_last_error.
std::unique_ptr<IDisplay> try_backend(
    const pixelgpu::internal::BackendDescriptor& desc,
    const DisplayParams&                          params,
    bool                                          need_bezel)
{
    if (need_bezel && !desc.supports_bezel) {
        tl_last_error =
            std::string("Backend '") + std::string(desc.name) +
            "' does not support bezels; text_arg was specified.";
        return nullptr;
    }

    auto display = desc.create(params);
    if (!display) {
        // desc.create() is expected to have set a registry error string
        const std::string reg_err =
            pixelgpu::internal::BackendRegistry::instance().last_error();
        tl_last_error =
            std::string("Backend '") + std::string(desc.name) +
            "' failed to create display" +
            (reg_err.empty() ? "." : ": " + reg_err);
    }
    return display;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
std::unique_ptr<IDisplay> DisplayFactory::create(const DisplayParams& params) noexcept {
    tl_last_error.clear();

    // Basic sanity checks before touching any backend.
    if (!params.resolution.is_valid()) {
        tl_last_error = "DisplayParams::resolution is invalid (zero width or height).";
        return nullptr;
    }
    if (params.framebuffer_ptr == nullptr) {
        tl_last_error = "DisplayParams::framebuffer_ptr is null.";
        return nullptr;
    }

    const bool need_bezel = !params.text_arg.empty();
    auto& registry = pixelgpu::internal::BackendRegistry::instance();

    // ── Path A: caller specified a priority list ───────────────────────────
    if (!params.backend_priority.empty()) {
        const auto names = split_priority(params.backend_priority);
        for (const auto& name : names) {
            const auto* desc = registry.find_by_name(name);
            if (!desc) {
                tl_last_error =
                    "Requested backend '" + name + "' is not registered. "
                    "Available: " + registry.registered_summary();
                continue; // try next in list
            }
            auto display = try_backend(*desc, params, need_bezel);
            if (display) return display;
        }
        // All named backends failed — compose a summary error.
        if (tl_last_error.find("not registered") == std::string::npos) {
            // Last error is already descriptive from try_backend(); keep it.
        }
        // If we only hit "not registered" errors, add context.
        if (tl_last_error.empty()) {
            tl_last_error = "No backend from priority list '"
                + params.backend_priority + "' could create a display.";
        }
        return nullptr;
    }

    // ── Path B: auto-select by preference score ────────────────────────────
    const auto candidates = registry.sorted_by_preference();
    if (candidates.empty()) {
        tl_last_error =
            "No display backends are registered. "
            "Ensure at least one backend library is linked.";
        return nullptr;
    }

    std::ostringstream tried;
    bool first = true;
    for (const auto& desc : candidates) {
        auto display = try_backend(desc, params, need_bezel);
        if (display) return display;

        if (!first) tried << ", ";
        tried << desc.name;
        first = false;
    }

    if (tl_last_error.empty()) {
        tl_last_error =
            "All registered backends failed to create a display. "
            "Tried: " + tried.str();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
std::string_view DisplayFactory::last_error_string() noexcept {
    return tl_last_error;
}

// ---------------------------------------------------------------------------
std::string DisplayFactory::registered_backends_summary() {
    return pixelgpu::internal::BackendRegistry::instance().registered_summary();
}

} // namespace pixelgpu

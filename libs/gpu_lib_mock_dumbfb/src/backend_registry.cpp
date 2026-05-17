#include "backend_registry.h"

#include <algorithm>
#include <sstream>

namespace pixelgpu::internal {

// ---------------------------------------------------------------------------
BackendRegistry& BackendRegistry::instance() noexcept {
    // Meyer's singleton — constructed once, destroyed at program exit.
    static BackendRegistry s_instance;
    return s_instance;
}

// ---------------------------------------------------------------------------
void BackendRegistry::register_backend(const BackendDescriptor& desc) {
    std::lock_guard lock(m_mutex);

    // Duplicate name: last-registered wins (allows unit tests to replace
    // backends without linker tricks).
    for (auto& existing : m_backends) {
        if (existing.name == desc.name) {
            existing = desc;
            return;
        }
    }
    m_backends.push_back(desc);
}

// ---------------------------------------------------------------------------
std::vector<BackendDescriptor> BackendRegistry::sorted_by_preference() const {
    std::lock_guard lock(m_mutex);

    auto sorted = m_backends; // copy
    std::stable_sort(sorted.begin(), sorted.end(),
        [](const BackendDescriptor& a, const BackendDescriptor& b) {
            return a.preference > b.preference; // descending
        });
    return sorted;
}

// ---------------------------------------------------------------------------
const BackendDescriptor* BackendRegistry::find_by_name(std::string_view name) const {
    std::lock_guard lock(m_mutex);
    for (const auto& d : m_backends) {
        if (d.name == name) return &d;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
void BackendRegistry::set_last_error(std::string msg) {
    std::lock_guard lock(m_mutex);
    m_last_error = std::move(msg);
}

std::string BackendRegistry::last_error() const {
    std::lock_guard lock(m_mutex);
    return m_last_error;
}

// ---------------------------------------------------------------------------
std::string BackendRegistry::registered_summary() const {
    std::lock_guard lock(m_mutex);

    auto sorted = m_backends;
    std::stable_sort(sorted.begin(), sorted.end(),
        [](const BackendDescriptor& a, const BackendDescriptor& b) {
            return a.preference > b.preference;
        });

    std::ostringstream oss;
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << sorted[i].name << "(" << sorted[i].preference << ")";
    }
    return oss.str();
}

} // namespace pixelgpu::internal

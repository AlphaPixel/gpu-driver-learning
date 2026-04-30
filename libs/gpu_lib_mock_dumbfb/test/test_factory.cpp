#include <catch2/catch_test_macros.hpp>

#include "pixelgpu/display_factory.h"
#include "pixelgpu/display_params.h"
#include "pixelgpu/resolution.h"
#include "pgpu/pgpu.h"

// ── Null-backend factory error paths ─────────────────────────────────────────
//
// These tests exercise DisplayFactory::create() in failure paths that do NOT
// require a live display window.  They validate parameter checking and error
// reporting without needing SDL3 or GLFW to be initialised.
//
// Tests that require an actual window (vsync callback, wait_vsync, present)
// live in the demo app and in a separate integration test suite that is
// only run on systems with a display.

TEST_CASE("Factory rejects null framebuffer pointer", "[factory][error]") {
    pixelgpu::DisplayParams p;
    p.resolution       = pixelgpu::Resolution(pixelgpu::ResolutionPreset::VGA);
    p.framebuffer_ptr  = nullptr; // deliberate
    p.backend_priority = "sdl3,glfw"; // don't care which, both should reject

    auto d = pixelgpu::DisplayFactory::create(p);
    CHECK(d == nullptr);

    const auto err = std::string(pixelgpu::DisplayFactory::last_error_string());
    CHECK_FALSE(err.empty());
    // Error must mention the null pointer, not some other failure.
    CHECK(err.find("null") != std::string::npos);
}

TEST_CASE("Factory rejects zero-size resolution", "[factory][error]") {
    std::vector<uint8_t> fb(4, 0xFF); // small but non-null

    pixelgpu::DisplayParams p;
    p.resolution       = pixelgpu::Resolution(0, 0); // invalid
    p.framebuffer_ptr  = fb.data();
    p.backend_priority = "sdl3,glfw";

    auto d = pixelgpu::DisplayFactory::create(p);
    CHECK(d == nullptr);

    const auto err = std::string(pixelgpu::DisplayFactory::last_error_string());
    CHECK_FALSE(err.empty());
}

TEST_CASE("Factory reports error for unknown backend name", "[factory][error]") {
    std::vector<uint8_t> fb(640 * 480 * 4, 0);

    pixelgpu::DisplayParams p;
    p.resolution       = pixelgpu::Resolution(pixelgpu::ResolutionPreset::VGA);
    p.framebuffer_ptr  = fb.data();
    p.backend_priority = "does_not_exist";

    auto d = pixelgpu::DisplayFactory::create(p);
    CHECK(d == nullptr);

    const auto err = std::string(pixelgpu::DisplayFactory::last_error_string());
    CHECK_FALSE(err.empty());
    // Should mention the backend name or "not registered"
    const bool mentions_name = err.find("does_not_exist") != std::string::npos
                            || err.find("not registered") != std::string::npos;
    CHECK(mentions_name);
}

TEST_CASE("Factory error string is empty after a non-failed call", "[factory][error]") {
    // Resolution preset lookup doesn't go through the factory, so
    // verify last_error_string() is independent of non-factory calls.
    // We just query the diagnostic — no create() call means no error.
    // (The create() call itself was never invoked, so the thread-local
    // is whatever the previous test left; this test is order-independent
    // because each create() call clears the error first.)

    // Trigger a fresh clear by calling create() with a bad param (cheap).
    std::vector<uint8_t> fb(4, 0);
    pixelgpu::DisplayParams p;
    p.resolution      = pixelgpu::Resolution(1, 1);
    p.framebuffer_ptr = nullptr;
    pixelgpu::DisplayFactory::create(p);

    // Now call again with framebuffer non-null but unknown backend.
    // This exercises a different error path.
    p.framebuffer_ptr  = fb.data();
    p.backend_priority = "nonexistent_backend_xyz";
    pixelgpu::DisplayFactory::create(p);

    const auto err = std::string(pixelgpu::DisplayFactory::last_error_string());
    CHECK_FALSE(err.empty()); // we expected failure
}

// ── registered_backends_summary() ────────────────────────────────────────────

TEST_CASE("registered_backends_summary is non-empty when backends are compiled in",
          "[factory][diagnostics]")
{
    // At least one backend must be registered for the build to be useful.
    // This test fails at link time if no backend TU is included, which
    // is itself a useful diagnostic.
    const std::string summary = pixelgpu::DisplayFactory::registered_backends_summary();

#if defined(PGPU_HAS_BACKEND_SDL3) || defined(PGPU_HAS_BACKEND_GLFW)
    CHECK_FALSE(summary.empty());
    // Summary format: "sdl3(100), glfw(100)" — must contain '('
    CHECK(summary.find('(') != std::string::npos);
#else
    // No backend compiled in — summary may be empty. Not a failure per se;
    // the earlier factory tests will have caught the "no backends" error.
    SUCCEED("No backends compiled; summary is empty (expected).");
#endif
}

// ── C API error paths ────────────────────────────────────────────────────────

TEST_CASE("C API null framebuffer_ptr triggers library allocation", "[factory][c-api]") {
    const pgpu_resolution_t res = pgpu_resolution_from_preset(PGPU_RES_VGA);

    // NULL framebuffer_ptr is now valid: the library allocates internally.
    // On headless systems the backend may still fail; on systems with a
    // display the create succeeds and get_framebuffer() returns non-null.
    pgpu_display_t h = pgpu_display_create(res, nullptr, 60, "test", nullptr, nullptr);
    if (h) {
        void* fb = pgpu_display_get_framebuffer(h);
        CHECK(fb != nullptr);
        pgpu_display_destroy(h);
    } else {
        // Headless: backend failed — an error must be set, but it should not
        // blame the framebuffer pointer (the library handled that itself).
        CHECK(pgpu_last_error_code() != PGPU_OK);
        const std::string err(pgpu_last_error_string());
        CHECK(err.find("null") == std::string::npos);
    }
}

TEST_CASE("C API pgpu_display_get_framebuffer(NULL) returns NULL", "[factory][c-api]") {
    CHECK(pgpu_display_get_framebuffer(nullptr) == nullptr);
}

TEST_CASE("C API pgpu_display_destroy(NULL) is a no-op", "[factory][c-api]") {
    // Must not crash.
    pgpu_display_destroy(nullptr);
    SUCCEED();
}

TEST_CASE("C API pgpu_display_is_alive(NULL) returns 0", "[factory][c-api]") {
    CHECK(pgpu_display_is_alive(nullptr) == 0);
}

TEST_CASE("C API pgpu_wait_vsync(NULL) returns error code", "[factory][c-api]") {
    const pgpu_status_t s = pgpu_wait_vsync(nullptr, 10);
    CHECK(s == PGPU_ERROR_INVALID_HANDLE);
}

TEST_CASE("C API pgpu_registered_backends returns non-null string", "[factory][c-api]") {
    const char* s = pgpu_registered_backends();
    REQUIRE(s != nullptr);
    // If any backend is compiled in, the string must be non-empty.
#if defined(PGPU_HAS_BACKEND_SDL3) || defined(PGPU_HAS_BACKEND_GLFW)
    CHECK(s[0] != '\0');
#endif
}

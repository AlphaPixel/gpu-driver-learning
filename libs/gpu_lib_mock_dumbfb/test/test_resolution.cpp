#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "pixelgpu/resolution.h"
#include "pgpu/pgpu.h"

using namespace pixelgpu;

// ── Preset table correctness ─────────────────────────────────────────────────

TEST_CASE("Resolution presets return correct dimensions", "[resolution][presets]") {
    // Spot-check a representative sample from each era.

    SECTION("CGA") {
        const Resolution r(ResolutionPreset::CGA);
        CHECK(r.width  == 320);
        CHECK(r.height == 200);
        CHECK(r.is_valid());
    }

    SECTION("VGA") {
        const Resolution r(ResolutionPreset::VGA);
        CHECK(r.width  == 640);
        CHECK(r.height == 480);
    }

    SECTION("HD1080") {
        const Resolution r(ResolutionPreset::HD1080);
        CHECK(r.width  == 1920);
        CHECK(r.height == 1080);
    }

    SECTION("UHD4K") {
        const Resolution r(ResolutionPreset::UHD4K);
        CHECK(r.width  == 3840);
        CHECK(r.height == 2160);
    }

    SECTION("GAMEBOY") {
        const Resolution r(ResolutionPreset::GAMEBOY);
        CHECK(r.width  == 160);
        CHECK(r.height == 144);
    }

    SECTION("GAMEBOY_ADVANCE") {
        const Resolution r(ResolutionPreset::GAMEBOY_ADVANCE);
        CHECK(r.width  == 240);
        CHECK(r.height == 160);
    }

    SECTION("PSP") {
        const Resolution r(ResolutionPreset::PSP);
        CHECK(r.width  == 480);
        CHECK(r.height == 272);
    }
}

TEST_CASE("resolution_from_preset() and preset constructor agree", "[resolution][presets]") {
    for (uint32_t i = 0; i < static_cast<uint32_t>(ResolutionPreset::_COUNT); ++i) {
        const auto preset = static_cast<ResolutionPreset>(i);
        const Resolution via_fn  = resolution_from_preset(preset);
        const Resolution via_ctor(preset);
        REQUIRE(via_fn == via_ctor);
        INFO("preset index " << i);
        CHECK(via_fn.is_valid());
    }
}

TEST_CASE("Out-of-range preset returns invalid Resolution", "[resolution][presets]") {
    const Resolution r = resolution_from_preset(
        static_cast<ResolutionPreset>(9999));
    CHECK_FALSE(r.is_valid());
    CHECK(r.width  == 0);
    CHECK(r.height == 0);
}

// ── Computed properties ───────────────────────────────────────────────────────

TEST_CASE("stride_bytes() is always width * 4", "[resolution][computed]") {
    const Resolution vga(ResolutionPreset::VGA);
    CHECK(vga.stride_bytes() == 640u * 4u);

    const Resolution custom(123, 456);
    CHECK(custom.stride_bytes() == 123u * 4u);
}

TEST_CASE("frame_bytes() is stride * height", "[resolution][computed]") {
    const Resolution vga(ResolutionPreset::VGA);
    CHECK(vga.frame_bytes() == static_cast<uint64_t>(640u * 4u) * 480u);
}

TEST_CASE("aspect_ratio() is correct", "[resolution][computed]") {
    const Resolution r43(640, 480);
    CHECK(r43.aspect_ratio() == Catch::Approx(4.0f / 3.0f).epsilon(0.001f));

    const Resolution r169(1920, 1080);
    CHECK(r169.aspect_ratio() == Catch::Approx(16.0f / 9.0f).epsilon(0.001f));

    // Zero height → 0
    const Resolution zero(0, 0);
    CHECK(zero.aspect_ratio() == Catch::Approx(0.0f));
}

TEST_CASE("Equality operators work correctly", "[resolution]") {
    const Resolution a(640, 480);
    const Resolution b(640, 480);
    const Resolution c(800, 600);

    CHECK(a == b);
    CHECK_FALSE(a == c);
    CHECK(a != c);
    CHECK_FALSE(a != b);
}

// ── Default construction ──────────────────────────────────────────────────────

TEST_CASE("Default-constructed Resolution is invalid", "[resolution]") {
    const Resolution r;
    CHECK_FALSE(r.is_valid());
    CHECK(r.width  == 0);
    CHECK(r.height == 0);
}

// ── C API binding ─────────────────────────────────────────────────────────────

TEST_CASE("C API resolution_from_preset matches C++ preset table", "[resolution][c-api]") {
    // VGA
    const pgpu_resolution_t cv = pgpu_resolution_from_preset(PGPU_RES_VGA);
    CHECK(cv.width  == 640);
    CHECK(cv.height == 480);

    // HD1080
    const pgpu_resolution_t ch = pgpu_resolution_from_preset(PGPU_RES_HD1080);
    CHECK(ch.width  == 1920);
    CHECK(ch.height == 1080);

    // GAMEBOY
    const pgpu_resolution_t cg = pgpu_resolution_from_preset(PGPU_RES_GAMEBOY);
    CHECK(cg.width  == 160);
    CHECK(cg.height == 144);
}

TEST_CASE("C API pgpu_resolution_make round-trips dimensions", "[resolution][c-api]") {
    const pgpu_resolution_t r = pgpu_resolution_make(1234, 5678);
    CHECK(r.width  == 1234);
    CHECK(r.height == 5678);
}

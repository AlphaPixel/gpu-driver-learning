# ── gpu_lib_mock_dumbfb ─────────────────────────────────────────────────────
#
# Core sources — always compiled.
# Backend sources are compiled into separate static libs and linked in
# conditionally below, depending on which dependencies are available.

add_library(gpu_lib_mock_dumbfb STATIC
    src/resolution_presets.cpp
    src/backend_registry.cpp
    src/display_factory.cpp
    src/c_api.cpp
)

target_link_libraries(gpu_lib_mock_dumbfb
    PRIVATE
        gpu_project_options
        gpu_project_warnings
)

target_compile_options(gpu_lib_mock_dumbfb PRIVATE $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/utf-8>)

# Public C++ headers  →  #include <pixelgpu/...>
target_include_directories(gpu_lib_mock_dumbfb
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
)

# Public C header     →  #include <pgpu/pgpu.h>
target_include_directories(gpu_lib_mock_dumbfb
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include_c>"
        "$<INSTALL_INTERFACE:include_c>"
)

# Internal src/ headers (backend_registry.h etc.) are PRIVATE.
target_include_directories(gpu_lib_mock_dumbfb
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# ── Backend: SDL3 ────────────────────────────────────────────────────────────
# Backends are OBJECT libraries whose compiled objects are merged into this
# static lib via $<TARGET_OBJECTS:>.  Using target_sources() instead of
# target_link_libraries() avoids tracking the backend as a CMake target
# dependency, which would otherwise require it to appear in the install
# export set alongside gpu_lib_mock_dumbfb.

if(FEATURE_BACKEND_SDL3)
    find_package(SDL3 CONFIG QUIET)
    if(SDL3_FOUND)
        add_subdirectory(backends/sdl3)
        target_sources(gpu_lib_mock_dumbfb PRIVATE $<TARGET_OBJECTS:gpu_backend_sdl3>)
        target_link_libraries(gpu_lib_mock_dumbfb PRIVATE SDL3::SDL3)
        target_compile_definitions(gpu_lib_mock_dumbfb PUBLIC PGPU_HAS_BACKEND_SDL3)
        message(STATUS "PixelGPU: SDL3 backend ENABLED (preference 100)")
    else()
        message(STATUS "PixelGPU: SDL3 not found — SDL3 backend DISABLED")
    endif()
else()
    message(STATUS "PixelGPU: SDL3 backend DISABLED by FEATURE_BACKEND_SDL3=OFF")
endif()

# ── Backend: GLFW ────────────────────────────────────────────────────────────

if(FEATURE_BACKEND_GLFW)
    find_package(glfw3 CONFIG QUIET)
    if(glfw3_FOUND)
        add_subdirectory(backends/glfw)
        target_sources(gpu_lib_mock_dumbfb PRIVATE $<TARGET_OBJECTS:gpu_backend_glfw>)
        find_package(OpenGL REQUIRED)
        target_link_libraries(gpu_lib_mock_dumbfb PRIVATE glfw OpenGL::GL)
        target_compile_definitions(gpu_lib_mock_dumbfb PUBLIC PGPU_HAS_BACKEND_GLFW)
        message(STATUS "PixelGPU: GLFW backend ENABLED (preference 100)")
    else()
        message(STATUS "PixelGPU: GLFW not found — GLFW backend DISABLED")
    endif()
else()
    message(STATUS "PixelGPU: GLFW backend DISABLED by FEATURE_BACKEND_GLFW=OFF")
endif()

# ── Backend: VNC (opt-in; off by default) ────────────────────────────────────
# FEATURE_BACKEND_VNC is declared in Features.cmake and defaults OFF.
# Wired here for future use; the backend subdirectory does not exist yet.
if(FEATURE_BACKEND_VNC)
    message(STATUS "PixelGPU: VNC backend requested — not yet implemented")
endif()

# ── Tests ────────────────────────────────────────────────────────────────────

if(FEATURE_TESTS)
    add_subdirectory(test)
endif()

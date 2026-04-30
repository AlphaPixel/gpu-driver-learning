add_library(gpu_backend_sdl3 OBJECT
    sdl3_display.cpp
    sdl3_registrar.cpp
)

target_link_libraries(gpu_backend_sdl3
    PRIVATE
        gpu_project_options
        gpu_project_warnings
)

target_include_directories(gpu_backend_sdl3
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/libs/gpu_lib_mock_dumbfb/src"
        "${CMAKE_SOURCE_DIR}/libs/gpu_lib_mock_dumbfb/include"
)

# PRIVATE — SDL3 is needed only to compile the backend object files.
# The parent lib (gpu_lib_mock_dumbfb) adds SDL3::SDL3 to its own link
# interface and the PGPU_HAS_BACKEND_SDL3 define to its public interface.
target_link_system_libraries(gpu_backend_sdl3 PRIVATE SDL3::SDL3)

target_compile_definitions(gpu_backend_sdl3 PRIVATE PGPU_HAS_BACKEND_SDL3)

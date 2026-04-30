add_library(gpu_backend_glfw OBJECT
    glfw_display.cpp
    glfw_registrar.cpp
)

target_link_libraries(gpu_backend_glfw
    PRIVATE
        gpu_project_options
        gpu_project_warnings
)

target_include_directories(gpu_backend_glfw
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/libs/gpu_lib_mock_dumbfb/src"
        "${CMAKE_SOURCE_DIR}/libs/gpu_lib_mock_dumbfb/include"
)

# PRIVATE — glfw and OpenGL are needed only to compile the backend objects.
# The parent lib adds these to its own link interface.
target_link_system_libraries(gpu_backend_glfw PRIVATE glfw)

find_package(OpenGL REQUIRED)
target_link_system_libraries(gpu_backend_glfw PRIVATE OpenGL::GL)

target_compile_definitions(gpu_backend_glfw PRIVATE PGPU_HAS_BACKEND_GLFW)

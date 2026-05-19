# Separate library part and executable part (`main()` function) to simplify testing.

# ----------------------------------------------------------------------------------------------------------------------
# library part
add_library(gpu_app_mock_dumbfb_lib-lesson1-done INTERFACE) # use interface or normal library according to the implementation
# link project_options/warnings
target_link_libraries(gpu_app_mock_dumbfb_lib-lesson1-done INTERFACE gpu_project_options gpu_project_warnings)

# Includes
target_include_interface_directories(gpu_app_mock_dumbfb_lib-lesson1-done include)

target_compile_options(gpu_app_mock_dumbfb_lib-lesson1-done INTERFACE $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/utf-8>)

# Link dependencies:
target_link_system_libraries(gpu_app_mock_dumbfb_lib-lesson1-done INTERFACE fmt::fmt)

# ----------------------------------------------------------------------------------------------------------------------
# executable part
add_executable(gpu_app_mock_dumbfb-lesson1-done "./src/gpu_app_mock_dumbfb-lesson1-done.cpp") # just for `main.cpp` which contains `main()` function
# link project_options/warnings
target_link_libraries(gpu_app_mock_dumbfb-lesson1-done PRIVATE gpu_project_options gpu_project_warnings)

# Includes, just link the library part
target_link_libraries(gpu_app_mock_dumbfb-lesson1-done PRIVATE gpu_app_mock_dumbfb_lib)

# Link the display library with WHOLE_ARCHIVE so the MSVC linker keeps the
# backend registrar objects (sdl3_registrar.obj, glfw_registrar.obj), which
# have no externally-referenced symbols and would otherwise be stripped.
# WHOLE_ARCHIVE also propagates the library's PUBLIC include directories.
target_link_libraries(gpu_app_mock_dumbfb-lesson1-done PRIVATE
    $<LINK_LIBRARY:WHOLE_ARCHIVE,gpu_lib_mock_dumbfb>)

# Copy runtime DLLs (SDL3, GLFW, fmt) next to the executable on Windows so
# it can run without manually setting PATH.
if(WIN32)
    add_custom_command(TARGET gpu_app_mock_dumbfb-lesson1-done POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:gpu_app_mock_dumbfb-lesson1-done>
            $<TARGET_FILE_DIR:gpu_app_mock_dumbfb-lesson1-done>
        COMMAND_EXPAND_LISTS
    )
endif()
# Link dependencies
target_link_system_libraries(gpu_app_mock_dumbfb-lesson1-done PRIVATE fmt::fmt)


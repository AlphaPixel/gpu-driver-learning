add_executable(gpu_lib_mock_dumbfb_tests
    test_resolution.cpp
    test_factory.cpp
)

target_link_libraries(gpu_lib_mock_dumbfb_tests
    PRIVATE
        $<LINK_LIBRARY:WHOLE_ARCHIVE,gpu_lib_mock_dumbfb>
        gpu_project_options
        gpu_project_warnings
        catch2_test_common
)

# Tests need access to both public header trees.
target_include_directories(gpu_lib_mock_dumbfb_tests
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../include_c"
)

include(Catch)
catch_discover_tests(gpu_lib_mock_dumbfb_tests)

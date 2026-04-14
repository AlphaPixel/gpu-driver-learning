# test dependencies

# test executable
add_executable(gpu_lib_mock_dumbfb_tests "./gpu_lib_mock_dumbfb_tests.cpp")

target_link_libraries(
  gpu_lib_mock_dumbfb_tests
  PRIVATE gpu_lib_mock_dumbfb
          gpu_project_warnings
          gpu_project_options
          catch2_test_common)

# use xml reporter if coverage is enabled
if(${ENABLE_COVERAGE})
  set(COVERAGE_ARGS REPORTER xml)
endif()

# automatically discover tests that are defined in catch based test files you can modify the tests
catch_discover_tests(gpu_lib_mock_dumbfb_tests ${COVERAGE_ARGS})

# constexpr tests
add_subdirectory("./constexpr")

# Add a file containing a set of constexpr tests
add_executable(gpu_lib_mock_dumbfb_constexpr_tests "./gpu_lib_mock_dumbfb_constexpr_tests.cpp")

target_link_libraries(
  gpu_lib_mock_dumbfb_constexpr_tests
  PRIVATE gpu_lib_mock_dumbfb
          gpu_project_warnings
          gpu_project_options
          catch2_test_common)

catch_discover_tests(gpu_lib_mock_dumbfb_constexpr_tests ${COVERAGE_ARGS})

# Disable the constexpr portion of the test, and build again this allows us to have an executable that we can debug when
# things go wrong with the constexpr testing
add_executable(gpu_lib_mock_dumbfb_relaxed_constexpr_tests "./gpu_lib_mock_dumbfb_constexpr_tests.cpp")

target_link_libraries(
  gpu_lib_mock_dumbfb_relaxed_constexpr_tests
  PRIVATE gpu_lib_mock_dumbfb
          gpu_project_warnings
          gpu_project_options
          catch2_test_common)
target_compile_definitions(gpu_lib_mock_dumbfb_relaxed_constexpr_tests PRIVATE -DCATCH_CONFIG_RUNTIME_STATIC_REQUIRE)

catch_discover_tests(gpu_lib_mock_dumbfb_relaxed_constexpr_tests ${COVERAGE_ARGS})

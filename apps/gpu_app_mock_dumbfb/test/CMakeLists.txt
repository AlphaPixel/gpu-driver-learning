# test dependencies

# ----------------------------------------------------------------------------------------------------------------------
# test the executable part
add_test(
  # calling my_exe executable directly
  NAME gpu_app_mock_dumbfb_test
  COMMAND gpu_app_mock_dumbfb ""
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# ----------------------------------------------------------------------------------------------------------------------
# test the library part
add_executable(gpu_app_mock_dumbfb_lib_tests "./gpu_app_mock_dumbfb_tests.cpp")
target_link_libraries(
  gpu_app_mock_dumbfb_lib_tests
  PRIVATE gpu_project_warnings
          gpu_project_options
          catch2_test_common
          gpu_app_mock_dumbfb_lib)

# use xml reporter if coverage is enabled
if(${ENABLE_COVERAGE})
  set(COVERAGE_ARGS REPORTER xml)
endif()

# automatically discover tests that are defined in catch based test files you can modify the tests
catch_discover_tests(gpu_app_mock_dumbfb_lib_tests ${COVERAGE_ARGS})

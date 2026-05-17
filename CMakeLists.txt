cmake_minimum_required(VERSION 3.21...3.28)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(FetchContent)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

# Fetch project_options
set(PROJECT_OPTIONS_VERSION "v0.41.0")
FetchContent_Declare(
  _project_options
  URL https://github.com/aminya/project_options/archive/refs/tags/${PROJECT_OPTIONS_VERSION}.zip)
FetchContent_MakeAvailable(_project_options)
include(${_project_options_SOURCE_DIR}/Index.cmake)

include("./Features.cmake")

option(ENABLE_VCPKG "Use vcpkg to install dependencies" ${WIN32})

# Install vcpkg dependencies when enabled — must come before project()
if(ENABLE_VCPKG)
  run_vcpkg(
    VCPKG_URL "https://github.com/microsoft/vcpkg.git"
    VCPKG_REV "50c0cb48a0cf2f6fc5c7b2c0d2bafbe26d0a7ca2")
endif()

project(
  gpu_driver_learning
  VERSION 0.1.0
  DESCRIPTION "PixelGPU driver learning project"
  HOMEPAGE_URL "https://github.com/AlphaPixel/gpu-driver-learning"
  LANGUAGES CXX C)

include("./cmake/Dependencies.cmake")

if(FEATURE_TESTS)
  enable_testing()
  add_library(catch2_test_common INTERFACE)
  target_find_dependencies(catch2_test_common INTERFACE_CONFIG Catch2)
  target_link_libraries(catch2_test_common INTERFACE Catch2::Catch2 Catch2::Catch2WithMain)
  target_compile_definitions(catch2_test_common INTERFACE DO_NOT_USE_WMAIN)
  include(Catch)
endif()

project_options(
  PREFIX "gpu"
  ENABLE_CACHE
  # Uncomment extras as needed:
  # ENABLE_CPPCHECK
  # ENABLE_CLANG_TIDY
  # ENABLE_VS_ANALYSIS
  # ENABLE_COVERAGE
  # ENABLE_SANITIZER_ADDRESS
)

add_subdirectory(./libs/)
add_subdirectory(./apps/)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT gpu_app)

if(CMAKE_SKIP_INSTALL_RULES)
  return()
endif()

package_project(
  TARGETS gpu_app_mock_dumbfb gpu_lib_mock_dumbfb gpu_project_options gpu_project_warnings)

include(FetchContent)

find_package(fmt 11.0.2 CONFIG QUIET)
if(NOT fmt_FOUND)
  FetchContent_Declare(
    fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/11.0.2.zip)
  FetchContent_MakeAvailable(fmt)
endif()

if(FEATURE_TESTS)
  find_package(Catch2 3.8.0 CONFIG QUIET)
  if(NOT Catch2_FOUND)
    FetchContent_Declare(
      Catch2
      URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.8.0.zip)
    FetchContent_MakeAvailable(Catch2)
  endif()

  if(DEFINED catch2_SOURCE_DIR AND EXISTS "${catch2_SOURCE_DIR}/extras/Catch.cmake")
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  elseif(DEFINED Catch2_DIR)
    find_file(
      CATCH2_CMAKE_MODULE
      Catch.cmake
      PATHS
        "${Catch2_DIR}"
        "${Catch2_DIR}/../extras"
        "${Catch2_DIR}/../../extras"
      NO_DEFAULT_PATH)
    if(CATCH2_CMAKE_MODULE)
      get_filename_component(CATCH2_CMAKE_MODULE_DIR "${CATCH2_CMAKE_MODULE}" DIRECTORY)
      list(APPEND CMAKE_MODULE_PATH "${CATCH2_CMAKE_MODULE_DIR}")
    endif()
  endif()
endif()

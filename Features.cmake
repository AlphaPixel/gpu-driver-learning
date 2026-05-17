# display backends
option(FEATURE_BACKEND_SDL3 "Enable SDL3 display backend"      ON)
option(FEATURE_BACKEND_GLFW "Enable GLFW display backend"      ON)
option(FEATURE_BACKEND_VNC  "Enable VNC/libvncserver backend"  OFF)

# building the tests
option(FEATURE_TESTS "Enable the tests" OFF)
if(FEATURE_TESTS)
  list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

# building the docs
option(FEATURE_DOCS "Enable the docs" OFF)

# fuzz tests
option(FEATURE_FUZZ_TESTS "Enable the fuzz tests" OFF)

option(ENABLE_CROSS_COMPILING "Detect cross compiler and setup toolchain" OFF)

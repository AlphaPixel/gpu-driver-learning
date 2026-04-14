add_library(gpu_lib_mock_dumbfb "./src/gpu_lib_mock_dumbfb/gpu_lib_mock_dumbfb.cpp")
# link project_options/warnings
target_link_libraries(gpu_lib_mock_dumbfb PRIVATE gpu_project_options gpu_project_warnings)

# Includes
target_include_interface_directories(gpu_lib_mock_dumbfb include)

# Find dependencies:
target_find_dependencies(gpu_lib_mock_dumbfb PRIVATE_CONFIG fmt)
set_target_properties(fmt::fmt PROPERTIES INTERFACE_COMPILE_OPTIONS
                                          $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/utf-8>)

# Link dependencies:
target_link_system_libraries(gpu_lib_mock_dumbfb PRIVATE fmt::fmt)

if(FEATURE_TESTS)
  add_subdirectory("./test")
endif()

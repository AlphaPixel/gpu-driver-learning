#include <catch2/catch_test_macros.hpp>

#include <gpu_lib_mock_dumbfb/gpu_lib_mock_dumbfb.h>

TEST_CASE("some_constexpr_fun") {
  STATIC_REQUIRE(some_constexpr_fun() == 0);
}

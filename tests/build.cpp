#define BOOST_TEST_MODULE uqm_build
#include <boost/test/unit_test.hpp>

#include <array>
#include <type_traits>

#include "libs/mathlib.h"

// Exercise the real C API from C++17, including the shared legacy headers.
static_assert(__cplusplus >= 201703L);
static_assert(std::is_same_v<decltype(true), bool>);
static_assert(sizeof(DWORD) == 4);

BOOST_AUTO_TEST_CASE(random_stream_can_be_replayed_from_cpp)
{
    TFB_SeedRandom(12345);
    std::array<DWORD, 32> values{};
    for (auto& value : values)
        value = TFB_Random();

    TFB_SeedRandom(12345);
    for (const auto value : values)
        BOOST_TEST(TFB_Random() == value);
}

BOOST_AUTO_TEST_CASE(random_seed_can_be_restored_from_cpp)
{
    TFB_SeedRandom(12345);
    const auto seed = TFB_SeedRandom(67890);
    BOOST_TEST(seed == 12345u);
    TFB_SeedRandom(seed);
    const auto next = TFB_Random();
    TFB_SeedRandom(12345);
    BOOST_TEST(TFB_Random() == next);
}

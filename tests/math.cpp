#include <boost/test/unit_test.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include "libs/mathlib.h"
#include "uqm/units.h"

extern "C" int math_c_contract(void);

static_assert(NORMALIZE_ANGLE(-1) == 63);
static_assert(NORMALIZE_FACING(-1) == 15);
static_assert(ANGLE_TO_FACING(FULL_CIRCLE) == 16);
static_assert(FACING_TO_ANGLE(16) == FULL_CIRCLE);
static_assert(DEGREES_TO_ANGLE(-90) == 48);
static_assert(ANGLE_TO_DEGREES(-16) == 270);

BOOST_AUTO_TEST_CASE(math_integer_square_root_boundaries_and_planet_inputs) {
    for (const DWORD value : {0u, 1u, 2u, 3u, 4u, 15u, 16u, 17u,
            65535u, 65536u, 65537u, 67000000u, 4294836224u,
            4294836225u, 4294836226u, UINT32_MAX}) {
        const uint64_t root = square_root(value);
        BOOST_TEST(root * root <= value);
        BOOST_TEST((root + 1) * (root + 1) > value);
    }
    // CalcTemp's nested integer roots for the generic 33% albedo.
    BOOST_TEST(square_root(square_root(67u * 100 * 10000)) == 90u);
    BOOST_TEST(square_root(UINT32_MAX) == 65535u);
}

BOOST_AUTO_TEST_CASE(math_random_legacy_sequence_and_standard_engine_difference) {
    RandomContext context{RandomDefaultSeed};
    const std::array<DWORD, 16> expected{207482415, 1790989824, 2035175616,
        77048696, 24794531, 109854999, 1644515420, 1256127050,
        1963079340, 1683198519, 715426902, 419002361, 573802814,
        1702319868, 2112876144, 319765416};
    for (const auto value : expected)
        BOOST_TEST(RandomContext_Random(&context) == value);
    // A direct standard-engine replacement would change the game's stream.
    std::minstd_rand0 standard(RandomDefaultSeed);
    standard.discard(14);
    BOOST_TEST(standard() == 2112876142u);
    BOOST_TEST(expected[14] == 2112876144u);
}

BOOST_AUTO_TEST_CASE(math_random_seed_coercion_and_global_context_agreement) {
    struct Example { DWORD input, state, next; };
    for (const auto example : {Example{0, 1, 16807}, {1, 1, 16807},
            {2147483647u, 2147483647u, 1}, {2147483648u, 1, 16807},
            {4294967294u, 2147483647u, 1}, {UINT32_MAX, 2147483648u, 16807}}) {
        RandomContext context{RandomDefaultSeed};
        BOOST_TEST(RandomContext_SeedRandom(&context, example.input) == RandomDefaultSeed);
        const RandomContext &readOnly = context;
        BOOST_TEST(RandomContext_GetSeed(&readOnly) == example.state);
        const auto saved = TFB_SeedRandom(example.input);
        BOOST_TEST(RandomContext_Random(&context) == example.next);
        BOOST_TEST(TFB_Random() == example.next);
        TFB_SeedRandom(saved);
    }
}

BOOST_AUTO_TEST_CASE(math_random_value_copy_and_checksum_style_restoration) {
    RandomContext context{RandomDefaultSeed};
    RandomContext_Random(&context);
    auto copy = context;
    const auto state = RandomContext_GetSeed(&context);
    const auto next = RandomContext_Random(&context);
    BOOST_TEST(RandomContext_GetSeed(&copy) == state);
    BOOST_TEST(RandomContext_Random(&copy) == next);
    RandomContext_SeedRandom(&context, state);
    BOOST_TEST(RandomContext_Random(&context) == next);
    const auto saved = TFB_SeedRandom(state);
    const auto checksumState = TFB_SeedRandom(0);
    TFB_SeedRandom(checksumState);
    BOOST_TEST(TFB_Random() == next);
    TFB_SeedRandom(saved);
}

BOOST_AUTO_TEST_CASE(math_angle_cardinals_wrapping_and_zero_vector) {
    BOOST_TEST(SINE(0, 100) == -100);
    BOOST_TEST(COSINE(0, 100) == 0);
    BOOST_TEST(SINE(16, 100) == 0);
    BOOST_TEST(COSINE(16, 100) == 100);
    BOOST_TEST(SINE(32, 100) == 100);
    BOOST_TEST(COSINE(32, 100) == 0);
    BOOST_TEST(SINE(48, 100) == 0);
    BOOST_TEST(COSINE(48, 100) == -100);
    BOOST_TEST(SINE(-64, 100) == -100);
    BOOST_TEST(COSINE(-16, 100) == -100);
    BOOST_TEST(ARCTAN(0, 0) == FULL_CIRCLE);
    BOOST_TEST(ARCTAN(0, -1) == 0u);
    BOOST_TEST(ARCTAN(1, 0) == 16u);
    BOOST_TEST(ARCTAN(0, 1) == 32u);
    BOOST_TEST(ARCTAN(-1, 0) == 48u);
    BOOST_TEST(ARCTAN(INT16_MIN, INT16_MIN) == 56u);
    BOOST_TEST(ARCTAN(1, -1000) == 0u);
    BOOST_TEST(ANGLE_TO_FACING(63) == 16);
    BOOST_TEST(NORMALIZE_FACING(ANGLE_TO_FACING(63)) == 0u);
    BOOST_TEST(DEGREES_TO_ANGLE(450) == 16u);
    BOOST_TEST(DEGREES_TO_ANGLE(-450) == 48u);
}

BOOST_AUTO_TEST_CASE(math_projection_preserves_heading_and_magnitude) {
    for (int angle = 0; angle < FULL_CIRCLE; ++angle) {
        const auto x = COSINE(angle, 2000);
        const auto y = SINE(angle, 2000);
        BOOST_TEST(std::abs(std::hypot(x, y) - 2000.0) < 1.0);
        BOOST_TEST(ARCTAN(x, y) == angle);
        BOOST_TEST(SINE(angle + FULL_CIRCLE, 2000) == y);
        BOOST_TEST(COSINE(angle - FULL_CIRCLE, 2000) == x);
    }
    BOOST_TEST(math_c_contract() == 1);
}

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

#include "types.h"
#include "libs/compiler.h"
#include "libs/uio/types.h"
#include "endian_uqm.h"
#include "uqm/units.h"

extern "C" {
uint64 value_types_c_combine(uint32 high, uint32 low, bool enabled);
int value_types_c_contract(void);
// The unit checks supply screen dimensions without linking the game engine.
int ScreenWidth = 320;
int ScreenHeight = 240;
}

static_assert(std::is_same_v<uint64, std::uint64_t>);
static_assert(std::is_same_v<sint64, std::int64_t>);
static_assert(std::is_same_v<uio_uintptr, std::uintptr_t>);
static_assert(std::numeric_limits<sint8>::min() == INT8_MIN);
static_assert(std::numeric_limits<sint16>::max() == INT16_MAX);
static_assert(MAKE_BYTE(1, 5) == 0x51);
static_assert(MAKE_WORD(0x34, 0x12) == 0x1234);
static_assert(MAKE_DWORD(0x5678, 0x1234) == 0x12345678);
static_assert(LONIBBLE(0x51) == 1 && HINIBBLE(0x51) == 5);
static_assert(LOBYTE(0x1234) == 0x34 && HIBYTE(0x1234) == 0x12);
static_assert(LOWORD(0x12345678) == 0x5678 && HIWORD(0x12345678) == 0x1234);
static_assert(FULL_CIRCLE == 64 && FACING_SHIFT == 4 && SIN_SCALE == 16384);

BOOST_AUTO_TEST_CASE(standard_types_and_packing_work_across_the_c_boundary)
{
    BOOST_TEST(value_types_c_contract() == 1);
    BOOST_TEST(value_types_c_combine(0x01234567, 0x89abcdef, true)
            == UINT64_C(0x0123456789abcdef));
    BOOST_TEST(value_types_c_combine(1, 2, false) == 0u);
    UWORD value = 0x1234;
    BOOST_TEST(LOBYTE(value++) == 0x34);
    BOOST_TEST(value == 0x1235);
}

BOOST_AUTO_TEST_CASE(byte_order_helpers_preserve_encoded_integer_values)
{
    BOOST_TEST(UQM_Swap16(0x1234) == 0x3412);
    BOOST_TEST(UQM_Swap32(0x12345678) == 0x78563412);
    BOOST_TEST(UQM_Swap64(UINT64_C(0x0123456789abcdef))
            == UINT64_C(0xefcdab8967452301));
    const uint64 native = UINT64_C(0x0123456789abcdef);
    BOOST_TEST(UQM_SwapLE64(UQM_SwapLE64(native)) == native);
    BOOST_TEST(UQM_SwapBE64(UQM_SwapBE64(native)) == native);
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    BOOST_TEST(UQM_SwapLE32(0x12345678) == 0x12345678);
    BOOST_TEST(UQM_SwapBE32(0x12345678) == 0x78563412);
#else
    BOOST_TEST(UQM_SwapBE32(0x12345678) == 0x12345678);
    BOOST_TEST(UQM_SwapLE32(0x12345678) == 0x78563412);
#endif
}

BOOST_AUTO_TEST_CASE(coordinate_helpers_still_use_current_screen_geometry)
{
    BOOST_TEST(UNIVERSE_TO_LOGX(0) == 0);
    BOOST_TEST(UNIVERSE_TO_LOGY(MAX_Y_UNIVERSE) == 0);
    BOOST_TEST(LOGX_TO_UNIVERSE(UNIVERSE_TO_LOGX(5000)) == 5000);
    const auto originalWidth = ScreenWidth;
    const auto original = UNIVERSE_TO_LOGX(5000);
    ScreenWidth = 640;
    const auto resized = UNIVERSE_TO_LOGX(5000);
    ScreenWidth = originalWidth;
    BOOST_TEST(resized > original);
}

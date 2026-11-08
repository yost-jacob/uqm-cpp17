#include "types.h"
#include "libs/uio/types.h"
#include "libs/compiler.h"
#include "endian_uqm.h"

/* These must remain valid C constant initializers, including packed flags in
 * static resource tables. Function-based C++ forms are checked separately. */
static const BYTE byte_value = MAKE_BYTE(1, 5);
static const UWORD word_value = MAKE_WORD(0x34, 0x12);
static const DWORD dword_value = MAKE_DWORD(0x5678, 0x1234);

/* Exercise the shared ABI with a native 64-bit value and standard C boolean. */
uint64 value_types_c_combine(uint32 high, uint32 low, bool enabled)
{
    return enabled ? ((uint64) high << 32) | low : 0;
}

/* Verify C-only initializers, one-time argument evaluation and pointer width. */
int value_types_c_contract(void)
{
    UWORD value = 0x1234;
    int object;
    uio_uintptr address = (uio_uintptr) &object;
    return byte_value == 0x51 && word_value == 0x1234
            && dword_value == 0x12345678 && LOBYTE(value++) == 0x34
            && value == 0x1235 && (void *) address == &object
            && UQM_Swap64(UINT64_C(0x0123456789abcdef))
                    == UINT64_C(0xefcdab8967452301);
}

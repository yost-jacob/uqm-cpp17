#include "uqm/state.h"
#include <string.h>

/* Exercise the C typed helpers and legacy close/reopen cursor contract. */
int world_state_c_round_trip(void)
{
    GAME_STATE_FILE *file = OpenStateFile(STARINFO_FILE, "wb");
    DWORD words[] = {0, UINT32_C(0xfedcba98), 17};
    DWORD result[3] = {0};
    if (!file || !swrite_a32(file, words, 3) || LengthStateFile(file) != sizeof words)
        return 0;
    CloseStateFile(file);
    file = OpenStateFile(STARINFO_FILE, "rb");
    if (!file || !sread_a32(file, result, 3))
        return 0;
    CloseStateFile(file);
    return memcmp(words, result, sizeof words) == 0;
}

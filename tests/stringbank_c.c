#include <string.h>

#include "libs/resource/stringbank.h"

/* Check the C menu-facing API with borrowed output pointers and mutable input.
 * Return true only if the bank owns the copies after the input changes. */
int string_bank_c_round_trip(void)
{
    stringbank *bank = StringBank_Create();
    char source[] = "first\nsecond";
    const char *lines[2];
    int ok = SplitString(source, '\n', 2, lines, bank) == 2;
    source[0] = 'X';
    ok = ok && strcmp(lines[0], "first") == 0;
    ok = ok && strcmp(lines[1], "second") == 0;
    ok = ok && lines[0] == StringBank_AddOrFindString(bank, "first");
    StringBank_Free(bank);
    StringBank_Free(NULL);
    return ok;
}

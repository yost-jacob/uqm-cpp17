#include <string.h>
#include "libs/strings/strintrn.h"

/* Exercise allocation, byte copying, navigation and destruction through the
 * opaque C interface. Pointer bytes may have arbitrary alignment/NULs. */
int string_table_c_round_trip(void) {
    int value = 42;
    void *pointer = &value;
    void *decoded = NULL;
    const char binary[] = {'a', '\0', 'b'};
    STRING_TABLE table = AllocStringTable(2);
    STRING first = CaptureStringTable(table);
    STRING second = SetRelStringTableIndex(first, 1);
    const STRING_TABLE_ENTRY_DESC *read_only = first;
    int ok;
    SetStringTableEntry(table, 0, binary, sizeof binary);
    SetStringTableEntry(table, 1, &pointer, sizeof pointer);
    memcpy(&decoded, GetStringAddress(second), sizeof decoded);
    ok = decoded == pointer && GetStringLengthBin(first) == sizeof binary
            && GetStringLength(read_only) == 1 && GetStringTableIndex(second) == 1
            && memcmp(GetStringAddress(first), binary, sizeof binary) == 0
            && SetRelStringTableIndex(first, -1) == second;
    SetStringTableEntry(table, 0, NULL, 0);
    ok = ok && GetStringAddress(first) == NULL && GetStringLengthBin(first) == 0;
    DestroyStringTable(table);
    return ok && value == 42;
}

#include "libs/uio/charhashtable.h"
#include "libs/strings/stringhashtable.h"

/* Exercise the exported C ABI with a stack-owned value and read-only lookup.
 * Return true only if insertion, lookup, iteration and removal all succeed. */
int hash_table_c_round_trip(void)
{
    int value = 42;
    CharHashTable_HashTable *table = CharHashTable_newHashTable();
    const CharHashTable_HashTable *read_only = table;
    int ok = CharHashTable_add(table, "resource", &value);
    CharHashTable_Iterator *it = CharHashTable_getIterator(read_only);
    ok = ok && CharHashTable_find(read_only, "resource") == &value;
    ok = ok && !CharHashTable_iteratorDone(it);
    ok = ok && CharHashTable_iteratorValue(it) == &value;
    CharHashTable_iteratorNext(it);
    ok = ok && CharHashTable_iteratorDone(it);
    CharHashTable_freeIterator(it);
    ok = ok && CharHashTable_remove(table, "resource");
    CharHashTable_deleteHashTable(table);
    return ok && value == 42;
}

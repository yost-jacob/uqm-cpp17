/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * Nota bene: later versions of the GNU General Public License do not apply
 * to this program.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef LIBS_STRINGS_STRINGHASHTABLE_H_
#define LIBS_STRINGS_STRINGHASHTABLE_H_

#include "libs/uio/charhashtable.h"

typedef struct string_table_entry STRING_TABLE_ENTRY_DESC;
typedef CharHashTable_HashTable StringHashTable_HashTable;

/* Typed name index for borrowed string-table entries. Keys are copied, so
 * source name buffers may move after insertion. Entries must outlive the index.
 * Allocation and synchronization contracts are those of CharHashTable. */

/* Create an empty owned index; release it with deleteHashTable below. */
static inline StringHashTable_HashTable *
StringHashTable_newHashTable(void) {
    return CharHashTable_newHashTable();
}

/* Add a NUL-terminated, non-NULL key and borrowed entry (which may be NULL).
 * table is required; duplicates return false and leave the index unchanged. */
static inline bool
StringHashTable_add(StringHashTable_HashTable *table, const char *key,
        STRING_TABLE_ENTRY_DESC *entry) {
    return CharHashTable_add(table, key, entry);
}

/* Look up a non-NULL key in a required, read-only index. Return a borrowed
 * entry, or NULL when absent; the entry itself remains mutable. */
static inline STRING_TABLE_ENTRY_DESC *
StringHashTable_find(const StringHashTable_HashTable *table, const char *key) {
    return (STRING_TABLE_ENTRY_DESC *) CharHashTable_find(table, key);
}

/* Delete the index and its key copies, leaving entries alive. NULL is allowed. */
static inline void
StringHashTable_deleteHashTable(StringHashTable_HashTable *table) {
    CharHashTable_deleteHashTable(table);
}

#endif /* LIBS_STRINGS_STRINGHASHTABLE_H_ */

/*
 * Copyright (C) 2003  Serge van den Boom
 *
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

#ifndef LIBS_UIO_CHARHASHTABLE_H_
#define LIBS_UIO_CHARHASHTABLE_H_

#include <stddef.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CharHashTable_HashTable CharHashTable_HashTable;
typedef struct CharHashTable_Iterator CharHashTable_Iterator;

/* Owns copies of NUL-terminated keys; values are borrowed and never freed.
 * All table/key arguments must be non-NULL. Values may be NULL.
 * Callers must synchronize access. Allocation failure terminates the program;
 * C++ exceptions never cross this C interface. */

/* Create an empty table. The caller owns the result and must delete it. */
CharHashTable_HashTable *CharHashTable_newHashTable(void);

/* Copy key and associate value. Return false for duplicates without replacing
 * the existing value; the caller retains ownership of both supplied arguments. */
bool CharHashTable_add(CharHashTable_HashTable *table, const char *key, void *value);

/* Remove key, returning whether it existed. Does not destroy the value. */
bool CharHashTable_remove(CharHashTable_HashTable *table, const char *key);

/* Borrow the associated value, or NULL if absent. A stored NULL value is
 * indistinguishable from an absent key. Does not modify the table. */
void *CharHashTable_find(const CharHashTable_HashTable *table, const char *key);

/* Return the number of keys, including keys whose associated value is NULL. */
size_t CharHashTable_count(const CharHashTable_HashTable *table);

/* Release keys and storage, leaving values alive. NULL is allowed.
 * Destroy all iterators before deleting their table. */
void CharHashTable_deleteHashTable(CharHashTable_HashTable *table);

/* Create an owned iterator over table, which must outlive it. Iteration order
 * is unspecified. Do not mutate the table while an iterator is in use. */
CharHashTable_Iterator *CharHashTable_getIterator(const CharHashTable_HashTable *table);

/* Return true when iterator (non-NULL) has no remaining entry. */
bool CharHashTable_iteratorDone(const CharHashTable_Iterator *iterator);

/* Borrow the current key until it is removed or the table is deleted.
 * iterator must be non-NULL and not done; callers must not modify the key. */
const char *CharHashTable_iteratorKey(const CharHashTable_Iterator *iterator);

/* Borrow the current value. iterator must be non-NULL and not done. */
void *CharHashTable_iteratorValue(const CharHashTable_Iterator *iterator);

/* Advance a non-NULL, non-done iterator and return the same iterator. */
CharHashTable_Iterator *CharHashTable_iteratorNext(CharHashTable_Iterator *iterator);

/* Release an iterator without modifying its table. NULL is allowed. */
void CharHashTable_freeIterator(CharHashTable_Iterator *iterator);

#ifdef __cplusplus
}
#endif
#endif /* LIBS_UIO_CHARHASHTABLE_H_ */

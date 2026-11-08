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

#include <cassert>
#include <exception>
#include <string>
#include <unordered_map>

#include "charhashtable.h"

struct CharHashTable_HashTable {
    std::unordered_map<std::string, void *> entries;
};

struct CharHashTable_Iterator {
    const CharHashTable_HashTable &table;
    std::unordered_map<std::string, void *>::const_iterator current;
};

CharHashTable_HashTable *
CharHashTable_newHashTable(void) try {
    return new CharHashTable_HashTable;
} catch (...) {
    std::terminate();
}

bool
CharHashTable_add(CharHashTable_HashTable *table, const char *key, void *value) try {
    assert(table && key);
    return table->entries.try_emplace(key, value).second;
} catch (...) {
    std::terminate();
}

bool
CharHashTable_remove(CharHashTable_HashTable *table, const char *key) try {
    assert(table && key);
    return table->entries.erase(key) != 0;
} catch (...) {
    std::terminate();
}

void *
CharHashTable_find(const CharHashTable_HashTable *table, const char *key) try {
    assert(table && key);
    const auto &entries = table->entries;
    const auto found = entries.find(key);
    return found == entries.end() ? nullptr : found->second;
} catch (...) {
    std::terminate();
}

size_t
CharHashTable_count(const CharHashTable_HashTable *table) {
    assert(table);
    return table->entries.size();
}

void
CharHashTable_deleteHashTable(CharHashTable_HashTable *table) {
    delete table;
}

CharHashTable_Iterator *
CharHashTable_getIterator(const CharHashTable_HashTable *table) try {
    assert(table);
    return new CharHashTable_Iterator{*table, table->entries.cbegin()};
} catch (...) {
    std::terminate();
}

bool
CharHashTable_iteratorDone(const CharHashTable_Iterator *iterator) {
    assert(iterator);
    return iterator->current == iterator->table.entries.cend();
}

const char *
CharHashTable_iteratorKey(const CharHashTable_Iterator *iterator) {
    assert(iterator && !CharHashTable_iteratorDone(iterator));
    return iterator->current->first.c_str();
}

void *
CharHashTable_iteratorValue(const CharHashTable_Iterator *iterator) {
    assert(iterator && !CharHashTable_iteratorDone(iterator));
    return iterator->current->second;
}

CharHashTable_Iterator *
CharHashTable_iteratorNext(CharHashTable_Iterator *iterator) {
    assert(iterator && !CharHashTable_iteratorDone(iterator));
    ++iterator->current;
    return iterator;
}

void
CharHashTable_freeIterator(CharHashTable_Iterator *iterator) {
    delete iterator;
}

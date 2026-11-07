//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <cassert>
#include <exception>
#include <utility>
#include "strintrn.h"

std::unique_ptr<string_table>
MakeStringTable(std::vector<StringEntry> entries)
{
    auto table = std::make_unique<string_table>();
    table->entries = std::move(entries);
    for (size_t i = 0; i < table->entries.size(); ++i) {
        auto &entry = table->entries[i];
        entry.text.parent = entry.name.parent = table.get();
        entry.text.index = i;
        entry.name.index = table->entries.size() + i;
        if (!entry.name.data.empty())
            table->nameIndex.try_emplace(entry.name.data.c_str(), &entry.name);
    }
    return table;
}

STRING_TABLE
AllocStringTable(const int num_entries) try
{
    assert(num_entries >= 0);
    return MakeStringTable(std::vector<StringEntry>(num_entries)).release();
} catch (...) {
    std::terminate();
}

void
SetStringTableEntry(STRING_TABLE table, const COUNT index, const void *data,
        const size_t length) try
{
    assert(table && index < table->entries.size() && (data || length == 0));
    auto &bytes = table->entries[index].text.data;
    if (length)
        bytes.assign(static_cast<const char *>(data), length);
    else
        bytes.clear();
} catch (...) {
    std::terminate();
}

void FreeStringTable(STRING_TABLE table) { delete table; }
BOOLEAN FreeStringTableData(void *data) {
    FreeStringTable(static_cast<STRING_TABLE>(data));
    return TRUE;
}
BOOLEAN DestroyStringTable(STRING_TABLE table) {
    FreeStringTable(table);
    return TRUE;
}
STRING CaptureStringTable(STRING_TABLE table) {
    return table && !table->entries.empty() ? &table->entries.front().text : nullptr;
}
STRING_TABLE ReleaseStringTable(STRING str) { return GetStringTable(str); }
STRING_TABLE GetStringTable(const STRING_TABLE_ENTRY_DESC *str) { return str ? str->parent : nullptr; }
COUNT GetStringTableCount(const STRING_TABLE_ENTRY_DESC *str) {
    const auto table = GetStringTable(str);
    return table ? table->entries.size() : 0;
}
COUNT GetStringTableIndex(const STRING_TABLE_ENTRY_DESC *str) { return str ? str->index : 0; }

STRING SetAbsStringTableIndex(STRING str, const COUNT index) {
    const auto table = GetStringTable(str);
    if (!table || table->entries.empty())
        return nullptr;
    return &table->entries[index % table->entries.size()].text;
}

STRING SetRelStringTableIndex(STRING str, const SIZE offset) {
    const auto table = GetStringTable(str);
    if (!table || table->entries.empty())
        return nullptr;
    const auto count = static_cast<ptrdiff_t>(table->entries.size());
    auto index = (static_cast<ptrdiff_t>(str->index) + offset) % count;
    if (index < 0)
        index += count;
    return &table->entries[index].text;
}

COUNT GetStringLength(const STRING_TABLE_ENTRY_DESC *str) {
    if (!str || str->data.empty())
        return 0;
    const auto start = str->data.data();
    return utf8StringCountN(start, start + str->data.size());
}
COUNT GetStringLengthBin(const STRING_TABLE_ENTRY_DESC *str) { return str ? str->data.size() : 0; }

// Convert owned bytes to the legacy mutable, borrowed view. Callers may edit
// existing bytes but must not write past the encoded length or free the view.
static char *borrow(std::string &bytes) {
    return bytes.empty() ? nullptr : bytes.data();
}

// A name handle has an index in the second logical range. Normalize it when
// looking up metadata, while retaining its original public index and bytes.
static StringEntry *entryFor(STRING str) {
    const auto table = GetStringTable(str);
    if (!table || table->entries.empty())
        return nullptr;
    return &table->entries[str->index % table->entries.size()];
}

STRINGPTR GetStringName(STRING str) {
    const auto entry = entryFor(str);
    return entry ? borrow(entry->name.data) : nullptr;
}
STRINGPTR GetStringSoundClip(STRING str) {
    const auto entry = entryFor(str);
    return entry ? borrow(entry->soundClip) : nullptr;
}
STRINGPTR GetStringTimeStamp(STRING str) {
    const auto entry = entryFor(str);
    return entry ? borrow(entry->timeStamp) : nullptr;
}
STRINGPTR GetStringAddress(STRING str) { return str ? borrow(str->data) : nullptr; }

STRING GetStringByName(const STRING_TABLE_DESC *table, const char *name) try {
    if (!table || !name)
        return nullptr;
    const auto found = table->nameIndex.find(name);
    return found == table->nameIndex.end() ? nullptr : found->second;
} catch (...) {
    std::terminate();
}

/*  stringbank.c, Copyright (c) 2005 Michael C. Martin */

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope thta it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  Se the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <cassert>
#include <exception>
#include <string>
#include <string_view>
#include <unordered_set>

#include "stringbank.h"

struct stringbank {
    // Rehashing preserves references to set elements. Strings are never changed
    // or erased individually, so their c_str() pointers remain valid.
    std::unordered_set<std::string> strings;
};

namespace {
/* Copy text into the required bank and borrow its immutable, stable contents.
 * text need only remain valid during this call; duplicates share one pointer. */
const char *intern(stringbank &bank, const std::string_view text) {
    return bank.strings.emplace(text).first->c_str();
}
}

stringbank *
StringBank_Create(void) try {
    return new stringbank;
} catch (...) {
    std::terminate();
}

void
StringBank_Free(stringbank *bank) {
    delete bank;
}

const char *
StringBank_AddOrFindString(stringbank *bank, const char *str) try {
    assert(bank && str);
    return intern(*bank, str);
} catch (...) {
    std::terminate();
}

int
SplitString(const char *s, const char splitchar, const int capacity,
        const char **result, stringbank *bank) try {
    assert(s && bank && capacity >= 0);
    if (capacity == 0)
        return 0;
    assert(result);

    std::string_view remaining(s);
    int count = 0;
    while (count < capacity - 1) {
        const auto delimiter = remaining.find(splitchar);
        if (delimiter == std::string_view::npos)
            break;
        result[count++] = intern(*bank, remaining.substr(0, delimiter));
        remaining.remove_prefix(delimiter + 1);
    }
    result[count++] = intern(*bank, remaining);
    return count;
} catch (...) {
    std::terminate();
}

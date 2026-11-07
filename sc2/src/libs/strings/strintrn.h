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

#ifndef LIBS_STRINGS_STRINTRN_H_
#define LIBS_STRINGS_STRINTRN_H_

#include <stdio.h>
#include <string.h>
#include "libs/strlib.h"
#include "libs/reslib.h"

#ifdef __cplusplus
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// STRING is a borrowed handle. Data retains its exact encoded byte count,
// including embedded/trailing NULs; an empty value has no backing C pointer.
struct string_table_entry {
    std::string data;
    string_table *parent = nullptr;
    size_t index = 0;
};

// Each logical entry owns its metadata. The name handle preserves the legacy
// lookup API: GetStringByName returns name bytes, not the dialog text handle.
struct StringEntry {
    string_table_entry text;
    string_table_entry name;
    std::string soundClip;
    std::string timeStamp;
};

struct string_table {
    // Fixed after construction: resizing/moving a published table would
    // invalidate borrowed handles and pointers. Only byte contents may change.
    std::vector<StringEntry> entries;
    std::unordered_map<std::string, STRING> nameIndex;
};

// Adopt complete entries, initialize stable handles and build the name index.
// Duplicate names retain the first entry. Exceptions stay within C++ callers.
std::unique_ptr<string_table> MakeStringTable(std::vector<StringEntry> entries);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate a fixed number of empty entries; num_entries must be nonnegative.
 * Caller owns the result. Allocation failure is fatal. */
STRING_TABLE AllocStringTable(int num_entries);
/* Copy length bytes into an entry during construction. table and index must
 * be valid; data may be NULL only for length == 0. Does not take ownership.
 * Replacing bytes invalidates previously borrowed pointers to that value. */
void SetStringTableEntry(STRING_TABLE table, COUNT index, const void *data,
        size_t length);
/* Release the table and all metadata; NULL is allowed. Invalidates every
 * borrowed handle/pointer. External objects encoded in bytes are not freed. */
void FreeStringTable(STRING_TABLE strtab);
/* Resource-manager deleter for STRTAB, BINTAB and CONVERSATION payloads.
 * Accepts NULL; releases the complete table rather than a raw allocation. */
BOOLEAN FreeStringTableData(void *data);

/* Read a text table from borrowed fp at its current position; length is the
 * byte range to read. Caller owns the returned table, or gets NULL if empty. */
void *_GetStringData (uio_Stream *fp, DWORD length);
/* Decode a borrowed binary stream containing a legacy length prefix and
 * big-endian table words. Caller owns the returned table; read failure yields
 * NULL. fp must be non-NULL and length must describe the encoded resource. */
void *_GetBinaryTableData (uio_Stream *fp, DWORD length);
/* Load path[:voice-prefix[:timestamp-path]] from contentDir. Both arguments
 * are required; resdata receives an owned table or NULL. Opened streams and
 * temporary buffers are released before return. Allocation failure is fatal. */
void _GetConversationData (const char *path, RESOURCE_DATA *resdata);

#ifdef __cplusplus
}
#endif

#endif /* LIBS_STRINGS_STRINTRN_H_ */


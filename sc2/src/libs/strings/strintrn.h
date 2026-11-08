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
#include "stringhashtable.h"

struct string_table_entry
{
	STRINGPTR data;
	int length;  /* Internal NULs are allowed */
	int index;
	struct string_table *parent;
};

struct string_table
{
	unsigned short flags;
	int size;
	STRING_TABLE_ENTRY_DESC *strings;
	StringHashTable_HashTable *nameIndex;
};

enum {
    HAS_SOUND_CLIPS = 1 << 0,
    HAS_TIMESTAMP = 1 << 1,
    HAS_NAMEINDEX = 1 << 2
};

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate num_entries entries per enabled dataset; num_entries must be
 * nonnegative and flags must use the HAS_* values above. The caller owns the
 * returned table. Allocation failure is fatal. */
STRING_TABLE AllocStringTable (int num_entries, int flags);
/* Release all datasets and the name index; NULL is allowed. Borrowed entry
 * and text pointers must no longer be used after the table is freed. */
void FreeStringTable (STRING_TABLE strtab);

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


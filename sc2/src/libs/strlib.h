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

#ifndef LIBS_STRLIB_H_
#define LIBS_STRLIB_H_

#include "libs/compiler.h"
#include "port.h"
#include "libs/uio.h"
#include "libs/unicode.h"

#include <stddef.h>

typedef struct string_table_entry STRING_TABLE_ENTRY_DESC;
typedef struct string_table STRING_TABLE_DESC;

typedef STRING_TABLE_DESC *STRING_TABLE;
typedef STRING_TABLE_ENTRY_DESC *STRING;
typedef char *STRINGPTR;

/* This has to go here because reslib requires the above typedefs. */
#include "libs/reslib.h"

#if defined(__cplusplus)
extern "C" {
#endif

extern BOOLEAN InstallStringTableResType (void);
extern STRING_TABLE LoadStringTableInstance (RESOURCE res);
extern STRING_TABLE LoadStringTableFile (uio_DirHandle *dir,
		const char *fileName);
/* Tables own their entry bytes and metadata. All STRING handles and returned
 * byte pointers are borrowed until table destruction; callers serialize access.
 * Destroy accepts NULL and returns TRUE. Capture returns the first text handle,
 * or NULL for a NULL/empty table. Release only retrieves the owner; it does not
 * free or detach anything. */
extern BOOLEAN DestroyStringTable (STRING_TABLE StringTable);
extern STRING CaptureStringTable (STRING_TABLE StringTable);
extern STRING_TABLE ReleaseStringTable (STRING String);
extern STRING_TABLE GetStringTable (const STRING_TABLE_ENTRY_DESC *String);
/* Count logical text entries; NULL returns zero. Text indices are zero-based.
 * A name handle has index count + text index, preserving legacy lookup values. */
extern COUNT GetStringTableCount (const STRING_TABLE_ENTRY_DESC *String);
extern COUNT GetStringTableIndex (const STRING_TABLE_ENTRY_DESC *String);
/* Select a text entry, wrapping modulo the table count. Relative offsets may
 * be negative. Accept any handle in the table, including a name handle;
 * NULL input returns NULL. Existing handles remain valid after selection. */
extern STRING SetAbsStringTableIndex (STRING String, COUNT StringTableIndex);
extern STRING SetRelStringTableIndex (STRING String, SIZE StringTableOffs);
/* Character length counts UTF-8 codepoints up to the first NUL. Binary length
 * counts all encoded bytes, including embedded/trailing NULs. NULL gives zero. */
extern COUNT GetStringLength (const STRING_TABLE_ENTRY_DESC *String);
extern COUNT GetStringLengthBin (const STRING_TABLE_ENTRY_DESC *String);
/* Return borrowed mutable bytes, or NULL if absent/input is NULL. Callers may
 * edit existing bytes, but must not free/resize them or write past binary length.
 * Binary entries need not be NUL-terminated or aligned for typed access; copy
 * typed values with memcpy. Metadata is NUL-terminated when present. */
extern STRINGPTR GetStringAddress (STRING String);
extern STRINGPTR GetStringName (STRING String);
extern STRINGPTR GetStringSoundClip (STRING String);
extern STRINGPTR GetStringTimeStamp (STRING String);
/* Find an exact NUL-terminated name; first duplicate wins. Return its borrowed
 * NAME handle (GetStringAddress reads the name). SetRelStringTableIndex(handle,
 * 0) selects its text. Missing names or NULL arguments return NULL. Editing name
 * bytes does not alter the lookup key, which is an owned copy. */
extern STRING GetStringByName (const STRING_TABLE_DESC *StringTable, const char *index);

#define UNICHAR_DEGREE_SIGN   0x00b0
#define STR_DEGREE_SIGN     "\xC2\xB0"
#define UNICHAR_INFINITY_SIGN 0x221e
#define STR_INFINITY_SIGN   "\xE2\x88\x9E"
#define UNICHAR_EARTH_SIGN    0x2641
#define STR_EARTH_SIGN      "\xE2\x99\x81"
#define UNICHAR_MIDDLE_DOT    0x00b7
#define STR_MIDDLE_DOT      "\xC2\xB7"
#define UNICHAR_BULLET        0x2022
#define STR_BULLET          "\xE2\x80\xA2"

#if defined(__cplusplus)
}
#endif

#endif /* LIBS_STRLIB_H_ */

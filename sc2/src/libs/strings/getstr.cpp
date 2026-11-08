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

#include <array>
#include <cassert>
#include <exception>
#include <memory>
#include <vector>

#include "options.h"
#include "strintrn.h"
#include "libs/reslib.h"
#include "libs/log.h"
#include "libs/memlib.h"

namespace {
constexpr size_t MaxStrings = 2048;
constexpr size_t LineCapacity = 1024;
using Lengths = std::array<DWORD, MaxStrings>;
using Stream = std::unique_ptr<uio_Stream, decltype(&res_CloseResFile)>;
using StringTable = std::unique_ptr<STRING_TABLE_DESC, decltype(&FreeStringTable)>;
using NameIndex = std::unique_ptr<StringHashTable_HashTable,
        decltype(&StringHashTable_deleteHashTable)>;
}

/* Decode num_dwords big-endian words in place; the array must be writable
 * and contain at least that many words. num_dwords must be positive. */
static void
dword_convert (DWORD *dword_array, COUNT num_dwords)
{
	BYTE *p = (BYTE*)dword_array;

	do
	{
		*dword_array++ = (static_cast<DWORD>(p[0]) << 24)
                | (static_cast<DWORD>(p[1]) << 16)
                | (static_cast<DWORD>(p[2]) << 8) | p[3];
		p += 4;
	} while (--num_dwords);
}

/* Replace one table entry with an owned byte copy. index must be in range;
 * value must contain len bytes (nullptr is allowed only for len == 0). */
static STRING
set_strtab_entry (STRING_TABLE_DESC &strtab, const size_t index, const char *value,
		const size_t len)
{
	STRING str = &strtab.strings[index];

	if (str->data)
	{
		HFree (str->data);
		str->data = nullptr;
		str->length = 0;
	}
	if (len)
	{
		str->data = static_cast<char *>(HMalloc (len));
		str->length = len;
		memcpy (str->data, value, len);
	}
	return str;
}

/* Copy count packed strings into a required table starting at firstIndex.
 * The sum of the first count lengths must fit data, and the destination range
 * must exist. Zero-length entries do not require backing bytes. */
static void
copy_strings_to_strtab (STRING_TABLE_DESC &strtab, const size_t firstIndex,
		const size_t count, const std::vector<char> &data, const Lengths &lens)
{
	size_t stringI;
	const char *off = data.data();

	for (stringI = 0; stringI < count; stringI++)
	{
		set_strtab_entry(strtab, firstIndex + stringI,
				off, lens[stringI]);
		if (lens[stringI] != 0)
			off += lens[stringI];
	}
}

void
_GetConversationData (const char *path, RESOURCE_DATA *resdata) try
{
	assert(path && resdata);
	resdata->ptr = nullptr;
	unsigned long dataLen;
	StringTable result(nullptr, FreeStringTable);
	int stringI;
	size_t path_len;
	DWORD opos;

	std::vector<char> namedata;
			// Contains the names (indexes) of the dialogs.
	Lengths nlen{};
			// Length of each of the names.
	DWORD NameOffs;

	std::vector<char> strdata;
			// Contains the dialog strings.
	Lengths slen{};
			// Length of each of the dialog strings.
	DWORD StringOffs;

	std::vector<char> clipdata;
			// Contains the file names of the speech files.
	Lengths clen{};
			// Length of each of the speech file names.
	DWORD ClipOffs;

	std::vector<char> ts_data;
			// Contains the timestamp data for synching the text with the
			// speech.
	Lengths tslen{};
			// Length of each of the timestamp strings.
	DWORD TSOffs;

	char CurrentLine[LineCapacity];
	char paths[LineCapacity];
	char *clip_path;
	char *ts_path;

	Stream fp(nullptr, res_CloseResFile);
	Stream timestamp_fp(nullptr, res_CloseResFile);
	NameIndex nameHashTable(nullptr, StringHashTable_deleteHashTable);
			// Hash table of string names (such as "GLAD_WHEN_YOU_COME_BACK")
			// to a STRING.

	/* Parse out the conversation components. */
	strncpy (paths, path, sizeof(paths) - 1);
	paths[sizeof(paths) - 1] = '\0';
	clip_path = strchr (paths, ':');
	if (clip_path == nullptr)
	{
		ts_path = nullptr;
	}
	else
	{
		*clip_path = '\0';
		clip_path++;

		ts_path = strchr (clip_path, ':');
		if (ts_path != nullptr)
		{
			*ts_path = '\0';
			ts_path++;
		}
	}

	fp.reset(res_OpenResFile (contentDir, paths, "rb"));
	if (!fp)
	{
		log_add (log_Warning, "Warning: Can't open '%s'", paths);
		resdata->ptr = nullptr;
		return;
	}

	dataLen = LengthResFile (fp.get());
	log_add (log_Info, "\t'%s' -- conversation phrases -- %lu bytes", paths,
			dataLen);
	if (clip_path)
		log_add (log_Info, "\t'%s' -- voice clip directory", clip_path);
	else
		log_add (log_Info, "\tNo associated voice clips");
	if (ts_path)
		log_add (log_Info, "\t'%s' -- timestamps", ts_path);
	else
		log_add (log_Info, "\tNo associated timestamp file");

	if (dataLen == 0)
	{
		log_add (log_Warning, "Warning: Trying to load empty file '%s'.",
				path);
		return;
	}



	nameHashTable.reset(StringHashTable_newHashTable());

	path_len = clip_path ? strlen (clip_path) : 0;

	if (ts_path)
	{
		timestamp_fp.reset(uio_fopen (contentDir, ts_path, "rb"));
	}

	opos = uio_ftell (fp.get());
	stringI = -1;
	NameOffs = 0;
	StringOffs = 0;
	ClipOffs = 0;
	TSOffs = 0;
	for (;;)
	{
		int l;

		if (uio_fgets (CurrentLine, sizeof (CurrentLine), fp.get()) == nullptr)
		{
			// EOF or read error.
			break;
		}

		if (stringI >= static_cast<int>(MaxStrings) - 1)
		{
			// Too many strings.
			break;
		}

		if (CurrentLine[0] == '#')
		{
			// String header, of the following form:
			//     #(GLAD_WHEN_YOU_COME_BACK) commander-000.ogg
			char CopyLine[LineCapacity];
			char *name;
			char *ts;

			strcpy (CopyLine, CurrentLine);
			name = strtok (&CopyLine[1], "()");
			if (name)
			{
				if (stringI >= 0)
				{
					while (slen[stringI] > 1 &&
							(strdata[StringOffs - 2] == '\n' ||
							strdata[StringOffs - 2] == '\r'))
					{
						--slen[stringI];
						--StringOffs;
						strdata[StringOffs - 1] = '\0';
					}
				}

				slen[++stringI] = 0;

				// Store the string name.
				l = strlen (name) + 1;
				namedata.resize(NameOffs + l);
				strcpy (&namedata[NameOffs], name);
				NameOffs += l;
				nlen[stringI] = l;

				// now lets check for timestamp data
				if (timestamp_fp)
				{
					// We have a time stamp file.
					char TimeStampLine[LineCapacity];
					char *tsptr;
					BOOLEAN ts_ok = FALSE;
					if (uio_fgets (TimeStampLine, sizeof (TimeStampLine), timestamp_fp.get())
							&& TimeStampLine[0] == '#')
					{
						// Line is of the following form:
						//     #(GIVE_FUEL_AGAIN) 3304,3255
						tslen[stringI] = 0;
						tsptr = strstr (TimeStampLine, name);
						if (tsptr)
						{
							tsptr += strlen(name) + 1;
							ts_ok = TRUE;
							while (! strcspn(tsptr," \t\r\n") && *tsptr)
								tsptr++;
							if (*tsptr)
							{
								l = strlen (tsptr) + 1;
								ts_data.resize(TSOffs + l);

								strcpy (&ts_data[TSOffs], tsptr);
								TSOffs += l;
								tslen[stringI] = l;
							}
						}
					}
					if (!ts_ok)
					{
						// timestamp data is invalid, remove all of it
						log_add (log_Warning, "Invalid timestamp data "
								"for '%s'.  Disabling timestamps", name);
						ts_data.clear();
						timestamp_fp.reset();
						TSOffs = 0;
					}
				}
				clen[stringI] = 0;
				ts = strtok (nullptr, " \t\r\n)");
				if (ts)
				{
					l = path_len + strlen (ts) + 1;
					clipdata.resize(ClipOffs + l);

					if (clip_path)
						strcpy (&clipdata[ClipOffs], clip_path);
					strcpy (&clipdata[ClipOffs + path_len], ts);
					ClipOffs += l;
					clen[stringI] = l;
				}
			}
		}
		else if (stringI >= 0)
		{
			char *s;
			l = strlen (CurrentLine) + 1;

			strdata.resize(StringOffs + l);

			if (slen[stringI])
			{
				--slen[stringI];
				--StringOffs;
			}
			s = &strdata[StringOffs];
			slen[stringI] += l;
			StringOffs += l;

			strcpy (s, CurrentLine);
		}

		if ((int)uio_ftell (fp.get()) - (int)opos >= (int)dataLen)
			break;
	}
	if (stringI >= 0)
	{
		while (slen[stringI] > 1 && (strdata[StringOffs - 2] == '\n'
				|| strdata[StringOffs - 2] == '\r'))
		{
			--slen[stringI];
			--StringOffs;
			strdata[StringOffs - 1] = '\0';
		}
	}

	if (++stringI)
	{
		int flags = 0;
		int stringCount = stringI;

		if (ClipOffs)
			flags |= HAS_SOUND_CLIPS;
		if (TSOffs)
			flags |= HAS_TIMESTAMP;
		flags |= HAS_NAMEINDEX;

		result.reset(AllocStringTable (stringCount, flags));
		if (result)
		{
			// Copy all the gatherered data in a STRING_TABLE
			STRING_TABLE_DESC *lpST = result.get();
			STRING str;
			stringI = 0;

			// Store the dialog string.
			copy_strings_to_strtab (
					*lpST, stringI, stringCount, strdata, slen);
			stringI += stringCount;

			// Store the dialog names.
			copy_strings_to_strtab (
					*lpST, stringI, stringCount, namedata, nlen);
			stringI += stringCount;

			// Store sound clip file names.
			if (lpST->flags & HAS_SOUND_CLIPS)
			{
				copy_strings_to_strtab (
						*lpST, stringI, stringCount, clipdata, clen);
				stringI += stringCount;
			}

			// Store time stamp data.
			if (lpST->flags & HAS_TIMESTAMP)
			{
				copy_strings_to_strtab (
						*lpST, stringI, stringCount, ts_data, tslen);
				//stringI += stringCount;
			}

			// Store the STRING in the hash table indexed by the dialog
			// name.
			str = &lpST->strings[stringCount];
			for (stringI = 0; stringI < stringCount; stringI++)
			{
				StringHashTable_add (nameHashTable.get(), str[stringI].data,
						&str[stringI]);
			}

			lpST->nameIndex = nameHashTable.release();
		}
	}
	resdata->ptr = result.release();
} catch (...) {
    std::terminate();
}

void *
_GetStringData (uio_Stream *fp, DWORD length) try
{
	assert(fp);
	StringTable result(nullptr, FreeStringTable);

	int stringI;
	DWORD opos;
	Lengths slen{};
	DWORD StringOffs;
	char CurrentLine[LineCapacity];
	std::vector<char> strdata;

	opos = uio_ftell (fp);
	stringI = -1;
	StringOffs = 0;
	for (;;)
	{
		int l;

		if (uio_fgets (CurrentLine, sizeof (CurrentLine), fp) == nullptr)
		{
			// EOF or read error.
			break;
		}

		if (stringI >= static_cast<int>(MaxStrings) - 1)
		{
			// Too many strings.
			break;
		}

		if (CurrentLine[0] == '#')
		{
			char CopyLine[LineCapacity];
			char *s;

			strcpy (CopyLine, CurrentLine);
			s = strtok (&CopyLine[1], "()");
			if (s)
			{
				if (stringI >= 0)
				{
					while (slen[stringI] > 1 &&
							(strdata[StringOffs - 2] == '\n' ||
							strdata[StringOffs - 2] == '\r'))
					{
						--slen[stringI];
						--StringOffs;
						strdata[StringOffs - 1] = '\0';
					}
				}

				slen[++stringI] = 0;
			}
		}
		else if (stringI >= 0)
		{
			char *s;
			l = strlen (CurrentLine) + 1;

			strdata.resize(StringOffs + l);

			if (slen[stringI])
			{
				--slen[stringI];
				--StringOffs;
			}
			s = &strdata[StringOffs];
			slen[stringI] += l;
			StringOffs += l;

			strcpy (s, CurrentLine);
		}

		if ((int)uio_ftell (fp) - (int)opos >= (int)length)
			break;
	}
	if (stringI >= 0)
	{
		while (slen[stringI] > 1 && (strdata[StringOffs - 2] == '\n'
				|| strdata[StringOffs - 2] == '\r'))
		{
			--slen[stringI];
			--StringOffs;
			strdata[StringOffs - 1] = '\0';
		}
	}

	if (++stringI)
	{
		int flags = 0;
		int stringCount = stringI;

		result.reset(AllocStringTable (stringI, flags));
		if (result)
		{
			STRING_TABLE_DESC *lpST = result.get();
			copy_strings_to_strtab (*lpST, 0, stringCount, strdata, slen);
		}
	}
	return result.release();
} catch (...) {
    std::terminate();
}

/* Decode a borrowed binary stream; the returned table belongs to the caller.
 * length is the encoded byte count. File-format interpretation is unchanged. */
void *
_GetBinaryTableData (uio_Stream *fp, DWORD length) try
{
    assert(fp);
    const std::unique_ptr<void, decltype(&HFree)> data(GetResourceData(fp, length), HFree);
    if (!data)
        return nullptr;

    DWORD *fileData = static_cast<DWORD *>(data.get());
    dword_convert(fileData, 1);
    StringTable table(AllocStringTable(fileData[0], 0), FreeStringTable);
    const int size = table->size;
    dword_convert(fileData + 1, size + 1);
    const BYTE *stringptr = reinterpret_cast<const BYTE *>(fileData + 2 + size + fileData[1]);
    for (int i = 0; i < size; ++i) {
        set_strtab_entry(*table, i, reinterpret_cast<const char *>(stringptr), fileData[2 + i]);
        stringptr += fileData[2 + i];
    }
    return table.release();
} catch (...) {
    std::terminate();
}

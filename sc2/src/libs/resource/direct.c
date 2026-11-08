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

#include "libs/strings/strintrn.h"
#include "port.h"
#include "libs/uio.h"
#include <sys/stat.h>

DIRENTRY_REF
LoadDirEntryTable (uio_DirHandle *dirHandle, const char *path,
		const char *pattern, match_MatchType matchType)
{
	uio_DirList *dirList;
	COUNT num_entries;
	COUNT i;
	uio_DirHandle *dir;
	STRING_TABLE StringTable;
	COUNT entry;

	dir = uio_openDirRelative (dirHandle, path, 0);
	assert(dir != NULL);
	dirList = uio_getDirList (dir, "", pattern, matchType);
	assert(dirList != NULL);
	num_entries = 0;

	// First, count the amount of space needed
	for (i = 0; i < dirList->numNames; i++)
	{
		struct stat sb;

		if (dirList->names[i][0] == '.')
		{
			dirList->names[i] = NULL;
			continue;
		}
		if (uio_stat (dir, dirList->names[i], &sb) == -1)
		{
			dirList->names[i] = NULL;
			continue;
		}
		if (!S_ISREG (sb.st_mode))
		{
			dirList->names[i] = NULL;
			continue;
		}
		num_entries++;
	}
	uio_closeDir (dir);

	if (num_entries == 0) {
		uio_DirList_free(dirList);
		return ((DIRENTRY_REF) 0);
	}

	StringTable = AllocStringTable (num_entries);
	entry = 0;

	for (i = 0; i < dirList->numNames; i++)
	{
		int size;
		if (dirList->names[i] == NULL)
			continue;
		size = strlen (dirList->names[i]) + 1;
		SetStringTableEntry (StringTable, entry++, dirList->names[i], size);
	}
	
	uio_DirList_free(dirList);
	return StringTable;
}	



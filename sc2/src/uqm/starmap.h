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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef STARMAP_H_INCL_
#define STARMAP_H_INCL_

#include "libs/compiler.h"
#include "planets/planets.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Current selection is a player-owned optional StarId. These C adapters borrow
 * a descriptor from the active session; NULL clears the selection. */
STAR_DESC *GetCurrentStar(void);
void SetCurrentStar(STAR_DESC *star);
/* Compatibility loader for the selected standalone session. Production sessions
 * load during construction. Never replace a catalog with live borrowers.
 * Names/descriptors expire with their owning session. Failure returns FALSE. */
BOOLEAN LoadStarMap(void);
void FreeStarMap(void);
/* Borrow a read-only C compatibility descriptor by stable index (including
 * sentinels); out-of-range indices return NULL. Do not modify this buffer.
 * GetStarIndex requires a descriptor returned here/FindStar, never a copy. */
STAR_DESC *GetStar(COUNT index);
COUNT GetStarIndex(const STAR_DESC *star);
/* Borrow NUL-terminated UTF-8 text owned by a catalogue StarDesc. */
const char *GetStarPrefix(const STAR_DESC *star);
const char *GetStarPostfix(const STAR_DESC *star);
const char *GetQuasiSpaceName(void);
/* C bridge for the class search; callers select the space explicitly. */
STAR_DESC *FindStarInSpace(const STAR_DESC *last, const POINT *position,
        SIZE xbounds, SIZE ybounds, bool quasispace);

enum { NUM_SOLAR_SYSTEMS = 502, NUM_QUASISPACE_LOCATIONS = 16,
    QUASISPACE_FIRST = NUM_SOLAR_SYSTEMS + 1, STAR_CATALOGUE_SIZE = QUASISPACE_FIRST + NUM_QUASISPACE_LOCATIONS + 1 };

extern STAR_DESC* FindStar (STAR_DESC *pLastStar, const POINT *puniverse,
		SIZE xbounds, SIZE ybounds);

/* Copy a display name, truncating safely at a UTF-8 boundary. buf must have
 * capacity bytes; zero capacity writes nothing. star must be a catalogue handle. */
extern void GetClusterName(const STAR_DESC *star, UNICODE *buf, size_t capacity);

#if defined(__cplusplus)
}
#endif

#endif  /* STARMAP_H_INCL_ */


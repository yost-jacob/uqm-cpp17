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

// Packed legacy save fields, shared by the C and C++ state interfaces.
#include "globdata.h"
#ifdef STATE_DEBUG
#include "libs/log.h"
#endif

BYTE
getGameState (const BYTE *state, int startBit, int endBit)
{
	return (BYTE) (((startBit >> 3) == (endBit >> 3)
			? (state[startBit >> 3] >> (startBit & 7))
			: ((state[startBit >> 3] >> (startBit & 7))
			  | (state[endBit >> 3]
			  << (endBit - startBit - (endBit & 7)))))
			& ((1 << (endBit - startBit + 1)) - 1));
}

void
setGameState (BYTE *state, int startBit, int endBit, BYTE val
#ifdef STATE_DEBUG
		, const char *name
#endif
)
{
	state[startBit >> 3] =
			(state[startBit >> 3]
			& (BYTE) ~(((1 << (endBit - startBit + 1)) - 1) << (startBit & 7)))
			| (BYTE)((val) << (startBit & 7));

	if ((startBit >> 3) < (endBit >> 3)) {
		state[endBit >> 3] =
				(state[endBit >> 3]
				& (BYTE)~((1 << ((endBit & 7) + 1)) - 1))
				| (BYTE)((val) >> (endBit - startBit - (endBit & 7)));
	}
#ifdef STATE_DEBUG
	log_add (log_Debug, "State '%s' set to %d.", name, (int)val);
#endif
}

DWORD
getGameState32 (const BYTE *state, int startBit)
{
	DWORD v;
	int shift;

	for (v = 0, shift = 0; shift < 32; shift += 8, startBit += 8)
	{
		v |= (DWORD)getGameState (state, startBit, startBit + 7) << shift;
	}

	return v;
}

void
setGameState32 (BYTE *state, int startBit, DWORD val
#ifdef STATE_DEBUG
		, const char *name
#endif
)
{
	DWORD v = val;
	int i;

	for (i = 0; i < 4; ++i, v >>= 8, startBit += 8)
	{
		setGameState (state, startBit, startBit + 7, v & 0xff
#ifdef STATE_DEBUG
				, "(ignored)"
#endif
				);
	}

#ifdef STATE_DEBUG
	log_add (log_Debug, "State '%s' set to %u.", name, (unsigned)val);
#endif
}

void
copyGameState (BYTE *dest, DWORD target, const BYTE *src, DWORD begin, DWORD end)
{
	while (begin < end)
	{
		BYTE b;
		DWORD delta = 7;
		if (begin + delta > end)
			delta = end - begin;
		b = getGameState (src, begin, begin + delta);
		setGameState (dest, target, target + delta, b
#ifdef STATE_DEBUG
                , "copyGameState"
#endif
                );
		begin += 8;
		target += 8;
	}
}


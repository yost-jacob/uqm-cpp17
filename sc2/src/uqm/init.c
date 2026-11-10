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

#include "init.h"
#include "units.h"
#include "nameref.h"
#include "globdata.h"
#include "resinst.h"
#include "libs/reslib.h"

FRAME stars_in_space;
FRAME asteroid[NUM_VIEWS];
FRAME blast[NUM_VIEWS];
FRAME explosion[NUM_VIEWS];


BOOLEAN
load_animation (FRAME *pixarray, RESOURCE big_res, RESOURCE med_res, RESOURCE
		sml_res)
{
	DRAWABLE d;

	d = LoadGraphic (big_res);
	if (!d)
		return FALSE;
	pixarray[0] = CaptureDrawable (d);

	if (med_res != NULL_RESOURCE)
	{
		d = LoadGraphic (med_res);
		if (!d)
			return FALSE;
	}
	pixarray[1] = CaptureDrawable (d);

	if (sml_res != NULL_RESOURCE)
	{
		d = LoadGraphic (sml_res);
		if (!d)
			return FALSE;
	}
	pixarray[2] = CaptureDrawable (d);

	return TRUE;
}

/* Warning: Some ships (such as the Umgah) will alias their pixarrays,
   so we need to track to make sure that we do not double-free. */
BOOLEAN
free_image (FRAME *pixarray)
{
	BOOLEAN retval;
	COUNT i, j;
	void *already_freed[NUM_VIEWS];

	retval = TRUE;
	for (i = 0; i < NUM_VIEWS; ++i)
	{
		if (pixarray[i] != NULL)
		{
			BOOLEAN ok = TRUE;
			for (j = 0; j < i; j++)
			{
				if (already_freed[j] == pixarray[i])
				{
					ok = FALSE;
					break;
				}
			}
			if (ok)
			{
				if (!DestroyDrawable (ReleaseDrawable (pixarray[i])))
					retval = FALSE;
			}
			already_freed[i] = pixarray[i];
			pixarray[i] = NULL;
		}
	}

	return (retval);
}

static BYTE space_ini_cnt;

BOOLEAN
InitSpace (void)
{
	if (space_ini_cnt++ == 0
			&& LOBYTE (GLOBAL (CurrentActivity)) <= IN_ENCOUNTER)
	{
		stars_in_space = CaptureDrawable (
				LoadGraphic (STAR_MASK_PMAP_ANIM));
		if (stars_in_space == NULL)
			return FALSE;

		if (!load_animation (explosion,
				BOOM_BIG_MASK_PMAP_ANIM,
				BOOM_MED_MASK_PMAP_ANIM,
				BOOM_SML_MASK_PMAP_ANIM))
			return FALSE;

		if (!load_animation (blast,
				BLAST_BIG_MASK_PMAP_ANIM,
				BLAST_MED_MASK_PMAP_ANIM,
				BLAST_SML_MASK_PMAP_ANIM))
			return FALSE;

		if (!load_animation (asteroid,
				ASTEROID_BIG_MASK_PMAP_ANIM,
				ASTEROID_MED_MASK_PMAP_ANIM,
				ASTEROID_SML_MASK_PMAP_ANIM))
			return FALSE;
	}

	return TRUE;
}

void
UninitSpace (void)
{
	if (space_ini_cnt && --space_ini_cnt == 0)
	{
		free_image (blast);
		free_image (explosion);
		free_image (asteroid);

		DestroyDrawable (ReleaseDrawable (stars_in_space));
		stars_in_space = 0;
	}
}


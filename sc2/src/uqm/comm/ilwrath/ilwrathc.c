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

#include "../commall.h"
#include "resinst.h"
#include "strings.h"

#include "uqm/gameev.h"


static LOCDATA ilwrath_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	ILWRATH_PMAP_ANIM, /* AlienFrame */
	ILWRATH_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH - 16, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_MIDDLE, /* AlienTextValign */
	ILWRATH_COLOR_MAP, /* AlienColorMap */
	ILWRATH_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	ILWRATH_CONVERSATION_PHRASES, /* PlayerPhrases */
	4, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{
			6, /* StartIndex */
			5, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			11, /* StartIndex */
			5, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			16, /* StartIndex */
			5, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			21, /* StartIndex */
			6, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
	},
	{ /* AlienTransitionDesc */
		0, /* StartIndex */
		0, /* NumFrames */
		0, /* AnimFlags */
		0, 0, /* FrameRate */
		0, 0, /* RestartRate */
		0, /* BlockMask */
	},
	{ /* AlienTalkDesc */
		1, /* StartIndex */
		5, /* NumFrames */
		0, /* AnimFlags */
		ONE_SECOND / 15, 0, /* FrameRate */
		ONE_SECOND / 12, 0, /* RestartRate */
		0, /* BlockMask */
	},
	NULL, /* AlienNumberSpeech - none */
	/* Filler for loaded resources */
	NULL, NULL, NULL,
	NULL,
	NULL,
};

// Native asset/lifecycle configuration; story branches live in JSON.
LOCDATA*
init_ilwrath_comm (void)
{
	LOCDATA *retval;

	ilwrath_desc.AlienTextBaseline.x = TEXT_X_OFFS + (SIS_TEXT_WIDTH >> 1);
	ilwrath_desc.AlienTextBaseline.y = 70;
	ilwrath_desc.AlienTextWidth = SIS_TEXT_WIDTH - 16;

	if (GET_GAME_STATE (PROBE_ILWRATH_ENCOUNTER)
			|| (GET_GAME_STATE (GLOBAL_FLAGS_AND_DATA)
			& ((1 << 4) | (1 << 5)))
			|| LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE)
	{
		setSegue (Segue_peace);
	}
	else
	{
		setSegue (Segue_hostile);
	}
	retval = &ilwrath_desc;

	return (retval);
}

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

#include "uqm/build.h"


static LOCDATA supox_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	SUPOX_PMAP_ANIM, /* AlienFrame */
	SUPOX_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH - 16, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_TOP, /* AlienTextValign */
	SUPOX_COLOR_MAP, /* AlienColorMap */
	SUPOX_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	SUPOX_CONVERSATION_PHRASES, /* PlayerPhrases */
	4, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{
			4, /* StartIndex */
			5, /* NumFrames */
			YOYO_ANIM
					| WAIT_TALKING, /* AnimFlags */
			ONE_SECOND / 10, ONE_SECOND / 10, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			9, /* StartIndex */
			10, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			19, /* StartIndex */
			10, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			29, /* StartIndex */
			13, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
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
		3, /* NumFrames */
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

// Dialogue branches and story actions are defined in comm.supox.dialogue.json.
// The descriptor retains only native media and encounter lifecycle bindings.
LOCDATA*
init_supox_comm (void)
{
	LOCDATA *retval;

	supox_desc.AlienTextBaseline.x = TEXT_X_OFFS + (SIS_TEXT_WIDTH >> 1);
	supox_desc.AlienTextBaseline.y = 0;
	supox_desc.AlienTextWidth = SIS_TEXT_WIDTH - 16;

	if (!GET_GAME_STATE (SUPOX_HOSTILE)
			|| LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE)
	{
		setSegue (Segue_peace);
	}
	else
	{
		setSegue (Segue_hostile);
	}
	retval = &supox_desc;

	return (retval);
}

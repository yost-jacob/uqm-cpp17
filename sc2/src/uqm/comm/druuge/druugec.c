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
#include "uqm/setup.h"
#include "uqm/sis.h"
		// for DeltaSISGauges()


static LOCDATA druuge_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	DRUUGE_PMAP_ANIM, /* AlienFrame */
	DRUUGE_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH - 16, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_MIDDLE, /* AlienTextValign */
	DRUUGE_COLOR_MAP, /* AlienColorMap */
	DRUUGE_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	DRUUGE_CONVERSATION_PHRASES, /* PlayerPhrases */
	11, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{
			5, /* StartIndex */
			4, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND * 3 / 40, 0, /* FrameRate */
			ONE_SECOND * 3 / 40, 0, /* RestartRate */
			0, /* BlockMask */
		},
		{
			9, /* StartIndex */
			4, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND * 3 / 40, 0, /* FrameRate */
			ONE_SECOND * 3 / 40, 0, /* RestartRate */
			0, /* BlockMask */
		},
		{
			13, /* StartIndex */
			6, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND * 3 / 40, 0, /* FrameRate */
			ONE_SECOND * 3 / 40, 0, /* RestartRate */
			0, /* BlockMask */
		},
		{
			19, /* StartIndex */
			3, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			22, /* StartIndex */
			3, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 12, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			25, /* StartIndex */
			3, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 12, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			28, /* StartIndex */
			3, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 12, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			31, /* StartIndex */
			2, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 12, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			33, /* StartIndex */
			7, /* NumFrames */
			CIRCULAR_ANIM | WAIT_TALKING, /* AnimFlags */
			ONE_SECOND / 12, 0, /* FrameRate */
			ONE_SECOND * 7, ONE_SECOND * 3,/* RestartRate */
			0, /* BlockMask */
		},
		{
			40, /* StartIndex */
			4, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND * 3 / 10, 0, /* FrameRate */
			ONE_SECOND * 3 / 10, 0, /* RestartRate */
			0, /* BlockMask */
		},
		{
			44, /* StartIndex */
			4, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 5, 0, /* FrameRate */
			ONE_SECOND / 5, 0, /* RestartRate */
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
		4, /* NumFrames */
		0, /* AnimFlags */
		ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
		ONE_SECOND / 12, ONE_SECOND / 12, /* RestartRate */
		0, /* BlockMask */
	},
	NULL, /* AlienNumberSpeech - none */
	/* Filler for loaded resources */
	NULL, NULL, NULL,
	NULL,
	NULL,
};

// Story branches and response routing live in the JSON conversation graph.
// C++ state operations own commerce/report plans; this descriptor retains media.
LOCDATA*
init_druuge_comm (void)
{
	LOCDATA *retval;



	druuge_desc.init_encounter_func = NULL;
	druuge_desc.post_encounter_func = NULL;
	druuge_desc.uninit_encounter_func = NULL;

	druuge_desc.AlienTextBaseline.x = TEXT_X_OFFS + (SIS_TEXT_WIDTH >> 1);
	druuge_desc.AlienTextBaseline.y = 70;
	druuge_desc.AlienTextWidth = SIS_TEXT_WIDTH - 16;

	if ((GET_GAME_STATE (DRUUGE_MANNER) == 0
			&& (GET_GAME_STATE (GLOBAL_FLAGS_AND_DATA) & (1 << 7)))
			|| LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE)
	{
		setSegue (Segue_peace);
	}
	else
	{
		setSegue (Segue_hostile);
	}
	retval = &druuge_desc;

	return (retval);
}

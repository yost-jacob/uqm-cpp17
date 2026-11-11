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


static LOCDATA arilou_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	ARILOU_PMAP_ANIM, /* AlienFrame */
	ARILOU_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH - 16, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_TOP, /* AlienTextValign */
	ARILOU_COLOR_MAP, /* AlienColorMap */
	ARILOU_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	ARILOU_CONVERSATION_PHRASES, /* PlayerPhrases */
	20, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{
			4, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 1) | (1L << 16)
		},
		{
			13, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 0) | (1L << 16)
		},
		{
			22, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1L << 16)
		},
		{
			31, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 4)
		},
		{
			40, /* StartIndex */
			10, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 3)
		},
		{
			50, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 7)
		},
		{
			59, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			67, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 5)
		},
		{
			76, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 9)
		},
		{
			85, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 8)
		},
		{
			94, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			103, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 13)
		},
		{
			112, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			121, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 11)
		},
		{
			129, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1L << 15)
		},
		{
			138, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 14)
		},
		{
			146, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 30, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 0) | (1 << 1) | (1 << 2)
		},
		{	/* Hands moving (right up) */
			155, /* StartIndex */
			2, /* NumFrames */
			CIRCULAR_ANIM | WAIT_TALKING, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 19), /* BlockMask */
		},
		{	/* Hands moving (left up) */
			157, /* StartIndex */
			2, /* NumFrames */
			CIRCULAR_ANIM | WAIT_TALKING, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 19), /* BlockMask */
		},
		{	/* Stars flashing next to the head */
			159, /* StartIndex */
			4, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 12, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 17) | (1 << 18), /* BlockMask */
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

// Resource/lifecycle compatibility. Dialogue and portal exchange live in JSON.
static void
post_arilou_enc (void)
{
	BYTE Manner;

	if (getSegue () == Segue_hostile
			&& (Manner = GET_GAME_STATE (ARILOU_MANNER)) != 2)
	{
		SET_GAME_STATE (ARILOU_MANNER, 1);
		if (Manner != 1)
		{
			SET_GAME_STATE (ARILOU_VISITS, 0);
			SET_GAME_STATE (ARILOU_HOME_VISITS, 0);
		}
	}

	if (GET_GAME_STATE (ARILOU_SPACE_SIDE) > 1
			&& GET_GAME_STATE (ARILOU_HOME_VISITS) <= 1)
	{
		SET_GAME_STATE (UMGAH_ZOMBIE_BLOBBIES, 1);
		SET_GAME_STATE (UMGAH_VISITS, 0);
		SET_GAME_STATE (UMGAH_HOME_VISITS, 0);

		if (GET_GAME_STATE (ARILOU_MANNER) < 2)
		{
			SET_GAME_STATE (ARILOU_MANNER, 3);
		}
	}
}

LOCDATA*
init_arilou_comm (void)
{
	LOCDATA *retval;

	arilou_desc.post_encounter_func = post_arilou_enc;

	arilou_desc.AlienTextBaseline.x = TEXT_X_OFFS + (SIS_TEXT_WIDTH >> 1);
	arilou_desc.AlienTextBaseline.y = 0;
	arilou_desc.AlienTextWidth = SIS_TEXT_WIDTH - 16;

	if (GET_GAME_STATE (ARILOU_SPACE_SIDE) > 1
			|| GET_GAME_STATE (ARILOU_MANNER) == 3
			|| LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE)
	{
		setSegue (Segue_peace);
	}
	else
	{
		setSegue (Segue_hostile);
	}
	retval = &arilou_desc;

	return (retval);
}

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

static LOCDATA urquan_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	URQUAN_PMAP_ANIM, /* AlienFrame */
	URQUAN_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH - 16, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_TOP, /* AlienTextValign */
	URQUAN_COLOR_MAP, /* AlienColorMap */
	URQUAN_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	URQUAN_CONVERSATION_PHRASES, /* PlayerPhrases */
	7, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{
			7, /* StartIndex */
			6, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 10, 0, /* FrameRate */
			ONE_SECOND / 10, 0, /* RestartRate */
			0, /* BlockMask */
		},
		{
			13, /* StartIndex */
			3, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			16, /* StartIndex */
			3, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			19, /* StartIndex */
			3, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			22, /* StartIndex */
			7, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			29, /* StartIndex */
			7, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			36, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
	},
	{ /* AlienTransitionDesc */
		1, /* StartIndex */
		2, /* NumFrames */
		0, /* AnimFlags */
		ONE_SECOND / 6, 0, /* FrameRate */
		0, 0, /* RestartRate */
		0, /* BlockMask */
	},
	{ /* AlienTalkDesc */
		2, /* StartIndex */
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

// Resource/lifecycle compatibility. Dialogue control flow lives in JSON.
static void Intro (void) {}

static COUNT
uninit_urquan (void)
{
	return (0);
}

static void
post_urquan_enc (void)
{
	SET_GAME_STATE (PLAYER_HYPNOTIZED, 0);
}

LOCDATA*
init_urquan_comm (void)
{
	LOCDATA *retval;

	DWORD GrpOffs;

	urquan_desc.init_encounter_func = Intro;
	urquan_desc.post_encounter_func = post_urquan_enc;
	urquan_desc.uninit_encounter_func = uninit_urquan;

	urquan_desc.AlienTextBaseline.x = TEXT_X_OFFS + (SIS_TEXT_WIDTH >> 1);
	urquan_desc.AlienTextBaseline.y = 0;
	urquan_desc.AlienTextWidth = SIS_TEXT_WIDTH - 16;

	GrpOffs = GET_GAME_STATE_32 (URQUAN_PROBE_GRPOFFS0);
	if (GET_GAME_STATE (PLAYER_HYPNOTIZED)
			|| LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE
			|| (LOBYTE (GLOBAL (CurrentActivity)) == IN_INTERPLANETARY
			&& GLOBAL (BattleGroupRef)
			&& GLOBAL (BattleGroupRef) == GrpOffs))
	{
		setSegue (Segue_peace);
	}
	else
	{
		setSegue (Segue_hostile);
	}
	retval = &urquan_desc;

	return (retval);
}

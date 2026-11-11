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
#include "uqm/gameev.h"


#define ZOQ_FG_COLOR WHITE_COLOR
#define ZOQ_BG_COLOR BLACK_COLOR
#define ZOQ_BASE_X (TEXT_X_OFFS + ((SIS_TEXT_WIDTH >> 1) >> 1))
#define ZOQ_BASE_Y 24
#define ZOQ_TALK_INDEX 18
#define ZOQ_TALK_FRAMES 5
#define FOT_TO_ZOQ 23

#define PIK_FG_COLOR WHITE_COLOR
#define PIK_BG_COLOR BLACK_COLOR
#define PIK_BASE_X (SIS_SCREEN_WIDTH - (TEXT_X_OFFS + ((SIS_TEXT_WIDTH >> 1) >> 1)))
#define PIK_BASE_Y 24
#define PIK_TALK_INDEX 29
#define PIK_TALK_FRAMES 2
#define FOT_TO_PIK 26

static LOCDATA zoqfot_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	ZOQFOTPIK_PMAP_ANIM, /* AlienFrame */
	ZOQFOTPIK_FONT, /* AlienFont */
	UNDEFINED_COLOR_INIT, /* AlienTextFColor */
	UNDEFINED_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_MIDDLE, /* AlienTextValign */
	ZOQFOTPIK_COLOR_MAP, /* AlienColorMap */
	ZOQFOTPIK_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	ZOQFOTPIK_CONVERSATION_PHRASES, /* PlayerPhrases */
	3, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{ /* Eye blink */
			1, /* StartIndex */
			4, /* NumFrames */
			YOYO_ANIM /* AnimFlags */
			| WAIT_TALKING,
			ONE_SECOND / 24, 0, /* FrameRate */
			0, ONE_SECOND * 10, /* RestartRate */
			0, /* BlockMask */
		},
		{ /* Blow smoke */
			5, /* StartIndex */
			5, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND * 7 / 120, 0, /* FrameRate */
			ONE_SECOND * 2, 0, /* RestartRate */
			0, /* BlockMask */
		},
		{ /* Gulp */
			10, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			0, ONE_SECOND * 10, /* RestartRate */
			0, /* BlockMask */
		},
	},
	{ /* AlienTransitionDesc - Move Eye */
		FOT_TO_ZOQ, /* StartIndex */
		3, /* NumFrames */
		0, /* AnimFlags */
		ONE_SECOND / 30, 0, /* FrameRate */
		0, 0, /* RestartRate */
		0, /* BlockMask */
	},
	{ /* AlienTalkDesc */
		ZOQ_TALK_INDEX, /* StartIndex */
		ZOQ_TALK_FRAMES, /* NumFrames */
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

// Story branches and actions are defined in comm.zoqfotpik.dialogue.json.
// Media cues are bound by the C++ presenter; this descriptor owns native resources.
LOCDATA*
init_zoqfot_comm (void)
{
	LOCDATA *retval;

	zoqfot_desc.AlienTextWidth = (SIS_TEXT_WIDTH >> 1) - TEXT_X_OFFS;

	if (CheckAlliance (ZOQFOTPIK_SHIP) == GOOD_GUY
			|| LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE)
	{
		setSegue (Segue_peace);
	}
	else
	{
		setSegue (Segue_hostile);
	}
	
	retval = &zoqfot_desc;

	return (retval);
}


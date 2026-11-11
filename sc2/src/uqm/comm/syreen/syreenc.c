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


static LOCDATA syreen_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	SYREEN_PMAP_ANIM, /* AlienFrame */
	SYREEN_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH - 16, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_TOP, /* AlienTextValign */
	SYREEN_COLOR_MAP, /* AlienColorMap */
	SYREEN_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	SYREEN_CONVERSATION_PHRASES, /* PlayerPhrases */
	15, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{
			5, /* StartIndex */
			2, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			7, /* StartIndex */
			2, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			9, /* StartIndex */
			2, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			11, /* StartIndex */
			2, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			13, /* StartIndex */
			2, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			15, /* StartIndex */
			2, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 12), /* BlockMask */
		},
		{
			17, /* StartIndex */
			2, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			19, /* StartIndex */
			2, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			(1 << 13),
		},
		{
			21, /* StartIndex */
			6, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			27, /* StartIndex */
			4, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND * 10, ONE_SECOND * 3, /* RestartRate */
			(1 << 14), /* BlockMask */
		},
		{
			31, /* StartIndex */
			6, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			37, /* StartIndex */
			4, /* NumFrames */
			RANDOM_ANIM, /* AnimFlags */
			ONE_SECOND / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND / 15, ONE_SECOND / 15, /* RestartRate */
			0, /* BlockMask */
		},
		{
			41, /* StartIndex */
			3, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
			ONE_SECOND / 10, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND * 10, ONE_SECOND * 3, /* RestartRate */
			(1 << 5), /* BlockMask */
		},
		{
			44, /* StartIndex */
			4, /* NumFrames */
			YOYO_ANIM
					| WAIT_TALKING, /* AnimFlags */
			ONE_SECOND / 6, 0, /* FrameRate */
			ONE_SECOND * 3, ONE_SECOND, /* RestartRate */
			(1 << 7) | (1 << 14), /* BlockMask */
		},
		{
			48, /* StartIndex */
			3, /* NumFrames */
			YOYO_ANIM
					| WAIT_TALKING, /* AnimFlags */
			ONE_SECOND * 2 / 15, ONE_SECOND / 15, /* FrameRate */
			ONE_SECOND * 10, ONE_SECOND,/* RestartRate */
			(1 << 9) | (1 << 13), /* BlockMask */
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

// Story branches and actions are defined in comm.syreen.dialogue.json.
// Presentation callbacks are registered in conversation_cues.cpp.
LOCDATA*
init_syreen_comm (void)
{
	LOCDATA *retval;

	syreen_desc.AlienTextBaseline.x = TEXT_X_OFFS + (SIS_TEXT_WIDTH >> 1);
	syreen_desc.AlienTextBaseline.y = 0;
	syreen_desc.AlienTextWidth = SIS_TEXT_WIDTH - 16;

	setSegue (Segue_peace);
	retval = &syreen_desc;

	return (retval);
}

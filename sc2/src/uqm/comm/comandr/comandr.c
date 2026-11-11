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

#include "uqm/setup.h"
#include "uqm/sis.h"
		// for DeltaSISGauges(), DrawLanders()
#include "libs/graphics/gfx_common.h"

static LOCDATA commander_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	COMMANDER_PMAP_ANIM, /* AlienFrame */
	COMMANDER_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_MIDDLE, /* AlienTextValign */
	COMMANDER_COLOR_MAP, /* AlienColorMap */
	COMMANDER_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	COMMANDER_CONVERSATION_PHRASES, /* PlayerPhrases */
	3, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{ /* Blink */
			1, /* StartIndex */
			3, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
			ONE_SECOND / 15, 0, /* FrameRate */
			0, ONE_SECOND * 8, /* RestartRate */
			0, /* BlockMask */
		},
		{ /* Running light */
			10, /* StartIndex */
			30, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 40, 0, /* FrameRate */
			ONE_SECOND * 2, 0, /* RestartRate */
			0, /* BlockMask */
		},
		{
			1, /* StartIndex */
			3, /* NumFrames */
			RANDOM_ANIM | COLORXFORM_ANIM,/* AnimFlags */
			0, ONE_SECOND / 30, /* FrameRate */
			0, ONE_SECOND / 15, /* RestartRate */
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
		4, /* StartIndex */
		6, /* NumFrames */
		0, /* AnimFlags */
		ONE_SECOND / 10, ONE_SECOND / 15, /* FrameRate */
		ONE_SECOND * 7 / 60, ONE_SECOND / 12, /* RestartRate */
		0, /* BlockMask */
	},
	NULL, /* AlienNumberSpeech - none */
	/* Filler for loaded resources */
	NULL, NULL, NULL,
	NULL,
	NULL,
};

// Story branches and actions are defined in comm.commander.dialogue.json.
// Media cues are bound by the C++ presenter; this descriptor owns native resources.
LOCDATA*
init_commander_comm ()
{
	LOCDATA *retval;

	if (GET_GAME_STATE (RADIOACTIVES_PROVIDED))
	{
		commander_desc.AlienAmbientArray[2].AnimFlags |= ANIM_DISABLED;
		// regular track -- let's make sure
		commander_desc.AlienSongFlags &= ~LDASF_USE_ALTERNATE;
	}
	else
	{	
		commander_desc.AlienAmbientArray[2].AnimFlags &= ~ANIM_DISABLED;
		// use alternate 'low-power' track if available
		commander_desc.AlienAltSongRes = COMMANDER_LOWPOW_MUSIC;
		commander_desc.AlienSongFlags |= LDASF_USE_ALTERNATE;
	}

	commander_desc.AlienTextWidth = 143;
	commander_desc.AlienTextBaseline.x = 164;
	commander_desc.AlienTextBaseline.y = 20;

	setSegue (Segue_peace);
	retval = &commander_desc;

	return (retval);
}

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
#include <stdlib.h>
#include "resinst.h"
#include "strings.h"

#include "options.h"
#include "uqm/battle.h"
#include "uqm/setup.h"


static const NUMBER_SPEECH_DESC probe_numbers_english;

static LOCDATA slylandro_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	SLYLAND_PMAP_ANIM, /* AlienFrame */
	SLYLAND_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH - 16, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_TOP, /* AlienTextValign */
	SLYLAND_COLOR_MAP, /* AlienColorMap */
	SLYLAND_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	SLYLAND_CONVERSATION_PHRASES, /* PlayerPhrases */
	6, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{
			1, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 40, 0, /* FrameRate */
			0, ONE_SECOND * 3, /* RestartRate */
			(1 << 3) /* BlockMask */
		},
		{
			10, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 40, 0, /* FrameRate */
			0, ONE_SECOND * 3, /* RestartRate */
			(1 << 4) /* BlockMask */
		},
		{
			18, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 40, 0, /* FrameRate */
			0, ONE_SECOND * 3, /* RestartRate */
			(1 << 5) /* BlockMask */
		},
		{
			26, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 40, 0, /* FrameRate */
			0, ONE_SECOND * 3, /* RestartRate */
			(1 << 0) /* BlockMask */
		},
		{
			34, /* StartIndex */
			8, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 40, 0, /* FrameRate */
			0, ONE_SECOND * 3, /* RestartRate */
			(1 << 1) /* BlockMask */
		},
		{
			42, /* StartIndex */
			9, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 40, 0, /* FrameRate */
			0, ONE_SECOND * 3, /* RestartRate */
			(1 << 2) /* BlockMask */
		},
	},
	{ /* AlienTransitionDesc - empty */
		0, /* StartIndex */
		0, /* NumFrames */
		0, /* AnimFlags */
		0, 0, /* FrameRate */
		0, 0, /* RestartRate */
		0, /* BlockMask */
	},
	{ /* AlienTalkDesc - empty */
		0, /* StartIndex */
		0, /* NumFrames */
		0, /* AnimFlags */
		0, 0, /* FrameRate */
		0, 0, /* RestartRate */
		0, /* BlockMask */
	},
	&probe_numbers_english, /* AlienNumberSpeech - default */
	/* Filler for loaded resources */
	NULL, NULL, NULL,
	NULL,
	NULL,
};

static COUNT probe_digit_names[] =
{
	ENUMERATE_ZERO,
	ENUMERATE_ONE,
	ENUMERATE_TWO,
	ENUMERATE_THREE,
	ENUMERATE_FOUR,
	ENUMERATE_FIVE,
	ENUMERATE_SIX,
	ENUMERATE_SEVEN,
	ENUMERATE_EIGHT,
	ENUMERATE_NINE,
};

static COUNT probe_teen_names[] =
{
	ENUMERATE_TEN,
	ENUMERATE_ELEVEN,
	ENUMERATE_TWELVE,
	ENUMERATE_THIRTEEN,
	ENUMERATE_FOURTEEN,
	ENUMERATE_FIFTEEN,
	ENUMERATE_SIXTEEN,
	ENUMERATE_SEVENTEEN,
	ENUMERATE_EIGHTEEN,
	ENUMERATE_NINETEEN,
};

static COUNT probe_tens_names[] =
{
	0, /* invalid */
	0, /* skip digit */
	ENUMERATE_TWENTY,
	ENUMERATE_THIRTY,
	ENUMERATE_FOURTY,
	ENUMERATE_FIFTY,
	ENUMERATE_SIXTY,
	ENUMERATE_SEVENTY,
	ENUMERATE_EIGHTY,
	ENUMERATE_NINETY,
};

static const NUMBER_SPEECH_DESC probe_numbers_english =
{
	5, /* NumDigits */
	{
		{ /* 1000-999999 */
			1000, /* Divider */
			0, /* Subtrahend */
			NULL, /* StrDigits - recurse */
			NULL, /* Names - not used */
			ENUMERATE_THOUSAND /* CommonIndex */
		},
		{ /* 100-999 */
			100, /* Divider */
			0, /* Subtrahend */
			probe_digit_names, /* StrDigits */
			NULL, /* Names - not used */
			ENUMERATE_HUNDRED /* CommonIndex */
		},
		{ /* 20-99 */
			10, /* Divider */
			0, /* Subtrahend */
			probe_tens_names, /* StrDigits */
			NULL, /* Names - not used */
			0 /* CommonIndex - not used */
		},
		{ /* 10-19 */
			1, /* Divider */
			10, /* Subtrahend */
			probe_teen_names, /* StrDigits */
			NULL, /* Names - not used */
			0 /* CommonIndex - not used */
		},
		{ /* 0-9 */
			1, /* Divider */
			0, /* Subtrahend */
			probe_digit_names, /* StrDigits */
			NULL, /* Names - not used */
			0 /* CommonIndex - not used */
		}
	}
};

static void
sayCoord (int coord)
{
	int ac;
	
	ac = abs (coord);

	NPCPhrase_splice (coord < 0 ? COORD_MINUS : COORD_PLUS);
	NPCNumber (ac / 10, "%03d");
	NPCPhrase_splice (COORD_POINT);
	NPCNumber (ac % 10, NULL);
}

void
QueueProbeCoordinates (void)
{
    SIZE dx = LOGX_TO_UNIVERSE (GetPlayerTravelPosition()->position.x) - 333;
    SIZE dy = 9812 - LOGY_TO_UNIVERSE (GetPlayerTravelPosition()->position.y);
    NPCPhrase (THIS_IS_PROBE_40);
    sayCoord (dy);
    NPCPhrase_splice (THIS_IS_PROBE_41);
    sayCoord (dx);
    NPCPhrase (THIS_IS_PROBE_42);
}

// Configuration/lifecycle compatibility only. The JSON graph owns the intro.
LOCDATA*
init_slyland_comm (void)
{
	LOCDATA *retval;

	slylandro_desc.AlienTextBaseline.x = TEXT_X_OFFS + (SIS_TEXT_WIDTH >> 1);
	slylandro_desc.AlienTextBaseline.y = 0;
	slylandro_desc.AlienTextWidth = SIS_TEXT_WIDTH - 16;

	setSegue (Segue_hostile);
	retval = &slylandro_desc;

	return (retval);
}


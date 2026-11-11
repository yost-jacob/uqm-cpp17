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
#include "uqm/conversation_native.h"

#include "uqm/build.h"
#include "uqm/hyper.h"
			// for SOL_X/SOL_Y


static LOCDATA chmmr_desc =
{
	NULL, /* init_encounter_func */
	NULL, /* post_encounter_func */
	NULL, /* uninit_encounter_func */
	CHMMR_PMAP_ANIM, /* AlienFrame */
	CHMMR_FONT, /* AlienFont */
	WHITE_COLOR_INIT, /* AlienTextFColor */
	BLACK_COLOR_INIT, /* AlienTextBColor */
	{0, 0}, /* AlienTextBaseline */
	0, /* SIS_TEXT_WIDTH - 16, */ /* AlienTextWidth */
	ALIGN_CENTER, /* AlienTextAlign */
	VALIGN_TOP, /* AlienTextValign */
	CHMMR_COLOR_MAP, /* AlienColorMap */
	CHMMR_MUSIC, /* AlienSong */
	NULL_RESOURCE, /* AlienAltSong */
	0, /* AlienSongFlags */
	CHMMR_CONVERSATION_PHRASES, /* PlayerPhrases */
	6, /* NumAnimations */
	{ /* AlienAmbientArray (ambient animations) */
		{
			12, /* StartIndex */
			5, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			17, /* StartIndex */
			5, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			22, /* StartIndex */
			5, /* NumFrames */
			CIRCULAR_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			27, /* StartIndex */
			20, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			47, /* StartIndex */
			14, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
			ONE_SECOND / 20, 0, /* FrameRate */
			ONE_SECOND, ONE_SECOND * 3, /* RestartRate */
			0, /* BlockMask */
		},
		{
			61, /* StartIndex */
			24, /* NumFrames */
			YOYO_ANIM, /* AnimFlags */
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
		11, /* NumFrames */
		0, /* AnimFlags */
		ONE_SECOND / 60, 0, /* FrameRate */
		ONE_SECOND / 60, 0, /* RestartRate */
		0, /* BlockMask */
	},
	NULL, /* AlienNumberSpeech - none */
	/* Filler for loaded resources */
	NULL, NULL, NULL,
	NULL,
	NULL,
};

// Bounded inventory/transport operation; dialogue and story flags are graph data.
// Preserve the established refit rules until SIS outfitting/travel have a common API.
void
ApplyChmmrBombRefit (int starbaseAvailable)
{
    int i;
		GLOBAL_SIS (ResUnits) = 1000000L;
		GLOBAL_SIS (NumLanders) = 0;
		GetGameData()->prices.modules[PLANET_LANDER] = 0;

#define EARTH_INDEX 2 /* earth is 3rd planet --> 3 - 1 = 2 */
/* Magic numbers for Earth */
#define EARTH_OUTER_X (-725)
#define EARTH_OUTER_Y (597)
#define EARTH_INNER_X (121)
#define EARTH_INNER_Y (113)
/* Magic numbers for Earth Starbase */
#define STARBASE_INNER_X (86)
#define STARBASE_INNER_Y (113)

		/* transport player to Earth */
		GetPlayerTravelPosition()->position.x = UNIVERSE_TO_LOGX (SOL_X);
		GetPlayerTravelPosition()->position.y = UNIVERSE_TO_LOGY (SOL_Y);
		GetPlayerTravelPosition()->flight.facing = 1;
		/* At Earth or at Starbase */
		GetPlayerTravelPosition()->flight.planet = EARTH_INDEX + 1;
		GetPlayerTravelPosition()->flight.loadOrbit = 0;
		/* XXX : this should be unhardcoded eventually */
		GetPlayerTravelPosition()->flight.position.x = EARTH_OUTER_X;
		GetPlayerTravelPosition()->flight.position.y = EARTH_OUTER_Y;

		if (starbaseAvailable)
		{	/* Normal game mode - you are transported to Starbase */
			GLOBAL_SIS (FuelOnBoard) = FUEL_RESERVE;
			GLOBAL_SIS (CrewEnlisted) = 0;
			GLOBAL_SIS (TotalElementMass) = 0;
			GetGameData()->prices.modules[STORAGE_BAY] = 0; /* disable Storage Bay */
			for (i = 0; i < NUM_ELEMENT_CATEGORIES; ++i)
				GLOBAL_SIS (ElementAmounts[i]) = 0;
			for (i = NUM_BOMB_MODULES; i < NUM_MODULE_SLOTS; ++i)
				GLOBAL_SIS (ModuleSlots[i]) = EMPTY_SLOT + 2;

			/* XXX : this should be unhardcoded eventually */
			/* transport to Starbase */
			GLOBAL (ShipStamp.origin.x) = STARBASE_INNER_X - SAFE_X;
			GLOBAL (ShipStamp.origin.y) = STARBASE_INNER_Y - SAFE_Y;
		}
		else
		{	/* 'Beating Game Differently' mode - never visited Starbase,
			 * so you are transported to Earth */
			/* compress the layout -- move all to front */
			for (i = NUM_MODULE_SLOTS - 1; i > 0; --i)
			{
				int m;

				/* find next unused slot */
				for (; i > 0
						&& GLOBAL_SIS (ModuleSlots[i]) != EMPTY_SLOT + 2;
						--i)
					;
				if (i == 0)
					break;
				/* find next module to move */
				for (m = i - 1; m >= 0
						&& GLOBAL_SIS (ModuleSlots[m]) == EMPTY_SLOT + 2;
						--m)
					;
				if (m < 0)
					break;
								
				/* move the module */
				GLOBAL_SIS (ModuleSlots[i]) = GLOBAL_SIS (ModuleSlots[m]);
				GLOBAL_SIS (ModuleSlots[m]) = EMPTY_SLOT + 2;
			}

			/* XXX : this should be unhardcoded eventually */
			/* transport to Earth itself */
			GLOBAL (ShipStamp.origin.x) = EARTH_INNER_X - SAFE_X;
			GLOBAL (ShipStamp.origin.y) = EARTH_INNER_Y - SAFE_Y;
		}

		/* install Chmmr-supplied modules */
		for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
			GLOBAL_SIS (DriveSlots[i]) = FUSION_THRUSTER;
		for (i = 0; i < NUM_JET_SLOTS; ++i)
			GLOBAL_SIS (JetSlots[i]) = TURNING_JETS;
		GLOBAL_SIS (ModuleSlots[0]) = BOMB_MODULE_4;
		GLOBAL_SIS (ModuleSlots[1]) = BOMB_MODULE_5;
		GLOBAL_SIS (ModuleSlots[2]) = BOMB_MODULE_3;
		GLOBAL_SIS (ModuleSlots[3]) = BOMB_MODULE_1;
		GLOBAL_SIS (ModuleSlots[4]) = BOMB_MODULE_0;
		GLOBAL_SIS (ModuleSlots[5]) = BOMB_MODULE_1;
		GLOBAL_SIS (ModuleSlots[6]) = BOMB_MODULE_3;
		GLOBAL_SIS (ModuleSlots[7]) = BOMB_MODULE_4;
		GLOBAL_SIS (ModuleSlots[8]) = BOMB_MODULE_5;
		GLOBAL_SIS (ModuleSlots[9]) = BOMB_MODULE_2;
}

// Story branches and actions are defined in comm.chmmr.dialogue.json.
// Media cues are bound by the C++ presenter; this descriptor owns native resources.
LOCDATA*
init_chmmr_comm (void)
{
	LOCDATA *retval;

	chmmr_desc.AlienTextBaseline.x = TEXT_X_OFFS + (SIS_TEXT_WIDTH >> 1);
	chmmr_desc.AlienTextBaseline.y = 0;
	chmmr_desc.AlienTextWidth = SIS_TEXT_WIDTH - 16;

	setSegue (Segue_peace);
	retval = &chmmr_desc;

	return (retval);
}

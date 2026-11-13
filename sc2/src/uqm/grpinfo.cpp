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

#include "build.h"
#include "game_state.hpp"
#include "navigation_rules.hpp"
#include "system_flight.hpp"
#include "starmap.h"
#include "gendef.h"
#include "libs/file.h"
#include "globdata.h"
#include "intel.h"
#include "state.h"
#include "grpintrn.h"

#include "libs/mathlib.h"
#include "libs/log.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

void
UninitGroupInfo (void)
{
	DeleteStateFile (DEFGRPINFO_FILE);
	DeleteStateFile (RANDGRPINFO_FILE);
}

HIPGROUP
BuildGroup (QUEUE *pDstQueue, BYTE race_id)
{
	HFLEETINFO hFleet;
	FLEET_INFO *TemplatePtr;
	HLINK hGroup;
	IP_GROUP *GroupPtr;

	assert (GetLinkSize (pDstQueue) == sizeof (IP_GROUP));

	hFleet = GetStarShipFromIndex (&GLOBAL (avail_race_q), race_id);
	if (!hFleet)
		return 0;

	hGroup = AllocLink (pDstQueue);
	if (!hGroup)
		return 0;
	
	TemplatePtr = LockFleetInfo (&GLOBAL (avail_race_q), hFleet);
	GroupPtr = LockIpGroup (pDstQueue, hGroup);
	memset (GroupPtr, 0, GetLinkSize (pDstQueue));
	GroupPtr->race_id = race_id;
	GroupPtr->melee_icon = TemplatePtr->melee_icon;
	UnlockFleetInfo (&GLOBAL (avail_race_q), hFleet);
	UnlockIpGroup (pDstQueue, hGroup);
	PutQueue (pDstQueue, hGroup);

	return hGroup;
}


void
uqm::buildGroups (const SystemGeometry &geometry, QUEUE &groups)
{
	BYTE Index;
	BYTE BestIndex = 0;
	COUNT BestPercent = 0;
	POINT universe;
	HFLEETINFO hFleet, hNextFleet;
	BYTE HomeWorld[] =
	{
		0,                /* ARILOU_SHIP */
		0,                /* CHMMR_SHIP */
		0,                /* HUMAN_SHIP */
		ORZ_DEFINED,      /* ORZ_SHIP */
		PKUNK_DEFINED,    /* PKUNK_SHIP */
		0,                /* SHOFIXTI_SHIP */
		SPATHI_DEFINED,   /* SPATHI_SHIP */
		SUPOX_DEFINED,    /* SUPOX_SHIP */
		THRADD_DEFINED,   /* THRADDASH_SHIP */
		UTWIG_DEFINED,    /* UTWIG_SHIP */
		VUX_DEFINED,      /* VUX_SHIP */
		YEHAT_DEFINED,    /* YEHAT_SHIP */
		0,                /* MELNORME_SHIP */
		DRUUGE_DEFINED,   /* DRUUGE_SHIP */
		ILWRATH_DEFINED,  /* ILWRATH_SHIP */
		MYCON_DEFINED,    /* MYCON_SHIP */
		0,                /* SLYLANDRO_SHIP */
		UMGAH_DEFINED,    /* UMGAH_SHIP */
		0,                /* URQUAN_SHIP */
		ZOQFOT_DEFINED,   /* ZOQFOTPIK_SHIP */

		0,                /* SYREEN_SHIP */
		0,                /* BLACK_URQUAN_SHIP */
		0,                /* YEHAT_REBEL_SHIP */
	};
	BYTE EncounterPercent[] =
	{
		RACE_INTERPLANETARY_PERCENT
	};

	EncounterPercent[SLYLANDRO_SHIP] *= GET_GAME_STATE (SLYLANDRO_MULTIPLIER);
	Index = GET_GAME_STATE (UTWIG_SUPOX_MISSION);
	if (Index > 1 && Index < 5)
	{
		// When the Utwig and Supox are on their mission, there won't be
		// new battle groups generated for the system.
		// Note that old groups may still exist (in which case this function
		// would not even be called), but those expire after spending a week
		// outside of the star system, or when a different star system is
		// entered.
		HomeWorld[UTWIG_SHIP] = 0;
		HomeWorld[SUPOX_SHIP] = 0;
	}

	universe = GetCurrentStar()->star_pt;
	for (hFleet = GetHeadLink (&GLOBAL (avail_race_q)), Index = 0;
			hFleet; hFleet = hNextFleet, ++Index)
	{
		COUNT i, encounter_radius;
		FLEET_INFO *FleetPtr;

		FleetPtr = LockFleetInfo (&GLOBAL (avail_race_q), hFleet);
		hNextFleet = _GetSuccLink (FleetPtr);

		if ((encounter_radius = FleetPtr->actual_strength)
				&& (i = EncounterPercent[Index]))
		{
			SIZE dx, dy;
			DWORD d_squared;
			BYTE race_enc;

			race_enc = HomeWorld[Index];
			if (race_enc && GetCurrentStar()->Index == race_enc)
			{	// In general, there are always ships at the Homeworld for
				// the races specified in HomeWorld[] array.
				BestIndex = Index;
				BestPercent = 70;
				if (race_enc == SPATHI_DEFINED || race_enc == SUPOX_DEFINED)
					BestPercent = 2;
				// Terminate the loop!
				hNextFleet = 0;

				goto FoundHome;
			}

			if (encounter_radius == INFINITE_RADIUS)
				encounter_radius = (MAX_X_UNIVERSE + 1) << 1;
			else
				encounter_radius =
						(encounter_radius * SPHERE_RADIUS_INCREMENT) >> 1;
			dx = universe.x - FleetPtr->loc.x;
			if (dx < 0)
				dx = -dx;
			dy = universe.y - FleetPtr->loc.y;
			if (dy < 0)
				dy = -dy;
			if ((COUNT)dx < encounter_radius
					&& (COUNT)dy < encounter_radius
					&& (d_squared = (DWORD)dx * dx + (DWORD)dy * dy) <
					(DWORD)encounter_radius * encounter_radius)
			{
				DWORD rand_val;

				// EncounterPercent is only used in practice for the Slylandro
				// Probes, for the rest of races the chance of encounter is
				// calced directly below from the distance to the Homeworld
				if (FleetPtr->actual_strength != INFINITE_RADIUS)
				{
					i = 70 - (COUNT)((DWORD)square_root (d_squared)
							* 60L / encounter_radius);
				}

				rand_val = TFB_Random ();
				if ((int)(LOWORD (rand_val) % 100) < (int)i
						&& (BestPercent == 0
						|| (HIWORD (rand_val) % (i + BestPercent)) < i))
				{
					if (FleetPtr->actual_strength == INFINITE_RADIUS)
					{	// The prevailing encounter chance is hereby limitted
						// to 4% for races with infinite SoI (currently, it
						// is only the Slylandro Probes)
						i = 4;
					}

					BestPercent = i;
					BestIndex = Index;
				}
			}
		}

FoundHome:
		UnlockFleetInfo (&GLOBAL (avail_race_q), hFleet);
	}

	if (BestPercent)
	{
		BYTE which_group, num_groups;
        static constexpr uqm::NavigationRules rules{};

		which_group = 0;
		num_groups = ((COUNT)TFB_Random () % (BestPercent >> 1)) + 1;
		if (num_groups > MAX_BATTLE_GROUPS)
			num_groups = MAX_BATTLE_GROUPS;
		else if (num_groups < 5
				&& (Index = HomeWorld[BestIndex])
				&& GetCurrentStar()->Index == Index)
			num_groups = 5;
		do
		{
			for (Index = rules.encounterSize.at(BestIndex).maximum; Index;
					--Index)
			{
				if (Index <= rules.encounterSize.at(BestIndex).minimum
						|| (COUNT)TFB_Random () % 100 < 50)
					CloneShipFragment (BestIndex,
							&GLOBAL (npc_built_ship_q), 0);
			}

			PutGroupInfo (GROUPS_RANDOM, ++which_group);
			ReinitQueue (&GLOBAL (npc_built_ship_q));
		} while (--num_groups);
	}

	initializeGroups (GROUPS_RANDOM, geometry, groups);
}

// C callers select the owner once. Native encounter/orbit callers use its archive.
BOOLEAN GetGroupInfo(DWORD offset, BYTE group) {
    return uqm::gameSession().groupArchive().load(offset, group) ? TRUE : FALSE;
}
DWORD PutGroupInfo(DWORD offset, BYTE group) {
    return uqm::gameSession().groupArchive().save(offset, group);
}
void InitGroupInfo(BOOLEAN firstTime) {
    uqm::gameSession().groupArchive().reset(firstTime != FALSE);
}
bool uqm::initializeGroups(DWORD offset, const SystemGeometry &geometry, QUEUE &groups) {
    return gameSession().groupArchive().initialize(offset, geometry, groups);
}

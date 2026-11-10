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

#include "genall.h"
#include "../../orbital_encounter.hpp"
#include "../../system_builders.hpp"
#include "../../game_state.hpp"
#include "../planets.h"
#include "../../build.h"
#include "../../comm.h"
#include "../../encount.h"
#include "../../starmap.h"
#include "../../globdata.h"
#include "../../ipdisp.h"
#include "../../nameref.h"
#include "../../setup.h"
#include "../../state.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class TalkingPetSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
private:
    void ZapToUrquanEncounter(void);
};
}














bool
TalkingPetSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[0].data_index = TELLURIC_WORLD;
	solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 204L / 100;
	angle = ARCTAN (solarSys->PlanetDesc[0].location.x,
			solarSys->PlanetDesc[0].location.y);
	solarSys->PlanetDesc[0].location.x =
			COSINE (angle, solarSys->PlanetDesc[0].radius);
	solarSys->PlanetDesc[0].location.y =
			SINE (angle, solarSys->PlanetDesc[0].radius);

	return true;
}

bool
TalkingPetSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET)
			&& (story_.read(StoryFlag::UmgahZombieBlobbies)
			|| !story_.read(StoryFlag::TalkingPet)
			|| StartSphereTracking (UMGAH_SHIP)))
	{
		NotifyOthers (UMGAH_SHIP, IPNL_ALL_CLEAR);
		uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

		if (StartSphereTracking (UMGAH_SHIP))
		{
			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story_.set(StoryCounter::EncounterContext, 1 << 7);
			if (!story_.read(StoryFlag::UmgahZombieBlobbies))
			{
				CloneShipFragment (UMGAH_SHIP,
						&GLOBAL (npc_built_ship_q), INFINITE_FLEET);
				communicate(UMGAH_CONVERSATION);
			}
			else
			{
				COUNT i;

				for (i = 0; i < 10; ++i)
				{
					CloneShipFragment (UMGAH_SHIP,
							&GLOBAL (npc_built_ship_q), 0);
				}
				communicate(TALKING_PET_CONVERSATION);
			}
		}

		if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
		{
			bool UmgahSurvivors;

			UmgahSurvivors = GetHeadLink (
					&GLOBAL (npc_built_ship_q)) != 0;
			GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;

			if (story_.read(StoryFlag::PlayerHypnotized))
				ZapToUrquanEncounter ();
			else if (story_.read(StoryFlag::UmgahZombieBlobbies)
					&& !UmgahSurvivors)
			{
				// Defeated the zombie fleet.
				communicate(TALKING_PET_CONVERSATION);
			}
			else if (!(StartSphereTracking (UMGAH_SHIP)))
			{
				// The Kohr-Ah have destroyed the Umgah, but the
				// talking pet survived.
				communicate(TALKING_PET_CONVERSATION);
			}

			encounter.restore();
		}

		return true;
	}

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (RUINS_STRTAB));
	}

	GenerateDefault_generateOrbital (solarSys, world);

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
		solarSys->SysInfo.PlanetInfo.Weather = 0;

	return true;
}

COUNT
TalkingPetSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	return 0;
}

bool
TalkingPetSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// Standard ruins report
		GenerateDefault_landerReportCycle (solarSys);
		return false;
	}

	(void) whichNode;
	return false;
}

void
TalkingPetSystemBehavior::ZapToUrquanEncounter(void)
{
	HENCOUNTER hEncounter;

	if ((hEncounter = AllocEncounter ()) || (hEncounter = GetHeadEncounter ()))
	{
		SIZE dx, dy;
		ENCOUNTER *EncounterPtr;
		HFLEETINFO hStarShip;
		FLEET_INFO *TemplatePtr;
		BRIEF_SHIP_INFO *BSIPtr;

		LockEncounter (hEncounter, &EncounterPtr);

		if (hEncounter == GetHeadEncounter ())
			RemoveEncounter (hEncounter);
		memset (EncounterPtr, 0, sizeof (*EncounterPtr));

		InsertEncounter (hEncounter, GetHeadEncounter ());

		hStarShip = GetStarShipFromIndex (&GLOBAL (avail_race_q), URQUAN_SHIP);
		TemplatePtr = LockFleetInfo (&GLOBAL (avail_race_q), hStarShip);
		EncounterPtr->origin = TemplatePtr->loc;
		EncounterPtr->radius = TemplatePtr->actual_strength;
		EncounterPtr->race_id = URQUAN_SHIP;
		EncounterPtr->num_ships = 1;
		EncounterPtr->flags = ONE_SHOT_ENCOUNTER;
		BSIPtr = &EncounterPtr->ShipList[0];
		BSIPtr->race_id = URQUAN_SHIP;
		BSIPtr->crew_level = TemplatePtr->crew_level;
		BSIPtr->max_crew = TemplatePtr->max_crew;
		BSIPtr->max_energy = TemplatePtr->max_energy;
		EncounterPtr->loc_pt.x = 5288;
		EncounterPtr->loc_pt.y = 4892;
		EncounterPtr->log_x = UNIVERSE_TO_LOGX (EncounterPtr->loc_pt.x);
		EncounterPtr->log_y = UNIVERSE_TO_LOGY (EncounterPtr->loc_pt.y);
		GetPlayerTravelPosition()->position.x = EncounterPtr->log_x;
		GetPlayerTravelPosition()->position.y = EncounterPtr->log_y;
		UnlockFleetInfo (&GLOBAL (avail_race_q), hStarShip);

		{
#define LOST_DAYS 15
			SleepThreadUntil (FadeScreen (FadeAllToBlack, ONE_SECOND * 2));
			MoveGameClockDays (LOST_DAYS);
		}

		GLOBAL (CurrentActivity) = MAKE_WORD (IN_HYPERSPACE, 0) | START_ENCOUNTER;

		dx = star().star_pt.x - EncounterPtr->loc_pt.x;
		dy = star().star_pt.y - EncounterPtr->loc_pt.y;
		dx = (SIZE)square_root ((long)dx * dx + (long)dy * dy)
				+ (FUEL_TANK_SCALE >> 1);

		DeltaSISGauges (0, -dx, 0);
		if (GLOBAL_SIS (FuelOnBoard) < 5 * FUEL_TANK_SCALE)
		{
			dx = ((5 + ((COUNT)TFB_Random () % 5)) * FUEL_TANK_SCALE)
					- (SIZE)GLOBAL_SIS (FuelOnBoard);
			DeltaSISGauges (0, dx, 0);
		}
		DrawSISMessage (NULL);
		DrawHyperCoords (EncounterPtr->loc_pt);

		UnlockEncounter (hEncounter);
	}
}


std::unique_ptr<SystemBehavior> createTalkingPetSystemBehavior(StoryState &story) {
    return std::make_unique<TalkingPetSystemBehavior>(story);
}
} // namespace uqm

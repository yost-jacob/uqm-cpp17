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
#include "../lander.h"
#include "../planets.h"
#include "../../build.h"
#include "../../comm.h"
#include "../../globdata.h"
#include "../../ipdisp.h"
#include "../../nameref.h"
#include "../../state.h"
#include "libs/mathlib.h"

#include <string.h>
namespace uqm {
namespace {
class DruugeSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}












bool
DruugeSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	memmove (&solarSys->PlanetDesc[1], &solarSys->PlanetDesc[0],
			sizeof (solarSys->PlanetDesc[0])
			* solarSys->SunDesc[0].NumPlanets);
	++solarSys->SunDesc[0].NumPlanets;

	solarSys->PlanetDesc[0].data_index = DUST_WORLD;
	solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 50L / 100;
	solarSys->PlanetDesc[0].NumPlanets = 0;
	angle = HALF_CIRCLE - OCTANT;
	solarSys->PlanetDesc[0].location.x =
			COSINE (angle, solarSys->PlanetDesc[0].radius);
	solarSys->PlanetDesc[0].location.y =
			SINE (angle, solarSys->PlanetDesc[0].radius);
	solarSys->PlanetDesc[0].rand_seed = MAKE_DWORD (
			solarSys->PlanetDesc[0].location.x,
			solarSys->PlanetDesc[0].location.y);

	return true;
}

bool
DruugeSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (DRUUGE_SHIP))
		{
			NotifyOthers (DRUUGE_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (DRUUGE_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story_.set(StoryCounter::EncounterContext, 1 << 7);
			communicate(DRUUGE_CONVERSATION);
			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}
			return true;
		}
		else
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (
							LoadStringTable (DRUUGE_RUINS_STRTAB));
			if (story_.read(StoryFlag::RosySphere))
			{	// Already picked up Rosy Sphere, skip the report
				solarSys->SysInfo.PlanetInfo.DiscoveryString =
						SetAbsStringTableIndex (
						solarSys->SysInfo.PlanetInfo.DiscoveryString, 1);
			}
		}
	}

	GenerateDefault_generateOrbital (solarSys, world);

	return true;
}

bool
DruugeSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		GenerateDefault_landerReportCycle (solarSys);

		// The artifact can be picked up from any ruin
		if (!story_.read(StoryFlag::RosySphere))
		{	// Just picked up the Rosy Sphere from a ruin
			SetLanderTakeoff ();

			story_.set(StoryFlag::RosySphere, (1) != 0);
			story_.set(StoryFlag::RosySphereOnShip, (1) != 0);
		}

		return false; // do not remove the node
	}

	(void) whichNode;
	return false;
}

COUNT
DruugeSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	return 0;
}


std::unique_ptr<SystemBehavior> createDruugeSystemBehavior(StoryState &story) {
    return std::make_unique<DruugeSystemBehavior>(story);
}
} // namespace uqm

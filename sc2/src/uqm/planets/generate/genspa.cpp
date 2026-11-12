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
#include "../lifeform.h"
#include "../lander.h"
#include "../planets.h"
#include "../scan.h"
#include "../../build.h"
#include "../../comm.h"
#include "../../globdata.h"
#include "../../ipdisp.h"
#include "../../nameref.h"
#include "../../state.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class SpathiSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    COUNT generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
    bool pickupLife(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}

bool
SpathiSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	PLANET_DESC *pMinPlanet;
	COUNT angle;

	pMinPlanet = &solarSys->PlanetDesc[0];
	solarSys->SunDesc[0].NumPlanets = 1;
	FillOrbits (solarSys,
			solarSys->SunDesc[0].NumPlanets, pMinPlanet, FALSE, &uqm::generationRandom(system), &uqm::systemStar(system));

	pMinPlanet->radius = EARTH_RADIUS * 1150L / 100;
	angle = ARCTAN (pMinPlanet->location.x, pMinPlanet->location.y);
	pMinPlanet->location.x = COSINE (angle, pMinPlanet->radius);
	pMinPlanet->location.y = SINE (angle, pMinPlanet->radius);
	pMinPlanet->data_index = WATER_WORLD;
	if (story_.read(StoryFlag::SpathiShieldedSelves))
		pMinPlanet->data_index |= PLANET_SHIELDED;
	pMinPlanet->NumPlanets = 1;

	return true;
}

bool
SpathiSystemBehavior::generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *planet = &body;
	COUNT angle;

	GenerateDefault_generateMoons (solarSys, planet);

	if (matchWorld (solarSys, planet, 0, MATCH_PLANET))
	{
#ifdef NOTYET
		utf8StringCopy (GLOBAL_SIS (PlanetName),
				sizeof (GLOBAL_SIS (PlanetName)),
				"Spathiwa");
#endif /* NOTYET */

		solarSys->MoonDesc[0].data_index = PELLUCID_WORLD;
		solarSys->MoonDesc[0].radius = MIN_MOON_RADIUS + MOON_DELTA;
		angle = NORMALIZE_ANGLE (LOWORD (RandomContext_Random (&uqm::generationRandom(system))));
		solarSys->MoonDesc[0].location.x =
				COSINE (angle, solarSys->MoonDesc[0].radius);
		solarSys->MoonDesc[0].location.y =
				SINE (angle, solarSys->MoonDesc[0].radius);
	}

	return true;
}

COUNT
SpathiSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, 0))
	{
		// This check is redundant since the retrieval bit will keep the
		// node from showing up again
		if (story_.read(StoryFlag::UmgahBroadcasters))
		{	// already picked up
			return 0;
		}

		return GenerateDefault_generateArtifact (solarSys, whichNode, info);
	}

	return 0;
}

bool
SpathiSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, 0))
	{
		assert (!story_.read(StoryFlag::UmgahBroadcasters) && whichNode == 0);

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		story_.set(StoryFlag::UmgahBroadcasters, (1) != 0);
		story_.set(StoryFlag::UmgahBroadcastersOnShip, (1) != 0);

		return true; // picked up
	}

	(void) whichNode;
	return false;
}

COUNT
SpathiSystemBehavior::generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		#define NUM_EVIL_ONES  32
		return GenerateRandomNodes (&solarSys->SysInfo, BIOLOGICAL_SCAN, NUM_EVIL_ONES,
				NUM_CREATURE_TYPES, whichNode, info, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
	}

	return 0;
}

bool
SpathiSystemBehavior::pickupLife(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		assert (!story_.read(StoryFlag::SpathiCreaturesEliminated) &&
				!story_.read(StoryFlag::SpathiShieldedSelves));

		story_.set(StoryFlag::SpathiCreaturesExamined, (1) != 0);
		if (countNodesRetrieved (&solarSys->SysInfo.PlanetInfo, BIOLOGICAL_SCAN)
				+ 1 == NUM_EVIL_ONES)
		{	// last creature picked up
			story_.set(StoryFlag::SpathiCreaturesEliminated, (1) != 0);
		}

		return true; // picked up
	}

	return GenerateDefault_pickupLife (solarSys, world, whichNode);
}

std::unique_ptr<SystemBehavior> createSpathiSystemBehavior(StoryState &story) {
    return std::make_unique<SpathiSystemBehavior>(story);
}
} // namespace uqm

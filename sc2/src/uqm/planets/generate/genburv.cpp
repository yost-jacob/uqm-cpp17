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
#include "../../system_builders.hpp"
#include "../../game_state.hpp"
#include "../lander.h"
#include "../planets.h"
#include "../../globdata.h"
#include "../../nameref.h"
#include "../../resinst.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class BurvixeseSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}













bool
BurvixeseSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[0].data_index = REDUX_WORLD;
	solarSys->PlanetDesc[0].NumPlanets = 1;
	solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 39L / 100;
	angle = ARCTAN (solarSys->PlanetDesc[0].location.x,
			solarSys->PlanetDesc[0].location.y);
	solarSys->PlanetDesc[0].location.x =
			COSINE (angle, solarSys->PlanetDesc[0].radius);
	solarSys->PlanetDesc[0].location.y =
			SINE (angle, solarSys->PlanetDesc[0].radius);
	return true;
}

bool
BurvixeseSystemBehavior::generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *planet = &body;
	GenerateDefault_generateMoons (solarSys, planet);

	if (matchWorld (solarSys, planet, 0, MATCH_PLANET))
	{
		COUNT angle;
		DWORD rand_val;

		solarSys->MoonDesc[0].data_index = SELENIC_WORLD;
		solarSys->MoonDesc[0].radius = MIN_MOON_RADIUS
				+ (MAX_MOONS - 1) * MOON_DELTA;
		rand_val = RandomContext_Random (&SysGenRNG);
		angle = NORMALIZE_ANGLE (LOWORD (rand_val));
		solarSys->MoonDesc[0].location.x =
				COSINE (angle, solarSys->MoonDesc[0].radius);
		solarSys->MoonDesc[0].location.y =
				SINE (angle, solarSys->MoonDesc[0].radius);
	}
	return true;
}

bool
BurvixeseSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	DWORD rand_val;

	DoPlanetaryAnalysis (&solarSys->SysInfo, world);
	rand_val = RandomContext_GetSeed (&SysGenRNG);

	solarSys->SysInfo.PlanetInfo.ScanSeed[BIOLOGICAL_SCAN] = rand_val;
	GenerateLifeForms (&solarSys->SysInfo, GENERATE_ALL, NULL);
	rand_val = RandomContext_GetSeed (&SysGenRNG);

	solarSys->SysInfo.PlanetInfo.ScanSeed[MINERAL_SCAN] = rand_val;
	GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL);

	solarSys->SysInfo.PlanetInfo.ScanSeed[ENERGY_SCAN] = rand_val;

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->PlanetSideFrame[1] =
				CaptureDrawable (
				LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (
						LoadStringTable (BURV_RUINS_STRTAB));
		solarSys->SysInfo.PlanetInfo.Weather = 0;
		solarSys->SysInfo.PlanetInfo.Tectonics = 0;
	}
	else if (matchWorld (solarSys, world, 0, 0)
			&& !story_.read(StoryFlag::BurvixeseBroadcasters))
	{
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->PlanetSideFrame[1] = CaptureDrawable (
				LoadGraphic (BURV_BCS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (BURV_BCS_STRTAB));
	}

	LoadPlanet (NULL);

	return true;
}

COUNT
BurvixeseSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	if (matchWorld (solarSys, world, 0, 0))
	{
		// This check is redundant since the retrieval bit will keep the
		// node from showing up again
		if (story_.read(StoryFlag::BurvixeseBroadcasters))
		{	// already picked up
			return 0;
		}

		return GenerateDefault_generateArtifact (solarSys, whichNode, info);
	}

	return 0;
}

bool
BurvixeseSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// Standard ruins report
		GenerateDefault_landerReportCycle (solarSys);
		return false;
	}

	if (matchWorld (solarSys, world, 0, 0))
	{
		assert (!story_.read(StoryFlag::BurvixeseBroadcasters) && whichNode == 0);

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		story_.set(StoryFlag::BurvixeseBroadcasters, (1) != 0);
		story_.set(StoryFlag::BurvBroadcastersOnShip, (1) != 0);

		return true; // picked up
	}

	(void) whichNode;
	return false;
}

std::unique_ptr<SystemBehavior> createBurvixeseSystemBehavior(StoryState &story) {
    return std::make_unique<BurvixeseSystemBehavior>(story);
}
} // namespace uqm

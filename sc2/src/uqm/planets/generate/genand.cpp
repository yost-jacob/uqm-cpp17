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
#include "../scan.h"
#include "../../globdata.h"
#include "../../state.h"
#include "../../world_c.h"
#include "../../system_runtime.hpp"
#include "../../starmap.h"
#include "../../nameref.h"
#include "../../resinst.h"
#include "../../sounds.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class AndrosynthSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}












bool
AndrosynthSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[1].data_index = TELLURIC_WORLD;
	solarSys->PlanetDesc[1].radius = EARTH_RADIUS * 204L / 100;
	angle = ARCTAN (solarSys->PlanetDesc[1].location.x,
			solarSys->PlanetDesc[1].location.y);
	solarSys->PlanetDesc[1].location.x =
			COSINE (angle, solarSys->PlanetDesc[1].radius);
	solarSys->PlanetDesc[1].location.y =
			SINE (angle, solarSys->PlanetDesc[1].radius);

	return true;
}

bool
AndrosynthSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
        COUNT visits = story_.discovery(solarSys->worlds->system->world(1).id()).visitedSites.count();

		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (
				LoadStringTable (ANDROSYNTH_RUINS_STRTAB));
        ConfigurePersonalEnergyBehavior (solarSys, 1, -1, NUM_RACE_RUINS,
                GetStringTableCount (solarSys->SysInfo.PlanetInfo.DiscoveryString));
        // The site remains in the shared world. This player's accepted visits
        // select the next report independently of physical retrieval masks.
		if (visits >= GetStringTableCount (
				solarSys->SysInfo.PlanetInfo.DiscoveryString))
		{	// All the reports were already given
			DestroyStringTable (ReleaseStringTable (
					solarSys->SysInfo.PlanetInfo.DiscoveryString));
			solarSys->SysInfo.PlanetInfo.DiscoveryString = 0;
		}
		else
		{	// Advance the report sequence to the first unread
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					SetRelStringTableIndex (
					solarSys->SysInfo.PlanetInfo.DiscoveryString, visits);
		}
	}

	GenerateDefault_generateOrbital (solarSys, world);

	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
		solarSys->SysInfo.PlanetInfo.AtmoDensity =
				EARTH_ATMOSPHERE * 144 / 100;
		solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 28;
		solarSys->SysInfo.PlanetInfo.Weather = 1;
		solarSys->SysInfo.PlanetInfo.Tectonics = 1;
	}

	return true;
}

bool
AndrosynthSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
        ENERGY_INTERACTION result = InteractWorldEnergy (solarSys, 1, -1, whichNode, FALSE);
        if (!result.accepted || !result.hasReport)
            return false;
        solarSys->SysInfo.PlanetInfo.DiscoveryString = SetAbsStringTableIndex (
                solarSys->SysInfo.PlanetInfo.DiscoveryString, result.report);

		// Androsynth ruins have several lander reports which form a story
		GenerateDefault_landerReportCycle (solarSys);

		return false; // do not remove the node from the surface
	}

	return false;
}

COUNT
AndrosynthSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	return 0;
}

std::unique_ptr<SystemBehavior> createAndrosynthSystemBehavior(StoryState &story) {
    return std::make_unique<AndrosynthSystemBehavior>(story);
}
} // namespace uqm

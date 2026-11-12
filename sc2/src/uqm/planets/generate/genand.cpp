#include "../../world_visit.hpp"
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
AndrosynthSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
        ENERGY_INTERACTION result = InteractWorldEnergy (solarSys, 1, -1, whichNode, FALSE);
        if (!result.accepted || !result.hasReport)
            return false;
        uqm::systemVisit(*solarSys).report().select(result.report);

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

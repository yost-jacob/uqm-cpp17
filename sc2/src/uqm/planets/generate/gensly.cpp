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
#include "../planets.h"
#include "../../comm.h"
namespace uqm {
namespace {
class SlylandroSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
};
}










bool
SlylandroSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[3].data_index = RED_GAS_GIANT;
	solarSys->PlanetDesc[3].NumPlanets = 1;

	return true;
}

bool
SlylandroSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 3, MATCH_PLANET))
	{
		communicate(SLYLANDRO_HOME_CONVERSATION);
		return true;
	}

	GenerateDefault_generateOrbital (solarSys, world);
	return true;
}


std::unique_ptr<SystemBehavior> createSlylandroSystemBehavior(StoryState &story) {
    return std::make_unique<SlylandroSystemBehavior>(story);
}
} // namespace uqm

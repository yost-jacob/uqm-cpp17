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
class SyreenSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body) override;
};
}

bool
SyreenSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[0].data_index = WATER_WORLD | PLANET_SHIELDED;
	solarSys->PlanetDesc[0].NumPlanets = 1;

	return true;
}

bool
SyreenSystemBehavior::generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *planet = &body;
	GenerateDefault_generateMoons (solarSys, planet);

	if (matchWorld (solarSys, planet, 0, MATCH_PLANET))
	{
		solarSys->MoonDesc[0].data_index = HIERARCHY_STARBASE;
		solarSys->MoonDesc[0].radius = MIN_MOON_RADIUS;
		solarSys->MoonDesc[0].location.x =
				COSINE (QUADRANT, solarSys->MoonDesc[0].radius);
		solarSys->MoonDesc[0].location.y =
				SINE (QUADRANT, solarSys->MoonDesc[0].radius);
	}

	return true;
}

std::unique_ptr<SystemBehavior> createSyreenSystemBehavior(StoryState &story) {
    return std::make_unique<SyreenSystemBehavior>(story);
}
} // namespace uqm

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
#include "../../gendef.h"
#include "../../starmap.h"
#include "../../globdata.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class RainbowWorldSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
};
}










bool
RainbowWorldSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[0].data_index = RAINBOW_WORLD;
	solarSys->PlanetDesc[0].NumPlanets = 0;
	solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 50L / 100;
	angle = ARCTAN (solarSys->PlanetDesc[0].location.x,
			solarSys->PlanetDesc[0].location.y);
	if (angle <= QUADRANT)
		angle += QUADRANT;
	else if (angle >= FULL_CIRCLE - QUADRANT)
		angle -= QUADRANT;
	solarSys->PlanetDesc[0].location.x =
			COSINE (angle, solarSys->PlanetDesc[0].radius);
	solarSys->PlanetDesc[0].location.y =
			SINE (angle, solarSys->PlanetDesc[0].radius);

	return true;
}

bool
RainbowWorldSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		BYTE which_rainbow;
		UWORD rainbow_mask;
		STAR_DESC *SDPtr;

		rainbow_mask = MAKE_WORD (
				campaign_.read(StoryCounter::RainbowWorld0),
				campaign_.read(StoryCounter::RainbowWorld1));

		which_rainbow = 0;
		SDPtr = GetStar(0);
		while (SDPtr != &star())
		{
			if (SDPtr->Index == RAINBOW_DEFINED)
				++which_rainbow;
			SDPtr = GetStar(GetStarIndex(SDPtr) + 1);
		}
		rainbow_mask |= 1 << which_rainbow;
		campaign_.set(StoryCounter::RainbowWorld0, LOBYTE (rainbow_mask));
		campaign_.set(StoryCounter::RainbowWorld1, HIBYTE (rainbow_mask));
	}

	GenerateDefault_generateOrbital (solarSys, world);
	return true;
}


std::unique_ptr<SystemBehavior> createRainbowWorldSystemBehavior(CampaignState &campaign) {
    return std::make_unique<RainbowWorldSystemBehavior>(campaign);
}
} // namespace uqm

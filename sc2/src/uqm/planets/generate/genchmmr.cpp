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
#include "../../system_behavior.hpp"
#include "../../game_state.hpp"
#include "../lander.h"
#include "../planets.h"
#include "../../build.h"
#include "../../comm.h"
#include "../../globdata.h"
#include "../../nameref.h"
#include "../../setup.h"
#include "../../sounds.h"
#include "../../state.h"
#include "libs/mathlib.h"

namespace uqm {
namespace {
class ChmmrSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &planet) override;
};
}
bool
ChmmrSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
	GenerateDefault_generatePlanets (&system);

	system.PlanetDesc[1].data_index = SAPPHIRE_WORLD;
	if (!story_.read(StoryFlag::ChmmrFreed))
		system.PlanetDesc[1].data_index |= PLANET_SHIELDED;
	system.PlanetDesc[1].NumPlanets = 1;

	return true;
}

bool
ChmmrSystemBehavior::generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body)
{
	GenerateDefault_generateMoons (&system, &body);

	if (matchWorld (&system, &body, 1, MATCH_PLANET))
	{
		COUNT angle;
		DWORD rand_val;

		system.MoonDesc[0].data_index = HIERARCHY_STARBASE;
		system.MoonDesc[0].radius = MIN_MOON_RADIUS;
		rand_val = RandomContext_Random (&SysGenRNG);
		angle = NORMALIZE_ANGLE (LOWORD (rand_val));
		system.MoonDesc[0].location.x =
				COSINE (angle, system.MoonDesc[0].radius);
		system.MoonDesc[0].location.y =
				SINE (angle, system.MoonDesc[0].radius);
	}

	return true;
}

std::unique_ptr<SystemBehavior> createChmmrSystemBehavior(StoryState &story) {
    return std::make_unique<ChmmrSystemBehavior>(story);
}
} // namespace uqm

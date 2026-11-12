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
#include "../lifeform.h"
#include "../planets.h"
#include "../../build.h"
#include "../../comm.h"
#include "../../gendef.h"
#include "../../starmap.h"
#include "../../globdata.h"
#include "../../ipdisp.h"
#include "../../nameref.h"
#include "../../setup.h"
#include "../../sounds.h"
#include "../../state.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class VuxSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    COUNT generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
    bool pickupLife(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}

bool
VuxSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	if (star().Index == MAIDENS_DEFINED)
	{
		GenerateDefault_generatePlanets (solarSys);
				// XXX: this is the second time that this function is
				// called. Is it safe to remove one, or does this change
				// the RNG so that the outcome is different?
		solarSys->PlanetDesc[0].data_index = REDUX_WORLD;
		solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 212L / 100;
		angle = ARCTAN (solarSys->PlanetDesc[0].location.x,
				solarSys->PlanetDesc[0].location.y);
		solarSys->PlanetDesc[0].location.x =
				COSINE (angle, solarSys->PlanetDesc[0].radius);
		solarSys->PlanetDesc[0].location.y =
				SINE (angle, solarSys->PlanetDesc[0].radius);
	}
	else
	{
		if (star().Index == VUX_DEFINED)
		{
			solarSys->PlanetDesc[0].data_index = REDUX_WORLD;
			solarSys->PlanetDesc[0].NumPlanets = 1;
			solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 42L / 100;
			angle = HALF_CIRCLE + OCTANT;
		}
		else /* if (star().Index == VUX_BEAST_DEFINED) */
		{
			memmove (&solarSys->PlanetDesc[1], &solarSys->PlanetDesc[0],
					sizeof (solarSys->PlanetDesc[0])
					* solarSys->SunDesc[0].NumPlanets);
			++solarSys->SunDesc[0].NumPlanets;

			angle = HALF_CIRCLE - OCTANT;
			solarSys->PlanetDesc[0].data_index = WATER_WORLD;
			solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 110L / 100;
			solarSys->PlanetDesc[0].NumPlanets = 0;
		}

		solarSys->PlanetDesc[0].location.x =
				COSINE (angle, solarSys->PlanetDesc[0].radius);
		solarSys->PlanetDesc[0].location.y =
				SINE (angle, solarSys->PlanetDesc[0].radius);
		solarSys->PlanetDesc[0].rand_seed = MAKE_DWORD (
				solarSys->PlanetDesc[0].location.x,
				solarSys->PlanetDesc[0].location.y);
	}
	return true;
}

COUNT
VuxSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (star().Index == MAIDENS_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// This check is redundant since the retrieval bit will keep the
		// node from showing up again
		if (story_.read(StoryFlag::ShofixtiMaidens))
		{	// already picked up
			return 0;
		}

		if (info)
		{
			info->loc_pt.x = MAP_WIDTH / 3;
			info->loc_pt.y = MAP_HEIGHT * 5 / 8;
		}

		return 1; // only matters when count is requested
	}

	if (star().Index == VUX_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	return 0;
}

bool
VuxSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (star().Index == MAIDENS_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		assert (!story_.read(StoryFlag::ShofixtiMaidens) && whichNode == 0);

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		story_.set(StoryFlag::ShofixtiMaidens, (1) != 0);
		story_.set(StoryFlag::MaidensOnShip, (1) != 0);

		return true; // picked up
	}

	if (star().Index == VUX_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// Standard ruins report
		GenerateDefault_landerReportCycle (solarSys);
		return false;
	}

	(void) whichNode;
	return false;
}

COUNT
VuxSystemBehavior::generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (star().Index == MAIDENS_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		static const SBYTE life[] =
		{
			 9,  9,  9,  9, /* Carousel Beast */
			14, 14, 14, 14, /* Amorphous Trandicula */
			18, 18, 18, 18, /* Penguin Cyclops */
			-1 /* term */
		};
		return GeneratePresetLife (&solarSys->SysInfo, life, whichNode, info, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
	}

	if (star().Index == VUX_BEAST_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		static const SBYTE life[] =
		{
			NUM_CREATURE_TYPES + 2, /* VUX Beast */
					// Must be the first node, see pickupLife() below
			3, 3, 3, 3, 3, /* Whackin' Bush */
			8, 8, 8, 8, 8, /* Glowing Medusa */
			-1 /* term */
		};
		return GeneratePresetLife (&solarSys->SysInfo, life, whichNode, info, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
	}

	return GenerateDefault_generateLife (solarSys, world, whichNode, info);
}

bool
VuxSystemBehavior::pickupLife(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (star().Index == VUX_BEAST_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (whichNode == 0)
		{	// Picked up Zex' Beauty
			assert (!story_.read(StoryFlag::VuxBeast));

			GenerateDefault_landerReport (solarSys);
			SetLanderTakeoff ();

			story_.set(StoryFlag::VuxBeast, (1) != 0);
			story_.set(StoryFlag::VuxBeastOnShip, (1) != 0);
		}

		return true; // picked up
	}

	return GenerateDefault_pickupLife (solarSys, world, whichNode);
}

std::unique_ptr<SystemBehavior> createVuxSystemBehavior(StoryState &story) {
    return std::make_unique<VuxSystemBehavior>(story);
}
} // namespace uqm

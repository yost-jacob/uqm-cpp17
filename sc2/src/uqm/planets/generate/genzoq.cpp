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
#include "../../build.h"
#include "../../comm.h"
#include "../../globdata.h"
#include "../../nameref.h"
#include "../../state.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class ZoqFotPikSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool initNpcs(SOLARSYS_STATE &system) override;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}













bool
ZoqFotPikSystemBehavior::initNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	if (campaign_.read(StoryCounter::ZoqfotDistress) != 1)
		GenerateDefault_initNpcs (solarSys);

	return true;
}

bool
ZoqFotPikSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[0].data_index = REDUX_WORLD;
	solarSys->PlanetDesc[0].NumPlanets = 1;
	solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 138L / 100;
	angle = ARCTAN (solarSys->PlanetDesc[0].location.x,
			solarSys->PlanetDesc[0].location.y);
	solarSys->PlanetDesc[0].location.x =
			COSINE (angle, solarSys->PlanetDesc[0].radius);
	solarSys->PlanetDesc[0].location.y =
			SINE (angle, solarSys->PlanetDesc[0].radius);

	return true;
}

bool
ZoqFotPikSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (ZOQFOTPIK_SHIP))
		{
			PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
			ReinitQueue (&GLOBAL (ip_group_q));
			assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

			if (campaign_.read(StoryCounter::ZoqfotDistress))
			{
				CloneShipFragment (BLACK_URQUAN_SHIP,
						&GLOBAL (npc_built_ship_q), 0);

				GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
				campaign_.set(StoryCounter::EncounterContext, 1 << 7);
				InitCommunication (BLACKURQ_CONVERSATION);

				if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
					return true;

				if (GetHeadLink (&GLOBAL (npc_built_ship_q)))
				{
					GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
					ReinitQueue (&GLOBAL (npc_built_ship_q));
					GetGroupInfo (GROUPS_RANDOM, GROUP_LOAD_IP);
					return true;
				}
			}

			CloneShipFragment (ZOQFOTPIK_SHIP, &GLOBAL (npc_built_ship_q),
					INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			campaign_.set(StoryCounter::EncounterContext, 1 << 7);
			InitCommunication (ZOQFOTPIK_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				ReinitQueue (&GLOBAL (npc_built_ship_q));
				GetGroupInfo (GROUPS_RANDOM, GROUP_LOAD_IP);
			}

			return true;
		}

		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (RUINS_STRTAB));
	}

	GenerateDefault_generateOrbital (solarSys, world);
	return true;
}

COUNT
ZoqFotPikSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	return 0;
}

bool
ZoqFotPikSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// Standard ruins report
		GenerateDefault_landerReportCycle (solarSys);
		return false;
	}

	(void) whichNode;
	return false;
}

std::unique_ptr<SystemBehavior> createZoqFotPikSystemBehavior(CampaignState &campaign) {
    return std::make_unique<ZoqFotPikSystemBehavior>(campaign);
}
} // namespace uqm

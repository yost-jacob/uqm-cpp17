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
#include "../planets.h"
#include "../../build.h"
#include "../../comm.h"
#include "../../gendef.h"
#include "../../starmap.h"
#include "../../globdata.h"
#include "../../ipdisp.h"
#include "../../nameref.h"
#include "../../setup.h"
#include "../../state.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class OrzSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}












bool
OrzSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	if (star().Index == ORZ_DEFINED)
	{
		solarSys->PlanetDesc[0].data_index = WATER_WORLD;
		solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 156L / 100;
		solarSys->PlanetDesc[0].NumPlanets = 0;
		angle = ARCTAN (solarSys->PlanetDesc[0].location.x,
				solarSys->PlanetDesc[0].location.y);
		solarSys->PlanetDesc[0].location.x =
				COSINE (angle, solarSys->PlanetDesc[0].radius);
		solarSys->PlanetDesc[0].location.y =
				SINE (angle, solarSys->PlanetDesc[0].radius);
	}

	return true;
}

bool
OrzSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if ((star().Index == ORZ_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
			|| (star().Index == TAALO_PROTECTOR_DEFINED
			&& matchWorld (solarSys, world, 1, 2)
			&& !campaign_.read(StoryFlag::TaaloProtector)))
	{
		COUNT i;

		if ((star().Index == ORZ_DEFINED
				|| !campaign_.read(StoryFlag::TaaloUnprotected))
				&& StartSphereTracking (ORZ_SHIP))
		{
			NotifyOthers (ORZ_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			if (star().Index == ORZ_DEFINED)
			{
				CloneShipFragment (ORZ_SHIP,
						&GLOBAL (npc_built_ship_q), INFINITE_FLEET);
				campaign_.set(StoryCounter::EncounterContext, 1 << 7);
			}
			else
			{
				for (i = 0; i < 14; ++i)
				{
					CloneShipFragment (ORZ_SHIP,
							&GLOBAL (npc_built_ship_q), 0);
				}
				campaign_.set(StoryCounter::EncounterContext, 1 << 6);
			}
			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			communicate(ORZ_CONVERSATION);

			if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
				return true;

			{
				bool OrzSurvivors;

				OrzSurvivors = GetHeadLink (&GLOBAL (npc_built_ship_q))
						&& (star().Index == ORZ_DEFINED
						|| !campaign_.read(StoryFlag::TaaloUnprotected));

				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();

				if (OrzSurvivors)
					return true;

				RepairSISBorder ();
			}
		}

		campaign_.set(StoryFlag::TaaloUnprotected, (1) != 0);
		if (star().Index == TAALO_PROTECTOR_DEFINED)
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->PlanetSideFrame[1] =
					CaptureDrawable (
					LoadGraphic (TAALO_DEVICE_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (
					LoadStringTable (TAALO_DEVICE_STRTAB));
		}
		else
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (RUINS_STRTAB));
		}
	}

	GenerateDefault_generateOrbital (solarSys, world);

	return true;
}

COUNT
OrzSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (star().Index == TAALO_PROTECTOR_DEFINED
			&& matchWorld (solarSys, world, 1, 2))
	{
		// This check is redundant since the retrieval bit will keep the
		// node from showing up again
		if (campaign_.read(StoryFlag::TaaloProtector))
		{	// already picked up
			return 0;
		}

		return GenerateDefault_generateArtifact (solarSys, whichNode, info);
	}

	if (star().Index == ORZ_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	return 0;
}

bool
OrzSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (star().Index == TAALO_PROTECTOR_DEFINED
			&& matchWorld (solarSys, world, 1, 2))
	{
		assert (!campaign_.read(StoryFlag::TaaloProtector) && whichNode == 0);

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		campaign_.set(StoryFlag::TaaloProtector, (1) != 0);
		campaign_.set(StoryFlag::TaaloProtectorOnShip, (1) != 0);

		return true; // picked up
	}

	if (star().Index == ORZ_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// Standard ruins report
		GenerateDefault_landerReportCycle (solarSys);
		return false;
	}

	(void) whichNode;
	return false;
}

std::unique_ptr<SystemBehavior> createOrzSystemBehavior(CampaignState &campaign) {
    return std::make_unique<OrzSystemBehavior>(campaign);
}
} // namespace uqm

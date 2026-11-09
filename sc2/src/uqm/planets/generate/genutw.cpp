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
class UtwigSystemBehavior final : public SystemBehavior {
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
UtwigSystemBehavior::initNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	if (CurStarDescPtr->Index == BOMB_DEFINED
			&& !campaign_.read(StoryFlag::UtwigBomb))
	{
		ReinitQueue (&GLOBAL (ip_group_q));
		assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);
	}
	else
	{
		GenerateDefault_initNpcs (solarSys);
	}

	return true;
}

bool
UtwigSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	if (CurStarDescPtr->Index == UTWIG_DEFINED)
	{
		solarSys->PlanetDesc[0].data_index = WATER_WORLD;
		solarSys->PlanetDesc[0].NumPlanets = 1;
		solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 174L / 100;
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
UtwigSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if ((CurStarDescPtr->Index == UTWIG_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
			|| (CurStarDescPtr->Index == BOMB_DEFINED
			&& matchWorld (solarSys, world, 5, 1)
			&& !campaign_.read(StoryFlag::UtwigBomb)))
	{
		if ((CurStarDescPtr->Index == UTWIG_DEFINED
				|| !campaign_.read(StoryFlag::UtwigHaveUltron))
				&& StartSphereTracking (UTWIG_SHIP))
		{
			NotifyOthers (UTWIG_SHIP, IPNL_ALL_CLEAR);
			PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
			ReinitQueue (&GLOBAL (ip_group_q));
			assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

			CloneShipFragment (UTWIG_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			if (CurStarDescPtr->Index == UTWIG_DEFINED)
			{
				campaign_.set(StoryCounter::EncounterContext, 1 << 7);
			}
			else
			{
				campaign_.set(StoryCounter::EncounterContext, 1 << 6);
			}
			InitCommunication (UTWIG_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				ReinitQueue (&GLOBAL (npc_built_ship_q));
				GetGroupInfo (GROUPS_RANDOM, GROUP_LOAD_IP);
			}
			return true;
		}

		if (CurStarDescPtr->Index == BOMB_DEFINED
				&& !campaign_.read(StoryFlag::BombUnprotected)
				&& StartSphereTracking (DRUUGE_SHIP))
		{
			COUNT i;

			PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
			ReinitQueue (&GLOBAL (ip_group_q));
			assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

			for (i = 0; i < 5; ++i)
			{
				CloneShipFragment (DRUUGE_SHIP,
						&GLOBAL (npc_built_ship_q), 0);
			}
			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			campaign_.set(StoryCounter::EncounterContext, 1 << 6);
			InitCommunication (DRUUGE_CONVERSATION);

			if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
				return true;

			{
				bool DruugeSurvivors;

				DruugeSurvivors =
						GetHeadLink (&GLOBAL (npc_built_ship_q)) != 0;

				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				ReinitQueue (&GLOBAL (npc_built_ship_q));
				GetGroupInfo (GROUPS_RANDOM, GROUP_LOAD_IP);

				if (DruugeSurvivors)
					return true;

				RepairSISBorder ();
				campaign_.set(StoryFlag::BombUnprotected, (1) != 0);
			}
		}

		if (CurStarDescPtr->Index == BOMB_DEFINED)
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (BOMB_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (BOMB_STRTAB));
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

	if (CurStarDescPtr->Index == UTWIG_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		solarSys->SysInfo.PlanetInfo.Weather = 1;
		solarSys->SysInfo.PlanetInfo.Tectonics = 1;
	}

	return true;
}

COUNT
UtwigSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (CurStarDescPtr->Index == UTWIG_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	if (CurStarDescPtr->Index == BOMB_DEFINED
			&& matchWorld (solarSys, world, 5, 1))
	{
		// This check is redundant since the retrieval bit will keep the
		// node from showing up again
		if (campaign_.read(StoryFlag::UtwigBomb))
		{	// already picked up
			return 0;
		}

		return GenerateDefault_generateArtifact (solarSys, whichNode, info);
	}

	return 0;
}

bool
UtwigSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (CurStarDescPtr->Index == UTWIG_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// Standard ruins report
		GenerateDefault_landerReportCycle (solarSys);
		return false;
	}

	if (CurStarDescPtr->Index == BOMB_DEFINED
			&& matchWorld (solarSys, world, 5, 1))
	{
		assert (!campaign_.read(StoryFlag::UtwigBomb) && whichNode == 0);

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		campaign_.set(StoryFlag::UtwigBomb, (1) != 0);
		campaign_.set(StoryFlag::UtwigBombOnShip, (1) != 0);
		campaign_.set(StoryFlag::DruugeManner, (1) != 0);
		campaign_.set(StoryCounter::DruugeVisits, 0);
		campaign_.set(StoryCounter::DruugeHomeVisits, 0);

		return true; // picked up
	}

	(void) whichNode;
	return false;
}

std::unique_ptr<SystemBehavior> createUtwigSystemBehavior(CampaignState &campaign) {
    return std::make_unique<UtwigSystemBehavior>(campaign);
}
} // namespace uqm

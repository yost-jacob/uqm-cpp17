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
#include "../scan.h"
#include "../../build.h"
#include "../../comm.h"
#include "../../gendef.h"
#include "../../starmap.h"
#include "../../globdata.h"
#include "../../world_c.h"
#include "../../ipdisp.h"
#include "../../nameref.h"
#include "../../setup.h"
#include "../../state.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class MyconSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    COUNT generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
};
}













bool
MyconSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[0].data_index = SHATTERED_WORLD;
	solarSys->PlanetDesc[0].radius = EARTH_RADIUS * 80L / 100;
	if (solarSys->PlanetDesc[0].NumPlanets > 2)
		solarSys->PlanetDesc[0].NumPlanets = 2;
	angle = ARCTAN (
			solarSys->PlanetDesc[0].location.x,
			solarSys->PlanetDesc[0].location.y);
	solarSys->PlanetDesc[0].location.x =
			COSINE (angle, solarSys->PlanetDesc[0].radius);
	solarSys->PlanetDesc[0].location.y =
			SINE (angle, solarSys->PlanetDesc[0].radius);

	return true;
}

bool
MyconSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
        if (star().Index == SUN_DEVICE_DEFINED
                || star().Index == EGG_CASE0_DEFINED
                || star().Index == EGG_CASE1_DEFINED
                || star().Index == EGG_CASE2_DEFINED)
            ConfigureArtifactEnergyBehavior (solarSys, 0, -1);

		if ((star().Index == MYCON_DEFINED
				|| star().Index == SUN_DEVICE_DEFINED)
				&& StartSphereTracking (MYCON_SHIP))
		{
			if (star().Index == MYCON_DEFINED
					|| !story_.read(StoryFlag::SunDeviceUnguarded))
			{
				NotifyOthers (MYCON_SHIP, IPNL_ALL_CLEAR);
				uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

				if (star().Index == MYCON_DEFINED
						|| !story_.read(StoryFlag::MyconFellForAmbush))
				{
					CloneShipFragment (MYCON_SHIP,
							&GLOBAL (npc_built_ship_q), INFINITE_FLEET);
				}
				else
				{
					COUNT i;

					for (i = 0; i < 5; ++i)
						CloneShipFragment (MYCON_SHIP,
								&GLOBAL (npc_built_ship_q), 0);
				}

				GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
				if (star().Index == MYCON_DEFINED)
				{
					story_.set(StoryCounter::EncounterContext, 1 << 7);
				}
				else
				{
					story_.set(StoryCounter::EncounterContext, 1 << 6);
				}
				communicate(MYCON_CONVERSATION);

				if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
					return true;

				{
					bool MyconSurvivors;

					MyconSurvivors =
							GetHeadLink (&GLOBAL (npc_built_ship_q)) != 0;

					GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
					encounter.restore();

					if (MyconSurvivors)
						return true;

					story_.set(StoryFlag::SunDeviceUnguarded, (1) != 0);
					RepairSISBorder ();
				}
			}
		}

		switch (star().Index)
		{
			case SUN_DEVICE_DEFINED:
				if (!story_.read(StoryFlag::SunDevice))
				{
					LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
					solarSys->graphics->PlanetSideFrame[1] =
							CaptureDrawable (
									LoadGraphic (SUN_DEVICE_MASK_PMAP_ANIM));
					solarSys->SysInfo.PlanetInfo.DiscoveryString =
							CaptureStringTable (
									LoadStringTable (SUN_DEVICE_STRTAB));
				}
				break;
			case EGG_CASE0_DEFINED:
			case EGG_CASE1_DEFINED:
			case EGG_CASE2_DEFINED:
				if (story_.read(StoryCounter::KnowAboutShattered) == 0)
					story_.set(StoryCounter::KnowAboutShattered, 1);

				if (!isNodeRetrieved (&solarSys->SysInfo.PlanetInfo,
						ENERGY_SCAN, 0))
				{
					LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
					solarSys->graphics->PlanetSideFrame[1] =
							CaptureDrawable (
									LoadGraphic (EGG_CASE_MASK_PMAP_ANIM));
					solarSys->SysInfo.PlanetInfo.DiscoveryString =
							CaptureStringTable (
									LoadStringTable (EGG_CASE_STRTAB));
				}
				break;
		}
	}

	GenerateDefault_generateOrbital (solarSys, world);
	return true;
}

COUNT
MyconSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (star().Index == SUN_DEVICE_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// This check is redundant since the retrieval bit will keep the
		// node from showing up again
		if (story_.read(StoryFlag::SunDevice))
		{	// already picked up
			return 0;
		}

		return GenerateDefault_generateArtifact (solarSys, whichNode, info);
	}

	if ((star().Index == EGG_CASE0_DEFINED
			|| star().Index == EGG_CASE1_DEFINED
			|| star().Index == EGG_CASE2_DEFINED)
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		// This check is redundant since the retrieval bit will keep the
		// node from showing up again
		// XXX: DiscoveryString is set by generateOrbital() only when the
		//   node has not been picked up yet
		if (!solarSys->SysInfo.PlanetInfo.DiscoveryString)
		{	// already picked up
			return 0;
		}

		return GenerateDefault_generateArtifact (solarSys, whichNode, info);
	}

	return 0;
}

bool
MyconSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (star().Index == SUN_DEVICE_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		assert (!story_.read(StoryFlag::SunDevice) && whichNode == 0);

        ENERGY_INTERACTION result = InteractWorldEnergy (solarSys, 0, -1, whichNode,
                isNodeRetrieved (&solarSys->SysInfo.PlanetInfo, ENERGY_SCAN, whichNode) ? TRUE : FALSE);
        if (!result.accepted)
            return false;

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		story_.set(StoryFlag::SunDevice, (1) != 0);
		story_.set(StoryFlag::SunDeviceOnShip, (1) != 0);
		story_.set(StoryCounter::MyconVisits, 0);

        return result.removeNode; // Physical retrieval is committed by the lander.
	}

	if ((star().Index == EGG_CASE0_DEFINED
			|| star().Index == EGG_CASE1_DEFINED
			|| star().Index == EGG_CASE2_DEFINED)
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		assert (whichNode == 0);

        ENERGY_INTERACTION result = InteractWorldEnergy (solarSys, 0, -1, whichNode,
                isNodeRetrieved (&solarSys->SysInfo.PlanetInfo, ENERGY_SCAN, whichNode) ? TRUE : FALSE);
        if (!result.accepted)
            return false;

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		switch (star().Index)
		{
			case EGG_CASE0_DEFINED:
				story_.set(StoryFlag::EggCase0OnShip, (1) != 0);
				break;
			case EGG_CASE1_DEFINED:
				story_.set(StoryFlag::EggCase1OnShip, (1) != 0);
				break;
			case EGG_CASE2_DEFINED:
				story_.set(StoryFlag::EggCase2OnShip, (1) != 0);
				break;
		}

        return result.removeNode; // Physical retrieval is committed by the lander.
	}

	(void) whichNode;
	return false;
}

COUNT
MyconSystemBehavior::generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	// Gee, I wonder why there isn't any life in Mycon systems...
	(void) whichNode;
	(void) solarSys;
	(void) world;
	(void) info;
	return 0;
}


std::unique_ptr<SystemBehavior> createMyconSystemBehavior(StoryState &story) {
    return std::make_unique<MyconSystemBehavior>(story);
}
} // namespace uqm

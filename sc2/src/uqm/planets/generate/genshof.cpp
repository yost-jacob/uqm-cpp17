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
#include "../../build.h"
#include "../../globdata.h"
#include "../../grpinfo.h"
#include "../../state.h"
#include "../planets.h"
namespace uqm {
namespace {
class ShofixtiSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool initNpcs(SOLARSYS_STATE &system) override;
    bool reinitNpcs(SOLARSYS_STATE &system) override;
    bool uninitNpcs(SOLARSYS_STATE &system) override;
    bool generatePlanets(SOLARSYS_STATE &system) override;
private:
    void check_old_shofixti(void);
};
}














bool
ShofixtiSystemBehavior::initNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	if (!story_.read(StoryFlag::ShofixtiRecruited)
			&& (!story_.read(StoryFlag::ShofixtiKia)
			|| (!story_.read(StoryFlag::ShofixtiBroKia)
			&& story_.read(StoryFlag::MaidensOnShip))))
	{
		GLOBAL (BattleGroupRef) = story_.read(StoryCounter::ShofixtiGrpoffs0);
		if (GLOBAL (BattleGroupRef) == 0
				|| !uqm::initializeGroups(GLOBAL(BattleGroupRef), {solarSys->SunDesc[0], solarSys->PlanetDesc}, GLOBAL(ip_group_q)))
		{
			HSHIPFRAG hStarShip;

			if (GLOBAL (BattleGroupRef) == 0)
				GLOBAL (BattleGroupRef) = ~0L;

			hStarShip = CloneShipFragment (SHOFIXTI_SHIP,
					&GLOBAL (npc_built_ship_q), 1);
			if (hStarShip)
			{	/* Set old Shofixti name; his brother if Tanaka died */
				SHIP_FRAGMENT *FragPtr = LockShipFrag (
						&GLOBAL (npc_built_ship_q), hStarShip);
				/* Name Tanaka or Katana (+1) */
				FragPtr->captains_name_index =
						NAME_OFFSET + NUM_CAPTAINS_NAMES +
						(story_.read(StoryFlag::ShofixtiKia) & 1);
				UnlockShipFrag (&GLOBAL (npc_built_ship_q), hStarShip);
			}

			GLOBAL (BattleGroupRef) = PutGroupInfo (
					GLOBAL (BattleGroupRef), 1);
			ReinitQueue (&GLOBAL (npc_built_ship_q));
			story_.set(StoryCounter::ShofixtiGrpoffs0, GLOBAL (BattleGroupRef));
		}
	}

	// This was originally a fallthrough to REINIT_NPCS.
	// XXX: is the call to check_old_shofixti() needed?
	GenerateDefault_initNpcs (solarSys);
	check_old_shofixti ();

	return true;
}

bool
ShofixtiSystemBehavior::reinitNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	GenerateDefault_reinitNpcs (solarSys);
	check_old_shofixti ();

	(void) solarSys;
	return true;
}

bool
ShofixtiSystemBehavior::uninitNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	if (GLOBAL (BattleGroupRef)
			&& !story_.read(StoryFlag::ShofixtiRecruited)
			&& GetHeadLink (&GLOBAL (ip_group_q)) == 0)
	{
		if (!story_.read(StoryFlag::ShofixtiKia))
		{
			story_.set(StoryFlag::ShofixtiKia, (1) != 0);
			story_.set(StoryCounter::ShofixtiVisits, 0);
		}
		else if (story_.read(StoryFlag::MaidensOnShip))
		{
			story_.set(StoryFlag::ShofixtiBroKia, (1) != 0);
		}
	}

	GenerateDefault_uninitNpcs (solarSys);
	return true;
}

bool
ShofixtiSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT i;

#define NUM_PLANETS 6
	solarSys->SunDesc[0].NumPlanets = NUM_PLANETS;
	for (i = 0; i < NUM_PLANETS; ++i)
	{
		PLANET_DESC *pCurDesc = &solarSys->PlanetDesc[i];

		pCurDesc->NumPlanets = 0;
		if (i < (NUM_PLANETS >> 1))
			pCurDesc->data_index = SELENIC_WORLD;
		else
			pCurDesc->data_index = METAL_WORLD;
	}

	FillOrbits (solarSys, NUM_PLANETS, solarSys->PlanetDesc, TRUE, &uqm::generationRandom(system), &uqm::systemStar(system));

	return true;
}


void
ShofixtiSystemBehavior::check_old_shofixti(void)
{
	HIPGROUP hGroup;
	IP_GROUP *GroupPtr;

	if (!GLOBAL (BattleGroupRef))
		return; // nothing to check

	hGroup = GetHeadLink (&GLOBAL (ip_group_q));
	if (!hGroup)
		return; // still nothing to check

	GroupPtr = LockIpGroup (&GLOBAL (ip_group_q), hGroup);
	// REFORM_GROUP was set in ipdisp.c:ip_group_collision()
	// during a collision with the flagship.
	if (GroupPtr->race_id == SHOFIXTI_SHIP
			&& (GroupPtr->task & REFORM_GROUP)
			&& story_.read(StoryFlag::ShofixtiRecruited))
	{
		GroupPtr->task = FLEE | IGNORE_FLAGSHIP | REFORM_GROUP;
		GroupPtr->dest_loc = 0;
	}
	UnlockIpGroup (&GLOBAL (ip_group_q), hGroup);
}


std::unique_ptr<SystemBehavior> createShofixtiSystemBehavior(StoryState &story) {
    return std::make_unique<ShofixtiSystemBehavior>(story);
}
} // namespace uqm

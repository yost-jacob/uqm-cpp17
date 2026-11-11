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
namespace uqm {
namespace {
class ZoqFotPikScoutSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool initNpcs(SOLARSYS_STATE &system) override;
    bool reinitNpcs(SOLARSYS_STATE &system) override;
};
}










bool
ZoqFotPikScoutSystemBehavior::initNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	if (!story_.read(StoryFlag::MetZoqfot))
	{
		GLOBAL (BattleGroupRef) = story_.read(StoryCounter::ZoqfotGrpoffs0);
		if (GLOBAL (BattleGroupRef) == 0)
		{
			CloneShipFragment (ZOQFOTPIK_SHIP,
					&GLOBAL (npc_built_ship_q), 0);
			GLOBAL (BattleGroupRef) = PutGroupInfo (GROUPS_ADD_NEW, 1);
			ReinitQueue (&GLOBAL (npc_built_ship_q));
			story_.set(StoryCounter::ZoqfotGrpoffs0, GLOBAL (BattleGroupRef));
		}
	}

	GenerateDefault_initNpcs (solarSys);

	return true;
}

bool
ZoqFotPikScoutSystemBehavior::reinitNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	HIPGROUP hGroup;
	IP_GROUP *GroupPtr;

	GenerateDefault_reinitNpcs (solarSys);

	if (!GLOBAL (BattleGroupRef))
		return true; // nothing to check

	hGroup = GetHeadLink (&GLOBAL (ip_group_q));
	if (!hGroup)
		return true; // still nothing to check

	GroupPtr = LockIpGroup (&GLOBAL (ip_group_q), hGroup);
	// REFORM_GROUP was set in ipdisp.c:ip_group_collision()
	// during a collision with the flagship.
	if (GroupPtr->race_id == ZOQFOTPIK_SHIP
			&& (GroupPtr->task & REFORM_GROUP))
	{
		GroupPtr->task = FLEE | IGNORE_FLAGSHIP | REFORM_GROUP;
		GroupPtr->dest_loc = 0;
	}
	UnlockIpGroup (&GLOBAL (ip_group_q), hGroup);

	return true;
}


std::unique_ptr<SystemBehavior> createZoqFotPikScoutSystemBehavior(StoryState &story) {
    return std::make_unique<ZoqFotPikScoutSystemBehavior>(story);
}
} // namespace uqm

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
#include "../../gendef.h"
#include "../../starmap.h"
#include "../../globdata.h"
#include "../../state.h"
#include "libs/log.h"
namespace uqm {
namespace {
class MelnormeSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool initNpcs(SOLARSYS_STATE &system) override;
private:
    std::optional<StoryCounter> SelectMelnormeRefVar(void);
    DWORD GetMelnormeRef(void);
    void SetMelnormeRef(DWORD Ref);
};
}













bool
MelnormeSystemBehavior::initNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	GLOBAL (BattleGroupRef) = GetMelnormeRef ();
	if (GLOBAL (BattleGroupRef) == 0)
	{
		CloneShipFragment (MELNORME_SHIP, &GLOBAL (npc_built_ship_q), 0);
		GLOBAL (BattleGroupRef) = PutGroupInfo (GROUPS_ADD_NEW, 1);
		ReinitQueue (&GLOBAL (npc_built_ship_q));
		SetMelnormeRef (GLOBAL (BattleGroupRef));
	}

	GenerateDefault_initNpcs (solarSys);

	return true;
}


std::optional<StoryCounter>
MelnormeSystemBehavior::SelectMelnormeRefVar(void)
{
	switch (CurStarDescPtr->Index)
	{
		case MELNORME0_DEFINED: return StoryCounter::Melnorme0Grpoffs0;
		case MELNORME1_DEFINED: return StoryCounter::Melnorme1Grpoffs0;
		case MELNORME2_DEFINED: return StoryCounter::Melnorme2Grpoffs0;
		case MELNORME3_DEFINED: return StoryCounter::Melnorme3Grpoffs0;
		case MELNORME4_DEFINED: return StoryCounter::Melnorme4Grpoffs0;
		case MELNORME5_DEFINED: return StoryCounter::Melnorme5Grpoffs0;
		case MELNORME6_DEFINED: return StoryCounter::Melnorme6Grpoffs0;
		case MELNORME7_DEFINED: return StoryCounter::Melnorme7Grpoffs0;
		case MELNORME8_DEFINED: return StoryCounter::Melnorme8Grpoffs0;
		default:
			return std::nullopt;
	}
}

DWORD
MelnormeSystemBehavior::GetMelnormeRef(void)
{
	auto RefVar = SelectMelnormeRefVar ();
	if (!RefVar)
	{
		log_add (log_Warning, "GetMelnormeRef(): reference unknown");
		return 0;
	}

	return campaign_.read(*RefVar);
}

void
MelnormeSystemBehavior::SetMelnormeRef(DWORD Ref)
{
	auto RefVar = SelectMelnormeRefVar ();
	if (!RefVar)
	{
		log_add (log_Warning, "SetMelnormeRef(): reference unknown");
		return;
	}

	campaign_.set(*RefVar, Ref);
}


std::unique_ptr<SystemBehavior> createMelnormeSystemBehavior(CampaignState &campaign) {
    return std::make_unique<MelnormeSystemBehavior>(campaign);
}
} // namespace uqm

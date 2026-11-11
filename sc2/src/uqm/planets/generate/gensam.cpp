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
#include "../../system_runtime.hpp"
#include "../planets.h"
#include "../../build.h"
#include "../../comm.h"
#include "../../encount.h"
#include "../../globdata.h"
#include "../../grpinfo.h"
#include "../../state.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class SaMatraSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool initNpcs(SOLARSYS_STATE &system) override;
    bool reinitNpcs(SOLARSYS_STATE &system) override;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body) override;
private:
    void BuildUrquanGuard(SOLARSYS_STATE *solarSys);
};
}















bool
SaMatraSystemBehavior::initNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	if (!story_.read(StoryFlag::UrquanMessedUp))
	{
		BuildUrquanGuard (solarSys);
	}
	else
	{	// Exorcise Ur-Quan ghosts upon system reentry
		PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
				// wipe out the group
	}

	(void) solarSys;
	return true;
}

bool
SaMatraSystemBehavior::reinitNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	bool GuardEngaged;
	HIPGROUP hGroup;
	HIPGROUP hNextGroup;

	GetGroupInfo (GROUPS_RANDOM, GROUP_LOAD_IP);
	EncounterGroup = 0;
	EncounterRace = -1;
			// Do not want guards to chase the player

	GuardEngaged = FALSE;
	for (hGroup = GetHeadLink (&GLOBAL (ip_group_q));
			hGroup; hGroup = hNextGroup)
	{
		IP_GROUP *GroupPtr;

		GroupPtr = LockIpGroup (&GLOBAL (ip_group_q), hGroup);
		hNextGroup = _GetSuccLink (GroupPtr);

		if (story_.read(StoryFlag::UrquanMessedUp))
		{
			GroupPtr->task &= REFORM_GROUP;
			GroupPtr->task |= FLEE | IGNORE_FLAGSHIP;
			GroupPtr->dest_loc = 0;
		}
		else if (GroupPtr->task & REFORM_GROUP)
		{
			// REFORM_GROUP was set in ipdisp.c:ip_group_collision
			// during a collision with the flagship.
			GroupPtr->task &= ~REFORM_GROUP;
			GroupPtr->group_counter = 0;

			GuardEngaged = TRUE;
		}

		UnlockIpGroup (&GLOBAL (ip_group_q), hGroup);
	}

	if (GuardEngaged)
	{
		COUNT angle;
		POINT org;

		org = uqm::planetOuterLocation(*solarSys, 4);
        auto &position = uqm::systemLocation(system).flight.position;
		angle = ARCTAN (position.x - org.x,
				position.y - org.y);
		position.x = org.x + COSINE (angle, 3000);
		position.y = org.y + SINE (angle, 3000);
		GLOBAL (ShipStamp.origin) = uqm::transformLocation(system, position, true);
	}

	(void) solarSys;
	return true;
}

bool
SaMatraSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	GenerateDefault_generatePlanets (solarSys);
	solarSys->PlanetDesc[4].NumPlanets = 1;
	return true;
}

bool
SaMatraSystemBehavior::generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *planet = &body;
	GenerateDefault_generateMoons (solarSys, planet);

	if (matchWorld (solarSys, planet, 4, MATCH_PLANET))
	{
		COUNT angle;
		DWORD rand_val;

		solarSys->MoonDesc[0].data_index = SA_MATRA;
		solarSys->MoonDesc[0].radius = MIN_MOON_RADIUS + (2 * MOON_DELTA);
		rand_val = RandomContext_Random (&SysGenRNG);
		angle = NORMALIZE_ANGLE (LOWORD (rand_val));
		solarSys->MoonDesc[0].location.x =
				COSINE (angle, solarSys->MoonDesc[0].radius);
		solarSys->MoonDesc[0].location.y =
				SINE (angle, solarSys->MoonDesc[0].radius);
	}

	return true;
}

bool
SaMatraSystemBehavior::generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	/* Samatra */
	if (matchWorld (solarSys, world, 4, 0))
	{
		uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

		if (!story_.read(StoryFlag::UrquanMessedUp))
		{
			CloneShipFragment (!story_.read(StoryFlag::KohrAhFrenzy) ?
					URQUAN_SHIP : BLACK_URQUAN_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);
		}
		else
		{
#define URQUAN_REMNANTS 3
			BYTE i;

			for (i = 0; i < URQUAN_REMNANTS; ++i)
			{
				CloneShipFragment (URQUAN_SHIP,
						&GLOBAL (npc_built_ship_q), 0);
				CloneShipFragment (BLACK_URQUAN_SHIP,
						&GLOBAL (npc_built_ship_q), 0);
			}
		}

		GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
		story_.set(StoryCounter::EncounterContext, 1 << 7);
		story_.set(StoryFlag::UrquanProtectingSamatra, (1) != 0);
		communicate(URQUAN_CONVERSATION);

		if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
		{
			bool UrquanSurvivors;

			UrquanSurvivors = GetHeadLink (&GLOBAL (npc_built_ship_q)) != 0;

			GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
			encounter.restore();
			if (UrquanSurvivors)
			{
				story_.set(StoryFlag::UrquanProtectingSamatra, (0) != 0);
			}
			else
			{
				EncounterGroup = 0;
				EncounterRace = -1;
				GLOBAL (CurrentActivity) = IN_LAST_BATTLE | START_ENCOUNTER;
				if (story_.read(StoryFlag::YehatCivilWar)
						&& StartSphereTracking (YEHAT_SHIP)
						&& EscortFeasibilityStudy (YEHAT_REBEL_SHIP))
					communicate(YEHAT_REBEL_CONVERSATION);
			}
		}
		return true;
	}

	GenerateDefault_generateOrbital (solarSys, world);
	return true;
}

void
SaMatraSystemBehavior::BuildUrquanGuard(SOLARSYS_STATE *solarSys)
{
	BYTE ship1, ship2;
	BYTE b0, b1;
	POINT org;
	HIPGROUP hGroup, hNextGroup;

	GLOBAL (BattleGroupRef) = story_.read(StoryCounter::SamatraGrpoffs0);

	if (!story_.read(StoryFlag::KohrAhFrenzy))
	{
		ship1 = URQUAN_SHIP;
		ship2 = BLACK_URQUAN_SHIP;
	}
	else
	{
		ship1 = BLACK_URQUAN_SHIP;
		ship2 = URQUAN_SHIP;
	}

	assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

	for (b0 = 0; b0 < MAX_SHIPS_PER_SIDE; ++b0)
		CloneShipFragment (ship1, &GLOBAL (npc_built_ship_q), 0);

	if (GLOBAL (BattleGroupRef) == 0)
	{
		GLOBAL (BattleGroupRef) = PutGroupInfo (GROUPS_ADD_NEW, 1);
		story_.set(StoryCounter::SamatraGrpoffs0, GLOBAL (BattleGroupRef));
	}

#define NUM_URQUAN_GUARDS0 12
	for (b0 = 1; b0 <= NUM_URQUAN_GUARDS0; ++b0)
		PutGroupInfo (GLOBAL (BattleGroupRef), b0);

	ReinitQueue (&GLOBAL (npc_built_ship_q));
	for (b0 = 0; b0 < MAX_SHIPS_PER_SIDE; ++b0)
		CloneShipFragment (ship2, &GLOBAL (npc_built_ship_q), 0);

#define NUM_URQUAN_GUARDS1 4
	for (b0 = 1; b0 <= NUM_URQUAN_GUARDS1; ++b0)
		PutGroupInfo (GLOBAL (BattleGroupRef),
				(BYTE)(NUM_URQUAN_GUARDS0 + b0));

	ReinitQueue (&GLOBAL (npc_built_ship_q));

	GetGroupInfo (GLOBAL (BattleGroupRef), GROUP_INIT_IP);

	org = uqm::planetOuterLocation(*solarSys, 4);
	hGroup = GetHeadLink (&GLOBAL (ip_group_q));
	for (b0 = 0, b1 = 0;
			b0 < NUM_URQUAN_GUARDS0;
			++b0, b1 += FULL_CIRCLE / (NUM_URQUAN_GUARDS0 + NUM_URQUAN_GUARDS1))
	{
		IP_GROUP *GroupPtr;

		if (b1 % (FULL_CIRCLE / NUM_URQUAN_GUARDS1) == 0)
			b1 += FULL_CIRCLE / (NUM_URQUAN_GUARDS0 + NUM_URQUAN_GUARDS1);

		GroupPtr = LockIpGroup (&GLOBAL (ip_group_q), hGroup);
		hNextGroup = _GetSuccLink (GroupPtr);
		GroupPtr->task = ON_STATION | IGNORE_FLAGSHIP;
		GroupPtr->sys_loc = 0;
		GroupPtr->dest_loc = 4 + 1;
		GroupPtr->orbit_pos = NORMALIZE_FACING (ANGLE_TO_FACING (b1));
		GroupPtr->group_counter = 0;
		GroupPtr->loc.x = org.x + COSINE (b1, STATION_RADIUS);
		GroupPtr->loc.y = org.y + SINE (b1, STATION_RADIUS);
		UnlockIpGroup (&GLOBAL (ip_group_q), hGroup);
		hGroup = hNextGroup;
	}

	for (b0 = 0, b1 = 0;
			b0 < NUM_URQUAN_GUARDS1;
			++b0, b1 += FULL_CIRCLE / NUM_URQUAN_GUARDS1)
	{
		IP_GROUP *GroupPtr;

		GroupPtr = LockIpGroup (&GLOBAL (ip_group_q), hGroup);
		hNextGroup = _GetSuccLink (GroupPtr);
		GroupPtr->task = ON_STATION | IGNORE_FLAGSHIP;
		GroupPtr->sys_loc = 0;
		GroupPtr->dest_loc = 4 + 1;
		GroupPtr->orbit_pos = NORMALIZE_FACING (ANGLE_TO_FACING (b1));
		GroupPtr->group_counter = 0;
		GroupPtr->loc.x = org.x + COSINE (b1, STATION_RADIUS);
		GroupPtr->loc.y = org.y + SINE (b1, STATION_RADIUS);
		UnlockIpGroup (&GLOBAL (ip_group_q), hGroup);
		hGroup = hNextGroup;
	}

	(void) solarSys;
}


std::unique_ptr<SystemBehavior> createSaMatraSystemBehavior(StoryState &story) {
    return std::make_unique<SaMatraSystemBehavior>(story);
}
} // namespace uqm

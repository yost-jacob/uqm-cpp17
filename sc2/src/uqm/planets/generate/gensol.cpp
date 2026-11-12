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
#include "../lifeform.h"
#include "../planets.h"
#include "../../build.h"
#include "../../encount.h"
#include "../../globdata.h"
#include "../../gamestr.h"
#include "../../grpinfo.h"
#include "../../nameref.h"
#include "../../state.h"
#include "libs/mathlib.h"
namespace uqm {
namespace {
class SolSystemBehavior final : public SystemBehavior {
public:
    using SystemBehavior::SystemBehavior;
    bool initNpcs(SOLARSYS_STATE &system) override;
    bool reinitNpcs(SOLARSYS_STATE &system) override;
    bool generatePlanets(SOLARSYS_STATE &system) override;
    bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body) override;
    bool generateName(const SOLARSYS_STATE &system, const PLANET_DESC &body) override;
    COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    COUNT generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT node, NODE_INFO *info) override;
    bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT node) override;
private:
    int init_probe(void);
    void check_probe(void);
};
}

bool
SolSystemBehavior::initNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	GLOBAL (BattleGroupRef) = story_.read(StoryCounter::UrquanProbeGrpoffs0);
	if (GLOBAL (BattleGroupRef) == 0)
	{
		CloneShipFragment (URQUAN_DRONE_SHIP, &GLOBAL (npc_built_ship_q), 0);
		GLOBAL (BattleGroupRef) = PutGroupInfo (GROUPS_ADD_NEW, 1);
		ReinitQueue (&GLOBAL (npc_built_ship_q));
		story_.set(StoryCounter::UrquanProbeGrpoffs0, GLOBAL (BattleGroupRef));
	}

	if (!init_probe ())
		GenerateDefault_initNpcs (solarSys);

	return true;
}

bool
SolSystemBehavior::reinitNpcs(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	if (story_.read(StoryCounter::BombProgress) != 3)
	{
		GenerateDefault_reinitNpcs (solarSys);
		check_probe ();
	}
	else
	{
		GLOBAL (BattleGroupRef) = 0;
		ReinitQueue (&GLOBAL (ip_group_q));
		assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);
	}
	return true;
}

bool
SolSystemBehavior::generatePlanets(SOLARSYS_STATE &system)
{
    auto *solarSys = &system;
	COUNT planetI;

#define SOL_SEED 334241042L
	RandomContext_SeedRandom (&SysGenRNG, SOL_SEED);

	solarSys->SunDesc[0].NumPlanets = 9;
	for (planetI = 0; planetI < 9; ++planetI)
	{
		COUNT angle;
		DWORD rand_val;
		UWORD word_val;
		PLANET_DESC *pCurDesc = &solarSys->PlanetDesc[planetI];

		pCurDesc->rand_seed = RandomContext_Random (&SysGenRNG);
		rand_val = pCurDesc->rand_seed;
		word_val = LOWORD (rand_val);
		angle = NORMALIZE_ANGLE ((COUNT)HIBYTE (word_val));

		switch (planetI)
		{
			case 0: /* MERCURY */
				pCurDesc->data_index = METAL_WORLD;
				pCurDesc->radius = EARTH_RADIUS * 39L / 100;
				pCurDesc->NumPlanets = 0;
				break;
			case 1: /* VENUS */
				pCurDesc->data_index = PRIMORDIAL_WORLD;
				pCurDesc->radius = EARTH_RADIUS * 72L / 100;
				pCurDesc->NumPlanets = 0;
				angle = NORMALIZE_ANGLE (FULL_CIRCLE - angle);
				break;
			case 2: /* EARTH */
				pCurDesc->data_index = WATER_WORLD | PLANET_SHIELDED;
				pCurDesc->radius = EARTH_RADIUS;
				pCurDesc->NumPlanets = 2;
				break;
			case 3: /* MARS */
				pCurDesc->data_index = DUST_WORLD;
				pCurDesc->radius = EARTH_RADIUS * 152L / 100;
				pCurDesc->NumPlanets = 0;
				break;
			case 4: /* JUPITER */
				pCurDesc->data_index = RED_GAS_GIANT;
				pCurDesc->radius = EARTH_RADIUS * 500L /* 520L */ / 100;
				pCurDesc->NumPlanets = 4;
				break;
			case 5: /* SATURN */
				pCurDesc->data_index = ORA_GAS_GIANT;
				pCurDesc->radius = EARTH_RADIUS * 750L /* 952L */ / 100;
				pCurDesc->NumPlanets = 1;
				break;
			case 6: /* URANUS */
				pCurDesc->data_index = GRN_GAS_GIANT;
				pCurDesc->radius = EARTH_RADIUS * 1000L /* 1916L */ / 100;
				pCurDesc->NumPlanets = 0;
				break;
			case 7: /* NEPTUNE */
				pCurDesc->data_index = BLU_GAS_GIANT;
				pCurDesc->radius = EARTH_RADIUS * 1250L /* 2999L */ / 100;
				pCurDesc->NumPlanets = 1;
				break;
			case 8: /* PLUTO */
				pCurDesc->data_index = PELLUCID_WORLD;
				pCurDesc->radius = EARTH_RADIUS * 1550L /* 3937L */ / 100;
				pCurDesc->NumPlanets = 0;
				angle = FULL_CIRCLE - OCTANT;
				break;
		}

		pCurDesc->location.x = COSINE (angle, pCurDesc->radius);
		pCurDesc->location.y = SINE (angle, pCurDesc->radius);
	}

	return true;
}

bool
SolSystemBehavior::generateMoons(SOLARSYS_STATE &system, PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *planet = &body;
	COUNT planetNr;
	DWORD rand_val;

	GenerateDefault_generateMoons (solarSys, planet);

	planetNr = planetIndex (solarSys, planet);
	switch (planetNr)
	{
		case 2: /* moons of EARTH */
		{
			COUNT angle;

			/* Starbase: */
			solarSys->MoonDesc[0].data_index = HIERARCHY_STARBASE;
			solarSys->MoonDesc[0].radius = MIN_MOON_RADIUS;
			angle = HALF_CIRCLE + QUADRANT;
			solarSys->MoonDesc[0].location.x =
					COSINE (angle, solarSys->MoonDesc[0].radius);
			solarSys->MoonDesc[0].location.y =
					SINE (angle, solarSys->MoonDesc[0].radius);

			/* Luna: */
			solarSys->MoonDesc[1].data_index = SELENIC_WORLD;
			solarSys->MoonDesc[1].radius = MIN_MOON_RADIUS
					+ (MAX_MOONS - 1) * MOON_DELTA;
			rand_val = RandomContext_Random (&SysGenRNG);
			angle = NORMALIZE_ANGLE (LOWORD (rand_val));
			solarSys->MoonDesc[1].location.x =
					COSINE (angle, solarSys->MoonDesc[1].radius);
			solarSys->MoonDesc[1].location.y =
					SINE (angle, solarSys->MoonDesc[1].radius);
			break;
		}
		case 4: /* moons of JUPITER */
			solarSys->MoonDesc[0].data_index = RADIOACTIVE_WORLD;
					/* Io */
			solarSys->MoonDesc[1].data_index = HALIDE_WORLD;
					/* Europa */
			solarSys->MoonDesc[2].data_index = CYANIC_WORLD;
					/* Ganymede */
			solarSys->MoonDesc[3].data_index = PELLUCID_WORLD;
					/* Callisto */
			break;
		case 5: /* moons of SATURN */
			solarSys->MoonDesc[0].data_index = ALKALI_WORLD;
					/* Titan */
			break;
		case 7: /* moons of NEPTUNE */
			solarSys->MoonDesc[0].data_index = VINYLOGOUS_WORLD;
					/* Triton */
			break;
	}

	return true;
}

bool
SolSystemBehavior::generateName(const SOLARSYS_STATE &system, const PLANET_DESC &body)
{
    auto *solarSys = &system;
    auto *world = &body;
	COUNT planetNr = planetIndex (solarSys, world);
	utf8StringCopy (GLOBAL_SIS (PlanetName), sizeof (GLOBAL_SIS (PlanetName)),
			GAME_STRING (PLANET_NUMBER_BASE + planetNr));
	story_.set(StoryCounter::BattlePlanet, solarSys->PlanetDesc[planetNr].data_index);

	return true;
}

COUNT
SolSystemBehavior::generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 8, MATCH_PLANET))
	{
		/* Pluto */
		// This check is needed because the retrieval bit is not set for
		// this node to keep it on the surface while the lander is taking off
		if (story_.read(StoryCounter::FoundPlutoSpathi))
		{	// already picked up
			return 0;
		}

		if (info)
		{
			info->loc_pt.x = 20;
			info->loc_pt.y = MAP_HEIGHT - 8;
		}

		return 1; // only matters when count is requested
	}

	if (matchWorld (solarSys, world, 2, 1))
	{
		/* Earth Moon */
		// This check is redundant since the retrieval bit will keep the
		// node from showing up again
		if (story_.read(StoryFlag::MoonbaseDestroyed))
		{	// already picked up
			return 0;
		}

		if (info)
		{
			info->loc_pt.x = MAP_WIDTH * 3 / 4;
			info->loc_pt.y = MAP_HEIGHT * 1 / 4;
		}

		return 1; // only matters when count is requested
	}

	(void) whichNode;
	return 0;
}

bool
SolSystemBehavior::pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &body, COUNT whichNode)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 8, MATCH_PLANET))
	{	// Pluto
		assert (!story_.read(StoryCounter::FoundPlutoSpathi) && whichNode == 0);

		// Ran into Fwiffo on Pluto
		#define FWIFFO_FRAGS  8
		if (!KillLanderCrewSeq (FWIFFO_FRAGS, ONE_SECOND / 20))
			return false; // lander probably died

		story_.set(StoryCounter::FoundPlutoSpathi, 1);

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		// Do not remove the node from the surface while the lander is
		// taking off. FOUND_PLUTO_SPATHI bit will keep the node from
		// showing up on subsequent visits.
		return false;
	}

	if (matchWorld (solarSys, world, 2, 1))
	{	// Earth Moon
		assert (!story_.read(StoryFlag::MoonbaseDestroyed) && whichNode == 0);

		GenerateDefault_landerReport (solarSys);
		SetLanderTakeoff ();

		story_.set(StoryFlag::MoonbaseDestroyed, (1) != 0);
		story_.set(StoryFlag::MoonbaseOnShip, (1) != 0);

		return true; // picked up
	}

	(void) whichNode;
	return false;
}

COUNT
SolSystemBehavior::generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &body, COUNT whichNode, NODE_INFO *info)
{
    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 2, 1))
	{
		/* Earth Moon */
		return GenerateRandomNodes (&solarSys->SysInfo, BIOLOGICAL_SCAN, 10,
				NUM_CREATURE_TYPES + 1, whichNode, info);
	}

	return 0;
}

int
SolSystemBehavior::init_probe(void)
{
	HIPGROUP hGroup;

	if (!story_.read(StoryFlag::ProbeMessageDelivered)
			&& GetGroupInfo (GLOBAL (BattleGroupRef), GROUP_INIT_IP)
			&& (hGroup = GetHeadLink (&GLOBAL (ip_group_q))))
	{
		IP_GROUP *GroupPtr;

		GroupPtr = LockIpGroup (&GLOBAL (ip_group_q), hGroup);
		GroupPtr->task = IN_ORBIT;
		GroupPtr->sys_loc = 2 + 1; /* orbitting earth */
		GroupPtr->dest_loc = 2 + 1; /* orbitting earth */
		GroupPtr->loc.x = 0;
		GroupPtr->loc.y = 0;
		GroupPtr->group_counter = 0;
		UnlockIpGroup (&GLOBAL (ip_group_q), hGroup);

		return 1;
	}
	else
		return 0;
}

void
SolSystemBehavior::check_probe(void)
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
	if (GroupPtr->race_id == URQUAN_DRONE_SHIP
			&& (GroupPtr->task & REFORM_GROUP))
	{
		// We just want the probe to take off as fast as possible,
		// so clear out REFORM_GROUP
		GroupPtr->task = FLEE | IGNORE_FLAGSHIP;
		GroupPtr->dest_loc = 0;
	}
	UnlockIpGroup (&GLOBAL (ip_group_q), hGroup);
}

std::unique_ptr<SystemBehavior> createSolSystemBehavior(StoryState &story) {
    return std::make_unique<SolSystemBehavior>(story);
}
} // namespace uqm

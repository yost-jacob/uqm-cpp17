//Copyright Paul Reiche, Fred Ford. 1992-2002
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef UQM_ORBIT_EVENTS_HPP_
#define UQM_ORBIT_EVENTS_HPP_
#include "world_visit.hpp"
#include "system_runtime.hpp"
#include "orbital_encounter.hpp"
#include "game_state.hpp"
#include "planets/generate/genall.h"
#include "planets/lander.h"
#include "planets/scan.h"
#include "build.h"
#include "comm.h"
#include "encount.h"
#include "gamestr.h"
#include "gendef.h"
#include "globdata.h"
#include "grpinfo.h"
#include "ipdisp.h"
#include "nameref.h"
#include "resinst.h"
#include "setup.h"
#include "sounds.h"
#include "starmap.h"
#include "state.h"
#include "world_c.h"
#include "libs/mathlib.h"
#include "libs/threadlib.h"

namespace uqm::orbit_events {
// Procedural orbit-entry scripts. Each visit must be active, with its selected
// world and graphics/location scopes established. Helpers borrow it only for
// this call; child modes run synchronously. Story conditions are read now,
// never during handler selection. enterPreparedOrbit() checks cancellation
// before presenting a prepared surface. No helper is a constructor or owns
// session state. Legacy descriptor access is confined to these migrated scripts.

// Androsynth ruins, personal discovery reports and environmental overrides.
inline void androsynth(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
        COUNT visits = story.discovery(solarSys->worlds->system->world(1).id()).visitedSites.count();

		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		visit.report().load(ANDROSYNTH_RUINS_STRTAB);
        ConfigurePersonalEnergyBehavior (solarSys, 1, -1, NUM_RACE_RUINS,
                visit.report().size());
        // The site remains in the shared world. This player's accepted visits
        // select the next report independently of physical retrieval masks.
		if (visits >= visit.report().size())
		{	// All the reports were already given
			visit.report().clear();
		}
		else
		{	// Advance the report sequence to the first unread
			visit.report().select(visits);
		}
	}

	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
		visit.data().AtmoDensity =
				EARTH_ATMOSPHERE * 144 / 100;
		visit.data().SurfaceTemperature = 28;
		visit.data().Weather = 1;
		visit.data().Tectonics = 1;
	}

	return visit.enterPreparedOrbit();
}

// Burvixese ruins and the broadcaster moon, preserving scan-seed ordering.
inline void burvixese(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	DWORD rand_val;

	uqm::analyzePlanet(system, body, solarSys->SysInfo, PLANET_ANALYSIS_ORBIT, PlanData);
	rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

	visit.data().ScanSeed[BIOLOGICAL_SCAN] = rand_val;
	GenerateLifeForms (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
	rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

	visit.data().ScanSeed[MINERAL_SCAN] = rand_val;
	GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});

	visit.data().ScanSeed[ENERGY_SCAN] = rand_val;

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (
				LoadGraphic (RUINS_MASK_PMAP_ANIM));
		visit.report().load(BURV_RUINS_STRTAB);
		visit.data().Weather = 0;
		visit.data().Tectonics = 0;
	}
	else if (matchWorld (solarSys, world, 0, 0)
			&& !story.read(StoryFlag::BurvixeseBroadcasters))
	{
		solarSys->graphics->PlanetSideFrame[1] = CaptureDrawable (
				LoadGraphic (BURV_BCS_MASK_PMAP_ANIM));
		visit.report().load(BURV_BCS_STRTAB);
	}

	visit.loadSurface(NULL);

	return visit.enterPreparedOrbit();
}

// Chmmr contact, Ilwrath interception, shielded orbit and the deserted base.
inline void chmmr(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

	if (matchWorld (&system, &body, 1, MATCH_PLANET))
	{
		if (story.read(StoryFlag::ChmmrFreed))
		{
			story.set(StoryCounter::EncounterContext, 1u << 7);
			visit.converse(CHMMR_CONVERSATION);

			if (story.read(StoryCounter::BombProgress) == 2)
			{
				GLOBAL (CurrentActivity) |= END_INTERPLANETARY;
			}

			return visit.enterPreparedOrbit();
		}
		else if (story.read(StoryFlag::SunDeviceOnShip)
				&& !story.read(StoryFlag::IlwrathDeceived)
				&& StartSphereTracking (ILWRATH_SHIP))
		{
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (ILWRATH_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);

			story.set(StoryCounter::EncounterContext, 1u << 6);
			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			visit.converse(ILWRATH_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}

			return visit.enterPreparedOrbit();
		}
	}
	else if (matchWorld (&system, &body, 1, 0))
	{
		/* Starbase */
		visit.report().load(CHMMR_BASE_STRTAB);

		visit.reportDiscovery();

		visit.report().clear();

		return visit.enterPreparedOrbit();
	}

	visit.prepareOrbit();

	return visit.enterPreparedOrbit();
}

// Starting colony planetary analysis and surface preparation.
inline void colony(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		uqm::analyzePlanet(system, body, solarSys->SysInfo, PLANET_ANALYSIS_ORBIT, PlanData);

		visit.data().AtmoDensity =
				EARTH_ATMOSPHERE * 98 / 100;
		visit.data().Weather = 0;
		visit.data().Tectonics = 0;
		visit.data().SurfaceTemperature = 28;

		visit.loadSurface(NULL);

		return visit.enterPreparedOrbit();
	}

	visit.prepareOrbit();

	return visit.enterPreparedOrbit();
}

// Druuge contact or ruins, including the already-purchased artifact report.
inline void druuge(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (DRUUGE_SHIP))
		{
			NotifyOthers (DRUUGE_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (DRUUGE_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story.set(StoryCounter::EncounterContext, 1 << 7);
			visit.converse(DRUUGE_CONVERSATION);
			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}
			return visit.enterPreparedOrbit();
		}
		else
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			visit.report().load(DRUUGE_RUINS_STRTAB);
			if (story.read(StoryFlag::RosySphere))
			{	// Already picked up Rosy Sphere, skip the report
				visit.report().select(1);
			}
		}
	}

	visit.prepareOrbit();

	return visit.enterPreparedOrbit();
}

// Ilwrath homeworld encounter or ruins and environmental overrides.
inline void ilwrath(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (ILWRATH_SHIP))
		{
			NotifyOthers (ILWRATH_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (ILWRATH_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story.set(StoryCounter::EncounterContext, 1 << 7);
			visit.converse(ILWRATH_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}
			return visit.enterPreparedOrbit();
		}
		else
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (
					LoadGraphic (RUINS_MASK_PMAP_ANIM));
			visit.report().load(RUINS_STRTAB);
		}
	}

	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		visit.data().Weather = 2;
		visit.data().Tectonics = 3;
	}

	return visit.enterPreparedOrbit();
}

// Mycon guards, the Sun Device, shattered worlds and their artifact reports.
inline void mycon(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
        if (visit.star().Index == SUN_DEVICE_DEFINED
                || visit.star().Index == EGG_CASE0_DEFINED
                || visit.star().Index == EGG_CASE1_DEFINED
                || visit.star().Index == EGG_CASE2_DEFINED)
            ConfigureArtifactEnergyBehavior (solarSys, 0, -1);

		if ((visit.star().Index == MYCON_DEFINED
				|| visit.star().Index == SUN_DEVICE_DEFINED)
				&& StartSphereTracking (MYCON_SHIP))
		{
			if (visit.star().Index == MYCON_DEFINED
					|| !story.read(StoryFlag::SunDeviceUnguarded))
			{
				NotifyOthers (MYCON_SHIP, IPNL_ALL_CLEAR);
				uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

				if (visit.star().Index == MYCON_DEFINED
						|| !story.read(StoryFlag::MyconFellForAmbush))
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
				if (visit.star().Index == MYCON_DEFINED)
				{
					story.set(StoryCounter::EncounterContext, 1 << 7);
				}
				else
				{
					story.set(StoryCounter::EncounterContext, 1 << 6);
				}
				visit.converse(MYCON_CONVERSATION);

				if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
					return visit.enterPreparedOrbit();

				{
					bool MyconSurvivors;

					MyconSurvivors =
							GetHeadLink (&GLOBAL (npc_built_ship_q)) != 0;

					GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
					encounter.restore();

					if (MyconSurvivors)
						return visit.enterPreparedOrbit();

					story.set(StoryFlag::SunDeviceUnguarded, (1) != 0);
					visit.restoreBorder();
				}
			}
		}

		switch (visit.star().Index)
		{
			case SUN_DEVICE_DEFINED:
				if (!story.read(StoryFlag::SunDevice))
				{
					solarSys->graphics->PlanetSideFrame[1] =
							CaptureDrawable (
									LoadGraphic (SUN_DEVICE_MASK_PMAP_ANIM));
					visit.report().load(SUN_DEVICE_STRTAB);
				}
				break;
			case EGG_CASE0_DEFINED:
			case EGG_CASE1_DEFINED:
			case EGG_CASE2_DEFINED:
				if (story.read(StoryCounter::KnowAboutShattered) == 0)
					story.set(StoryCounter::KnowAboutShattered, 1);

				if (!isNodeRetrieved (&solarSys->SysInfo.PlanetInfo,
						ENERGY_SCAN, 0))
				{
					solarSys->graphics->PlanetSideFrame[1] =
							CaptureDrawable (
									LoadGraphic (EGG_CASE_MASK_PMAP_ANIM));
					visit.report().load(EGG_CASE_STRTAB);
				}
				break;
		}
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Mycon trap-world orbital preparation and environmental overrides.
inline void myconTrap(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();

    auto *solarSys = &system;
    auto *world = &body;
	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		visit.data().AtmoDensity = EARTH_ATMOSPHERE * 2;
		visit.data().SurfaceTemperature = 35;
		visit.data().Weather = 3;
		visit.data().Tectonics = 1;
	}

	return visit.enterPreparedOrbit();
}

// Orz contact and the Taalo protector moon encounter/artifact.
inline void orz(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if ((visit.star().Index == ORZ_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
			|| (visit.star().Index == TAALO_PROTECTOR_DEFINED
			&& matchWorld (solarSys, world, 1, 2)
			&& !story.read(StoryFlag::TaaloProtector)))
	{
		COUNT i;

		if ((visit.star().Index == ORZ_DEFINED
				|| !story.read(StoryFlag::TaaloUnprotected))
				&& StartSphereTracking (ORZ_SHIP))
		{
			NotifyOthers (ORZ_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			if (visit.star().Index == ORZ_DEFINED)
			{
				CloneShipFragment (ORZ_SHIP,
						&GLOBAL (npc_built_ship_q), INFINITE_FLEET);
				story.set(StoryCounter::EncounterContext, 1 << 7);
			}
			else
			{
				for (i = 0; i < 14; ++i)
				{
					CloneShipFragment (ORZ_SHIP,
							&GLOBAL (npc_built_ship_q), 0);
				}
				story.set(StoryCounter::EncounterContext, 1 << 6);
			}
			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			visit.converse(ORZ_CONVERSATION);

			if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
				return visit.enterPreparedOrbit();

			{
				bool OrzSurvivors;

				OrzSurvivors = GetHeadLink (&GLOBAL (npc_built_ship_q))
						&& (visit.star().Index == ORZ_DEFINED
						|| !story.read(StoryFlag::TaaloUnprotected));

				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();

				if (OrzSurvivors)
					return visit.enterPreparedOrbit();

				visit.restoreBorder();
			}
		}

		story.set(StoryFlag::TaaloUnprotected, (1) != 0);
		if (visit.star().Index == TAALO_PROTECTOR_DEFINED)
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (
					LoadGraphic (TAALO_DEVICE_MASK_PMAP_ANIM));
			visit.report().load(TAALO_DEVICE_STRTAB);
		}
		else
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			visit.report().load(RUINS_STRTAB);
		}
	}

	visit.prepareOrbit();

	return visit.enterPreparedOrbit();
}

// Pkunk homeworld contact or the appropriate ruins report.
inline void pkunk(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (PKUNK_SHIP))
		{
			NotifyOthers (PKUNK_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (PKUNK_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story.set(StoryCounter::EncounterContext, 1 << 7);
			visit.converse(PKUNK_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}
			return visit.enterPreparedOrbit();
		}
		else
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			visit.report().load(PKUNK_RUINS_STRTAB);
			if (story.read(StoryFlag::ClearSpindle))
			{	// Already picked up the Clear Spindle, skip the report
				visit.report().select(1);
			}
		}
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Record discovery of this rainbow world before preparing orbit.
inline void rainbowWorld(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		BYTE which_rainbow;
		UWORD rainbow_mask;
		STAR_DESC *SDPtr;

		rainbow_mask = MAKE_WORD (
				story.read(StoryCounter::RainbowWorld0),
				story.read(StoryCounter::RainbowWorld1));

		which_rainbow = 0;
		SDPtr = GetStar(0);
		while (SDPtr != &visit.star())
		{
			if (SDPtr->Index == RAINBOW_DEFINED)
				++which_rainbow;
			SDPtr = GetStar(GetStarIndex(SDPtr) + 1);
		}
		rainbow_mask |= 1 << which_rainbow;
		story.set(StoryCounter::RainbowWorld0, LOBYTE (rainbow_mask));
		story.set(StoryCounter::RainbowWorld1, HIBYTE (rainbow_mask));
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Sa-Matra defenders and the final-battle handoff after their defeat.
inline void saMatra(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	/* Samatra */
	if (matchWorld (solarSys, world, 4, 0))
	{
		uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

		if (!story.read(StoryFlag::UrquanMessedUp))
		{
			CloneShipFragment (!story.read(StoryFlag::KohrAhFrenzy) ?
					URQUAN_SHIP : BLACK_URQUAN_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);
		}
		else
		{
        constexpr BYTE UrquanRemnants = 3;
			BYTE i;

			for (i = 0; i < UrquanRemnants; ++i)
			{
				CloneShipFragment (URQUAN_SHIP,
						&GLOBAL (npc_built_ship_q), 0);
				CloneShipFragment (BLACK_URQUAN_SHIP,
						&GLOBAL (npc_built_ship_q), 0);
			}
		}

		GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
		story.set(StoryCounter::EncounterContext, 1 << 7);
		story.set(StoryFlag::UrquanProtectingSamatra, (1) != 0);
		visit.converse(URQUAN_CONVERSATION);

		if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
		{
			bool UrquanSurvivors;

			UrquanSurvivors = GetHeadLink (&GLOBAL (npc_built_ship_q)) != 0;

			GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
			encounter.restore();
			if (UrquanSurvivors)
			{
				story.set(StoryFlag::UrquanProtectingSamatra, (0) != 0);
			}
			else
			{
				EncounterGroup = 0;
				EncounterRace = -1;
				GLOBAL (CurrentActivity) = IN_LAST_BATTLE | START_ENCOUNTER;
				if (story.read(StoryFlag::YehatCivilWar)
						&& StartSphereTracking (YEHAT_SHIP)
						&& EscortFeasibilityStudy (YEHAT_REBEL_SHIP))
					visit.converse(YEHAT_REBEL_CONVERSATION);
			}
		}
		return visit.enterPreparedOrbit();
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Syreen ship-vault report selected from current shuttle/unlock state.
inline void shipVault(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, 0))
	{
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (VAULT_MASK_PMAP_ANIM));
		visit.report().load(VAULT_STRTAB);
		if (story.read(StoryFlag::ShipVaultUnlocked))
		{
			visit.report().select(2);
		}
		else if (story.read(StoryFlag::SyreenShuttleOnShip))
		{
			visit.report().select(1);
		}
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Contact the Slylandro at their homeworld; ordinary orbit elsewhere.
inline void slylandro(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 3, MATCH_PLANET))
	{
		visit.converse(SLYLANDRO_HOME_CONVERSATION);
		return visit.enterPreparedOrbit();
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Sol-specific physical properties, scan seeds, discoveries and Earth starbase.
inline void sol(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	DWORD rand_val;
	COUNT planetNr;

	if (matchWorld (solarSys, world, 2, 0))
	{
		/* Starbase */
		PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
		ReinitQueue (&GLOBAL (ip_group_q));
		assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

		EncounterGroup = 0;
		GLOBAL (CurrentActivity) |= START_ENCOUNTER;
		story.set(StoryCounter::EncounterContext, (BYTE)~0);
		return visit.enterPreparedOrbit();
	}

	uqm::analyzePlanet(system, body, solarSys->SysInfo, PLANET_ANALYSIS_ORBIT, PlanData);
	rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

	visit.data().ScanSeed[MINERAL_SCAN] = rand_val;
	GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
	rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

	planetNr = planetIndex (solarSys, world);
	if (worldIsPlanet (solarSys, world))
	{
		switch (planetNr)
		{
			case 0: /* MERCURY */
				visit.data().AtmoDensity = 0;
				visit.data().PlanetDensity = 98;
				visit.data().PlanetRadius = 38;
				visit.data().AxialTilt = 3;
				visit.data().Weather = 0;
				visit.data().Tectonics = 2;
				visit.data().RotationPeriod = 59 * 240;
				visit.data().SurfaceTemperature = 165;
				break;
			case 1: /* VENUS */
				visit.data().AtmoDensity = 90 *
						EARTH_ATMOSPHERE;
				visit.data().PlanetDensity = 95;
				visit.data().PlanetRadius = 95;
				visit.data().AxialTilt = 177;
				visit.data().Weather = 7;
				visit.data().Tectonics = 1;
				visit.data().RotationPeriod = 243 * 240;
				visit.data().SurfaceTemperature = 457;
				break;
			case 2: /* EARTH */
				visit.data().AtmoDensity =
						EARTH_ATMOSPHERE;
				visit.data().PlanetDensity = 100;
				visit.data().PlanetRadius = 100;
				visit.data().AxialTilt = 23;
				visit.data().Weather = 1;
				visit.data().Tectonics = 1;
				visit.data().RotationPeriod = 240;
				visit.data().SurfaceTemperature = 22;
				break;
			case 3: /* MARS */
				// XXX: Mars atmo should actually be 1/2 in current units
				visit.data().AtmoDensity = 1;
				visit.data().PlanetDensity = 72;
				visit.data().PlanetRadius = 53;
				visit.data().AxialTilt = 24;
				visit.data().Weather = 1;
				visit.data().Tectonics = 1;
				visit.data().RotationPeriod = 246;
				visit.data().SurfaceTemperature = -53;
				break;
			case 4: /* JUPITER */
				visit.data().AtmoDensity =
						GAS_GIANT_ATMOSPHERE;
				visit.data().PlanetDensity = 24;
				visit.data().PlanetRadius = 1120;
				visit.data().AxialTilt = 3;
				visit.data().Weather = 7;
				visit.data().Tectonics = 0;
				visit.data().RotationPeriod = 98;
				visit.data().SurfaceTemperature = -143;
				visit.data().PlanetToSunDist =
						EARTH_RADIUS * 520L / 100;
				break;
			case 5: /* SATURN */
				visit.data().AtmoDensity =
						GAS_GIANT_ATMOSPHERE;
				visit.data().PlanetDensity = 13;
				visit.data().PlanetRadius = 945;
				visit.data().AxialTilt = 27;
				visit.data().Weather = 7;
				visit.data().Tectonics = 0;
				visit.data().RotationPeriod = 102;
				visit.data().SurfaceTemperature = -197;
				visit.data().PlanetToSunDist =
						EARTH_RADIUS * 952L / 100;
				break;
			case 6: /* URANUS */
				visit.data().AtmoDensity =
						GAS_GIANT_ATMOSPHERE;
				visit.data().PlanetDensity = 21;
				visit.data().PlanetRadius = 411;
				visit.data().AxialTilt = 98;
				visit.data().Weather = 7;
				visit.data().Tectonics = 0;
				visit.data().RotationPeriod = 172;
				visit.data().SurfaceTemperature = -217;
				visit.data().PlanetToSunDist =
						EARTH_RADIUS * 1916L / 100;
				break;
			case 7: /* NEPTUNE */
				visit.data().AtmoDensity =
						GAS_GIANT_ATMOSPHERE;
				visit.data().PlanetDensity = 28;
				visit.data().PlanetRadius = 396;
				visit.data().AxialTilt = 30;
				visit.data().Weather = 7;
				visit.data().Tectonics = 0;
				visit.data().RotationPeriod = 182;
				visit.data().SurfaceTemperature = -229;
				visit.data().PlanetToSunDist =
						EARTH_RADIUS * 2999L / 100;
				break;
			case 8: /* PLUTO */
				if (!story.read(StoryCounter::FoundPlutoSpathi))
				{
					solarSys->graphics->PlanetSideFrame[1] =
							CaptureDrawable (
							LoadGraphic (SPAPLUTO_MASK_PMAP_ANIM));
					visit.report().load(SPAPLUTO_STRTAB);
				}

				visit.data().AtmoDensity = 0;
				visit.data().PlanetDensity = 33;
				visit.data().PlanetRadius = 18;
				visit.data().AxialTilt = 119;
				visit.data().Weather = 0;
				visit.data().Tectonics = 0;
				visit.data().RotationPeriod = 1533;
				visit.data().SurfaceTemperature = -235;
				visit.data().PlanetToSunDist =
						EARTH_RADIUS * 3937L / 100;
				break;
		}

		visit.data().SurfaceGravity =
				CalcGravity (&solarSys->SysInfo.PlanetInfo);
		visit.loadSurface(planetNr == 2 ?
				CaptureDrawable (LoadGraphic (EARTH_MASK_ANIM)) : NULL);
	}
	else
	{
		// World is a moon.
		COUNT moonNr = moonIndex (solarSys, world);

		visit.data().AxialTilt = 0;
		visit.data().AtmoDensity = 0;
		visit.data().Weather = 0;
		switch (planetNr)
		{
			case 2: /* moons of EARTH */
				// NOTE: Even though we save the seed here, it is irrelevant.
				//   The seed will be used to randomly place the tractors, but
				//   since they are mobile, they will be moved to different
				//   locations not governed by this seed.
				visit.data().ScanSeed[BIOLOGICAL_SCAN] =
						rand_val;

				if (!story.read(StoryFlag::MoonbaseDestroyed))
				{
					solarSys->graphics->PlanetSideFrame[1] =
							CaptureDrawable (
							LoadGraphic (MOONBASE_MASK_PMAP_ANIM));
					visit.report().load(MOONBASE_STRTAB);
				}

				visit.data().PlanetDensity = 60;
				visit.data().PlanetRadius = 25;
				visit.data().AxialTilt = 0;
				visit.data().Tectonics = 0;
				visit.data().RotationPeriod = 240 * 29;
				visit.data().SurfaceTemperature = -18;
				break;

			case 4: /* moons of JUPITER */
				visit.data().PlanetToSunDist =
						EARTH_RADIUS * 520L / 100;
				switch (moonNr)
				{
					case 0: /* Io */
						visit.data().PlanetDensity = 69;
						visit.data().PlanetRadius = 25;
						visit.data().Tectonics = 3;
						visit.data().RotationPeriod = 390;
						visit.data().SurfaceTemperature = -163;
						break;
					case 1: /* Europa */
						visit.data().PlanetDensity = 54;
						visit.data().PlanetRadius = 25;
						visit.data().Tectonics = 1;
						visit.data().RotationPeriod = 840;
						visit.data().SurfaceTemperature = -161;
						break;
					case 2: /* Ganymede */
						visit.data().PlanetDensity = 35;
						visit.data().PlanetRadius = 41;
						visit.data().Tectonics = 0;
						visit.data().RotationPeriod = 1728;
						visit.data().SurfaceTemperature = -164;
						break;
					case 3: /* Callisto */
						visit.data().PlanetDensity = 35;
						visit.data().PlanetRadius = 38;
						visit.data().Tectonics = 1;
						visit.data().RotationPeriod = 4008;
						visit.data().SurfaceTemperature = -167;
						break;
				}
				break;

			case 5: /* moon of SATURN: Titan */
				visit.data().PlanetToSunDist =
						EARTH_RADIUS * 952L / 100;
				visit.data().AtmoDensity = 160;
				visit.data().Weather = 2;
				visit.data().PlanetDensity = 34;
				visit.data().PlanetRadius = 40;
				visit.data().Tectonics = 1;
				visit.data().RotationPeriod = 3816;
				visit.data().SurfaceTemperature = -178;
				break;

			case 7: /* moon of NEPTUNE: Triton */
				visit.data().PlanetToSunDist =
						EARTH_RADIUS * 2999L / 100;
				visit.data().AtmoDensity = 10;
				visit.data().Weather = 1;
				visit.data().PlanetDensity = 95;
				visit.data().PlanetRadius = 27;
				visit.data().Tectonics = 0;
				visit.data().RotationPeriod = 4300;
				visit.data().SurfaceTemperature = -216;
				break;
		}

		visit.data().SurfaceGravity =
				CalcGravity (&solarSys->SysInfo.PlanetInfo);
		visit.loadSurface(NULL);
	}

	return visit.enterPreparedOrbit();
}

// Spathi moon contact, shielded homeworld and current-state surface reports.
inline void spathi(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	DWORD rand_val;

	if (matchWorld (solarSys, world, 0, 0))
	{
		/* Spathiwa's moon */
		if (!story.read(StoryFlag::SpathiShieldedSelves)
				&& StartSphereTracking (SPATHI_SHIP))
		{
			NotifyOthers (SPATHI_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (SPATHI_SHIP, &GLOBAL (npc_built_ship_q),
					INFINITE_FLEET);

			story.set(StoryCounter::EncounterContext, 1 << 7);
			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			visit.converse(SPATHI_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}
			return visit.enterPreparedOrbit();
		}

		uqm::analyzePlanet(system, body, solarSys->SysInfo, PLANET_ANALYSIS_ORBIT, PlanData);
		rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

		visit.data().ScanSeed[BIOLOGICAL_SCAN] = rand_val;
		GenerateLifeForms (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
		rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

		visit.data().ScanSeed[MINERAL_SCAN] = rand_val;
		GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});

		visit.data().ScanSeed[ENERGY_SCAN] = rand_val;

		visit.data().Weather = 0;
		visit.data().Tectonics = 0;
		visit.data().SurfaceTemperature = 28;
		if (!story.read(StoryFlag::UmgahBroadcasters))
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (UMGAH_BCS_MASK_PMAP_ANIM));
			visit.report().load(UMGAH_BCS_STRTAB);
			if (!story.read(StoryFlag::SpathiShieldedSelves))
			{	// The first report talks extensively about Spathi
				// slave-shielding selves. If they never did so, the report
				// makes no sense, so use an alternate.
				visit.report().select(1);
			}
		}
		visit.loadSurface(NULL);
		return visit.enterPreparedOrbit();
	}
	else if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		/* visiting Spathiwa */
		uqm::analyzePlanet(system, body, solarSys->SysInfo, PLANET_ANALYSIS_ORBIT, PlanData);
		rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

		visit.data().ScanSeed[MINERAL_SCAN] = rand_val;
		GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
		rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

		visit.data().ScanSeed[BIOLOGICAL_SCAN] = rand_val;

		visit.data().PlanetRadius = 120;
		visit.data().SurfaceGravity =
				CalcGravity (&solarSys->SysInfo.PlanetInfo);
		visit.data().Weather = 0;
		visit.data().Tectonics = 0;
		visit.data().SurfaceTemperature = 31;

		visit.loadSurface(NULL);
		return visit.enterPreparedOrbit();
	}

	visit.prepareOrbit();

	return visit.enterPreparedOrbit();
}

// Supox homeworld contact or the appropriate ruins report.
inline void supox(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (SUPOX_SHIP))
		{
			NotifyOthers (SUPOX_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (SUPOX_SHIP, &GLOBAL (npc_built_ship_q),
					INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story.set(StoryCounter::EncounterContext, 1 << 7);
			visit.converse(SUPOX_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}
			return visit.enterPreparedOrbit();
		}
		else
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			visit.report().load(SUPOX_RUINS_STRTAB);
			if (story.read(StoryCounter::UltronCondition))
			{	// Already picked up the Ultron, skip the report
				visit.report().select(1);
			}
		}
	}

	visit.prepareOrbit();

	return visit.enterPreparedOrbit();
}

// Syreen homeworld conditions and conversation at its starbase.
inline void syreen(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		/* Syreen home planet */
		visit.prepareOrbit();

		visit.data().SurfaceTemperature = 19;
		visit.data().Tectonics = 0;
		visit.data().Weather = 0;
		visit.data().AtmoDensity = EARTH_ATMOSPHERE * 9 / 10;
		return visit.enterPreparedOrbit();
	}

	if (matchWorld (solarSys, world, 0, 0))
	{
		/* Starbase */
		visit.converse(SYREEN_CONVERSATION);
		return visit.enterPreparedOrbit();
	}

	visit.prepareOrbit();

	return visit.enterPreparedOrbit();
}


namespace detail {
// Talking Pet abduction: schedule the forced encounter and travel/time cost.
inline void abductToUrquan(WorldVisit &visit)
{
    if (!visit.canInteract()) return;
    auto &entries = visit.encounters();
    auto entry = entries.insert();
    if (entry == entries.end()) entry = entries.begin(); // Preserve the full-capacity fallback.
    if (entry != entries.end()) {
        SIZE dx, dy;
        HFLEETINFO hStarShip;
        FLEET_INFO *TemplatePtr;
        BRIEF_SHIP_INFO *BSIPtr;
        const auto id = entry->runtime_id;
        *entry = {};
        entry->runtime_id = id;
        entries.select(entry);
        auto *EncounterPtr = &*entry;

		hStarShip = GetStarShipFromIndex (&GLOBAL (avail_race_q), URQUAN_SHIP);
		TemplatePtr = LockFleetInfo (&GLOBAL (avail_race_q), hStarShip);
		EncounterPtr->origin = TemplatePtr->loc;
		EncounterPtr->radius = TemplatePtr->actual_strength;
		EncounterPtr->race_id = URQUAN_SHIP;
		EncounterPtr->num_ships = 1;
		EncounterPtr->flags = ONE_SHOT_ENCOUNTER;
		BSIPtr = &EncounterPtr->ShipList[0];
		BSIPtr->race_id = URQUAN_SHIP;
		BSIPtr->crew_level = TemplatePtr->crew_level;
		BSIPtr->max_crew = TemplatePtr->max_crew;
		BSIPtr->max_energy = TemplatePtr->max_energy;
		EncounterPtr->loc_pt.x = 5288;
		EncounterPtr->loc_pt.y = 4892;
		EncounterPtr->log_x = UNIVERSE_TO_LOGX (EncounterPtr->loc_pt.x);
		EncounterPtr->log_y = UNIVERSE_TO_LOGY (EncounterPtr->loc_pt.y);
		GetPlayerTravelPosition()->position.x = EncounterPtr->log_x;
		GetPlayerTravelPosition()->position.y = EncounterPtr->log_y;
		UnlockFleetInfo (&GLOBAL (avail_race_q), hStarShip);

		{
            constexpr int LostDays = 15;
			SleepThreadUntil (FadeScreen (FadeAllToBlack, ONE_SECOND * 2));
			MoveGameClockDays (LostDays);
		}

		GLOBAL (CurrentActivity) = MAKE_WORD (IN_HYPERSPACE, 0) | START_ENCOUNTER;

		dx = visit.star().star_pt.x - EncounterPtr->loc_pt.x;
		dy = visit.star().star_pt.y - EncounterPtr->loc_pt.y;
		dx = (SIZE)square_root ((long)dx * dx + (long)dy * dy)
				+ (FUEL_TANK_SCALE >> 1);

		DeltaSISGauges (0, -dx, 0);
		if (GLOBAL_SIS (FuelOnBoard) < 5 * FUEL_TANK_SCALE)
		{
			dx = ((5 + ((COUNT)TFB_Random () % 5)) * FUEL_TANK_SCALE)
					- (SIZE)GLOBAL_SIS (FuelOnBoard);
			DeltaSISGauges (0, dx, 0);
		}
		DrawSISMessage (NULL);
		DrawHyperCoords (EncounterPtr->loc_pt);

	}
}
}

// Umgah/Talking Pet encounters, abduction and subsequent ruins reports.
inline void talkingPet(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET)
			&& (story.read(StoryFlag::UmgahZombieBlobbies)
			|| !story.read(StoryFlag::TalkingPet)
			|| StartSphereTracking (UMGAH_SHIP)))
	{
		NotifyOthers (UMGAH_SHIP, IPNL_ALL_CLEAR);
		uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

		if (StartSphereTracking (UMGAH_SHIP))
		{
			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story.set(StoryCounter::EncounterContext, 1 << 7);
			if (!story.read(StoryFlag::UmgahZombieBlobbies))
			{
				CloneShipFragment (UMGAH_SHIP,
						&GLOBAL (npc_built_ship_q), INFINITE_FLEET);
				visit.converse(UMGAH_CONVERSATION);
			}
			else
			{
				COUNT i;

				for (i = 0; i < 10; ++i)
				{
					CloneShipFragment (UMGAH_SHIP,
							&GLOBAL (npc_built_ship_q), 0);
				}
				visit.converse(TALKING_PET_CONVERSATION);
			}
		}

		if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
		{
			bool UmgahSurvivors;

			UmgahSurvivors = GetHeadLink (
					&GLOBAL (npc_built_ship_q)) != 0;
			GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;

			if (story.read(StoryFlag::PlayerHypnotized))
				detail::abductToUrquan(visit);
			else if (story.read(StoryFlag::UmgahZombieBlobbies)
					&& !UmgahSurvivors)
			{
				// Defeated the zombie fleet.
				visit.converse(TALKING_PET_CONVERSATION);
			}
			else if (!(StartSphereTracking (UMGAH_SHIP)))
			{
				// The Kohr-Ah have destroyed the Umgah, but the
				// talking pet survived.
				visit.converse(TALKING_PET_CONVERSATION);
			}

			encounter.restore();
		}

		return visit.enterPreparedOrbit();
	}

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		visit.report().load(RUINS_STRTAB);
	}

	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
		visit.data().Weather = 0;

	return visit.enterPreparedOrbit();
}

// Thraddash guards, the Aqua Helix and current-state surface reports.
inline void thraddash(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (THRADDASH_SHIP)
				&& (visit.star().Index == THRADD_DEFINED
				|| (!story.read(StoryFlag::HelixUnprotected)
				&& (BYTE)(story.read(StoryCounter::ThraddMission) - 1) >= 3)))
		{
			NotifyOthers (THRADDASH_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (THRADDASH_SHIP, &GLOBAL (npc_built_ship_q),
					INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			if (visit.star().Index == THRADD_DEFINED)
			{
				story.set(StoryCounter::EncounterContext, 1 << 7);
			}
			else
			{
				story.set(StoryCounter::EncounterContext, 1 << 6);
			}
			visit.converse(THRADD_CONVERSATION);

			if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
				return visit.enterPreparedOrbit();

			GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
			encounter.restore();

			if (visit.star().Index == THRADD_DEFINED
					|| (!story.read(StoryFlag::HelixUnprotected)
					&& (BYTE)(story.read(StoryCounter::ThraddMission) - 1) >= 3))
				return visit.enterPreparedOrbit();

			visit.restoreBorder();
		}

		if (visit.star().Index == AQUA_HELIX_DEFINED
				&& !story.read(StoryFlag::AquaHelix))
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (AQUA_MASK_PMAP_ANIM));
			visit.report().load(AQUA_STRTAB);
		}
		else if (visit.star().Index == THRADD_DEFINED)
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			visit.report().load(RUINS_STRTAB);
		}
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Ur-Quan wreck report, accounting for the portal key already retrieved.
inline void urquanWreck(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 6, MATCH_PLANET))
	{
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (WRECK_MASK_PMAP_ANIM));
		visit.report().load(WRECK_STRTAB);
		if (story.read(StoryFlag::PortalKey))
		{	// Already picked it up, skip the first report
			visit.report().select(1);
		}
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Utwig homeworld and bomb-moon guards, artifact reports and aftermath.
inline void utwig(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if ((visit.star().Index == UTWIG_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
			|| (visit.star().Index == BOMB_DEFINED
			&& matchWorld (solarSys, world, 5, 1)
			&& !story.read(StoryFlag::UtwigBomb)))
	{
		if ((visit.star().Index == UTWIG_DEFINED
				|| !story.read(StoryFlag::UtwigHaveUltron))
				&& StartSphereTracking (UTWIG_SHIP))
		{
			NotifyOthers (UTWIG_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (UTWIG_SHIP,
					&GLOBAL (npc_built_ship_q), INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			if (visit.star().Index == UTWIG_DEFINED)
			{
				story.set(StoryCounter::EncounterContext, 1 << 7);
			}
			else
			{
				story.set(StoryCounter::EncounterContext, 1 << 6);
			}
			visit.converse(UTWIG_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}
			return visit.enterPreparedOrbit();
		}

		if (visit.star().Index == BOMB_DEFINED
				&& !story.read(StoryFlag::BombUnprotected)
				&& StartSphereTracking (DRUUGE_SHIP))
		{
			COUNT i;

			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			for (i = 0; i < 5; ++i)
			{
				CloneShipFragment (DRUUGE_SHIP,
						&GLOBAL (npc_built_ship_q), 0);
			}
			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story.set(StoryCounter::EncounterContext, 1 << 6);
			visit.converse(DRUUGE_CONVERSATION);

			if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
				return visit.enterPreparedOrbit();

			{
				bool DruugeSurvivors;

				DruugeSurvivors =
						GetHeadLink (&GLOBAL (npc_built_ship_q)) != 0;

				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();

				if (DruugeSurvivors)
					return visit.enterPreparedOrbit();

				visit.restoreBorder();
				story.set(StoryFlag::BombUnprotected, (1) != 0);
			}
		}

		if (visit.star().Index == BOMB_DEFINED)
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (BOMB_MASK_PMAP_ANIM));
			visit.report().load(BOMB_STRTAB);
		}
		else
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			visit.report().load(RUINS_STRTAB);
		}
	}

	visit.prepareOrbit();

	if (visit.star().Index == UTWIG_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		visit.data().Weather = 1;
		visit.data().Tectonics = 1;
	}

	return visit.enterPreparedOrbit();
}

// VUX/ZEX encounters, Shofixti maidens, the beast and surface conditions.
inline void vux(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if ((matchWorld (solarSys, world, 0, MATCH_PLANET)
			&& (visit.star().Index == VUX_DEFINED
			|| (visit.star().Index == MAIDENS_DEFINED
			&& !story.read(StoryFlag::ZexIsDead))))
			&& StartSphereTracking (VUX_SHIP))
	{
		NotifyOthers (VUX_SHIP, IPNL_ALL_CLEAR);
		uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

		CloneShipFragment (VUX_SHIP,
				&GLOBAL (npc_built_ship_q), INFINITE_FLEET);
		if (visit.star().Index == VUX_DEFINED)
		{
			story.set(StoryCounter::EncounterContext, 1 << 7);
		}
		else
		{
			story.set(StoryCounter::EncounterContext, 1 << 6);
		}

		GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
		visit.converse(VUX_CONVERSATION);

		if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
			return visit.enterPreparedOrbit();

		{
			GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
			encounter.restore();

			if (visit.star().Index == VUX_DEFINED
					|| !story.read(StoryFlag::ZexIsDead))
				return visit.enterPreparedOrbit();

			visit.restoreBorder();
		}
	}

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (visit.star().Index == MAIDENS_DEFINED)
		{
			if (!story.read(StoryFlag::ShofixtiMaidens))
			{
				solarSys->graphics->PlanetSideFrame[1] = CaptureDrawable (
						LoadGraphic (MAIDENS_MASK_PMAP_ANIM));
				visit.report().load(MAIDENS_STRTAB);
			}
		}
		else if (visit.star().Index == VUX_BEAST_DEFINED)
		{
			if (!story.read(StoryFlag::VuxBeast))
			{
				solarSys->graphics->PlanetSideFrame[1] = 0;
				visit.report().load(BEAST_STRTAB);
			}
		}
		else
		{
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			visit.report().load(RUINS_STRTAB);
		}
	}

	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		visit.data().Weather = 2;
		visit.data().Tectonics = 0;
	}

	return visit.enterPreparedOrbit();
}

// Yehat homeworld contact or ruins.
inline void yehat(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (YEHAT_SHIP))
		{
			NotifyOthers (YEHAT_SHIP, IPNL_ALL_CLEAR);
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			CloneShipFragment (YEHAT_SHIP, &GLOBAL (npc_built_ship_q),
					INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story.set(StoryCounter::EncounterContext, 1 << 7);
			visit.converse(YEHAT_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}

			return visit.enterPreparedOrbit();
		}

		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		visit.report().load(RUINS_STRTAB);
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}

// Resolve the Zoq-Fot-Pik distress encounter before homeworld contact.
inline void zoqFotPik(WorldVisit &visit)
{
    auto &system = visit.legacySystem();
    auto &body = visit.legacyDescriptor();
    auto &story = visit.story();

    auto *solarSys = &system;
    auto *world = &body;
	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		if (StartSphereTracking (ZOQFOTPIK_SHIP))
		{
			uqm::OrbitalEncounter encounter{GLOBAL(ip_group_q), GLOBAL(npc_built_ship_q), GLOBAL(CurrentActivity)};

			if (story.read(StoryCounter::ZoqfotDistress))
			{
				CloneShipFragment (BLACK_URQUAN_SHIP,
						&GLOBAL (npc_built_ship_q), 0);

				GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
				story.set(StoryCounter::EncounterContext, 1 << 7);
				visit.converse(BLACKURQ_CONVERSATION);

				if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
					return visit.enterPreparedOrbit();

				if (GetHeadLink (&GLOBAL (npc_built_ship_q)))
				{
					GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
					encounter.restore();
					return visit.enterPreparedOrbit();
				}
			}

			CloneShipFragment (ZOQFOTPIK_SHIP, &GLOBAL (npc_built_ship_q),
					INFINITE_FLEET);

			GLOBAL (CurrentActivity) |= START_INTERPLANETARY;
			story.set(StoryCounter::EncounterContext, 1 << 7);
			visit.converse(ZOQFOTPIK_CONVERSATION);

			if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			{
				GLOBAL (CurrentActivity) &= ~START_INTERPLANETARY;
				encounter.restore();
			}

			return visit.enterPreparedOrbit();
		}

		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		visit.report().load(RUINS_STRTAB);
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}
}
#endif

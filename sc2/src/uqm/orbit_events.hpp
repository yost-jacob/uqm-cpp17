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

		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (
				LoadStringTable (ANDROSYNTH_RUINS_STRTAB));
        ConfigurePersonalEnergyBehavior (solarSys, 1, -1, NUM_RACE_RUINS,
                GetStringTableCount (solarSys->SysInfo.PlanetInfo.DiscoveryString));
        // The site remains in the shared world. This player's accepted visits
        // select the next report independently of physical retrieval masks.
		if (visits >= GetStringTableCount (
				solarSys->SysInfo.PlanetInfo.DiscoveryString))
		{	// All the reports were already given
			DestroyStringTable (ReleaseStringTable (
					solarSys->SysInfo.PlanetInfo.DiscoveryString));
			solarSys->SysInfo.PlanetInfo.DiscoveryString = 0;
		}
		else
		{	// Advance the report sequence to the first unread
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					SetRelStringTableIndex (
					solarSys->SysInfo.PlanetInfo.DiscoveryString, visits);
		}
	}

	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
		solarSys->SysInfo.PlanetInfo.AtmoDensity =
				EARTH_ATMOSPHERE * 144 / 100;
		solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 28;
		solarSys->SysInfo.PlanetInfo.Weather = 1;
		solarSys->SysInfo.PlanetInfo.Tectonics = 1;
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

	solarSys->SysInfo.PlanetInfo.ScanSeed[BIOLOGICAL_SCAN] = rand_val;
	GenerateLifeForms (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
	rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

	solarSys->SysInfo.PlanetInfo.ScanSeed[MINERAL_SCAN] = rand_val;
	GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});

	solarSys->SysInfo.PlanetInfo.ScanSeed[ENERGY_SCAN] = rand_val;

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (
				LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (
						LoadStringTable (BURV_RUINS_STRTAB));
		solarSys->SysInfo.PlanetInfo.Weather = 0;
		solarSys->SysInfo.PlanetInfo.Tectonics = 0;
	}
	else if (matchWorld (solarSys, world, 0, 0)
			&& !story.read(StoryFlag::BurvixeseBroadcasters))
	{
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] = CaptureDrawable (
				LoadGraphic (BURV_BCS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (BURV_BCS_STRTAB));
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
		LoadStdLanderFont (&system.SysInfo.PlanetInfo);
		system.SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (CHMMR_BASE_STRTAB));

		visit.reportDiscovery();

		DestroyStringTable (ReleaseStringTable (
				system.SysInfo.PlanetInfo.DiscoveryString));
		system.SysInfo.PlanetInfo.DiscoveryString = 0;
		FreeLanderFont (&system.SysInfo.PlanetInfo);

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

		solarSys->SysInfo.PlanetInfo.AtmoDensity =
				EARTH_ATMOSPHERE * 98 / 100;
		solarSys->SysInfo.PlanetInfo.Weather = 0;
		solarSys->SysInfo.PlanetInfo.Tectonics = 0;
		solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 28;

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
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (
							LoadStringTable (DRUUGE_RUINS_STRTAB));
			if (story.read(StoryFlag::RosySphere))
			{	// Already picked up Rosy Sphere, skip the report
				solarSys->SysInfo.PlanetInfo.DiscoveryString =
						SetAbsStringTableIndex (
						solarSys->SysInfo.PlanetInfo.DiscoveryString, 1);
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
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (
					LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (RUINS_STRTAB));
		}
	}

	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		solarSys->SysInfo.PlanetInfo.Weather = 2;
		solarSys->SysInfo.PlanetInfo.Tectonics = 3;
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
				if (story.read(StoryCounter::KnowAboutShattered) == 0)
					story.set(StoryCounter::KnowAboutShattered, 1);

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
		solarSys->SysInfo.PlanetInfo.AtmoDensity = EARTH_ATMOSPHERE * 2;
		solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 35;
		solarSys->SysInfo.PlanetInfo.Weather = 3;
		solarSys->SysInfo.PlanetInfo.Tectonics = 1;
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
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (
					LoadGraphic (TAALO_DEVICE_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (
					LoadStringTable (TAALO_DEVICE_STRTAB));
		}
		else
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (RUINS_STRTAB));
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
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (PKUNK_RUINS_STRTAB));
			if (story.read(StoryFlag::ClearSpindle))
			{	// Already picked up the Clear Spindle, skip the report
				solarSys->SysInfo.PlanetInfo.DiscoveryString =
						SetAbsStringTableIndex (
						solarSys->SysInfo.PlanetInfo.DiscoveryString, 1);
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
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (VAULT_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (VAULT_STRTAB));
		if (story.read(StoryFlag::ShipVaultUnlocked))
		{
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					SetAbsStringTableIndex (
					solarSys->SysInfo.PlanetInfo.DiscoveryString, 2);
		}
		else if (story.read(StoryFlag::SyreenShuttleOnShip))
		{
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					SetAbsStringTableIndex (
					solarSys->SysInfo.PlanetInfo.DiscoveryString, 1);
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

	solarSys->SysInfo.PlanetInfo.ScanSeed[MINERAL_SCAN] = rand_val;
	GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
	rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

	planetNr = planetIndex (solarSys, world);
	if (worldIsPlanet (solarSys, world))
	{
		switch (planetNr)
		{
			case 0: /* MERCURY */
				solarSys->SysInfo.PlanetInfo.AtmoDensity = 0;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 98;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 38;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 3;
				solarSys->SysInfo.PlanetInfo.Weather = 0;
				solarSys->SysInfo.PlanetInfo.Tectonics = 2;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 59 * 240;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 165;
				break;
			case 1: /* VENUS */
				solarSys->SysInfo.PlanetInfo.AtmoDensity = 90 *
						EARTH_ATMOSPHERE;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 95;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 95;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 177;
				solarSys->SysInfo.PlanetInfo.Weather = 7;
				solarSys->SysInfo.PlanetInfo.Tectonics = 1;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 243 * 240;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 457;
				break;
			case 2: /* EARTH */
				solarSys->SysInfo.PlanetInfo.AtmoDensity =
						EARTH_ATMOSPHERE;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 100;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 100;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 23;
				solarSys->SysInfo.PlanetInfo.Weather = 1;
				solarSys->SysInfo.PlanetInfo.Tectonics = 1;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 240;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 22;
				break;
			case 3: /* MARS */
				// XXX: Mars atmo should actually be 1/2 in current units
				solarSys->SysInfo.PlanetInfo.AtmoDensity = 1;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 72;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 53;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 24;
				solarSys->SysInfo.PlanetInfo.Weather = 1;
				solarSys->SysInfo.PlanetInfo.Tectonics = 1;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 246;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -53;
				break;
			case 4: /* JUPITER */
				solarSys->SysInfo.PlanetInfo.AtmoDensity =
						GAS_GIANT_ATMOSPHERE;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 24;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 1120;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 3;
				solarSys->SysInfo.PlanetInfo.Weather = 7;
				solarSys->SysInfo.PlanetInfo.Tectonics = 0;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 98;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -143;
				solarSys->SysInfo.PlanetInfo.PlanetToSunDist =
						EARTH_RADIUS * 520L / 100;
				break;
			case 5: /* SATURN */
				solarSys->SysInfo.PlanetInfo.AtmoDensity =
						GAS_GIANT_ATMOSPHERE;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 13;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 945;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 27;
				solarSys->SysInfo.PlanetInfo.Weather = 7;
				solarSys->SysInfo.PlanetInfo.Tectonics = 0;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 102;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -197;
				solarSys->SysInfo.PlanetInfo.PlanetToSunDist =
						EARTH_RADIUS * 952L / 100;
				break;
			case 6: /* URANUS */
				solarSys->SysInfo.PlanetInfo.AtmoDensity =
						GAS_GIANT_ATMOSPHERE;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 21;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 411;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 98;
				solarSys->SysInfo.PlanetInfo.Weather = 7;
				solarSys->SysInfo.PlanetInfo.Tectonics = 0;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 172;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -217;
				solarSys->SysInfo.PlanetInfo.PlanetToSunDist =
						EARTH_RADIUS * 1916L / 100;
				break;
			case 7: /* NEPTUNE */
				solarSys->SysInfo.PlanetInfo.AtmoDensity =
						GAS_GIANT_ATMOSPHERE;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 28;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 396;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 30;
				solarSys->SysInfo.PlanetInfo.Weather = 7;
				solarSys->SysInfo.PlanetInfo.Tectonics = 0;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 182;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -229;
				solarSys->SysInfo.PlanetInfo.PlanetToSunDist =
						EARTH_RADIUS * 2999L / 100;
				break;
			case 8: /* PLUTO */
				if (!story.read(StoryCounter::FoundPlutoSpathi))
				{
					LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
					solarSys->graphics->PlanetSideFrame[1] =
							CaptureDrawable (
							LoadGraphic (SPAPLUTO_MASK_PMAP_ANIM));
					solarSys->SysInfo.PlanetInfo.DiscoveryString =
							CaptureStringTable (
							LoadStringTable (SPAPLUTO_STRTAB));
				}

				solarSys->SysInfo.PlanetInfo.AtmoDensity = 0;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 33;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 18;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 119;
				solarSys->SysInfo.PlanetInfo.Weather = 0;
				solarSys->SysInfo.PlanetInfo.Tectonics = 0;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 1533;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -235;
				solarSys->SysInfo.PlanetInfo.PlanetToSunDist =
						EARTH_RADIUS * 3937L / 100;
				break;
		}

		solarSys->SysInfo.PlanetInfo.SurfaceGravity =
				CalcGravity (&solarSys->SysInfo.PlanetInfo);
		visit.loadSurface(planetNr == 2 ?
				CaptureDrawable (LoadGraphic (EARTH_MASK_ANIM)) : NULL);
	}
	else
	{
		// World is a moon.
		COUNT moonNr = moonIndex (solarSys, world);

		solarSys->SysInfo.PlanetInfo.AxialTilt = 0;
		solarSys->SysInfo.PlanetInfo.AtmoDensity = 0;
		solarSys->SysInfo.PlanetInfo.Weather = 0;
		switch (planetNr)
		{
			case 2: /* moons of EARTH */
				// NOTE: Even though we save the seed here, it is irrelevant.
				//   The seed will be used to randomly place the tractors, but
				//   since they are mobile, they will be moved to different
				//   locations not governed by this seed.
				solarSys->SysInfo.PlanetInfo.ScanSeed[BIOLOGICAL_SCAN] =
						rand_val;

				if (!story.read(StoryFlag::MoonbaseDestroyed))
				{
					LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
					solarSys->graphics->PlanetSideFrame[1] =
							CaptureDrawable (
							LoadGraphic (MOONBASE_MASK_PMAP_ANIM));
					solarSys->SysInfo.PlanetInfo.DiscoveryString =
							CaptureStringTable (
							LoadStringTable (MOONBASE_STRTAB));
				}

				solarSys->SysInfo.PlanetInfo.PlanetDensity = 60;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 25;
				solarSys->SysInfo.PlanetInfo.AxialTilt = 0;
				solarSys->SysInfo.PlanetInfo.Tectonics = 0;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 240 * 29;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -18;
				break;

			case 4: /* moons of JUPITER */
				solarSys->SysInfo.PlanetInfo.PlanetToSunDist =
						EARTH_RADIUS * 520L / 100;
				switch (moonNr)
				{
					case 0: /* Io */
						solarSys->SysInfo.PlanetInfo.PlanetDensity = 69;
						solarSys->SysInfo.PlanetInfo.PlanetRadius = 25;
						solarSys->SysInfo.PlanetInfo.Tectonics = 3;
						solarSys->SysInfo.PlanetInfo.RotationPeriod = 390;
						solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -163;
						break;
					case 1: /* Europa */
						solarSys->SysInfo.PlanetInfo.PlanetDensity = 54;
						solarSys->SysInfo.PlanetInfo.PlanetRadius = 25;
						solarSys->SysInfo.PlanetInfo.Tectonics = 1;
						solarSys->SysInfo.PlanetInfo.RotationPeriod = 840;
						solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -161;
						break;
					case 2: /* Ganymede */
						solarSys->SysInfo.PlanetInfo.PlanetDensity = 35;
						solarSys->SysInfo.PlanetInfo.PlanetRadius = 41;
						solarSys->SysInfo.PlanetInfo.Tectonics = 0;
						solarSys->SysInfo.PlanetInfo.RotationPeriod = 1728;
						solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -164;
						break;
					case 3: /* Callisto */
						solarSys->SysInfo.PlanetInfo.PlanetDensity = 35;
						solarSys->SysInfo.PlanetInfo.PlanetRadius = 38;
						solarSys->SysInfo.PlanetInfo.Tectonics = 1;
						solarSys->SysInfo.PlanetInfo.RotationPeriod = 4008;
						solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -167;
						break;
				}
				break;

			case 5: /* moon of SATURN: Titan */
				solarSys->SysInfo.PlanetInfo.PlanetToSunDist =
						EARTH_RADIUS * 952L / 100;
				solarSys->SysInfo.PlanetInfo.AtmoDensity = 160;
				solarSys->SysInfo.PlanetInfo.Weather = 2;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 34;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 40;
				solarSys->SysInfo.PlanetInfo.Tectonics = 1;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 3816;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -178;
				break;

			case 7: /* moon of NEPTUNE: Triton */
				solarSys->SysInfo.PlanetInfo.PlanetToSunDist =
						EARTH_RADIUS * 2999L / 100;
				solarSys->SysInfo.PlanetInfo.AtmoDensity = 10;
				solarSys->SysInfo.PlanetInfo.Weather = 1;
				solarSys->SysInfo.PlanetInfo.PlanetDensity = 95;
				solarSys->SysInfo.PlanetInfo.PlanetRadius = 27;
				solarSys->SysInfo.PlanetInfo.Tectonics = 0;
				solarSys->SysInfo.PlanetInfo.RotationPeriod = 4300;
				solarSys->SysInfo.PlanetInfo.SurfaceTemperature = -216;
				break;
		}

		solarSys->SysInfo.PlanetInfo.SurfaceGravity =
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

		solarSys->SysInfo.PlanetInfo.ScanSeed[BIOLOGICAL_SCAN] = rand_val;
		GenerateLifeForms (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
		rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

		solarSys->SysInfo.PlanetInfo.ScanSeed[MINERAL_SCAN] = rand_val;
		GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});

		solarSys->SysInfo.PlanetInfo.ScanSeed[ENERGY_SCAN] = rand_val;

		solarSys->SysInfo.PlanetInfo.Weather = 0;
		solarSys->SysInfo.PlanetInfo.Tectonics = 0;
		solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 28;
		if (!story.read(StoryFlag::UmgahBroadcasters))
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (UMGAH_BCS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (UMGAH_BCS_STRTAB));
			if (!story.read(StoryFlag::SpathiShieldedSelves))
			{	// The first report talks extensively about Spathi
				// slave-shielding selves. If they never did so, the report
				// makes no sense, so use an alternate.
				solarSys->SysInfo.PlanetInfo.DiscoveryString =
						SetAbsStringTableIndex (
						solarSys->SysInfo.PlanetInfo.DiscoveryString, 1);
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

		solarSys->SysInfo.PlanetInfo.ScanSeed[MINERAL_SCAN] = rand_val;
		GenerateMineralDeposits (&solarSys->SysInfo, GENERATE_ALL, NULL, &uqm::generationRandom(system), EXTENT{static_cast<COORD>(MAP_WIDTH), static_cast<COORD>(MAP_HEIGHT)});
		rand_val = RandomContext_GetSeed (&uqm::generationRandom(system));

		solarSys->SysInfo.PlanetInfo.ScanSeed[BIOLOGICAL_SCAN] = rand_val;

		solarSys->SysInfo.PlanetInfo.PlanetRadius = 120;
		solarSys->SysInfo.PlanetInfo.SurfaceGravity =
				CalcGravity (&solarSys->SysInfo.PlanetInfo);
		solarSys->SysInfo.PlanetInfo.Weather = 0;
		solarSys->SysInfo.PlanetInfo.Tectonics = 0;
		solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 31;

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
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (SUPOX_RUINS_STRTAB));
			if (story.read(StoryCounter::UltronCondition))
			{	// Already picked up the Ultron, skip the report
				solarSys->SysInfo.PlanetInfo.DiscoveryString =
						SetAbsStringTableIndex (
						solarSys->SysInfo.PlanetInfo.DiscoveryString, 1);
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

		solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 19;
		solarSys->SysInfo.PlanetInfo.Tectonics = 0;
		solarSys->SysInfo.PlanetInfo.Weather = 0;
		solarSys->SysInfo.PlanetInfo.AtmoDensity = EARTH_ATMOSPHERE * 9 / 10;
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
	HENCOUNTER hEncounter;

	if ((hEncounter = AllocEncounter ()) || (hEncounter = GetHeadEncounter ()))
	{
		SIZE dx, dy;
		ENCOUNTER *EncounterPtr;
		HFLEETINFO hStarShip;
		FLEET_INFO *TemplatePtr;
		BRIEF_SHIP_INFO *BSIPtr;

		LockEncounter (hEncounter, &EncounterPtr);

		if (hEncounter == GetHeadEncounter ())
			RemoveEncounter (hEncounter);
		*EncounterPtr = {};

		InsertEncounter (hEncounter, GetHeadEncounter ());

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

		UnlockEncounter (hEncounter);
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
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (RUINS_STRTAB));
	}

	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
		solarSys->SysInfo.PlanetInfo.Weather = 0;

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
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (AQUA_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (AQUA_STRTAB));
		}
		else if (visit.star().Index == THRADD_DEFINED)
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (RUINS_STRTAB));
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
		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (WRECK_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (WRECK_STRTAB));
		if (story.read(StoryFlag::PortalKey))
		{	// Already picked it up, skip the first report
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					SetAbsStringTableIndex (
					solarSys->SysInfo.PlanetInfo.DiscoveryString, 1);
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
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (BOMB_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (BOMB_STRTAB));
		}
		else
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (RUINS_STRTAB));
		}
	}

	visit.prepareOrbit();

	if (visit.star().Index == UTWIG_DEFINED
			&& matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		solarSys->SysInfo.PlanetInfo.Weather = 1;
		solarSys->SysInfo.PlanetInfo.Tectonics = 1;
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
				LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
				solarSys->graphics->PlanetSideFrame[1] = CaptureDrawable (
						LoadGraphic (MAIDENS_MASK_PMAP_ANIM));
				solarSys->SysInfo.PlanetInfo.DiscoveryString =
						CaptureStringTable (
						LoadStringTable (MAIDENS_STRTAB));
			}
		}
		else if (visit.star().Index == VUX_BEAST_DEFINED)
		{
			if (!story.read(StoryFlag::VuxBeast))
			{
				LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
				solarSys->graphics->PlanetSideFrame[1] = 0;
				solarSys->SysInfo.PlanetInfo.DiscoveryString =
						CaptureStringTable (
						LoadStringTable (BEAST_STRTAB));
			}
		}
		else
		{
			LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
			solarSys->graphics->PlanetSideFrame[1] =
					CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					CaptureStringTable (LoadStringTable (RUINS_STRTAB));
		}
	}

	visit.prepareOrbit();

	if (matchWorld (solarSys, world, 0, MATCH_PLANET))
	{
		solarSys->SysInfo.PlanetInfo.Weather = 2;
		solarSys->SysInfo.PlanetInfo.Tectonics = 0;
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

		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (RUINS_STRTAB));
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

		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->graphics->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (LoadStringTable (RUINS_STRTAB));
	}

	visit.prepareOrbit();
	return visit.enterPreparedOrbit();
}
}
#endif

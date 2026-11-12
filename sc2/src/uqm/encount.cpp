#include "libs/threadlib.h"
#include "game_data.hpp"
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

#include "encount.h"
#include "simulation_engine.hpp"
#include "encounter_runtime.hpp"
#include "fleet_legacy.hpp"
#include "game_state.hpp"
#include <iterator>

#include "battle.h"
#include "build.h"
#include "colors.h"
#include "starmap.h"
#include "cons_res.h"
#include "controls.h"
#include "menustat.h"
#include "gameopt.h"
#include "gamestr.h"
#include "globdata.h"
#include "sis.h"
		// for DrawStatusMessage(), SetStatusMessageMode()
#include "init.h"
#include "pickship.h"
#include "intel.h"
#include "nameref.h"
#include "resinst.h"
#include "settings.h"
#include "setup.h"
#include "sounds.h"
#include "libs/graphics/gfx_common.h"
#include "libs/log.h"
#include "libs/mathlib.h"
#include "libs/inplib.h"


extern "C" {
extern FRAME planet[];
}

static void DrawFadeText (const UNICODE *str1, const UNICODE *str2,
		BOOLEAN fade_in, RECT *pRect);


static BOOLEAN
DoSelectAction (MENU_STATE *pMS)
{
	SetMenuSounds (MENU_SOUND_ARROWS, MENU_SOUND_SELECT);
	if (GLOBAL (CurrentActivity) & CHECK_ABORT)
	{
		pMS->CurState = ATTACK + 1;
		return (FALSE);
	}
	if (!pMS->Initialized)
	{
		pMS->Initialized = TRUE;
		pMS->InputFunc = DoSelectAction;
	}
	else if (PulsedInputState.menu[KEY_MENU_SELECT])
	{
		switch (pMS->CurState)
		{
			case HAIL:
			case ATTACK:
				if (LOBYTE (GLOBAL (CurrentActivity)) == IN_LAST_BATTLE)
					pMS->CurState = HAIL;
				return (FALSE);
			case ATTACK + 1:
				// Clearing FlashRect is not necessary
				if (!GameOptions ())
					return FALSE;
				DrawMenuStateStrings (PM_CONVERSE, pMS->CurState);
				SetFlashRect (SFR_MENU_3DO);
				break;
			default:
				printf ("Unknown option: %d\n", pMS->CurState);
		}
	}
	DoMenuChooser (pMS, PM_CONVERSE);
	return (TRUE);
}

static QUEUE *
GetShipFragQueueForPlayer (COUNT playerNr)
{
	if (playerNr == RPG_PLAYER_NUM)
		return GetPlayerFleetQueue();
	else
		return &GLOBAL (npc_built_ship_q);
}

// Called by comm code to intialize battle fleets during encounter
void uqm::EncounterRuntime::buildBattle(RosterSide side)
{
    EncounterRace = -1;
    if (!GetHeadLink(&GLOBAL(npc_built_ship_q))) {
        story_.set(StoryFlag::BattleSegue, false);
        return;
    }

    if (side == RosterSide::Second) {
        // The NPC side is prepared first, before conversation and combat. Its
        // arena belongs to the encounter, including exceptional exits.
        switch (LOBYTE(GLOBAL(CurrentActivity))) {
            case IN_LAST_BATTLE:
                arena_.load(NUMBER_OF_PLANET_TYPES);
                break;
            case IN_HYPERSPACE:
                arena_.load(static_cast<BYTE>(static_cast<COUNT>(engine_.random()())
                    % NUMBER_OF_PLANET_TYPES));
                break;
            default:
                story_.set(StoryCounter::EscapeCounter, 110);
                arena_.load(story_.read(StoryCounter::BattlePlanet));
                break;
        }
    }

    auto &source = side == RosterSide::First && session_
        ? detail::FleetAccess::queue(session_->player().fleet())
        : *GetShipFragQueueForPlayer(static_cast<COUNT>(side));
    auto &roster = engine_.shipRoster();
    roster.clear(side);
    for (auto handle = GetHeadLink(&source); handle;) {
        const auto &fragment = *LockShipFrag(&source, handle);
        const auto next = _GetSuccLink(&fragment);
        if (auto participant = roster.append(side,
                fragment.race_id == SAMATRA_SHIP ? SA_MATRA_ID : fragment.SpeciesID)) {
            auto &ship = participant->get();
            ship.fleet_owner = &source;
            ship.fleet_ship_id = fragment.ship_id;
            ship.captains_name_index = fragment.captains_name_index;
            ship.crew_level = fragment.crew_level == INFINITE_FLEET
                ? fragment.max_crew : fragment.crew_level;
            ship.max_crew = fragment.max_crew;
            ship.race_strings = fragment.race_strings;
            ship.icons = fragment.icons;
            ship.index = fragment.index;
        }
        UnlockShipFrag(&source, handle);
        handle = next;
    }

    if (side == RosterSide::First) {
        if (auto flagship = roster.append(side, SIS_SHIP_ID)) {
            // Crew/equipment are read from the player's SIS when spawned.
            flagship->get().index = static_cast<COUNT>(-1);
            flagship->get().energy_counter = MAX_ENERGY_SIZE;
        }
    }
}

BOOLEAN
FleetIsInfinite (COUNT playerNr)
{
	QUEUE *pQueue;
	HSHIPFRAG hShipFrag;
	SHIP_FRAGMENT *FragPtr;
	BOOLEAN ret;

	pQueue = GetShipFragQueueForPlayer (playerNr);
	hShipFrag = GetHeadLink (pQueue);
	if (!hShipFrag)
	{	// Ship queue is empty in SuperMelee or for RPG player w/o escorts
		return FALSE;
	}

	FragPtr = LockShipFrag (pQueue, hShipFrag);
	ret = FragPtr->crew_level == INFINITE_FLEET ? TRUE : FALSE;
	UnlockShipFrag (pQueue, hShipFrag);

	return ret;
}


/*
 * Encountering an alien.
 * Draws the encounter screen, plays the red alert music, and
 * waits for a decision of the player on how to handle the situation.
 * Returns either HAIL or ATTACK.
 */
COUNT
uqm::EncounterRuntime::selectAction()
{
	COUNT i;
	FRAME SegueFrame;
	STAMP s;
	TEXT t;
	MUSIC_REF MR;


	SetContext (SpaceContext);
	SetContextFont (TinyFont);

	MR = LoadMusic (REDALERT_MUSIC);
	PlayMusic (MR, FALSE, 1);
	SegueFrame = CaptureDrawable (LoadGraphic (SEGUE_PMAP_ANIM));
	WaitForSoundEnd (TFBSOUND_WAIT_ALL);
	StopMusic ();
	DestroyMusic (MR);
	s.origin.x = s.origin.y = 0;

	SetTransitionSource (NULL);
	BatchGraphics ();
	
	SetContextBackGroundColor (BLACK_COLOR);
	ClearDrawable ();
	s.frame = SegueFrame;
	DrawStamp (&s);

//    t.baseline.x = SIS_SCREEN_WIDTH >> 1;
	t.baseline.x = (SIS_SCREEN_WIDTH >> 1) + 1;
	t.baseline.y = 10;
	t.align = ALIGN_CENTER;

	SetContextFont (MicroFont);
	SetContextForeGroundColor (
			BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x14), 0x01));
	if (inHQSpace ())
	{
		t.pStr = GAME_STRING (ENCOUNTER_STRING_BASE + 0);
				// "ENCOUNTER IN"
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
		t.baseline.y += 12;
		t.pStr = GAME_STRING (ENCOUNTER_STRING_BASE + 1);
				// "DEEP SPACE"
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
	}
	else
	{
		UNICODE buf[256];

		t.pStr = GAME_STRING (ENCOUNTER_STRING_BASE + 2);
				// "ENCOUNTER AT"
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
		t.baseline.y += 12;
		GetClusterName (&star(), buf, sizeof(buf));
		t.pStr = buf;
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
		t.baseline.y += 12;
		t.pStr = GLOBAL_SIS (PlanetName);
		t.CharCount = (COUNT)~0;
		font_DrawText (&t);
	}
	DrawSISMessage (NULL);

	s.origin.x = SIS_SCREEN_WIDTH >> 1;
	s.origin.y = SIS_SCREEN_HEIGHT >> 1;
	s.frame = planet[0];
	DrawStamp (&s);

	if (LOBYTE (GLOBAL (CurrentActivity)) != IN_LAST_BATTLE)
	{
		HSHIPFRAG hStarShip, hNextShip;
		static constexpr POINT display_pt[] =
		{
			{ 10,  51},
			{-10,  51},
			{ 33,  40},
			{-33,  40},
			{ 49,  18},
			{-49,  18},
			{ 52,  -6},
			{-52,  -6},
			{ 44, -27},
			{-44, -27},
		};
        constexpr auto NUM_DISPLAY_PTS = std::size(display_pt);

		for (hStarShip = GetHeadLink (&GLOBAL (npc_built_ship_q)), i = 0;
				hStarShip && i < 60; hStarShip = hNextShip, ++i)
		{
			RECT r;
			SHIP_FRAGMENT *FragPtr;

			FragPtr = LockShipFrag (&GLOBAL (npc_built_ship_q), hStarShip);
			if (FragPtr->crew_level != INFINITE_FLEET)
				hNextShip = _GetSuccLink (FragPtr);
			else /* if infinite ships */
				hNextShip = hStarShip;

			s.origin = display_pt[i % NUM_DISPLAY_PTS];
			if (i >= NUM_DISPLAY_PTS)
			{
				COUNT angle, radius;

				radius = square_root ((long)s.origin.x * s.origin.x
						+ (long)s.origin.y * s.origin.y)
						+ ((i / NUM_DISPLAY_PTS) * 18);

				angle = ARCTAN (s.origin.x, s.origin.y);
				s.origin.x = COSINE (angle, radius);
				s.origin.y = SINE (angle, radius);
			}
			s.frame = SetAbsFrameIndex (FragPtr->icons, 0);
			GetFrameRect (s.frame, &r);
			s.origin.x += (SIS_SCREEN_WIDTH >> 1) - (r.extent.width >> 1);
			s.origin.y += (SIS_SCREEN_HEIGHT >> 1) - (r.extent.height >> 1);
			DrawStamp (&s);

			UnlockShipFrag (&GLOBAL (npc_built_ship_q), hStarShip);
		}
	}

	UnbatchGraphics ();
	DestroyDrawable (ReleaseDrawable (SegueFrame));
	ScreenTransition (3, NULL);


	{
		MENU_STATE MenuState{};

		MenuState.InputFunc = DoSelectAction;
		MenuState.Initialized = FALSE;

		DrawMenuStateStrings (PM_CONVERSE, MenuState.CurState = HAIL);
		SetFlashRect (SFR_MENU_3DO);

		DoInput (&MenuState, TRUE);

		SetFlashRect (NULL);

		return (MenuState.CurState);
	}
}

static void
DrawFadeText (const UNICODE *str1, const UNICODE *str2, BOOLEAN fade_in,
		RECT *pRect)
{
	SIZE i;
	DWORD TimeIn;
	TEXT t1, t2;
	static const Color fade_cycle[] =
	{
		BUILD_COLOR (MAKE_RGB15_INIT (0x0A, 0x0A, 0x0A), 0x1D),
		BUILD_COLOR (MAKE_RGB15_INIT (0x09, 0x09, 0x09), 0x1E),
		BUILD_COLOR (MAKE_RGB15_INIT (0x08, 0x08, 0x08), 0x1F),
		BUILD_COLOR (MAKE_RGB15_INIT (0x06, 0x06, 0x06), 0x20),
		BUILD_COLOR (MAKE_RGB15_INIT (0x05, 0x05, 0x05), 0x21),
		BUILD_COLOR (MAKE_RGB15_INIT (0x04, 0x04, 0x04), 0x22),
		BUILD_COLOR (MAKE_RGB15_INIT (0x03, 0x03, 0x03), 0x23),
	};
    constexpr auto NUM_FADES = std::size(fade_cycle);

	t1.baseline.x = pRect->corner.x + 100;
	t1.baseline.y = pRect->corner.y + 45;
	t1.align = ALIGN_CENTER;
	t1.pStr = str1;
	t1.CharCount = (COUNT)~0;
	t2 = t1;
	t2.baseline.y += 11;
	t2.pStr = str2;

	FlushInput ();
	TimeIn = GetTimeCounter ();
	if (fade_in)
	{
		for (i = 0; i < (SIZE) NUM_FADES; ++i)
		{
			if (AnyButtonPress (TRUE))
				i = NUM_FADES - 1;

			SetContextForeGroundColor (fade_cycle[i]);
			font_DrawText (&t1);
			font_DrawText (&t2);
			SleepThreadUntil (TimeIn + (ONE_SECOND / 20));
			TimeIn = GetTimeCounter ();
		}
	}
	else
	{
		for (i = NUM_FADES - 1; i >= 0; --i)
		{
			if (AnyButtonPress (TRUE))
				i = 0;

			SetContextForeGroundColor (fade_cycle[i]);
			font_DrawText (&t1);
			font_DrawText (&t2);
			SleepThreadUntil (TimeIn + (ONE_SECOND / 20));
			TimeIn = GetTimeCounter ();
		}
		SetContextForeGroundColor (
				BUILD_COLOR (MAKE_RGB15 (0x0A, 0x0A, 0x0A), 0x08));
		font_DrawText (&t1);
		font_DrawText (&t2);
	}
}

COUNT
uqm::EncounterRuntime::finish()
{
	COUNT ships_killed;

	ships_killed = 0;

    arena_.reset(); // Release presentation before tallying; never apply rewards in a destructor.

	if ((GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
			|| GLOBAL_SIS (CrewEnlisted) == (COUNT)~0
			|| LOBYTE (GLOBAL (CurrentActivity)) == IN_LAST_BATTLE
			|| LOBYTE (GLOBAL (CurrentActivity)) == WON_LAST_BATTLE)
		goto ExitUninitEncounter;

	if (story_.read(uqm::StoryFlag::BattleSegue) == 0)
	{
		engine_.shipRoster().clear();
	}
	else
	{
		BOOLEAN Sleepy;
		SIZE VictoryState;
		COUNT RecycleAmount = 0;
		SIZE i;
		RECT r;
		RECT scavenge_r = {{0, 0}, {0, 0}};
		TEXT t;
		STAMP ship_s;
		const UNICODE *str1 = NULL;
		const UNICODE *str2 = NULL;
		StatMsgMode prevMsgMode;
		UNICODE buf[80];
		HSHIPFRAG hStarShip;
		SHIP_FRAGMENT *FragPtr;
		static const Color fade_ship_cycle[] =
		{
			BUILD_COLOR (MAKE_RGB15_INIT (0x07, 0x00, 0x00), 0x2F),
			BUILD_COLOR (MAKE_RGB15_INIT (0x0F, 0x00, 0x00), 0x2D),
			BUILD_COLOR (MAKE_RGB15_INIT (0x17, 0x00, 0x00), 0x2B),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x0A, 0x0A), 0x27),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x14, 0x14), 0x25),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x1F, 0x1F), 0x0F),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x14, 0x14), 0x25),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1F, 0x0A, 0x0A), 0x27),
			BUILD_COLOR (MAKE_RGB15_INIT (0x1B, 0x00, 0x00), 0x2A),
			BUILD_COLOR (MAKE_RGB15_INIT (0x17, 0x00, 0x00), 0x2B),
		};
        constexpr auto NUM_SHIP_FADES = std::size(fade_ship_cycle);



		story_.set(uqm::StoryFlag::BattleSegue, 0);
		story_.set(uqm::StoryFlag::BombCarrier, 0);

		VictoryState = (
				engine_.shipRoster().remaining(RosterSide::Second)
                || !engine_.shipRoster().remaining(RosterSide::First)
				|| story_.read(uqm::StoryFlag::UrquanProtectingSamatra)
				) ? 0 : 1;

		hStarShip = GetHeadLink (&GLOBAL (npc_built_ship_q));
		FragPtr = LockShipFrag (&GLOBAL (npc_built_ship_q), hStarShip);
		EncounterRace = FragPtr->race_id;
		if (GetStarShipFromIndex (&GLOBAL (avail_race_q), EncounterRace) == 0)
		{
			/* Suppress the final tally and salvage info */
			VictoryState = -1;
			InitSISContexts ();
		}
		UnlockShipFrag (&GLOBAL (npc_built_ship_q), hStarShip);

		prevMsgMode = SetStatusMessageMode (SMM_RES_UNITS);
		ship_s.origin.x = 0;
		ship_s.origin.y = 0;
		Sleepy = TRUE;
		for (i = 0; i < NUM_SIDES; ++i)
		{
			QUEUE *pQueue;
			HSHIPFRAG hNextShip;

			if (i == 0)
				pQueue = GetPlayerFleetQueue();
			else
			{
				if (VictoryState < 0)
					VictoryState = 0;
				else
				{
					DrawSISFrame ();
					DrawSISMessage (NULL);
					if (inHQSpace ())
						DrawHyperCoords (GLOBAL (ShipStamp.origin));
					else if (GetPlayerTravelPosition()->flight.planet == 0)
						DrawHyperCoords (star().star_pt);
					else
						DrawSISTitle(GLOBAL_SIS (PlanetName));

					SetContext (SpaceContext);
					if (VictoryState)
						DrawArmadaPickShip (TRUE, &scavenge_r);
				}
				pQueue = &GLOBAL (npc_built_ship_q);
			}
				
			engine_.shipRoster().clear(static_cast<RosterSide>((NUM_SIDES - 1) - i));

			for (hStarShip = GetHeadLink (pQueue); hStarShip;
					hStarShip = hNextShip)
			{
				FragPtr = LockShipFrag (pQueue, hStarShip);
				hNextShip = _GetSuccLink (FragPtr);

				if (FragPtr->crew_level == 0
						|| (VictoryState && i == NUM_SIDES - 1))
				{
					if (i == NUM_SIDES - 1)
					{
						++ships_killed;
						if (VictoryState)
						{
                            constexpr int MAX_DEAD_DISPLAYED = 5;
							COUNT j;

							if (ships_killed == 1)
							{
								RecycleAmount = 0;

								DrawStatusMessage (NULL);
								
								ship_s.origin.x = scavenge_r.corner.x + 32;
								ship_s.origin.y = scavenge_r.corner.y + 56;
								ship_s.frame = IncFrameIndex (FragPtr->icons);
								DrawStamp (&ship_s);
								SetContextForeGroundColor (
										BUILD_COLOR (MAKE_RGB15 (0x08, 0x08, 0x08), 0x1F));
								SetContextFont (TinyFont);

								utf8StringCopy (buf, sizeof buf,
										GetStringAddress (FragPtr->race_strings));
								// XXX: this will not work with UTF-8 strings
								strupr (buf);

								t.baseline.x = scavenge_r.corner.x + 100;
								t.baseline.y = scavenge_r.corner.y + 68;
								t.align = ALIGN_CENTER;
								t.pStr = buf;
								t.CharCount = (COUNT)~0;
								font_DrawText (&t);
								t.baseline.y += 6;
								t.pStr = GAME_STRING (
										ENCOUNTER_STRING_BASE + 3);
										// "BATTLE GROUP"
								t.CharCount = (COUNT)~0;
								font_DrawText (&t);

								ship_s.frame = FragPtr->icons;

								SetContextFont (MicroFont);
								str1 = GAME_STRING (
										ENCOUNTER_STRING_BASE + 4);
										// "Enemy Ships"
								str2 = GAME_STRING (
										ENCOUNTER_STRING_BASE + 5),
										// "Destroyed"
								DrawFadeText (str1, str2, TRUE, &scavenge_r);
							}

							r.corner.y = scavenge_r.corner.y + 9;
							r.extent.height = 22;

							SetContextForeGroundColor (BLACK_COLOR);

							r.extent.width = 34;
							r.corner.x = scavenge_r.corner.x +
									scavenge_r.extent.width
									- (10 + r.extent.width);
							DrawFilledRectangle (&r);

							/* collect bounty ResUnits */
							j = uqm::shipPrice(EncounterRace) >> 3;
							RecycleAmount += j;
							sprintf (buf, "%u", RecycleAmount);
							t.baseline.x = r.corner.x + r.extent.width - 1;
							t.baseline.y = r.corner.y + 14;
							t.align = ALIGN_RIGHT;
							t.pStr = buf;
							t.CharCount = (COUNT)~0;
							SetContextForeGroundColor (
									BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x18), 0x50));
							font_DrawText (&t);
							DeltaSISGauges (0, 0, j);

							if ((VictoryState++ - 1) % MAX_DEAD_DISPLAYED)
								ship_s.origin.x += 17;
							else
							{
								SetContextForeGroundColor (BLACK_COLOR);

								r.corner.x = scavenge_r.corner.x + 10;
								r.extent.width = 104;
								DrawFilledRectangle (&r);

								ship_s.origin.x = r.corner.x + 2;
								ship_s.origin.y = scavenge_r.corner.y + 12;
							}

							if (Sleepy)
							{
								TimeCount Time = GetTimeCounter ();
								for (j = 0; j < NUM_SHIP_FADES; ++j)
								{
									Sleepy = !AnyButtonPress (TRUE) &&
											!(GLOBAL (CurrentActivity) & CHECK_ABORT) ? TRUE : FALSE;
									if (!Sleepy)
										break;

									SetContextForeGroundColor (fade_ship_cycle[j]);
									DrawFilledStamp (&ship_s);

									SleepThreadUntil (Time + (ONE_SECOND / 15));
									Time = GetTimeCounter ();
								}
							}
							DrawStamp (&ship_s);
						}
					}

                    const uqm::ShipId vessel{FragPtr->ship_id};
					UnlockShipFrag (pQueue, hStarShip);
                    if (pQueue == GetPlayerFleetQueue()) {
                        auto &player = session_ ? session_->player() : uqm::gameSession().player();
                        player.fleet().erase(vessel);
                    } else {
					    RemoveQueue (pQueue, hStarShip);
					    FreeShipFrag (pQueue, hStarShip);
                    }

					continue;
				}

				UnlockShipFrag (pQueue, hStarShip);
			}
		}
		SetStatusMessageMode (prevMsgMode);

		if (VictoryState)
		{
#ifdef NEVER
			DestroyDrawable (ReleaseDrawable (s.frame));
#endif /* NEVER */

			WaitForAnyButton (TRUE, ONE_SECOND * 3, FALSE);
			if (!CurrentInputState.key[PlayerControls[0]][KEY_ESCAPE])
			{
				DrawFadeText (str1, str2, FALSE, &scavenge_r);
				if (!CurrentInputState.key[PlayerControls[0]][KEY_ESCAPE])
				{
					SetContextForeGroundColor (BLACK_COLOR);
					r.corner.x = scavenge_r.corner.x + 10;
					r.extent.width = 132;
					DrawFilledRectangle (&r);
					sprintf (buf, "%u %s", RecycleAmount,
							GAME_STRING (STATUS_STRING_BASE + 1)); // "RU"
					t.baseline.x = r.corner.x + (r.extent.width >> 1);
					t.baseline.y = r.corner.y + 14;
					t.align = ALIGN_CENTER;
					t.pStr = buf;
					t.CharCount = (COUNT)~0;
					SetContextForeGroundColor (
							BUILD_COLOR (MAKE_RGB15 (0x00, 0x00, 0x18), 0x50));
					font_DrawText (&t);

					str1 = GAME_STRING (ENCOUNTER_STRING_BASE + 6);
							// "Debris"
					str2 = GAME_STRING (ENCOUNTER_STRING_BASE + 7);
							// "Scavenged"
					DrawFadeText (str1, str2, TRUE, &scavenge_r);
					WaitForAnyButton (TRUE, ONE_SECOND * 2, FALSE);
					if (!CurrentInputState.key[PlayerControls[0]][KEY_ESCAPE])
						DrawFadeText (str1, str2, FALSE, &scavenge_r);
				}
			}

			DrawStatusMessage (NULL);
		}

		if (ships_killed && EncounterRace == THRADDASH_SHIP
				&& !story_.read(uqm::StoryFlag::ThraddManner))
		{
			if ((ships_killed += story_.read(uqm::StoryCounter::ThraddashBodyCount)) >
					THRADDASH_BODY_THRESHOLD)
				ships_killed = THRADDASH_BODY_THRESHOLD;
			story_.set(uqm::StoryCounter::ThraddashBodyCount, ships_killed);
		}
	}
ExitUninitEncounter:

	return (ships_killed);
}

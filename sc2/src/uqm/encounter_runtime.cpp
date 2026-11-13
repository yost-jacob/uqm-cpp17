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

#include "encounter_runtime.hpp"
#include "conversation.hpp"
#include "combat_mode.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "build.h"
#include "commglue.h"
#include "controls.h"
#include "encount.h"
#include "starmap.h"
#include "sis.h"
#include "save.h"
#include "setup.h"
#include "sis_legacy.hpp"
#include "uqmdebug.h"
#include "libs/log.h"
#include <stdexcept>

namespace {
COUNT runLegacyEncounter(uqm::GameSession &session, const STAR_DESC *star, CONVERSATION conversation);
}

const STAR_DESC &uqm::EncounterRuntime::star() const {
    if (!star_) throw std::logic_error("system encounter requires a catalogue star");
    return *star_;
}

COUNT
uqm::EncounterRuntime::communicate(CONVERSATION which_comm)
{
    auto &session = session_;
    if (!session.isRunning()) return runLegacyEncounter(session, star_, which_comm);
	COUNT status;
	LOCDATA *LocDataPtr;

#ifdef DEBUG
	if (disableInteractivity)
		return 0;
#endif


	if (session_.activities().last & CHECK_LOAD)
	{
		session_.activities().last &= ~CHECK_LOAD;
		if (which_comm != COMMANDER_CONVERSATION)
		{
			if (LOBYTE (session_.activities().last) == 0)
			{
				DrawSISFrame ();
			}
			else
			{
				ClearSISRect (DRAW_SIS_DISPLAY);
				RepairSISBorder ();
			}
			DrawSISMessage (NULL);
			if ((LOBYTE(campaign_.activity) == IN_HYPERSPACE))
				DrawHyperCoords (campaign_.shipStamp.origin);
			else if (session_.player().travel().flight.planet == 0)
				DrawHyperCoords (star().star_pt);
			else
				DrawSISTitle (detail::SISAccess::record(session_.player().sis()).PlanetName);
		}
	}


	if (which_comm == URQUAN_DRONE_CONVERSATION)
	{
		status = URQUAN_DRONE_SHIP;
		which_comm = URQUAN_CONVERSATION;
	}
	else
	{
		if (which_comm == YEHAT_REBEL_CONVERSATION)
		{
			status = YEHAT_REBEL_SHIP;
			which_comm = YEHAT_CONVERSATION;
		}
		else
		{
			static constexpr COUNT commToShip[] = {
				RACE_SHIP_FOR_COMM
			};
			status = commToShip[which_comm];
			if (status >= YEHAT_REBEL_SHIP) {
				/* conversation exception, set to self */
				status = HUMAN_SHIP;
			}
		}
		StartSphereTracking (status);

		if (which_comm == ORZ_CONVERSATION
				|| (which_comm == TALKING_PET_CONVERSATION
				&& (!story_.read(uqm::StoryFlag::TalkingPetOnShip)
				|| LOBYTE (campaign_.activity) == IN_LAST_BATTLE))
				|| (which_comm != CHMMR_CONVERSATION
				&& which_comm != SYREEN_CONVERSATION
				))//&& CheckAlliance (status) == BAD_GUY))
			buildBattle(RosterSide::Second);
	}

	LocDataPtr = init_race (
			status != YEHAT_REBEL_SHIP ? which_comm :
			YEHAT_REBEL_CONVERSATION);

	if (story_.read(uqm::StoryFlag::BattleSegue) == 0)
	{
		// Not offered the chance to attack.
		status = HAIL;
	}
	else if ((status = selectAction()) == HAIL && LocDataPtr)
	{
		// The player chose to talk.
		story_.set(uqm::StoryFlag::BattleSegue, 0);
	}
	else
	{
		// The player chose to attack.
		status = ATTACK;
		story_.set(uqm::StoryFlag::BattleSegue, 1);
	}

    if (LocDataPtr) {
        auto conversation = std::make_unique<ConversationRuntime>(session, *LocDataPtr);
        if (status == HAIL) {
            if (auto exit = session.swapGameMode(createConversationMode(session, std::move(conversation))))
                campaign_.activity |= exit->kind == SessionExitKind::LoadGame ? CHECK_LOAD : CHECK_ABORT;
        } else {
            // An attack still runs the race's existing post/uninit hooks.
            conversation->finishWithoutDialogue();
        }
    }

	status = 0;
	if (!(campaign_.activity & (CHECK_ABORT | CHECK_LOAD)))
	{
		// The Sa-Matra battle is skipped when Cyborg is enabled.
		// Most likely because the Cyborg is too dumb to know what
		// to do in this battle.
		if (LOBYTE (campaign_.activity) == IN_LAST_BATTLE
				&& (campaign_.flags & CYBORG_ENABLED))
			ReinitQueue (&campaign_.ships);

		// Clear the location flags
		story_.set(uqm::StoryCounter::EncounterContext, 0);
		status = (story_.read(uqm::StoryFlag::BattleSegue)
				&& GetHeadLink (&campaign_.ships));
		if (status)
		{
			// Start combat
			buildBattle(RosterSide::First);
            if (auto exit = session.swapGameMode(createCampaignCombatMode(session, engine_.shipRoster())))
                campaign_.activity |= exit->kind == SessionExitKind::LoadGame ? CHECK_LOAD : CHECK_ABORT;
		}
		else
		{
			story_.set(uqm::StoryFlag::BattleSegue, 0);
		}
	}

	finish();

	return (status);
}

void
uqm::EncounterRuntime::run()
{
	COUNT i, status;
	HSHIPFRAG hStarShip;
	SHIP_FRAGMENT *FragPtr;
	ENCOUNTER *selectedEncounter = nullptr;
	static constexpr CONVERSATION RaceComm[] =
	{
		RACE_COMMUNICATION
	};

	if (LOBYTE (campaign_.activity) == IN_LAST_BATTLE)
	{
		/* Going into talking pet conversation */
		ReinitQueue (&campaign_.ships);
		CloneShipFragmentFromTemplate(SAMATRA_SHIP, &session_.raceFleets().at(SAMATRA_SHIP), &campaign_.ships, 0);
		communicate(TALKING_PET_CONVERSATION);
		if (!(campaign_.activity & (CHECK_ABORT | CHECK_LOAD))
				&& session_.player().sis().crew() != (COUNT)~0)
		{
			campaign_.activity = WON_LAST_BATTLE;
		}
		return;
	}
	else if (session_.activities().next & CHECK_LOAD)
	{
		BYTE ec;

		ec = story_.read(uqm::StoryCounter::EscapeCounter);

		communicate(conversationForRestoredEncounter(story_));
		if (session_.player().sis().crew() != (COUNT)~0)
		{
			session_.activities().next = campaign_.activity & ~START_ENCOUNTER;
			if (LOBYTE (session_.activities().next) == IN_INTERPLANETARY)
				session_.activities().next |= START_INTERPLANETARY;
			campaign_.activity |= CHECK_LOAD; /* fake a load game */
		}

		story_.set(uqm::StoryCounter::EscapeCounter, ec);
		return;
	}
	else if ((LOBYTE(campaign_.activity) == IN_HYPERSPACE))
	{
		ReinitQueue (&campaign_.ships);
		if (story_.read(uqm::StoryCounter::ArilouSpaceSide) >= 2)
		{
			communicate(ARILOU_CONVERSATION);
			return;
		}
		else
		{
			/* Encounter with a black globe in HS, prepare enemy ship list */
			ENCOUNTER *EncounterPtr;

            auto &entries = session_.encounters();
            if (entries.current() == entries.end()) throw std::logic_error("no selected campaign encounter");
            EncounterPtr = selectedEncounter = &*entries.current();

			for (i = 0; i < EncounterPtr->num_ships; ++i)
			{
				CloneShipFragmentFromTemplate(EncounterPtr->race_id,
                        &session_.raceFleets().at(EncounterPtr->race_id),
						&campaign_.ships,
						EncounterPtr->ShipList[i].crew_level);
			}

		}
	}

	// First ship in the npc queue defines which alien race
	// the player will be talking to
	hStarShip = GetHeadLink (&campaign_.ships);
	FragPtr = LockShipFrag (&campaign_.ships, hStarShip);
	i = FragPtr->race_id;
	UnlockShipFrag (&campaign_.ships, hStarShip);

	status = communicate(RaceComm[i]);

	if (campaign_.activity & (CHECK_ABORT | CHECK_LOAD))
		return;

	if (i == CHMMR_SHIP)
		ReinitQueue (&campaign_.ships);

	if (LOBYTE (campaign_.activity) == IN_INTERPLANETARY)
	{
		/* if used destruct code in interplanetary */
		if (i == SLYLANDRO_SHIP && status == 0)
			ReinitQueue (&campaign_.ships);
	}
	else if (selectedEncounter)
	{
		/* Update HSpace encounter info, ships lefts, etc. */
		BYTE i, NumShips;
		ENCOUNTER *EncounterPtr;

		EncounterPtr = selectedEncounter;

		NumShips = CountLinks (&campaign_.ships);
		EncounterPtr->num_ships = NumShips;
		EncounterPtr->flags |= ENCOUNTER_REFORMING;
		if (status == 0)
			EncounterPtr->flags |= ONE_SHOT_ENCOUNTER;

		for (i = 0; i < NumShips; ++i)
		{
			HSHIPFRAG hStarShip;
			SHIP_FRAGMENT *FragPtr;
			BRIEF_SHIP_INFO *BSIPtr;

			hStarShip = GetStarShipFromIndex (&campaign_.ships, i);
			FragPtr = LockShipFrag (&campaign_.ships, hStarShip);
			BSIPtr = &EncounterPtr->ShipList[i];
			BSIPtr->race_id = FragPtr->race_id;
			BSIPtr->crew_level = FragPtr->crew_level;
			BSIPtr->max_crew = FragPtr->max_crew;
			BSIPtr->max_energy = FragPtr->max_energy;
			UnlockShipFrag (&campaign_.ships, hStarShip);
		}

		ReinitQueue (&campaign_.ships);
	}
}

// C callers borrow the active campaign once at this compatibility boundary.
// Native modes construct their runtime with explicit campaign/star context.
namespace {
// Credits/outtakes and direct C debug entry points have no enclosing mode yet.
// Host the encounter as a root so its conversation still uses a child swap.
class LegacyEncounterHost final : public uqm::GameMode {
public:
    LegacyEncounterHost(uqm::GameSession &session, const STAR_DESC *star, CONVERSATION conversation, COUNT &result)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session),
          conversation_(conversation), result_(result), star_(star) {}
    uqm::ModeResult run() override {
        uqm::EncounterRuntime encounter{session_, star_, engine()};
        result_ = encounter.communicate(conversation_);
        if (auto exit = session_.menuExit()) return *exit;
        return uqm::ModeCompleted{};
    }
private:
    uqm::GameSession &session_;
    CONVERSATION conversation_;
    COUNT &result_;
    const STAR_DESC *star_;
};
COUNT runLegacyEncounter(uqm::GameSession &session, const STAR_DESC *star, CONVERSATION conversation) {
    COUNT result = 0;
    const auto exit = session.run(std::make_unique<LegacyEncounterHost>(session, star, conversation, result));
    if (exit.kind == uqm::SessionExitKind::LoadGame) uqm::detail::encounterLegacy(session.player()).activity |= CHECK_LOAD;
    else if (exit.kind != uqm::SessionExitKind::ReturnToTitle) uqm::detail::encounterLegacy(session.player()).activity |= CHECK_ABORT;
    return result;
}
}
COUNT InitCommunication(CONVERSATION conversation) {
    auto &session = uqm::gameSession();
    uqm::EncounterRuntime encounter{session, GetCurrentStar(), uqm::simulationEngine()};
    return encounter.communicate(conversation);
}

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

#include "battle.h"

#include "battlecontrols.h"
#include "controls.h"
#include "init.h"
#include "element.h"
#include "ship.h"
#include "process.h"
#include "tactrans.h"
		// for flee_preprocess()
#include "intel.h"
#ifdef NETPLAY
#	include "supermelee/netplay/netmelee.h"
#	ifdef NETPLAY_CHECKSUM
#		include "supermelee/netplay/checksum.h"
#	endif
#	include "supermelee/netplay/notifyall.h"
#endif
#include "supermelee/pickmele.h"
#include "resinst.h"
#include "nameref.h"
#include "setup.h"
#include "settings.h"
#include "sounds.h"
#include "libs/async.h"
#include "libs/graphics/gfx_common.h"
#include "libs/log.h"
#include "libs/mathlib.h"

#include "simulation_loop.hpp"
#include "simulation_engine.hpp"
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <type_traits>
extern "C" UWORD nth_frame;

BATTLE_INPUT_STATE
frameInputHuman (HumanInputContext *context, STARSHIP *StarShipPtr)
{
	(void) StarShipPtr;
	return CurrentInputToBattleInput (context->playerNr);
}

static bool
RunAwayAllowed (void)
{
	return (LOBYTE (GLOBAL (CurrentActivity)) == IN_ENCOUNTER
			|| LOBYTE (GLOBAL (CurrentActivity)) == IN_LAST_BATTLE)
			&& GET_GAME_STATE (STARBASE_AVAILABLE)
			&& !GET_GAME_STATE (BOMB_CARRIER);
}

static void
DoRunAway (STARSHIP *StarShipPtr)
{
	ELEMENT *ElementPtr;

	LockElement (StarShipPtr->hShip, &ElementPtr);
	if (GetPrimType (&DisplayArray[ElementPtr->PrimIndex]) == STAMP_PRIM
			&& ElementPtr->life_span == NORMAL_LIFE
			&& !(ElementPtr->state_flags & FINITE_LIFE)
			&& ElementPtr->mass_points != MAX_SHIP_MASS * 10
			&& !(ElementPtr->state_flags & APPEARING))
	{
		uqm::simulationEngine().shipRoster().retire(uqm::RosterSide::First);

		ElementPtr->turn_wait = 3;
		ElementPtr->thrust_wait = 4;
		ElementPtr->colorCycleIndex = 0;
		ElementPtr->preprocess_func = flee_preprocess;
		ElementPtr->mass_points = MAX_SHIP_MASS * 10;
		ZeroVelocityComponents (&ElementPtr->velocity);
		StarShipPtr->cur_status_flags &=
				~(SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED);

		SetPrimColor (&DisplayArray[ElementPtr->PrimIndex],
				BUILD_COLOR (MAKE_RGB15 (0x0B, 0x00, 0x00), 0x2E));
				// XXX: I think this is supposed to be the same as the
				// first entry of the color cycle table in flee_preeprocess,
				// but it is slightly different (0x0A as red value). - SvdB.
		SetPrimType (&DisplayArray[ElementPtr->PrimIndex], STAMPFILL_PRIM);
	
		StarShipPtr->ship_input_state = 0;
	}
	UnlockElement (StarShipPtr->hShip);
}

void
uqm::SimulationLoop::processInput()
{
	bool CanRunAway;
	size_t sideI;

#ifdef NETPLAY
	netInput ();
#endif

	CanRunAway = RunAwayAllowed ();
		
	for (sideI = 0; sideI < NUM_SIDES; sideI++)
	{
		HSTARSHIP hBattleShip, hNextShip;
		size_t cur_player = inputOrder_[sideI];

		for (hBattleShip = GetHeadLink (&race_q[cur_player]);
				hBattleShip != 0; hBattleShip = hNextShip)
		{
			BATTLE_INPUT_STATE InputState;
			STARSHIP *StarShipPtr;

			StarShipPtr = LockStarShip (&race_q[cur_player], hBattleShip);
			hNextShip = _GetSuccLink (StarShipPtr);

			if (StarShipPtr->hShip)
			{
				// TODO: review and see if we have to do this every frame, or
				//   if we can do this once somewhere
				StarShipPtr->control = PlayerControl[cur_player];
				
				InputState = PlayerInput[cur_player]->handlers->frameInput (
						PlayerInput[cur_player], StarShipPtr);

#if CREATE_JOURNAL
				JournalInput (InputState);
#endif /* CREATE_JOURNAL */
#ifdef NETPLAY
				if (!(PlayerControl[cur_player] & NETWORK_CONTROL))
				{
					BattleInputBuffer *bib = getBattleInputBuffer(cur_player);
					Netplay_NotifyAll_battleInput (InputState);
					flushPacketQueues ();

					BattleInputBuffer_push (bib, InputState);
							// Add this input to the end of the buffer.
					BattleInputBuffer_pop (bib, &InputState);
							// Get the input from the front of the buffer.
				}
#endif

				StarShipPtr->ship_input_state = 0;
				if (StarShipPtr->RaceDescPtr->ship_info.crew_level)
				{
					if (InputState & BATTLE_LEFT)
						StarShipPtr->ship_input_state |= LEFT;
					else if (InputState & BATTLE_RIGHT)
						StarShipPtr->ship_input_state |= RIGHT;
					if (InputState & BATTLE_THRUST)
						StarShipPtr->ship_input_state |= THRUST;
					if (InputState & BATTLE_WEAPON)
						StarShipPtr->ship_input_state |= WEAPON;
					if (InputState & BATTLE_SPECIAL)
						StarShipPtr->ship_input_state |= SPECIAL;

					if (CanRunAway && cur_player == 0 &&
							(InputState & BATTLE_ESCAPE))
						DoRunAway (StarShipPtr);
				}
			}

			UnlockStarShip (&race_q[cur_player], hBattleShip);
		}
	}
	
#ifdef NETPLAY
	flushPacketQueues ();
#endif

	if (GLOBAL (CurrentActivity) & (CHECK_LOAD | CHECK_ABORT))
		GLOBAL (CurrentActivity) &= ~IN_BATTLE;
}

bool
uqm::SimulationLoop::tick()
{
	RECT r{};
	BYTE battle_speed;

	SetMenuSounds (MENU_SOUND_NONE, MENU_SOUND_NONE);

#if defined (NETPLAY) && defined (NETPLAY_CHECKSUM)
	if (getNumNetConnections() > 0 &&
			battleFrameCount % NETPLAY_CHECKSUM_INTERVAL == 0)
	{
		crc_State state;
		Checksum checksum;

		crc_init(&state);
		crc_processState (&state);
		checksum = (Checksum) crc_finish (&state);

		Netplay_NotifyAll_checksum ((uint32) battleFrameCount,
				(uint32) checksum);
		flushPacketQueues ();
		addLocalChecksum (battleFrameCount, checksum);
	}
#endif
	processInput();
			// Also calls NetInput()
#if defined (NETPLAY) && defined (NETPLAY_CHECKSUM)
	if (getNumNetConnections() > 0)
	{
		size_t delay = getBattleInputDelay();

		if (battleFrameCount >= delay
				&& (battleFrameCount - delay) % NETPLAY_CHECKSUM_INTERVAL == 0)
		{
			if (!(GLOBAL (CurrentActivity) & CHECK_ABORT))
			{
				if (!verifyChecksums (battleFrameCount - delay)) {
					GLOBAL(CurrentActivity) |= CHECK_ABORT;
					resetConnections (ResetReason_syncLoss);
				}
			}
		}
	}
#endif

	if (firstFrame_)
	{
		r.corner.x = SIS_ORG_X;
		r.corner.y = SIS_ORG_Y;
		r.extent.width = SIS_SCREEN_WIDTH;
		r.extent.height = SIS_SCREEN_HEIGHT;
		SetTransitionSource (&r);
	}
    struct Batch { Batch() { BatchGraphics(); } ~Batch() { UnbatchGraphics(); } };
    {
        Batch batch;
        if (beforeSimulation_) beforeSimulation_();
        RedrawQueue(TRUE);
        if (firstFrame_) {
            firstFrame_ = false;
            ScreenTransition(3, &r);
        }
    } // Unbatch even if a navigation hook or simulation callback throws.

	if ((!(GLOBAL (CurrentActivity) & IN_BATTLE)) ||
			(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
	{
		return false;
	}

	battle_speed = HIBYTE (nth_frame);
	if (battle_speed == (BYTE)~0)
	{	// maximum speed, nothing rendered at all
		Async_process ();
		TaskSwitch ();
	}
	else
	{
		SleepThreadUntil (nextTime_
				+ BATTLE_FRAME_RATE / (battle_speed + 1));
		nextTime_ = GetTimeCounter ();
	}

	if ((GLOBAL (CurrentActivity) & IN_BATTLE) == 0)
		return false;

#ifdef NETPLAY
	battleFrameCount++;
#endif
	return true;
}

BOOLEAN uqm::SimulationLoop::input(void *state) {
    return static_cast<Input *>(state)->owner->tick() ? TRUE : FALSE;
}
void uqm::SimulationLoop::run() {
    if (&simulationEngine() != &engine_) throw std::logic_error("frame loop engine is not selected");
    static_assert(NUM_SIDES == 2);
    static_assert(std::is_standard_layout_v<Input> && offsetof(Input, InputFunc) == 0);
    std::iota(inputOrder_.begin(), inputOrder_.end(), 0u);
#ifdef NETPLAY
    std::stable_partition(inputOrder_.begin(), inputOrder_.end(), [](std::size_t side) {
        return !(PlayerControl[side] & NETWORK_CONTROL);
    });
#endif
    Input state{&SimulationLoop::input, this};
    DoInput(&state, FALSE);
}

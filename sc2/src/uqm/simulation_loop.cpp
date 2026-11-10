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
DoRunAway (uqm::SimulationEngine &engine, STARSHIP &ship)
{
	ELEMENT *ElementPtr;

	LockElement (ship.hShip, &ElementPtr);
	if (GetPrimType (&DisplayArray[ElementPtr->PrimIndex]) == STAMP_PRIM
			&& ElementPtr->life_span == NORMAL_LIFE
			&& !(ElementPtr->state_flags & FINITE_LIFE)
			&& ElementPtr->mass_points != MAX_SHIP_MASS * 10
			&& !(ElementPtr->state_flags & APPEARING))
	{
		engine.shipRoster().retire(uqm::RosterSide::First);

		ElementPtr->turn_wait = 3;
		ElementPtr->thrust_wait = 4;
		ElementPtr->colorCycleIndex = 0;
		ElementPtr->preprocess_func = flee_preprocess;
		ElementPtr->mass_points = MAX_SHIP_MASS * 10;
		ZeroVelocityComponents (&ElementPtr->velocity);
		ship.cur_status_flags &=
				~(SHIP_AT_MAX_SPEED | SHIP_BEYOND_MAX_SPEED);

		SetPrimColor (&DisplayArray[ElementPtr->PrimIndex],
				BUILD_COLOR (MAKE_RGB15 (0x0B, 0x00, 0x00), 0x2E));
				// XXX: I think this is supposed to be the same as the
				// first entry of the color cycle table in flee_preeprocess,
				// but it is slightly different (0x0A as red value). - SvdB.
		SetPrimType (&DisplayArray[ElementPtr->PrimIndex], STAMPFILL_PRIM);
	
		ship.ship_input_state = 0;
	}
	UnlockElement (ship.hShip);
}

void uqm::SimulationLoop::processInput()
{
#ifdef NETPLAY
    netInput();
#endif
    const bool canRunAway = RunAwayAllowed();
    auto &roster = engine_.shipRoster();
    for (const auto player : inputOrder_) {
        const auto side = static_cast<RosterSide>(player);
        for (auto participant = roster.first(side); participant;) {
            auto &ship = participant->get();
            participant = roster.next(side, ship); // Save traversal before input callbacks.
            if (!ship.hShip) continue;
            ship.control = PlayerControl[player];
            auto input = PlayerInput[player]->handlers->frameInput(PlayerInput[player], &ship);
#if CREATE_JOURNAL
            JournalInput(input);
#endif
#ifdef NETPLAY
            if (!(PlayerControl[player] & NETWORK_CONTROL)) {
                auto *buffer = getBattleInputBuffer(player);
                Netplay_NotifyAll_battleInput(input);
                flushPacketQueues();
                BattleInputBuffer_push(buffer, input);
                BattleInputBuffer_pop(buffer, &input);
            }
#endif
            ship.ship_input_state = 0;
            if (ship.RaceDescPtr->ship_info.crew_level) {
                if (input & BATTLE_LEFT) ship.ship_input_state |= LEFT;
                else if (input & BATTLE_RIGHT) ship.ship_input_state |= RIGHT;
                if (input & BATTLE_THRUST) ship.ship_input_state |= THRUST;
                if (input & BATTLE_WEAPON) ship.ship_input_state |= WEAPON;
                if (input & BATTLE_SPECIAL) ship.ship_input_state |= SPECIAL;
                if (canRunAway && player == 0 && (input & BATTLE_ESCAPE))
                    DoRunAway(engine_, ship);
            }
        }
    }
#ifdef NETPLAY
    flushPacketQueues();
#endif
    if (GLOBAL(CurrentActivity) & (CHECK_LOAD | CHECK_ABORT))
        GLOBAL(CurrentActivity) &= ~IN_BATTLE;
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

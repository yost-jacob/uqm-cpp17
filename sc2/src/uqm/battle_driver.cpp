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

#include "battle_driver.hpp"
#include "battle_scene.hpp"
#include "hyper_navigation.hpp"
#include "simulation_loop.hpp"
#include "simulation_engine.hpp"
#include "battle.h"
#include "battlecontrols.h"
#include "intel.h"
#include "ship.h"
#include "globdata.h"
#include "pickship.h"
#include "settings.h"
#include "setup.h"
#include "sounds.h"
#include "tactrans.h"
#include "libs/graphics/gfx_common.h"
#include "libs/log.h"
#include "libs/mathlib.h"
#ifdef NETPLAY
#include "supermelee/netplay/netmelee.h"
#ifdef NETPLAY_CHECKSUM
#include "supermelee/netplay/checksum.h"
#endif
#endif

namespace uqm {
namespace {
// Buffers and connection presence exist for one run, including selection or
// readiness failure. Teardown is resource release; result UI is explicit below.
class BattleRound {
public:
    ~BattleRound() {
#ifdef NETPLAY
        if (inputs_) uninitBattleInputBuffers();
#ifdef NETPLAY_CHECKSUM
        if (checksums_) uninitChecksumBuffers();
#endif
        if (connected_) setBattleActiveConnections(false);
#endif
        StopDitty(); StopMusic(); StopSound();
    }
    void initialize(SimulationEngine &engine) {
#ifdef NETPLAY
        inputs_ = true; initBattleInputBuffers();
#ifdef NETPLAY_CHECKSUM
        checksums_ = true; initChecksumBuffers();
#endif
        engine.resetBattleFrame();
        ResetWinnerStarShip();
        connected_ = true; setBattleActiveConnections(true);
#endif
    }
    bool ready() {
#ifdef NETPLAY
        initBattleStateDataConnections();
        return negotiateReadyConnections(true, NetState_inBattle);
#else
        return true;
#endif
    }
private:
#ifdef NETPLAY
    bool inputs_ = false, connected_ = false;
#ifdef NETPLAY_CHECKSUM
    bool checksums_ = false;
#endif
#endif
};
void journalSeed() {
#if DEMO_MODE || CREATE_JOURNAL
    static DWORD seed = 0;
    if (!seed) seed = TFB_Random();
    TFB_SeedRandom(seed);
    seed = TFB_Random();
#endif
}
bool selectCombatShips() {
#ifdef NETPLAY
    if ((PlayerControl[0] & NETWORK_CONTROL) && (PlayerControl[1] & NETWORK_CONTROL)) {
        log_add(log_Error, "Only one side at a time can be network controlled.");
        return false;
    }
#endif
    return GetInitialStarShips() != FALSE;
}
void finishRound(ACTIVITY &activity) {
    if (LOBYTE(activity) != SUPER_MELEE) return;
    if (activity & CHECK_ABORT) {
#ifdef NETPLAY
        waitResetConnections(NetState_inSetup);
#endif
        activity &= ~CHECK_ABORT; // Return to fleet setup.
    } else MeleeGameOver(); // Explicit normal result UI, never destructor work.
}
void initializeRound(SimulationEngine &engine, ACTIVITY &activity) {
    activity |= IN_BATTLE;
    engine.shipRoster().beginBattle();
    if (optMeleeScale != TFB_SCALE_STEP) SetGraphicScaleMode(optMeleeScale);
}
}
bool CombatDriver::run(bool instantVictory, std::function<bool()> cancelled) {
    auto &activity = activity_;
    if (cancelled && cancelled()) return false;
    journalSeed();
    detail::BattleScene scene(engine_, activity);
    scene.initialize();
    if (instantVictory) {
        engine_.shipRoster().setRemaining(RosterSide::First, 1);
        engine_.shipRoster().setRemaining(RosterSide::Second, 0);
    } else {
        initializeRound(engine_, activity);
        BattleRound round;
        round.initialize(engine_);
        if (!selectCombatShips()) activity |= CHECK_ABORT;
        else {
            scene.playMusic();
            if (!round.ready()) activity |= CHECK_ABORT;
            else { SimulationLoop loop(engine_); loop.run(); }
        }
        if (cancelled && cancelled()) return false;
        if (activity & CHECK_LOAD) return false;
        finishRound(activity);
    }
    if ((cancelled && cancelled()) || (activity & CHECK_LOAD)) return false;
    scene.complete();
    return true;
}
void runSpaceNavigation(SpaceNavigation &navigation, std::function<void()> beforeSimulation) {
    auto &engine = navigation.engine();
    auto &activity = navigation.activity();
    journalSeed();
    detail::BattleScene scene(engine, activity, navigation);
    scene.initialize();
    initializeRound(engine, activity);
    {
        BattleRound round;
        round.initialize(engine);
        if (!GetNextStarShip(nullptr, 0)) activity |= CHECK_ABORT;
        else {
            scene.playMusic();
            if (!round.ready()) activity |= CHECK_ABORT;
            else {
                SimulationLoop loop(engine, std::move(beforeSimulation), FrameTransition::EnterSpace);
                loop.run();
            }
        }
    }
    scene.complete();
}
}
#ifdef NETPLAY
COUNT GetPlayerOrder(COUNT i) {
    if (((PlayerControl[0] & NETWORK_CONTROL) && !NetConnection_getDiscriminant(netConnections[0])) ||
        ((PlayerControl[1] & NETWORK_CONTROL) && !NetConnection_getDiscriminant(netConnections[1]))) return i;
    return 1 - i;
}
#endif

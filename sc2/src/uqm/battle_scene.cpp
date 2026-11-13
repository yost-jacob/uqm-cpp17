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

#include "battle_scene.hpp"
#include "mode_rendering.hpp"
#include "combat_planet.hpp"
#include "simulation_engine.hpp"
#include "battle.h"
#include "build.h"
#include "cons_res.h"
#include "colors.h"
#include "encount.h"
#include "globdata.h"
#include "hyper_navigation.hpp"
#include "init.h"
#include "nameref.h"
#include "pickship.h"
#include "process.h"
#include "resinst.h"
#include "settings.h"
#include "setup.h"
#include "ship.h"
#include "tactrans.h"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>
namespace {
thread_local uqm::detail::BattleScene *activeScene = nullptr;
COUNT floatingCrew() {
    COUNT count = 0;
    for (auto handle = GetHeadElement(); handle;) {
        ELEMENT *element;
        LockElement(handle, &element);
        const auto next = GetSuccElement(element);
        if (element->state_flags & CREW_OBJECT) ++count;
        UnlockElement(handle);
        handle = next;
    }
    return count;
}
}
void uqm::detail::BattleScene::initialize() {
    if (&simulationEngine() != &engine_) throw std::logic_error("scene engine is not selected");
    drawing_ = suspendDrawingBindings({ScreenContext, SpaceContext, StatusContext, OffScreenContext});
    previous_ = activeScene; activeScene = this; bound_ = true;
    loadMusic();
    spaceAcquired_ = true; // InitSpace acquires its reference before loading assets.
    if (!InitSpace()) throw std::runtime_error("could not initialize scene assets");
    SetContext(StatusContext);
    SetContext(SpaceContext);
    InitDisplayList();
    InitGalaxy();
    if (navigation_ != nullptr) {
        engine_.shipRoster().clear();
        engine_.shipRoster().append(RosterSide::First, SIS_SHIP_ID);
        navigation_->LoadHyperspace();
        return;
    }
    SetContextFGFrame(Screen);
    RECT clip{{SAFE_X, SAFE_Y}, {static_cast<COORD>(SPACE_WIDTH), static_cast<COORD>(SPACE_HEIGHT)}};
    SetContextClipRect(&clip);
    SetContextBackGroundColor(BLACK_COLOR);
    const auto previous = SetContext(ScreenContext);
    SetContextBackGroundColor(BLACK_COLOR);
    ClearDrawable();
    SetContext(previous);
    if (LOBYTE(activity_) == IN_LAST_BATTLE) free_gravity_well();
    else {
        constexpr unsigned AsteroidCount = 5;
        for (unsigned i = 0; i < AsteroidCount; ++i) spawn_asteroid(nullptr);
        combat::spawnPlanet(engine_);
    }
}
void uqm::detail::BattleScene::complete() {
    StopSound();
    const auto crew = floatingCrew();
    std::array<STARSHIP *, NUM_PLAYERS> survivors{};
    for (auto handle = GetHeadElement(); handle;) {
        ELEMENT *element;
        LockElement(handle, &element);
        const auto next = GetSuccElement(element);
        if ((element->state_flags & PLAYER_SHIP) || element->death_func == new_ship) {
            STARSHIP *ship;
            GetElementStarShip(element, &ship);
            auto &info = ship->RaceDescPtr->ship_info;
            // The living final vessel collects floating crew, capped by capacity.
            if (info.crew_level)
                info.crew_level = std::min<unsigned>(info.max_crew, unsigned(info.crew_level) + crew);
            ship->crew_level = info.crew_level;
            survivors[ship->playerNr] = ship;
            auto *descriptor = std::exchange(ship->RaceDescPtr, nullptr);
            free_ship(descriptor, TRUE); // Explicit Ship::finish, never teardown.
        }
        UnlockElement(handle);
        handle = next;
    }
    activity_ &= ~IN_BATTLE;
    if (LOBYTE(activity_) == IN_ENCOUNTER && !(activity_ & CHECK_ABORT)) {
        // Persistent fleet results retain the established reverse-side order.
        for (int side = NUM_PLAYERS - 1; side >= 0; --side)
            if (survivors[side] && !FleetIsInfinite(side)) UpdateShipFragCrew(survivors[side]);
    }
    if (LOBYTE(activity_) != IN_ENCOUNTER) engine_.shipRoster().clear();
    completed_ = true;
}
uqm::detail::BattleScene::~BattleScene() {
    if (!bound_) return;
    // Release entities before assets. Cancellation skips Ship::finish and crew
    // reconciliation; completed encounter participants remain for parent aftermath.
    engine_.reset();
    if (!completed_) engine_.shipRoster().clear();
    activity_ &= ~IN_BATTLE;
    if (navigation_ != nullptr) navigation_->FreeHyperspace();
    if (spaceAcquired_) UninitSpace();
    DestroyMusic(music_);
    activeScene = previous_;
}
void uqm::detail::BattleScene::loadMusic() {
    if (music_) return;
    music_ = LoadMusic(navigation_ ? (navigation_->isQuasiSpace() ? QUASISPACE_MUSIC : HYPERSPACE_MUSIC) : BATTLE_MUSIC);
}
void uqm::detail::BattleScene::playMusic() { loadMusic(); PlayMusic(music_, TRUE, 1); }
extern "C" void BattleSong(BOOLEAN play) {
    if (!activeScene) std::terminate();
    if (play) activeScene->playMusic();
}

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

#include "ship_selection.hpp"
#include "runtime_ship_roster_legacy.hpp"
#include "game_state.hpp"
#include "sis_state.hpp"
#include "pickship.h"
#include "supermelee/pickmele.h"
#include "build.h"
#include "libs/math/random.hpp"
#include <stdexcept>
namespace uqm {
namespace {
void validate(RuntimeShipRoster &roster, RosterSide side, Participant last) {
    if (last && !roster.contains(side, last->get()))
        throw std::invalid_argument("previous ship belongs to another roster side");
}
}
Participant selectNavigationShip(RuntimeShipRoster &roster) noexcept { return roster.first(RosterSide::First); }
Participant selectArcadeShip(RuntimeShipRoster &roster, RosterSide side) {
    if (!roster.remaining(RosterSide::First) || !roster.remaining(RosterSide::Second)) return std::nullopt;
    HSTARSHIP selected = nullptr;
    if (!GetNextMeleeStarShip(static_cast<COUNT>(side), &selected)) return std::nullopt;
    return detail::RuntimeRosterAccess::borrow(roster, side, selected);
}
Participant selectCampaignPlayerShip(RuntimeShipRoster &roster, PlayerEntity &player, Participant last) {
    constexpr auto side = RosterSide::First;
    validate(roster, side, last);
    if (!last && roster.remaining(side) == 1) return roster.last(side); // Sole SIS, no dialog.
    if (roster.remaining(side)) {
        if (!roster.remaining(RosterSide::Second)) return std::nullopt;
        auto selected = detail::RuntimeRosterAccess::borrow(roster, side, PickCampaignStarShip());
        const auto flagship = roster.last(side);
        if (selected && flagship && &selected->get() == &flagship->get())
            roster.setRemaining(side, 1); // Selecting SIS forfeits further escort selection.
        return selected;
    }
    if (last) {
        auto &sis = player.sis();
        if (!last->get().crew_level) sis.setCrew(static_cast<COUNT>(~0u));
        else {
            constexpr DWORD WithdrawalFuel = 5 * FUEL_TANK_SCALE;
            sis.setFuel(sis.fuel() > WithdrawalFuel ? sis.fuel() - WithdrawalFuel : 0);
        }
    }
    return std::nullopt;
}
Participant selectCampaignOpponentShip(RuntimeShipRoster &roster, Participant last,
        bool infiniteFleet, RandomStream &random) {
    constexpr auto side = RosterSide::Second;
    validate(roster, side, last);
    if (!infiniteFleet) return last ? roster.next(side, last->get()) : roster.first(side);
    if (last) {
        auto &ship = last->get();
        ship.crew_level = ship.max_crew;
        ship.playerNr = static_cast<COUNT>(side);
        // Preserve the established one-draw captain-name selection and resource indices.
        ship.captains_name_index = (random() & (NUM_CAPTAINS_NAMES - 1)) + NAME_OFFSET;
    }
    roster.setRemaining(side, roster.remaining(side) + 1);
    return roster.first(side);
}
}

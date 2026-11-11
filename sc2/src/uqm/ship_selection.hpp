#ifndef UQM_SHIP_SELECTION_HPP_
#define UQM_SHIP_SELECTION_HPP_
#include "runtime_ship_roster.hpp"
namespace uqm {
class PlayerEntity;
class RandomStream;
// Selection returns a borrow, never ownership or a copied vessel. last must
// belong to the specified side of roster. Native policy has no global session.
// Navigation always borrows the first vessel on the player side.
Participant selectNavigationShip(RuntimeShipRoster &roster) noexcept;
// Requires availability on both sides. A canceled or foreign UI handle is empty.
Participant selectArcadeShip(RuntimeShipRoster &roster, RosterSide side);
// Campaign construction places SIS last on the first side. Empty last means
// initial selection; a retired SIS records defeat or the existing withdrawal cost.
Participant selectCampaignPlayerShip(RuntimeShipRoster &roster, PlayerEntity &player, Participant last);
// Finite fleets advance in queue order. Infinite fleets reuse the first vessel;
// replacing a retired incarnation restores crew and consumes one captain RNG draw.
Participant selectCampaignOpponentShip(RuntimeShipRoster &roster, Participant last,
    bool infiniteFleet, RandomStream &random);
}
#endif

#include "fleet_legacy.hpp"
#include "build.h"
#include "encount.h"
#include <algorithm>

// C recruitment/NPC boundary. The supplied queue must own SHIP_FRAGMENT slots.
// Preserve the stable vessel ID assigned by its native Ship pool allocator.
HSHIPFRAG CreateFleetFragment(QUEUE *queue, SPECIES_ID species) {
    assert(GetLinkSize(queue) == sizeof(SHIP_FRAGMENT));
    const auto link = AllocLink(queue);
    if (!link) return nullptr;
    auto &record = *LockShipFrag(queue, link);
    const auto id = record.ship_id;
    record = {};
    record.ship_id = id;
    record.SpeciesID = species;
    PutQueue(queue, link);
    return link;
}

// C engine callback: the native result targets identity, never queue order.
// Flagship and Super Melee have zero identity and retain their existing paths.
void UpdateShipFragCrew(STARSHIP *participant) {
    if (!participant->fleet_ship_id || !participant->fleet_owner) return;
    auto &queue = *participant->fleet_owner;
    const auto ship = uqm::detail::findShip(queue, {participant->fleet_ship_id});
    if (ship && ship->get().alive() && !ship->get().infinite()) {
        // Saved capacity can differ from edited ship tuning. Contain legacy
        // results at this buffer boundary; native mutations remain range-checked.
        ship->get().updateCrew(std::min(participant->crew_level, ship->get().maxCrew()));
    }
}

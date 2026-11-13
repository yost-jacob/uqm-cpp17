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

// Creation uses only the supplied defaults/queue. Captain selection still uses
// the selected gameplay RNG through the existing NameCaptain service.
HSHIPFRAG CloneShipFragmentFromTemplate(COUNT race, const FLEET_INFO *definition, QUEUE *ships, COUNT crew) {
    assert(definition && GetLinkSize(ships) == sizeof(SHIP_FRAGMENT));
    const auto captain = race == SAMATRA_SHIP ? BYTE{0} : NameCaptain(ships, definition->SpeciesID);
    const auto link = CreateFleetFragment(ships, definition->SpeciesID);
    if (!link) return nullptr;
    auto &ship = *LockShipFrag(ships, link);
    ship.captains_name_index = captain;
    ship.race_strings = definition->race_strings;
    ship.icons = definition->icons;
    ship.melee_icon = definition->melee_icon;
    ship.crew_level = crew ? crew : definition->crew_level;
    ship.max_crew = definition->max_crew;
    ship.max_energy = definition->max_energy;
    ship.race_id = static_cast<BYTE>(race);
    UnlockShipFrag(ships, link);
    return link;
}

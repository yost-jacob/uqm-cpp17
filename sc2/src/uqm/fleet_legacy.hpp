#ifndef UQM_FLEET_LEGACY_HPP_
#define UQM_FLEET_LEGACY_HPP_
#include "fleet.hpp"
namespace uqm::detail {
// Internal C-buffer/pool access only. Native callers use Fleet/Ship methods.
struct FleetAccess {
    static QUEUE &queue(Fleet &fleet) noexcept { return fleet.queue_; }
    static SHIP_FRAGMENT &record(Ship &ship) noexcept { return ship.data_; }
    static const SHIP_FRAGMENT &record(const Ship &ship) noexcept { return ship.data_; }
    static Ship &ship(SHIP_FRAGMENT &record) noexcept;
    static const Ship &ship(const SHIP_FRAGMENT &record) noexcept;
    static void activate(Ship &ship) noexcept;
};
// Also supports encounter-owned legacy NPC queues, which use the same Ship pool.
std::optional<std::reference_wrapper<Ship>> findShip(QUEUE &queue, ShipId id);
std::optional<std::reference_wrapper<const Ship>> findShip(const QUEUE &queue, ShipId id);
}
#endif

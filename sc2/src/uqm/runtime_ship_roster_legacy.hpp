#ifndef UQM_RUNTIME_SHIP_ROSTER_LEGACY_HPP_
#define UQM_RUNTIME_SHIP_ROSTER_LEGACY_HPP_
#include "runtime_ship_roster.hpp"
namespace uqm::detail {
struct RuntimeRosterAccess {
    // Borrow only while the roster owns these pools. Publish allocated slots
    // before attaching a descriptor; each descriptor is uniquely owned by its slot.
    static QUEUE *queues(RuntimeShipRoster &roster) noexcept { return roster.queues_.data(); }
};
}
#endif

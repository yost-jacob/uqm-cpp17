#ifndef UQM_RUNTIME_SHIP_ROSTER_LEGACY_HPP_
#define UQM_RUNTIME_SHIP_ROSTER_LEGACY_HPP_
#include "runtime_ship_roster.hpp"
namespace uqm::detail {
struct RuntimeRosterAccess {
    // Borrow only while the roster owns these pools. Publish allocated slots
    // before attaching a descriptor; each descriptor is uniquely owned by its slot.
    static BYTE *remaining(RuntimeShipRoster &roster) noexcept { return roster.remaining_.data(); }
    // Validate an opaque UI/C handle by identity before dereferencing it.
    static Participant borrow(RuntimeShipRoster &roster, RosterSide side, HSTARSHIP raw) noexcept {
        auto &queue = roster.queues_[static_cast<unsigned>(side)];
        for (auto handle = GetHeadLink(&queue); handle; handle = _GetSuccLink(handle))
            if (handle == raw) return std::ref(*LockStarShip(&queue, handle));
        return std::nullopt;
    }
    static QUEUE *queues(RuntimeShipRoster &roster) noexcept { return roster.queues_.data(); }
};
}
#endif

#ifndef UQM_RUNTIME_SHIP_ROSTER_HPP_
#define UQM_RUNTIME_SHIP_ROSTER_HPP_
#include "races.h"
#include <array>
namespace uqm {
namespace detail { struct RuntimeRosterAccess; }
enum class RosterSide : unsigned { First = 0, Second = 1 };
// Temporary participants for one mode, separate from persistent Fleet ships.
// Owns stable STARSHIP slots and any loaded behavior/assets attached to them.
class RuntimeShipRoster {
public:
    RuntimeShipRoster();
    ~RuntimeShipRoster();
    RuntimeShipRoster(const RuntimeShipRoster &) = delete;
    RuntimeShipRoster &operator=(const RuntimeShipRoster &) = delete;
    COUNT size(RosterSide side) const noexcept;
    // Releases attached resources without committing gameplay results. Explicit
    // battle retirement/completion remains responsible for crew/story updates.
    void clear(RosterSide side) noexcept;
    void clear() noexcept;
    // Transfers pool ownership without changing participant addresses/handles.
    void swap(RuntimeShipRoster &other) noexcept;
private:
    friend struct detail::RuntimeRosterAccess;
    std::array<QUEUE, 2> queues_{};
};
// The encounter prepares participants and reads aftermath. Its combat child
// temporarily owns the roster, then returns results before either mode dies.
class RuntimeRosterTransfer {
public:
    RuntimeRosterTransfer(RuntimeShipRoster &source, RuntimeShipRoster &destination) noexcept
        : source_(source), destination_(destination) { source_.swap(destination_); }
    ~RuntimeRosterTransfer() { source_.swap(destination_); }
    RuntimeRosterTransfer(const RuntimeRosterTransfer &) = delete;
    RuntimeRosterTransfer &operator=(const RuntimeRosterTransfer &) = delete;
private:
    RuntimeShipRoster &source_, &destination_;
};
}
#endif

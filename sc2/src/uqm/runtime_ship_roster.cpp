#include "runtime_ship_roster.hpp"
#include "init.h"
#include "ship_assets_legacy.hpp"
#include <cassert>
namespace uqm {
namespace {
std::size_t index(RosterSide side) noexcept {
    const auto value = static_cast<std::size_t>(side);
    assert(value < 2);
    return value;
}
}
RuntimeShipRoster::RuntimeShipRoster() {
    static_assert(NUM_PLAYERS == 2, "combat rosters currently have two sides");
    for (auto &queue : queues_) InitQueue(&queue, MAX_SHIPS_PER_SIDE, QUEUE_STARSHIP);
}
RuntimeShipRoster::~RuntimeShipRoster() {
    clear();
    for (auto &queue : queues_) UninitQueue(&queue);
}
COUNT RuntimeShipRoster::size(RosterSide side) const noexcept { return CountLinks(&queues_[index(side)]); }
void RuntimeShipRoster::clear(RosterSide side) noexcept {
    auto &queue = queues_[index(side)];
    for (auto handle = GetHeadLink(&queue); handle;) {
        auto *ship = LockStarShip(&queue, handle);
        const auto next = _GetSuccLink(ship);
        if (ship->RaceDescPtr) {
            auto *descriptor = ship->RaceDescPtr;
            ship->RaceDescPtr = nullptr;
            detail::discardLoadedShip(*descriptor);
        }
        UnlockStarShip(&queue, handle);
        handle = next;
    }
    ReinitQueue(&queue);
}
void RuntimeShipRoster::clear() noexcept { clear(RosterSide::First); clear(RosterSide::Second); }
void RuntimeShipRoster::swap(RuntimeShipRoster &other) noexcept {
    for (std::size_t side = 0; side < queues_.size(); ++side) SwapQueues(&queues_[side], &other.queues_[side]);
}
}

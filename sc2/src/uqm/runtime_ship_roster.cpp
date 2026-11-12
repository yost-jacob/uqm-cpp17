#include "runtime_ship_roster.hpp"
#include "game_collections.hpp"
#include "init.h"
#include "build.h"
#include "libs/math/random.hpp"
#include "ship_assets_legacy.hpp"
#include <cassert>
#include <stdexcept>
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
Participant RuntimeShipRoster::append(RosterSide side, SPECIES_ID species) noexcept {
    auto &queue = queues_[index(side)];
    const auto handle = AllocLink(&queue);
    if (!handle) return std::nullopt;
    auto &ship = *LockStarShip(&queue, handle);
    ship = {};
    ship.SpeciesID = species;
    ship.playerNr = static_cast<COUNT>(side);
    PutQueue(&queue, handle);
    return std::ref(ship);
}
BYTE RuntimeShipRoster::chooseCaptain(RosterSide side, SPECIES_ID species, RandomStream &random) const {
    std::array<bool, NUM_CAPTAINS_NAMES> used{};
    const auto &queue = queues_[index(side)];
    for (const auto *participant : detail::battleParticipants(queue)) {
        const auto &ship = *participant;
        if (ship.SpeciesID == species && ship.captains_name_index >= NAME_OFFSET
                && ship.captains_name_index < NAME_OFFSET + used.size())
            used[ship.captains_name_index - NAME_OFFSET] = true;
    }
    // A side has at most 14 ships and the resource provides 16 names.
    static_assert(MAX_SHIPS_PER_SIDE < NUM_CAPTAINS_NAMES);
    for (;;) {
        const auto candidate = random() & (NUM_CAPTAINS_NAMES - 1);
        if (!used[candidate]) return static_cast<BYTE>(candidate + NAME_OFFSET);
    }
}
Participant RuntimeShipRoster::first(RosterSide side) noexcept {
    auto &queue = queues_[index(side)];
    auto handle = GetHeadLink(&queue);
    return handle ? Participant{std::ref(*LockStarShip(&queue, handle))} : std::nullopt;
}
Participant RuntimeShipRoster::last(RosterSide side) noexcept {
    auto &queue = queues_[index(side)];
    auto handle = GetTailLink(&queue);
    return handle ? Participant{std::ref(*LockStarShip(&queue, handle))} : std::nullopt;
}
bool RuntimeShipRoster::contains(RosterSide side, const STARSHIP &ship) const noexcept {
    const auto &queue = queues_[index(side)];
    for (const auto *participant : detail::battleParticipants(queue))
        if (participant == &ship) return true;
    return false;
}
Participant RuntimeShipRoster::next(RosterSide side, const STARSHIP &previous) {
    if (!contains(side, previous)) throw std::invalid_argument("participant belongs to another roster side");
    auto &queue = queues_[index(side)];
    const auto handle = _GetSuccLink(&previous);
    return handle ? Participant{std::ref(*LockStarShip(&queue, handle))} : std::nullopt;
}
void RuntimeShipRoster::beginBattle() noexcept {
    for (std::size_t side = 0; side < queues_.size(); ++side) remaining_[side] = CountLinks(&queues_[side]);
}
BYTE RuntimeShipRoster::remaining(RosterSide side) const noexcept { return remaining_[index(side)]; }
void RuntimeShipRoster::setRemaining(RosterSide side, BYTE count) noexcept { remaining_[index(side)] = count; }
void RuntimeShipRoster::retire(RosterSide side) noexcept {
    auto &count = remaining_[index(side)];
    assert(count); if (count) --count;
}
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
    remaining_.swap(other.remaining_);
    for (std::size_t side = 0; side < queues_.size(); ++side) SwapQueues(&queues_[side], &other.queues_[side]);
}
}

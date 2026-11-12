#include "fleet_legacy.hpp"
#include "game_collections.hpp"
#include "build.h"
#include "libs/math/random.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>
namespace uqm {
void Ship::updateCrew(COUNT crew) {
    if (infinite()) throw std::logic_error("infinite fleet has no finite crew result");
    if (crew > maxCrew()) throw std::out_of_range("crew exceeds vessel capacity");
    if (!alive() && crew) throw std::logic_error("cannot update a destroyed vessel to alive");
    data_.crew_level = crew;
}
Fleet::Fleet(COUNT capacity, SISPresence sis) : capacity_(capacity) {
    InitQueue(&queue_, capacity, QUEUE_SHIP_FRAGMENT);
    if (sis == SISPresence::Present) sis_.emplace();
}
std::optional<std::reference_wrapper<SIS>> Fleet::sis() noexcept {
    if (!sis_) return std::nullopt;
    return std::ref(*sis_);
}
std::optional<std::reference_wrapper<const SIS>> Fleet::sis() const noexcept {
    if (!sis_) return std::nullopt;
    return std::cref(*sis_);
}
Fleet::~Fleet() { UninitQueue(&queue_); }
std::optional<ShipId> Fleet::create(const ShipSpec &spec) {
    if (spec.species <= NO_ID || spec.species >= NUM_SPECIES_ID || spec.race >= NUM_AVAILABLE_RACES
            || spec.slot >= capacity_ || !spec.crew
            || spec.crew > spec.maxCrew || spec.crew == INFINITE_FLEET)
        throw std::invalid_argument("invalid vessel specification");
    if (size() == capacity_) return std::nullopt;
    for (const auto id : ships())
        if (find(id)->get().slot() == spec.slot)
            throw std::invalid_argument("fleet slot is already occupied");
    const auto link = AllocLink(&queue_);
    if (!link) return std::nullopt;
    auto &record = *LockShipFrag(&queue_, link);
    const auto id = record.ship_id;
    record = {};
    record.ship_id = id;
    record.SpeciesID = spec.species;
    record.race_id = spec.race;
    record.captains_name_index = spec.captain;
    record.crew_level = spec.crew;
    record.max_crew = spec.maxCrew;
    record.index = spec.slot;
    PutQueue(&queue_, link);
    return ShipId{id};
}
std::optional<ShipId> Fleet::create(COUNT race, const FLEET_INFO &definition, RandomStream &random) {
    if (race >= NUM_AVAILABLE_RACES || definition.SpeciesID <= NO_ID || definition.SpeciesID >= NUM_SPECIES_ID ||
        !definition.crew_level || definition.crew_level == INFINITE_FLEET || definition.crew_level > definition.max_crew)
        throw std::invalid_argument("invalid escort template");
    if (size() == capacity_ || size() >= 256) return std::nullopt;
    const auto ids = ships();
    BYTE slot = 0;
    while (std::any_of(ids.begin(), ids.end(), [&](ShipId id) { return find(id)->get().slot() == slot; })) ++slot;
    std::array<bool, NUM_CAPTAINS_NAMES> occupied{};
    for (const auto id : ids) {
        const auto &ship = find(id)->get();
        if (ship.species() == definition.SpeciesID && ship.captain() >= NAME_OFFSET && ship.captain() < NAME_OFFSET + NUM_CAPTAINS_NAMES)
            occupied[ship.captain() - NAME_OFFSET] = true;
    }
    if (std::all_of(occupied.begin(), occupied.end(), [](bool value) { return value; })) return std::nullopt;
    BYTE captain;
    do { captain = NAME_OFFSET + random() % NUM_CAPTAINS_NAMES; }
    while (std::any_of(ids.begin(), ids.end(), [&](ShipId id) {
        const auto &ship = find(id)->get();
        return ship.species() == definition.SpeciesID && ship.captain() == captain;
    }));
    const auto id = create({definition.SpeciesID, static_cast<BYTE>(race),
        captain, definition.crew_level, definition.max_crew, slot});
    if (id) {
        auto &record = detail::FleetAccess::record(find(*id)->get());
        record.race_strings = definition.race_strings;
        record.icons = definition.icons;
        record.melee_icon = definition.melee_icon;
        record.max_energy = definition.max_energy;
        // Keep native entries in picker order, including a newly filled gap.
        RemoveQueue(&queue_, &record);
        auto next = queue_.head;
        while (next && LockShipFrag(&queue_, next)->index < slot) next = _GetSuccLink(next);
        InsertQueue(&queue_, &record, next);
    }
    return id;
}
std::optional<std::reference_wrapper<Ship>> Fleet::find(ShipId id) { return detail::findShip(queue_, id); }
std::optional<std::reference_wrapper<const Ship>> Fleet::find(ShipId id) const { return detail::findShip(queue_, id); }
std::vector<ShipId> Fleet::ships() const {
    std::vector<ShipId> ids;
    ids.reserve(size());
    for (const auto *ship : detail::fleetShips(queue_)) ids.push_back(ship->id());
    return ids;
}
COUNT Fleet::size() const noexcept { return CountLinks(&queue_); }
bool Fleet::erase(ShipId id) {
    const auto ship = find(id);
    if (!ship) return false;
    auto *record = &detail::FleetAccess::record(ship->get());
    RemoveQueue(&queue_, record);
    FreeShipFrag(&queue_, record);
    return true;
}
void Fleet::pruneRetired() {
    for (const auto id : ships()) if (!find(id)->get().alive()) erase(id);
}
void Fleet::clear() noexcept { ReinitQueue(&queue_); }
namespace detail {
Ship &FleetAccess::ship(SHIP_FRAGMENT &record) noexcept {
    static_assert(std::is_standard_layout_v<Ship>);
    static_assert(offsetof(Ship, data_) == 0 && sizeof(Ship) == sizeof(SHIP_FRAGMENT));
    return *reinterpret_cast<Ship *>(&record);
}
const Ship &FleetAccess::ship(const SHIP_FRAGMENT &record) noexcept {
    return ship(const_cast<SHIP_FRAGMENT &>(record));
}
void FleetAccess::activate(Ship &ship) noexcept {
    static std::atomic<std::uint64_t> next{1};
    ship.data_.ship_id = next.fetch_add(1, std::memory_order_relaxed);
    if (!ship.data_.ship_id) std::terminate();
}
std::optional<std::reference_wrapper<const Ship>> findShip(const QUEUE &queue, ShipId id) {
    if (!id.value || !queue.storage) return std::nullopt;
    for (const auto *ship : fleetShips(queue))
        if (ship->id() == id) return std::cref(*ship);
    return std::nullopt;
}
std::optional<std::reference_wrapper<Ship>> findShip(QUEUE &queue, ShipId id) {
    const auto result = findShip(std::as_const(queue), id);
    if (!result) return std::nullopt;
    return std::ref(const_cast<Ship &>(result->get()));
}
}
}

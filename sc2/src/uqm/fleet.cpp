#include "fleet_legacy.hpp"
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
Fleet::Fleet(COUNT capacity) : capacity_(capacity) { InitQueue(&queue_, capacity, QUEUE_SHIP_FRAGMENT); }
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
std::optional<std::reference_wrapper<Ship>> Fleet::find(ShipId id) { return detail::findShip(queue_, id); }
std::optional<std::reference_wrapper<const Ship>> Fleet::find(ShipId id) const { return detail::findShip(queue_, id); }
std::vector<ShipId> Fleet::ships() const {
    std::vector<ShipId> ids;
    ids.reserve(size());
    for (auto link = queue_.head; link; link = _GetSuccLink(link))
        ids.push_back({LockShipFrag(&queue_, link)->ship_id});
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
    if (!id.value) return std::nullopt;
    for (auto link = queue.head; link; link = _GetSuccLink(link)) {
        const auto &record = *LockShipFrag(&queue, link);
        if (record.ship_id == id.value) return std::cref(FleetAccess::ship(record));
    }
    return std::nullopt;
}
std::optional<std::reference_wrapper<Ship>> findShip(QUEUE &queue, ShipId id) {
    const auto result = findShip(std::as_const(queue), id);
    if (!result) return std::nullopt;
    return std::ref(const_cast<Ship &>(result->get()));
}
}
}

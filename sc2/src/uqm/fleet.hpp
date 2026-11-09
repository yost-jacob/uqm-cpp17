#ifndef UQM_FLEET_HPP_
#define UQM_FLEET_HPP_
#include "races.h"
#include "sis_state.hpp"
#include <functional>
#include <optional>
#include <vector>
#include <cstdint>
namespace uqm {
struct ShipId {
    std::uint64_t value = 0;
    friend bool operator==(ShipId a, ShipId b) noexcept { return a.value == b.value; }
    friend bool operator!=(ShipId a, ShipId b) noexcept { return !(a == b); }
};
namespace detail { struct FleetAccess; }
// Persistent vessel, distinct from combat::Ship (the temporary behavior).
// Its C compatibility record is the same storage used by legacy callers.
// Borrow only until removal/reset; retain ShipId across mutations, not references.
class Ship {
public:
    Ship() = default;
    Ship(const Ship &) = delete;
    Ship &operator=(const Ship &) = delete;
    ShipId id() const noexcept { return {data_.ship_id}; }
    SPECIES_ID species() const noexcept { return data_.SpeciesID; }
    BYTE race() const noexcept { return data_.race_id; }
    BYTE captain() const noexcept { return data_.captains_name_index; }
    COUNT crew() const noexcept { return data_.crew_level; }
    COUNT maxCrew() const noexcept { return data_.max_crew; }
    BYTE slot() const noexcept { return data_.index; }
    bool alive() const noexcept { return crew() != 0; }
    bool infinite() const noexcept { return crew() == INFINITE_FLEET; }
    // Finite crew must not exceed maxCrew. Zero retires this vessel; native
    // updates cannot resurrect it. Pkunk reincarnation resolves before retirement.
    void updateCrew(COUNT crew);
    void destroy() noexcept { data_.crew_level = 0; }
private:
    friend struct detail::FleetAccess;
    SHIP_FRAGMENT data_{};
};
// Explicit creation values. Captain is the race resource's name index; the
// existing asset/template factory remains responsible for choosing defaults.
struct ShipSpec {
    SPECIES_ID species;
    BYTE race, captain;
    COUNT crew, maxCrew;
    BYTE slot;
};
// Owns persistent vessels in a fixed-capacity pool. Destruction/reset invalidates
// all borrows; IDs are never reused, including across loads and other fleets.
// Ships borrow race graphics/text resources, whose owners outlive their use.
enum class SISPresence { Absent, Present };
class Fleet {
public:
    explicit Fleet(COUNT capacity, SISPresence sis = SISPresence::Absent);
    std::optional<std::reference_wrapper<SIS>> sis() noexcept;
    std::optional<std::reference_wrapper<const SIS>> sis() const noexcept;
    ~Fleet();
    Fleet(const Fleet &) = delete;
    Fleet &operator=(const Fleet &) = delete;
    // Validate before allocation; returns nullopt when full. No partial insertion.
    std::optional<ShipId> create(const ShipSpec &spec);
    std::optional<std::reference_wrapper<Ship>> find(ShipId id);
    std::optional<std::reference_wrapper<const Ship>> find(ShipId id) const;
    std::vector<ShipId> ships() const; // IDs in fleet/display order, including dead.
    COUNT size() const noexcept;
    bool erase(ShipId id); // Also invalidates borrows to the removed vessel.
    void pruneRetired(); // Call after combat selection/aftermath no longer needs X slots.
    void clear() noexcept;
private:
    friend struct detail::FleetAccess;
    QUEUE queue_{};
    COUNT capacity_;
    std::optional<SIS> sis_;
};
}
#endif

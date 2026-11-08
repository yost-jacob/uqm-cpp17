#ifndef UQM_WORLD_TYPES_HPP_
#define UQM_WORLD_TYPES_HPP_

#include "star_map.hpp"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

namespace uqm {
struct PlayerId {
    std::uint64_t value;
    constexpr bool operator==(PlayerId other) const noexcept { return value == other.value; }
    constexpr bool operator!=(PlayerId other) const noexcept { return !(*this == other); }
};
inline constexpr PlayerId SinglePlayerId{1};

struct UniverseId {
    std::uint64_t value;
    constexpr bool operator==(UniverseId other) const noexcept { return value == other.value; }
    constexpr bool operator!=(UniverseId other) const noexcept { return !(*this == other); }
};
inline constexpr UniverseId SinglePlayerUniverse{1};
using SystemLayout = std::vector<std::uint16_t>; // Moon count for each planet slot.
struct WorldProgress { std::bitset<32> minerals, biologicals, energy; };

// Stable identity, independent of runtime descriptor addresses and save offsets.
// Planet/moon slots are zero-based; nullopt identifies the planet itself.
struct WorldId {
    UniverseId universe;
    StarId system;
    std::uint16_t planet;
    std::optional<std::uint16_t> moon;
    bool operator==(const WorldId &other) const noexcept {
        return universe == other.universe && system == other.system
                && planet == other.planet && moon == other.moon;
    }
    bool operator<(const WorldId &other) const noexcept {
        return std::tie(universe.value, system.value, planet, moon)
                < std::tie(other.universe.value, other.system.value, other.planet, other.moon);
    }
};
struct DiscoveryProgress { std::bitset<32> visitedSites; };
using DiscoverySnapshot = std::map<WorldId, DiscoveryProgress>;

// Save-adapter metadata, NOT identity or another mutable progress store. Offsets
// are relative to a system block, so reordering blocks in a save is harmless.
struct LegacyDiscoveryLocation {
    WorldId world;
    std::uint32_t energyWord; // Word index within the system, not the whole file.
    std::uint32_t systemWords;
};
} // namespace uqm
#endif

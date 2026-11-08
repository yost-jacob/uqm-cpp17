#ifndef UQM_DESIGN_STATE_TYPES_HPP_
#define UQM_DESIGN_STATE_TYPES_HPP_

// Shared value types only: no state owner, session, locks or network connection.
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "uqm/star_map.hpp"

namespace uqm::design {
struct UniverseId { std::uint64_t value; }; // Opaque host-assigned instance identity.
struct PlayerId { std::uint64_t value; };
bool operator==(PlayerId left, PlayerId right) noexcept;
struct PlayerIdHash { std::size_t operator()(PlayerId id) const noexcept; };
// Stable slots, not screen/orbit order. A missing moon identifies the planet.
struct WorldId {
    StarId system;
    std::uint16_t planet;
    std::optional<std::uint16_t> moon;
};
bool operator==(const WorldId &left, const WorldId &right) noexcept;
struct WorldIdHash { std::size_t operator()(const WorldId &id) const noexcept; };
enum class NodeKind { Mineral, Biological, Energy };
struct NodeId { NodeKind kind; std::size_t index; }; // index 0..31.
struct WorldProgress {
    std::bitset<32> minerals, biologicals, energy;
};
enum class PresenceArea { HyperSpace, QuasiSpace, SolarSystem, Orbit };
// Coordinate frame and accepted position. System/world must agree with area.
struct PlayerLocation {
    PresenceArea area = PresenceArea::HyperSpace;
    std::optional<StarId> system;
    std::optional<WorldId> world;
    StarPosition position{};
    unsigned facing = 0;
};
// Portable game identity/profile, not passwords, credentials or live sockets.
struct UserProfile { PlayerId id; std::string displayName; };
struct FlagshipState {
    unsigned crew = 0, fuel = 0, biologicalCargo = 0;
    std::array<unsigned, 8> mineralCargo{};
    std::vector<unsigned> installedModules;
};
struct FleetProgress {
    unsigned species = 0, strength = 0;
    StarPosition position{};
};
struct CampaignDate { unsigned year, month, day; };
struct ScheduledStoryEvent { CampaignDate due; unsigned eventId; };
struct WorldClock {
    CampaignDate date{2155, 2, 17};
    std::vector<ScheduledStoryEvent> pending;
};
struct PickupReward {
    std::array<unsigned, 8> minerals{};
    unsigned biological = 0;
};
} // namespace uqm::design
#endif

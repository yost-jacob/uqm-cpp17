#ifndef UQM_DESIGN_CAMPAIGN_STATE_HPP_
#define UQM_DESIGN_CAMPAIGN_STATE_HPP_

// Declaration-only architecture sketch. Not part of the game build.
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>
#include "uqm/star_map.hpp"

namespace uqm::design {

// Persistent identity, independent of addresses and current screen/orbit order.
// Planet and moon slots must reproduce the existing generation/save ordering.
// A missing moon identifies the planet itself. Hashes are never saved as IDs.
struct WorldId {
    StarId system;
    std::uint16_t planet;
    std::optional<std::uint16_t> moon;
};
bool operator==(const WorldId &left, const WorldId &right) noexcept;
struct WorldIdHash {
    std::size_t operator()(const WorldId &id) const noexcept;
};
enum class NodeKind { Mineral, Biological, Energy };
struct NodeId {
    NodeKind kind;
    std::size_t index; // 0..31 within this world's deterministic scan sequence.
};
struct WorldProgress {
    // Existing persistence semantics: three 32-bit retrieved-node masks.
    // Species, terrain, creature movement and stun state are regenerated/live.
    std::bitset<32> minerals;
    std::bitset<32> biologicals;
    std::bitset<32> energy;
};

// Representative fields only. Existing story names, widths and save mappings
// must be audited before filling in the complete schema or changing storage.
enum class StoryFlag : std::size_t {
    ChmmrUnleashed, StarbaseAvailable, SunDeviceOnShip, Count
};
enum class StoryCounter : std::size_t { BombProgress, ShofixtiVisits, Count };
constexpr unsigned counterMaximum(StoryCounter counter) noexcept {
    switch (counter) {
        case StoryCounter::BombProgress: return 3; // Existing two-bit value.
        case StoryCounter::ShofixtiVisits: return 7; // Existing three-bit value.
        default: return 0; // Invalid fields are rejected by the public setters.
    }
}
struct FlagshipState {
    unsigned crew = 0;
    unsigned fuel = 0;
    unsigned biologicalCargo = 0;
    std::array<unsigned, 8> mineralCargo{};
    std::vector<unsigned> installedModules;
    // Further equipment/inventory fields will follow the existing SIS schema.
};
struct FleetProgress {
    unsigned species = 0;
    unsigned strength = 0;
    StarPosition position{};
};
struct CampaignDate { unsigned year, month, day; };
struct ScheduledStoryEvent {
    CampaignDate due;
    unsigned eventId; // Stable saved ID, never a function pointer or lambda.
};
struct CampaignClock {
    CampaignDate date{2155, 2, 17};
    std::vector<ScheduledStoryEvent> pending;
};
class World;

// Owns persistent campaign facts. It does not choose handlers, run encounters,
// own rendered surfaces, or contain callbacks. This replaces the conceptual
// role of both GlobData story bits and the separate planet-progress store.
class CampaignState {
public:
    CampaignState();
    CampaignState(const CampaignState &) = delete;
    CampaignState &operator=(const CampaignState &) = delete;

    // Unknown enum values throw out_of_range. Boolean flags cannot be counters.
    bool read(StoryFlag flag) const;
    unsigned read(StoryCounter counter) const;
    void set(StoryFlag flag);
    void clear(StoryFlag flag);
    // Reject values outside the field's range; never mask/truncate into neighbors.
    void set(StoryCounter counter, unsigned value);
    // Optional spelling when both arguments are compile-time constants.
    template<StoryCounter Counter, unsigned Value>
    void set() {
        static_assert(static_cast<std::size_t>(Counter)
                < static_cast<std::size_t>(StoryCounter::Count), "unknown story counter");
        static_assert(Value <= counterMaximum(Counter), "story counter value out of range");
        set(Counter, Value);
    }
    // An unvisited world reads as empty progress. The returned reference is
    // read-only and valid while the campaign lives; callers never erase records.
    const WorldProgress &worldProgress(const WorldId &id) const;
    const FlagshipState &flagship() const noexcept;
    const CampaignClock &clock() const noexcept;
    // Concrete cargo/fleet/time operations will validate their own invariants.
    void addMinerals(unsigned category, unsigned amount);
    void schedule(ScheduledStoryEvent event);

private:
    friend class World;
    // Ensure a stable record before publishing a live World. May allocate/throw
    // during construction. Rehash preserves references; records must not be
    // erased or replaced while any live world refers to them.
    WorldProgress &prepareWorldProgress(const WorldId &id);

    std::bitset<static_cast<std::size_t>(StoryFlag::Count)> flags_;
    std::array<unsigned, static_cast<std::size_t>(StoryCounter::Count)> counters_{};
    std::unordered_map<WorldId, WorldProgress, WorldIdHash> worlds_;
    FlagshipState flagship_;
    std::vector<FleetProgress> fleets_;
    CampaignClock clock_;
};
} // namespace uqm::design
#endif

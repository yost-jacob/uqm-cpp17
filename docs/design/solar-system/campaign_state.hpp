#ifndef UQM_DESIGN_CAMPAIGN_STATE_HPP_
#define UQM_DESIGN_CAMPAIGN_STATE_HPP_

// Declaration-only architecture sketch. Not part of the game build.
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
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
    ChmmrUnleashed, StarbaseAvailable, Count
};
enum class StoryCounter : std::size_t { BombProgress, ShofixtiVisits, Count };
constexpr unsigned counterMaximum(StoryCounter counter) noexcept {
    switch (counter) {
        case StoryCounter::BombProgress: return 3; // Existing two-bit value.
        case StoryCounter::ShofixtiVisits: return 7; // Existing three-bit value.
        default: return 0; // Invalid fields are rejected by the public setters.
    }
}
// A player/ship identity is distinct from a shared-universe world identity.
struct PlayerId { std::uint64_t value; };
bool operator==(PlayerId left, PlayerId right) noexcept;
struct PlayerIdHash { std::size_t operator()(PlayerId id) const noexcept; };
enum class PlayerFlag : std::size_t { SunDeviceOnShip, Count };
struct FlagshipState {
    std::bitset<static_cast<std::size_t>(PlayerFlag::Count)> flags;
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
// A detached value snapshot: copy under ONE read lock, then serialize/use it
// after unlocking. It contains facts/IDs, never mutexes, pointers or handlers.
struct CampaignSnapshot {
    std::uint64_t revision = 0;
    std::bitset<static_cast<std::size_t>(StoryFlag::Count)> flags;
    std::array<unsigned, static_cast<std::size_t>(StoryCounter::Count)> counters{};
    std::unordered_map<WorldId, WorldProgress, WorldIdHash> worlds;
    std::unordered_map<PlayerId, FlagshipState, PlayerIdHash> players;
    std::vector<FleetProgress> fleets;
    CampaignClock clock;
};
struct PickupReward {
    std::array<unsigned, 8> minerals{};
    unsigned biological = 0;
};

// Shared ownership is lifetime management, not synchronization. The future
// implementation must lock all access to data_: shared_lock for queries and
// unique_lock for mutations. No mutable references or lock guards escape.
// Callers are trusted authoritative simulation code; this is not a network API.
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
    bool read(PlayerId player, PlayerFlag flag) const;
    void set(PlayerId player, PlayerFlag flag);
    void clear(PlayerId player, PlayerFlag flag);
    void set(StoryCounter counter, unsigned value); // Range-checked, never masked.
    template<StoryCounter Counter, unsigned Value>
    void set() {
        static_assert(static_cast<std::size_t>(Counter)
                < static_cast<std::size_t>(StoryCounter::Count), "unknown story counter");
        static_assert(Value <= counterMaximum(Counter), "story counter value out of range");
        set(Counter, Value);
    }
    // Return copies while holding a read lock; never hand out references into
    // mutable shared containers. Unvisited worlds read as empty progress.
    WorldProgress worldProgress(const WorldId &id) const;
    FlagshipState flagship(PlayerId player) const;
    CampaignClock clock() const;
    CampaignSnapshot snapshot() const;

    // Validate the world/node/player and trusted, server-calculated reward.
    // Under ONE write lock, claim the node and grant the reward together.
    // An already claimed node returns false and awards nothing. Do not run
    // behavior callbacks, rendering, network operations or file I/O under lock.
    bool tryCollect(PlayerId player, const WorldId &world, NodeId node,
            const PickupReward &reward);
    // Merge confirmed retrieved bits (OR), never replace another session's masks.
    // This checkpoint grants no cargo/reward and is idempotent. It must never
    // publish speculative client claims. Applies only to permanent collection;
    // future respawn/reset operations need a separate versioned policy so stale
    // checkpoints cannot resurrect old bits. Lock/allocation errors may throw.
    void mergeWorldProgress(const WorldId &id, const WorldProgress &confirmed);
    // Server/session setup: register a unique player before accessing its ship.
    void addPlayer(PlayerId player, FlagshipState initial);
    void addMinerals(PlayerId player, unsigned category, unsigned amount);
    void schedule(ScheduledStoryEvent event);

private:
    mutable std::shared_mutex mutex_;
    CampaignSnapshot data_;
};
} // namespace uqm::design
#endif

#ifndef UQM_DESIGN_WORLD_STATE_HPP_
#define UQM_DESIGN_WORLD_STATE_HPP_

#include <shared_mutex>
#include <unordered_map>
#include "state_types.hpp"

namespace uqm::design {
// Representative shared physical facts; exact story scoping needs an audit.
enum class WorldFlag : std::size_t { ChmmrUnleashed, Count };
struct WorldSnapshot {
    UniverseId id;
    std::uint64_t revision = 0;
    std::bitset<static_cast<std::size_t>(WorldFlag::Count)> flags;
    std::unordered_map<WorldId, WorldProgress, WorldIdHash> worlds;
    std::vector<FleetProgress> fleets;
    WorldClock clock;
};
enum class PresenceArea { HyperSpace, QuasiSpace, SolarSystem, Orbit };
// Public runtime projection only. Position units depend on area; system/world
// identify the local coordinate frame. No cargo, quest flags or account secrets.
struct PlayerPresence {
    PlayerId player;
    std::string displayName;
    PresenceArea area;
    std::optional<StarId> system;
    std::optional<WorldId> world;
    StarPosition position;
    unsigned facing;
};
class CampaignState;
class StateActions;

// ONE shared universe instance, not a planet object and not a player campaign.
// Several sessions/worlds retain shared_ptr ownership. It never owns them back.
class WorldState {
public:
    explicit WorldState(UniverseId id);
    WorldState(const WorldState &) = delete;
    WorldState &operator=(const WorldState &) = delete;
    UniverseId id() const;
    bool read(WorldFlag flag) const;
    void set(WorldFlag flag);
    void clear(WorldFlag flag);
    WorldProgress worldProgress(const WorldId &id) const; // Empty if unvisited.
    WorldClock clock() const;
    WorldSnapshot snapshot() const; // Persistent world data, NOT online presence.
    // OR only confirmed PERMANENT collection bits; never award cargo here or
    // upload speculative client masks. Respawn/reset needs separate versioning.
    void mergeWorldProgress(const WorldId &id, const WorldProgress &confirmed);
    void schedule(ScheduledStoryEvent event);

    // Authority publishes accepted movement with an authenticated player binding;
    // it validates area/frame, speed and current session generation. Clients do
    // not arbitrarily publish another player's coordinates. Copies outlive locks.
    void publishPresence(PlayerPresence presence);
    std::vector<PlayerPresence> nearbyPlayers(PlayerId observer) const;
    // Server disconnect/reconnect handling must reject stale session removals.
    void removePresence(PlayerId player);

private:
    friend class StateActions;
    mutable std::shared_mutex mutex_;
    WorldSnapshot data_;
    std::unordered_map<PlayerId, PlayerPresence, PlayerIdHash> presence_;
};

// Trusted in-process coordinator: a pickup changes TWO owners. Lock campaign
// and universe together with std::scoped_lock, not one lock followed by another.
// Claim the shared node and award personal cargo as one atomic state operation;
// no reward for an existing claim. Validate player, world, node and capacity.
// Rewards are server-calculated; never accept arbitrary network rewards. Do not
// execute story callbacks, I/O or public lock-taking methods inside this lock.
// Remote/container services will need an authoritative transaction/command
// protocol instead: shared_ptr and mutex do not cross a process boundary.
class StateActions {
public:
    static bool tryCollect(CampaignState &player, WorldState &universe,
            const WorldId &world, NodeId node, const PickupReward &reward);
};
} // namespace uqm::design
#endif

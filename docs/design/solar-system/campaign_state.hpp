#ifndef UQM_DESIGN_CAMPAIGN_STATE_HPP_
#define UQM_DESIGN_CAMPAIGN_STATE_HPP_

// Declaration-only, per-user state. Not part of the game build.
#include <unordered_set>
#include <unordered_map>
#include "state_types.hpp"

namespace uqm::design {
// Representative PERSONAL facts. Physical universe changes use WorldFlag.
enum class StoryFlag : std::size_t { StarbaseAccessGranted, SunDeviceOnShip, Count };
enum class StoryCounter : std::size_t { BombProgress, ShofixtiVisits, Count };
constexpr unsigned counterMaximum(StoryCounter counter) noexcept {
    switch (counter) {
        case StoryCounter::BombProgress: return 3;
        case StoryCounter::ShofixtiVisits: return 7;
        default: return 0;
    }
}
// Personal site history is distinct from shared depletion. Legacy energy masks
// also encode these records; see discovery-state.md before migrating scan bits.
struct DiscoveryProgress { std::bitset<32> visitedSites; };
// Detached per-user save value; never an aggregate of every player's campaign.
struct CampaignSnapshot {
    std::uint64_t revision = 0;
    std::bitset<static_cast<std::size_t>(StoryFlag::Count)> flags;
    std::array<unsigned, static_cast<std::size_t>(StoryCounter::Count)> counters{};
    std::vector<FleetProgress> escorts;
    std::unordered_set<WorldId, WorldIdHash> visitedWorlds;
    std::unordered_map<WorldId, DiscoveryProgress, WorldIdHash> discoveries;
    std::vector<ScheduledStoryEvent> personalEvents;
};
class StateActions;

// Personal story progress, owned by value inside PlayerEntity. Access is confined
// to the simulation thread; save through the enclosing player snapshot.
class CampaignState {
public:
    bool read(StoryFlag flag) const;
    unsigned read(StoryCounter counter) const;
    void set(StoryFlag flag);
    void clear(StoryFlag flag);
    void set(StoryCounter counter, unsigned value); // Range-checked, never masked.
    template<StoryCounter Counter, unsigned Value>
    void set() {
        static_assert(static_cast<std::size_t>(Counter)
                < static_cast<std::size_t>(StoryCounter::Count), "unknown story counter");
        static_assert(Value <= counterMaximum(Counter), "story counter value out of range");
        set(Counter, Value);
    }
    bool visited(const WorldId &id) const;
    void markVisited(const WorldId &id);
    CampaignSnapshot snapshot() const;
    void schedulePersonal(ScheduledStoryEvent event);

private:
    friend class StateActions;
    CampaignSnapshot data_;
};
} // namespace uqm::design
#endif

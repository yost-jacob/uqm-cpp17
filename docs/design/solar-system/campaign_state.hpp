#ifndef UQM_DESIGN_CAMPAIGN_STATE_HPP_
#define UQM_DESIGN_CAMPAIGN_STATE_HPP_

// Declaration-only, per-user state. Not part of the game build.
#include <shared_mutex>
#include <unordered_set>
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
// Detached per-user save value; never an aggregate of every player's campaign.
struct CampaignSnapshot {
    std::uint64_t revision = 0;
    UserProfile user;
    std::optional<UniverseId> universe; // Binding for visited IDs/quest policy.
    std::bitset<static_cast<std::size_t>(StoryFlag::Count)> flags;
    std::array<unsigned, static_cast<std::size_t>(StoryCounter::Count)> counters{};
    FlagshipState flagship;
    std::vector<FleetProgress> escorts;
    std::unordered_set<WorldId, WorldIdHash> visitedWorlds;
    std::vector<ScheduledStoryEvent> personalEvents;
};
class StateActions;

// One player owns one campaign. Its session/UI/jobs may retain shared_ptr copies
// for lifetime, but another player has a different CampaignState. Shared physical
// facts, collection masks, global clock and player presence belong to WorldState.
class CampaignState {
public:
    explicit CampaignState(UserProfile user); // ID immutable for this instance.
    CampaignState(const CampaignState &) = delete;
    CampaignState &operator=(const CampaignState &) = delete;
    PlayerId player() const;
    UserProfile user() const;
    std::optional<UniverseId> universe() const;
    void bindUniverse(UniverseId universe); // Host-authorized join/transfer only.
    void rename(std::string displayName); // Validate; never change the player ID.
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
    FlagshipState flagship() const;
    CampaignSnapshot snapshot() const;
    void addMinerals(unsigned category, unsigned amount);
    void schedulePersonal(ScheduledStoryEvent event);

private:
    friend class StateActions;
    // All future access to data_ must be locked; public queries return copies.
    // No mutable references/guards escape and no I/O/callback runs while locked.
    mutable std::shared_mutex mutex_;
    CampaignSnapshot data_;
};
} // namespace uqm::design
#endif

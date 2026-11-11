#ifndef UQM_DESIGN_PLAYER_ENTITY_HPP_
#define UQM_DESIGN_PLAYER_ENTITY_HPP_

#include <memory>
#include "story_state.hpp"
namespace uqm { class Fleet; class SIS; }
#include "world_state.hpp"

namespace uqm::design {
// One detached personal save. Identity, ship and campaign have one owner each.
struct PlayerSnapshot {
    std::uint64_t revision = 0;
    UserProfile user;
    std::optional<UniverseId> universe;
    PlayerLocation location;
    FlagshipState ship;
    StorySnapshot story;
    std::vector<FleetProgress> escorts;
};

// Simulation-thread confined. GameState owns the entity; sessions borrow it.
// Reserve synchronization here if worker access is introduced, rather than
// independently locking ship and campaign. No pointers to other owners are saved.
class PlayerEntity {
public:
    explicit PlayerEntity(UserProfile user);
    PlayerEntity(const PlayerEntity &) = delete;
    PlayerEntity &operator=(const PlayerEntity &) = delete;
    PlayerId id() const noexcept; // Immutable identity supplied at construction.
    UserProfile profile() const;
    PlayerPresence presence() const; // Public projection, excluding private data.
    FlagshipState ship() const;
    uqm::SIS &sis() noexcept;
    const uqm::SIS &sis() const noexcept;
    uqm::Fleet &fleet() noexcept;
    const uqm::Fleet &fleet() const noexcept;
    StoryState &story() noexcept;
    const StoryState &story() const noexcept;
    PlayerSnapshot snapshot() const; // Copy at an authoritative action boundary.

private:
    friend class GameState;
    friend class StateActions;
    std::uint64_t revision_ = 0;
    UserProfile user_;
    std::optional<UniverseId> universe_;
    PlayerLocation location_;
    StoryState story_;
    std::unique_ptr<uqm::Fleet> fleet_;
};
} // namespace uqm::design
#endif

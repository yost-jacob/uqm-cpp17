#ifndef UQM_DESIGN_GAME_STATE_HPP_
#define UQM_DESIGN_GAME_STATE_HPP_

#include <memory>
#include "player_entity.hpp"

namespace uqm::design {
// Storage boundary, initially containing exactly one player. Callers use IDs,
// never collection iterators, indices or assumptions about a linked list.
// All access and attachment changes run on the authoritative simulation thread.
class GameState {
public:
    // universe must be non-null; profile supplies the sole initial player ID.
    GameState(UserProfile profile, std::shared_ptr<WorldState> universe);
    GameState(const GameState &) = delete;
    GameState &operator=(const GameState &) = delete;
    // Checked lookup: throw std::out_of_range for an unknown ID. References are
    // borrowed for this simulation operation, not retained across state changes.
    PlayerEntity &player(PlayerId id);
    const PlayerEntity &player(PlayerId id) const;
    WorldState &world() noexcept;
    const WorldState &world() const noexcept;
    // Runtime worlds retain this owner; non-null and independent of player storage.
    std::shared_ptr<WorldState> worldOwner();
    // Validate identity/binding, publish accepted location and public presence.
    void updatePresence(PlayerId id, PlayerLocation acceptedMovement);
    bool tryCollect(PlayerId player, const WorldId &world, NodeId node,
            const PickupReward &trustedReward);
    PlayerSnapshot snapshot(PlayerId id) const;
    // Validate identity/universe first. Update in place at an action boundary;
    // loading one player's save must never rewind the shared universe.
    void restore(PlayerId id, PlayerSnapshot candidate);

private:
    PlayerEntity player_; // Replaceable storage detail; no collection needed yet.
    std::shared_ptr<WorldState> universe_;
};
} // namespace uqm::design
#endif

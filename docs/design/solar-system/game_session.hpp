#ifndef UQM_DESIGN_GAME_SESSION_HPP_
#define UQM_DESIGN_GAME_SESSION_HPP_

#include <iosfwd>
#include "game_state.hpp"
#include "world.hpp"

namespace uqm::design {
enum class ConversationId { Chmmr, Thraddash, EarthStarbase };
enum class BattleId { Encounter, SaMatra };

// One player's runtime. The state boundary hides single/multiplayer storage.
// Session attachment and borrowed entity access are simulation-thread confined.
class GameSession {
public:
    // state must be non-null and contain player. Reject unknown IDs on attachment.
    GameSession(std::shared_ptr<GameState> state, PlayerId player);
    ~GameSession();
    GameSession(const GameSession &) = delete;
    GameSession &operator=(const GameSession &) = delete;
    GameSession(GameSession &&) = delete;
    GameSession &operator=(GameSession &&) = delete;
    PlayerId player() const;
    PlayerEntity &playerEntity(); // Checked lookup through state_, not cached.
    const PlayerEntity &playerEntity() const;
    StoryState &story();
    const StoryState &story() const;
    WorldState &worldState();
    const WorldState &worldState() const;
    void enterSystem(const StarDesc &star, RandomStream &random);
    void leaveSystem(); // Checkpoint confirmed shared progress, then tear down.
    void updatePresence(PlayerLocation acceptedMovement);
    std::vector<PlayerPresence> nearbyPlayers() const;
    // Route accepted collection to StateActions for an atomic shared claim and
    // personal reward. This method runs on the authoritative interaction path.
    bool tryCollect(const WorldId &world, NodeId node, const PickupReward &reward);
    // A player save serializes only their PlayerSnapshot. Shared-world
    // persistence is a separate host responsibility with coordinated checkpoints.
    void save(std::ostream &destination);
    // Validate candidate personal state, identity and universe binding first.
    // A client cannot roll back shared nodes or replace the universe by loading
    // a player save. Online restoration/rewards require host authorization.
    // Target persistence API only. Production menus request a slot and unwind;
    // decoding runs at the outer session boundary, never beneath a live mode.
    void load(std::istream &source);
    void openNormalOrbit(World &world);
    void launchLander(World &world); // Reject when world.canLand() is false.
    void startConversation(ConversationId conversation);
    void startBattle(BattleId battle);

private:
    std::shared_ptr<GameState> state_;
    PlayerId player_;
    std::unique_ptr<SolarSystem> system_;
};
} // namespace uqm::design
#endif

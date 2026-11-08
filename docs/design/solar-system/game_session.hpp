#ifndef UQM_DESIGN_GAME_SESSION_HPP_
#define UQM_DESIGN_GAME_SESSION_HPP_

#include <iosfwd>
#include "campaign_state.hpp"
#include "world.hpp"

namespace uqm::design {
enum class ConversationId { Chmmr, Thraddash, EarthStarbase };
enum class BattleId { Encounter, SaMatra };

// One player's runtime. CampaignState belongs to this user; WorldState is shared
// with other users. Runtime/session attachment stays simulation-thread confined.
// Workers receive their own shared_ptr copies, not racing access to these members.
class GameSession {
public:
    // Non-null owners; player identity comes from campaign.user(), not another
    // independent ID that could disagree with the user's campaign.
    GameSession(std::shared_ptr<CampaignState> campaign,
            std::shared_ptr<WorldState> universe);
    ~GameSession();
    GameSession(const GameSession &) = delete;
    GameSession &operator=(const GameSession &) = delete;
    GameSession(GameSession &&) = delete;
    GameSession &operator=(GameSession &&) = delete;
    PlayerId player() const;
    std::shared_ptr<CampaignState> campaign();
    std::shared_ptr<const CampaignState> campaign() const;
    std::shared_ptr<WorldState> worldState();
    std::shared_ptr<const WorldState> worldState() const;
    void enterSystem(const StarDesc &star, RandomStream &random);
    void leaveSystem(); // Checkpoint confirmed shared progress, then tear down.
    // Join/leave an instance without copying private campaigns into its state.
    // Publish only public presence. Shared_ptr lifetime is not access permission.
    void attachUniverse(std::shared_ptr<WorldState> universe);
    void updatePresence(PlayerPresence acceptedMovement);
    std::vector<PlayerPresence> nearbyPlayers() const;
    // Route accepted collection to StateActions for an atomic shared claim and
    // personal reward. This method runs on the authoritative interaction path.
    bool tryCollect(const WorldId &world, NodeId node, const PickupReward &reward);
    // A player save serializes only their CampaignSnapshot. Shared-world
    // persistence is a separate host responsibility with coordinated checkpoints.
    void save(std::ostream &destination);
    // Validate candidate personal state, identity and universe binding first.
    // A client cannot roll back shared nodes or replace the universe by loading
    // a player save. Online restoration/rewards require host authorization.
    void load(std::istream &source);
    void openNormalOrbit(World &world);
    void launchLander(World &world); // Reject when world.canLand() is false.
    void startConversation(ConversationId conversation);
    void startBattle(BattleId battle);

private:
    std::shared_ptr<CampaignState> campaign_;
    std::shared_ptr<WorldState> universe_;
    std::unique_ptr<SolarSystem> system_;
};
} // namespace uqm::design
#endif

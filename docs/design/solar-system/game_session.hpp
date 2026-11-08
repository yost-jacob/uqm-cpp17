#ifndef UQM_DESIGN_GAME_SESSION_HPP_
#define UQM_DESIGN_GAME_SESSION_HPP_

#include <iosfwd>
#include "world.hpp"

namespace uqm::design {
// Representative interaction IDs; final mappings must preserve existing content.
enum class ConversationId { Chmmr, Thraddash, EarthStarbase };
enum class BattleId { Encounter, SaMatra };

// Per-player running session attached to a possibly shared campaign. Session and
// local World runtime methods are simulation-thread confined. Other threads get
// their own shared_ptr copies and use the synchronized CampaignState interface;
// they must not race attachment changes on this shared_ptr member itself.
class GameSession {
public:
    explicit GameSession(PlayerId player, std::shared_ptr<CampaignState> campaign); // Non-null.
    ~GameSession();
    GameSession(const GameSession &) = delete;
    GameSession &operator=(const GameSession &) = delete;
    GameSession(GameSession &&) = delete;
    GameSession &operator=(GameSession &&) = delete;
    PlayerId player() const noexcept;
    std::shared_ptr<CampaignState> campaign();
    std::shared_ptr<const CampaignState> campaign() const;
    void enterSystem(const StarDesc &star, RandomStream &random);
    void leaveSystem(); // Explicit checkpoint before runtime teardown; may throw.
    // Detach only this player/session. Other sessions/worlds keep the old state
    // alive. Online campaign replacement requires authoritative host coordination.
    void attachCampaign(std::shared_ptr<CampaignState> campaign);
    // Checkpoint confirmed progress, take one coherent CampaignSnapshot under a
    // read lock, release the lock, then write it. Do not hold locks during I/O.
    // This is a point-in-time universe snapshot, not a pause of every client.
    void save(std::ostream &destination);
    // Offline/host-authorized load: validate a candidate first, checkpoint/tear
    // down this session's old runtime, then attach the candidate and rebuild.
    // A client may not replace shared universe state for other players. Reject
    // delayed commands from a prior campaign/session generation after attachment.
    void load(std::istream &source);
    // Behavior methods request these existing game operations through the session.
    // They query/change current campaign state, never a constructor-time snapshot.
    void openNormalOrbit(World &world);
    void launchLander(World &world); // Reject when world.canLand() is false.
    void startConversation(ConversationId conversation);
    void startBattle(BattleId battle);

private:
    PlayerId player_;
    std::shared_ptr<CampaignState> campaign_;
    std::unique_ptr<SolarSystem> system_;
};
} // namespace uqm::design
#endif

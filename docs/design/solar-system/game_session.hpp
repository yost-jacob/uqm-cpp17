#ifndef UQM_DESIGN_GAME_SESSION_HPP_
#define UQM_DESIGN_GAME_SESSION_HPP_

#include <iosfwd>
#include "world.hpp"

namespace uqm::design {
// Representative interaction IDs; final mappings must preserve existing content.
enum class ConversationId { Chmmr, Thraddash, EarthStarbase };
enum class BattleId { Encounter, SaMatra };

// Coordinates the running game. Own campaign BEFORE the system so reverse member
// destruction destroys worlds (and commits them) before releasing campaign data.
class GameSession {
public:
    explicit GameSession(std::unique_ptr<CampaignState> campaign); // Non-null.
    ~GameSession();
    GameSession(const GameSession &) = delete;
    GameSession &operator=(const GameSession &) = delete;
    GameSession(GameSession &&) = delete;
    GameSession &operator=(GameSession &&) = delete;
    CampaignState &campaign() noexcept;
    const CampaignState &campaign() const noexcept;
    void enterSystem(const StarDesc &star, RandomStream &random);
    void leaveSystem(); // Destroy the system while its campaign still lives.
    // Flush working world progress, then serialize the campaign. Keep playing:
    // no runtime object destruction, encounter restart or loss of stunned bio.
    void save(std::ostream &destination);
    // Parse/validate a candidate campaign first. On validation failure, preserve
    // this session. Then destroy old worlds (committing to OLD campaign), replace
    // the campaign owner, and reconstruct the saved location and handlers.
    void load(std::istream &source);
    // Behavior methods request these existing game operations through the session.
    // They query/change current campaign state, never a constructor-time snapshot.
    void openNormalOrbit(World &world);
    void launchLander(World &world); // Reject when world.canLand() is false.
    void startConversation(ConversationId conversation);
    void startBattle(BattleId battle);

private:
    std::unique_ptr<CampaignState> campaign_;
    std::unique_ptr<SolarSystem> system_;
};
} // namespace uqm::design
#endif

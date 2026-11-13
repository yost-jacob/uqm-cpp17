#ifndef UQM_COMBAT_MODE_HPP_
#define UQM_COMBAT_MODE_HPP_
#include "game_mode.hpp"
namespace uqm {
class StoryState;
class GameSession;
class Session;
// Session-specific resource/state preparation for the same CombatMode. Resources
// acquired by prepare must be released by the derived destructor, including errors.
// prepare receives the child's owned engine with its compatibility binding selected;
// borrow it only until this preparation is destroyed. false skips a match. complete is
// explicit normal completion only, never a destructor or cancellation callback.
class CombatPreparation {
public:
    virtual ~CombatPreparation() = default;
    virtual bool prepare(SimulationEngine &engine) = 0;
    virtual void complete() {}
};
// session/source and any state borrowed by preparation must outlive this child.
// Participant pool addresses stay stable; results return before parent resume.
std::unique_ptr<GameMode> createCombatMode(Session &session, RuntimeShipRoster &source,
    std::uint32_t seed, std::unique_ptr<CombatPreparation> preparation);
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, RuntimeShipRoster &source);
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, RuntimeShipRoster &source, std::uint32_t seed);
std::unique_ptr<CombatPreparation> createCampaignCombatPreparation(GameSession &session);
}
#endif

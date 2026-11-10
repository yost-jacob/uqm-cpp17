#ifndef UQM_COMBAT_MODE_HPP_
#define UQM_COMBAT_MODE_HPP_
#include "game_mode.hpp"
namespace uqm {
class StoryState;
class GameSession;
// Session-specific resource/state preparation for the same CombatMode. Resources
// acquired by prepare must be released by the derived destructor, including errors.
// prepare runs with the child engine selected; false skips a match. complete is
// explicit normal completion only, never a destructor or cancellation callback.
class CombatPreparation {
public:
    virtual ~CombatPreparation() = default;
    virtual bool prepare() = 0;
    virtual void complete() {}
};
// host/source and any state borrowed by preparation must outlive this child.
// Participant pool addresses stay stable; results return before parent resume.
std::unique_ptr<GameMode> createCombatMode(ModeDispatcher &host, RuntimeShipRoster &source,
    std::uint32_t seed, std::unique_ptr<CombatPreparation> preparation);
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session);
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, std::uint32_t seed);
std::unique_ptr<CombatPreparation> createCampaignCombatPreparation(StoryState &story);
}
#endif

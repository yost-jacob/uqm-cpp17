#ifndef UQM_COMBAT_MODE_HPP_
#define UQM_COMBAT_MODE_HPP_
#include "game_mode.hpp"
namespace uqm {
class StoryState;
class GameSession;
// Borrow the session for one synchronous campaign battle. The enclosing
// encounter prepares both rosters and applies aftermath after the child returns.
// This does not yet provide the campaign-independent Super Melee interface.
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session);
// Explicit seed for a recorded/negotiated battle; no clock reseeding in Battle.
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session, std::uint32_t seed);
// Legacy combat backend: story state must belong to the active encounter and
// outlive this call. Temporary sound/input/activity bindings are scoped here;
// the mode engine owns its queues; loaded assets retain the existing backend adapters.
void runCampaignCombat(StoryState &story);
}
#endif

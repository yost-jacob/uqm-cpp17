#ifndef UQM_COMBAT_MODE_HPP_
#define UQM_COMBAT_MODE_HPP_
#include "game_mode.hpp"
namespace uqm {
class CampaignState;
class GameSession;
// Borrow the session for one synchronous campaign battle. The enclosing
// encounter prepares both rosters and applies aftermath after the child returns.
// This does not yet provide the campaign-independent Super Melee interface.
std::unique_ptr<GameMode> createCampaignCombatMode(GameSession &session);
// Legacy combat backend: campaign must belong to the active encounter and
// outlive this call. Temporary sound/input/activity bindings are scoped here;
// physical battle queues and engine resources retain their legacy ownership.
void runCampaignCombat(CampaignState &campaign);
}
#endif

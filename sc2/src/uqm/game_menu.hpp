#ifndef UQM_GAME_MENU_HPP_
#define UQM_GAME_MENU_HPP_
#include "game_mode.hpp"
namespace uqm {
class GameSession;
// Synchronous menu: nullopt resumes its caller, a value must propagate upward.
// The implementation is the C buffer boundary; no load is performed inside it.
MenuResult runGameOptionsMenu(GameSession &session);
}
#endif

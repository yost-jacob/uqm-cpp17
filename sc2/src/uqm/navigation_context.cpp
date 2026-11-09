#include "hyper_navigation.hpp"
#include "game_state.hpp"
#include <stdexcept>
namespace uqm {
HyperSpaceNavigation::HyperSpaceNavigation(PlayerEntity &player) noexcept
    : player_(player), story_(player.story()) {}
NavigationBinding::NavigationBinding(GameSession &session, HyperSpaceNavigation &navigation)
    : session_(session), previous_(session.navigation_) {
    if (&session.player() != &navigation.player_)
        throw std::invalid_argument("navigation belongs to another player");
    session.navigation_ = &navigation;
}
NavigationBinding::~NavigationBinding() { session_.navigation_ = previous_; }
HyperSpaceNavigation &GameSession::navigation() {
    if (!navigation_) throw std::logic_error("navigation callback outside its owning mode");
    return *navigation_;
}
}

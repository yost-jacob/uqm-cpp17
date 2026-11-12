#include "hyper_navigation.hpp"
#include "game_state.hpp"
#include <stdexcept>
namespace uqm {
SpaceNavigation::SpaceNavigation(PlayerEntity &player, SimulationEngine &engine, CampaignClock &clock, NavigationRealm realm) noexcept
    : realm_(realm), player_(player), clock_(clock), engine_(engine), story_(player.story()) {}
PLAYER_TRAVEL_POSITION &SpaceNavigation::travel() {
    if (isQuasiSpace())
        return std::get<QuasiSpaceLocation>(player_.location());
    return std::get<HyperSpaceLocation>(player_.location());
}
void SpaceNavigation::prepareRealmTransition(NavigationRealm destination, PLAYER_MAP_POSITION position) {
    travel().position = position;
    story_.set(StoryCounter::ArilouSpaceSide, destination == NavigationRealm::QuasiSpace ? 3 : 0);
}
NavigationBinding::NavigationBinding(GameSession &session, SpaceNavigation &navigation)
    : session_(session), previous_(session.navigation_) {
    if (&session.player() != &navigation.player_ || &session.clock() != &navigation.clock_)
        throw std::invalid_argument("navigation belongs to another player or campaign clock");
    session.navigation_ = &navigation;
}
NavigationBinding::~NavigationBinding() { session_.navigation_ = previous_; }
SpaceNavigation &GameSession::navigation() {
    if (!navigation_) throw std::logic_error("navigation callback outside its owning mode");
    return *navigation_;
}
}

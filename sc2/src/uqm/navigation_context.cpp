#include "hyper_navigation.hpp"
#include "game_state.hpp"
#include <stdexcept>
namespace uqm {
HyperSpaceNavigation::HyperSpaceNavigation(PlayerEntity &player, SimulationEngine &engine, CampaignClock &clock) noexcept
    : player_(player), clock_(clock), engine_(engine), story_(player.story()) {}
PLAYER_TRAVEL_POSITION &HyperSpaceNavigation::travel() {
    if (story_.read(StoryCounter::ArilouSpaceSide) > 1)
        return std::get<QuasiSpaceLocation>(player_.location());
    return std::get<HyperSpaceLocation>(player_.location());
}
NavigationBinding::NavigationBinding(GameSession &session, HyperSpaceNavigation &navigation)
    : session_(session), previous_(session.navigation_) {
    if (&session.player() != &navigation.player_ || &session.clock() != &navigation.clock_)
        throw std::invalid_argument("navigation belongs to another player or campaign clock");
    session.navigation_ = &navigation;
}
NavigationBinding::~NavigationBinding() { session_.navigation_ = previous_; }
HyperSpaceNavigation &GameSession::navigation() {
    if (!navigation_) throw std::logic_error("navigation callback outside its owning mode");
    return *navigation_;
}
}

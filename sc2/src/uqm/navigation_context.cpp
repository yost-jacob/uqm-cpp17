#include "hyper_navigation.hpp"
#include "game_state.hpp"
#include <stdexcept>
namespace uqm {
SpaceNavigation::SpaceNavigation(PlayerEntity &player, SimulationEngine &engine, CampaignClock &clock, NavigationRealm realm, CampaignEncounters &records) noexcept
    : records_(records), realm_(realm), player_(player), clock_(clock), engine_(engine), story_(player.story()) {}
PLAYER_TRAVEL_POSITION &SpaceNavigation::travel() {
    if (isQuasiSpace())
        return std::get<QuasiSpaceLocation>(player_.location());
    return std::get<HyperSpaceLocation>(player_.location());
}
void SpaceNavigation::prepareRealmTransition(NavigationRealm destination, PLAYER_MAP_POSITION position) {
    travel().position = position;
    story_.set(StoryCounter::ArilouSpaceSide, destination == NavigationRealm::QuasiSpace ? 3 : 0);
}
}

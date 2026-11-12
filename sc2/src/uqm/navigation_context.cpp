#include "hyper_navigation.hpp"
#include "game_state.hpp"
#include <stdexcept>
namespace uqm {
SpaceNavigation::SpaceNavigation(GameSession &session, SimulationEngine &engine, NavigationRealm realm)
    : legacy_(detail::navigationLegacy(session.player())), session_(session), records_(session.encounters()), realm_(realm),
      player_(session.player()), clock_(session.clock()), engine_(engine), story_(player_.story()) {}
const StarMap &SpaceNavigation::catalogue() const { return session_.definitions().stars(); }
std::optional<StarId> &SpaceNavigation::selectedStar() noexcept { return player_.navigationState().currentStar; }
std::vector<StarId> SpaceNavigation::starsNear(const POINT &position, SIZE xbounds, SIZE ybounds) const {
    return catalogue().find({isQuasiSpace() ? StarSpace::QuasiSpace : StarSpace::HyperSpace,
        {position.x, position.y}, xbounds < 0 ? std::nullopt : std::optional<int>{xbounds},
        ybounds < 0 ? std::nullopt : std::optional<int>{ybounds}});
}
PLAYER_TRAVEL_POSITION &SpaceNavigation::travel() {
    if (isQuasiSpace())
        return std::get<QuasiSpaceLocation>(player_.location());
    return std::get<HyperSpaceLocation>(player_.location());
}
void SpaceNavigation::prepareRealmTransition(NavigationRealm destination, PLAYER_MAP_POSITION position) {
    travel().position = position;
    story_.set(StoryCounter::ArilouSpaceSide, destination == NavigationRealm::QuasiSpace ? 3 : 0);
    transition_ = NavigationTransition{destination == NavigationRealm::QuasiSpace
        ? NavigationDestination::QuasiSpace : NavigationDestination::HyperSpace, std::nullopt};
}
void SpaceNavigation::settlePortalSide(bool defiesPhysics) {
    if (transition_) return;
    if (!defiesPhysics || story_.read(StoryCounter::ArilouSpaceCounter) == 0)
        story_.set(StoryCounter::ArilouSpaceSide, isQuasiSpace() ? 3 : 0);
}
}

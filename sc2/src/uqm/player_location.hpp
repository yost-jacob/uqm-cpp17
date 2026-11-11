#ifndef UQM_PLAYER_LOCATION_HPP_
#define UQM_PLAYER_LOCATION_HPP_
#include "player_location.h"
#include "world_types.hpp"
#include <optional>
#include <variant>

namespace uqm {
using MapPosition = PLAYER_MAP_POSITION;
using FlightState = PLAYER_FLIGHT_POSITION;
// The common travel record also retains legacy entry/save markers while the
// old loops migrate. There is one live record, never a synchronized copy in SIS
// or global campaign storage. In a system, position anchors its star on the map.
struct HyperSpaceLocation : PLAYER_TRAVEL_POSITION {};
struct QuasiSpaceLocation : PLAYER_TRAVEL_POSITION {};
struct OrbitLocation { WorldId world; };
struct SurfaceLocation { POINT position{}; };
struct SolarSystemLocation : PLAYER_TRAVEL_POSITION {
    std::optional<StarId> system; // Set once the selected star is resolved.
    std::optional<OrbitLocation> orbit;
    std::optional<SurfaceLocation> surface;
    // Entering orbit preserves flight; landing preserves both flight and orbit.
    void enterOrbit(WorldId world) { orbit = OrbitLocation{world}; surface.reset(); }
    // Requires an active orbit; a broken call sequence throws bad_optional_access.
    void land(POINT point) { orbit.value(); surface = SurfaceLocation{point}; }
    void leaveSurface() noexcept { surface.reset(); }
    void leaveOrbit() noexcept { surface.reset(); orbit.reset(); }
};
using PlayerLocation = std::variant<HyperSpaceLocation, QuasiSpaceLocation, SolarSystemLocation>;
// All borrows expire when the alternative changes or the player is destroyed.
inline PLAYER_TRAVEL_POSITION &travelPosition(PlayerLocation &location) noexcept {
    return std::visit([](auto &value) -> PLAYER_TRAVEL_POSITION & { return value; }, location);
}
inline const PLAYER_TRAVEL_POSITION &travelPosition(const PlayerLocation &location) noexcept {
    return std::visit([](const auto &value) -> const PLAYER_TRAVEL_POSITION & { return value; }, location);
}
}
#endif

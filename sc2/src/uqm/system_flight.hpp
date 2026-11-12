#ifndef UQM_SYSTEM_FLIGHT_HPP_
#define UQM_SYSTEM_FLIGHT_HPP_
#include "planets/planets.h"
#include "sis_state.hpp"
#include "displist.h"
#include "world_types.hpp"
#include <optional>
namespace uqm {
// Visit-local counters only. Persistent position remains in PlayerEntity.
struct SystemFlightState {
    bool active = false;
    COUNT waitIntersect = 0; // ~0 suppresses all body collisions until clear.
    BYTE turnCounter = 0, turnWait = 0, thrustCounter = 0, maxSpeed = 0;
    TimeCount nextFrame = 0;
    std::optional<WorldId> pendingOrbit;
    // Recompute limits from equipped drives/jets; preserve active countdowns.
    void configure(const SISLoadout &loadout) noexcept {
        maxSpeed = 5 * IP_SHIP_THRUST_INCREMENT;
        for (const auto drive : loadout.drives) if (drive == FUSION_THRUSTER) maxSpeed += IP_SHIP_THRUST_INCREMENT;
        turnWait = IP_SHIP_TURN_WAIT;
        for (const auto jet : loadout.jets) if (jet == TURNING_JETS) turnWait -= IP_SHIP_TURN_DECREMENT;
    }
};
// Read-only descriptor projection until step 7 moves geometry into native bodies.
// Referenced arrays must outlive the flight entities and group initialization.
struct SystemGeometry {
    const PLANET_DESC &sun;
    const PLANET_DESC (&planets)[MAX_PLANETS];
    POINT planetLocation(COUNT index) const;
};
// Explicit exploration entry; spawned entities borrow geometry/player state
// until the flight scene is cleared. No orbital/report resources are borrowed.
void runSystemMissions(SOLARSYS_STATE &system);
void notifyGroups(QUEUE &groups, COUNT race, BYTE destination);
// Initialize persistent group records using only the supplied geometry/queue.
bool initializeGroups(DWORD offset, const SystemGeometry &geometry, QUEUE &groups);
void buildGroups(const SystemGeometry &geometry, QUEUE &groups);
}
#endif

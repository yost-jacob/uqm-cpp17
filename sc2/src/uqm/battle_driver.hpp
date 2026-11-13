#ifndef UQM_BATTLE_DRIVER_HPP_
#define UQM_BATTLE_DRIVER_HPP_
#include "globdata.h"
#include <functional>
namespace uqm {
class SimulationEngine;
class SpaceNavigation;
// Match orchestration for the shared CombatMode. engine must remain selected
// while running and outlive this driver. The session has already supplied its
// roster, seed and service bindings; the driver never reseeds from the clock.
class CombatDriver {
public:
    CombatDriver(SimulationEngine &engine, ACTIVITY &activity) noexcept : engine_(engine), activity_(activity) {}
    // false means load/shutdown cancellation: do not publish survivor results.
    bool run(bool instantVictory = false, std::function<bool()> cancelled = {});
private:
    SimulationEngine &engine_;
    ACTIVITY &activity_;
};
// Navigation has its own entry/exit path but uses the same frame loop. The hook
// and navigation must remain alive until this synchronous call returns. Its
// mode-owned engine must be selected; no active-session navigation lookup is needed.
void runSpaceNavigation(SpaceNavigation &navigation, std::function<void()> beforeSimulation);
}
#endif

#ifndef UQM_BATTLE_DRIVER_HPP_
#define UQM_BATTLE_DRIVER_HPP_
#include <functional>
namespace uqm {
class SimulationEngine;
class HyperSpaceNavigation;
// Match orchestration for the shared CombatMode. engine must remain selected
// while running and outlive this driver. The session has already supplied its
// roster, seed and service bindings; the driver never reseeds from the clock.
class CombatDriver {
public:
    explicit CombatDriver(SimulationEngine &engine) noexcept : engine_(engine) {}
    void run();
private:
    SimulationEngine &engine_;
};
// Navigation has its own entry/exit path but uses the same frame loop. The hook
// and navigation must remain alive until this synchronous call returns. Its
// mode-owned engine must be selected; no active-session navigation lookup is needed.
void runSpaceNavigation(HyperSpaceNavigation &navigation, std::function<void()> beforeSimulation);
}
#endif

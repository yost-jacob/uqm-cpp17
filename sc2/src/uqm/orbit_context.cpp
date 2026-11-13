#include "orbit_runtime.hpp"
#include "world_visit.hpp"
namespace uqm {
OrbitRuntime::OrbitRuntime(WorldVisit &visit, SimulationEngine &engine) noexcept
    : session_(visit.session()), system_(visit.legacySystem()), engine_(engine),
      campaign_(detail::encounterLegacy(session_.player())) {}
OrbitRuntime &orbitRuntime(const SOLARSYS_STATE &system) {
    return systemVisit(system).orbit();
}
}

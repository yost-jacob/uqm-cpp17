#ifndef UQM_ORBIT_RUNTIME_HPP_
#define UQM_ORBIT_RUNTIME_HPP_
#include "system_runtime.hpp"
#include "encounter_legacy.hpp"
namespace uqm {
class WorldVisit;
// One orbital child's transient state. The session, visit, selected world and
// engine outlive this object and its synchronous scan/lander children. Terrain
// belongs to World; permanent retrieval/inventory changes belong to the session.
class OrbitRuntime {
public:
    OrbitRuntime(WorldVisit &visit, SimulationEngine &engine) noexcept;
    OrbitRuntime(const OrbitRuntime &) = delete;
    OrbitRuntime &operator=(const OrbitRuntime &) = delete;
    void run(); // Existing orbit/scan menus, with explicit owner context.
    GameSession &session() const noexcept { return session_; }
    SOLARSYS_STATE &system() const noexcept { return system_; }
    SimulationEngine &engine() const noexcept { return engine_; }
    SurfaceScanState &scan() noexcept { return scan_; }
    const detail::EncounterLegacyView &campaign() const noexcept { return campaign_; }
private:
    GameSession &session_;
    SOLARSYS_STATE &system_;
    SimulationEngine &engine_;
    detail::EncounterLegacyView campaign_;
    SurfaceScanState scan_{};
};
// Resolve only from the explicit visit; never from the active session/engine.
OrbitRuntime &orbitRuntime(const SOLARSYS_STATE &system);
}
#endif

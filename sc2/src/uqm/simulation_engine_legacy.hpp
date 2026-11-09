#ifndef UQM_SIMULATION_ENGINE_LEGACY_HPP_
#define UQM_SIMULATION_ENGINE_LEGACY_HPP_
#include "simulation_engine.hpp"
namespace uqm::detail {
struct SimulationAccess {
    static PRIMITIVE *primitives(SimulationEngine &engine) noexcept { return engine.primitives_.data(); }
    static void saveProjection(SimulationEngine &engine) noexcept;
    static void bindProjection(SimulationEngine &engine) noexcept;
    static QUEUE &queue(SimulationEngine &engine) noexcept { return engine.entities_; }
};
}
#endif

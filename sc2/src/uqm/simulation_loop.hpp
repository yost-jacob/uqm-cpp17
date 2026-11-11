#ifndef UQM_SIMULATION_LOOP_HPP_
#define UQM_SIMULATION_LOOP_HPP_
#include "libs/compiler.h"
#include <array>
#include <cstddef>
#include <functional>
#include <utility>
namespace uqm {
class SimulationEngine;
enum class FrameTransition { None, EnterSpace };
// Shared input/simulation/presentation/timing loop. engine must be selected and
// outlive this synchronous run. beforeSimulation is an optional native hook,
// called after controls and before the simulation pass (HyperSpace navigation).
class SimulationLoop {
public:
    explicit SimulationLoop(SimulationEngine &engine,
        std::function<void()> beforeSimulation = {},
        FrameTransition transition = FrameTransition::None)
        : engine_(engine), beforeSimulation_(std::move(beforeSimulation)),
          firstFrame_(transition == FrameTransition::EnterSpace) {}
    void run();
private:
    // The only opaque input buffer: DoInput borrows it for this call only.
    struct Input { BOOLEAN (*InputFunc)(void *); SimulationLoop *owner; };
    static BOOLEAN input(void *state);
    bool tick();
    void processInput();
    SimulationEngine &engine_;
    std::function<void()> beforeSimulation_;
    bool firstFrame_;
    DWORD nextTime_ = 0;
    std::array<std::size_t, 2> inputOrder_{};
};
}
#endif

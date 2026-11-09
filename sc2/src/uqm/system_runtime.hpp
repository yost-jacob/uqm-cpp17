#ifndef UQM_SYSTEM_RUNTIME_HPP_
#define UQM_SYSTEM_RUNTIME_HPP_
#include "world.hpp"
#include "planets/planets.h"
#include "game_state.hpp"
#include <cassert>
namespace uqm { enum class ExplorationView { Flight, Orbit, Scan }; }
// Opaque to C. The production entry scope owns this context; the C loop borrows
// it only until return. Campaign lifetime is guaranteed by the enclosing mode.
struct UQM_SYSTEM_RUNTIME {
    uqm::CampaignState &campaign;
    std::shared_ptr<uqm::WorldState> world;
    std::unique_ptr<uqm::SolarSystem> system;
    uqm::GameSession *session = nullptr;
    uqm::ExplorationView view = uqm::ExplorationView::Flight;
    SOLARSYS_STATE engine{}; // Owned descriptor backing the C engine, never serialized.
};
namespace uqm {
// Compatibility helpers borrow the explicitly attached exploration context.
inline CampaignState &systemCampaign(const SOLARSYS_STATE &system) {
    assert(system.worlds);
    return system.worlds->campaign;
}

// Borrowed lookup; system must have a prepared native runtime.
SystemBehavior &systemBehavior(const SOLARSYS_STATE &system);
// Mode-owned exploration lifetime. The campaign outlives this object.
class SolarSystemExploration {
public:
    explicit SolarSystemExploration(GameSession &session);
    void run();
    std::unique_ptr<ModeSuspension> suspendRendering();
    void resumeRendering();
    SolarSystemExploration(const SolarSystemExploration &) = delete;
    SolarSystemExploration &operator=(const SolarSystemExploration &) = delete;
private:
    UQM_SYSTEM_RUNTIME runtime_;
};
// Nested menu scopes restore their parent view even on early exits.
class ExplorationViewScope {
public:
    ExplorationViewScope(UQM_SYSTEM_RUNTIME &runtime, ExplorationView view)
        : runtime_(runtime), previous_(runtime.view) { runtime_.view = view; }
    ~ExplorationViewScope() { runtime_.view = previous_; }
    ExplorationViewScope(const ExplorationViewScope &) = delete;
    ExplorationViewScope &operator=(const ExplorationViewScope &) = delete;
private:
    UQM_SYSTEM_RUNTIME &runtime_;
    ExplorationView previous_;
};
void redrawExploration(SOLARSYS_STATE &system, ExplorationView view);
// Native engine entry, called only by SolarSystemExploration::run.
void runExploration(UQM_SYSTEM_RUNTIME &runtime);
// Create a child visit for this session and its prepared exploration system.
// The owning exploration mode and campaign must outlive the child callbacks.
std::unique_ptr<GameMode> createLanderMode(GameSession &session, SOLARSYS_STATE &system, POINT planetLoc);
}
#endif

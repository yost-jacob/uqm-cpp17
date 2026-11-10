#ifndef UQM_SYSTEM_RUNTIME_HPP_
#define UQM_SYSTEM_RUNTIME_HPP_
#include "world.hpp"
#include "planets/planets.h"
#include "game_state.hpp"
#include <cassert>
namespace uqm {
enum class ExplorationView { Flight, Orbit, Scan };
struct SurfaceScanState {
    POINT location{}, previousFlashLocation{};
    RECT cursorRect{};
    FRAME eraseFrame = nullptr; // Owned by the active scan visit's cleanup scope.
    BYTE flashColor = 0;
    int flashStep = -2;
    TimeCount nextFlash = 0;
};
}
// Opaque to C. The production entry scope owns this context; the C loop borrows
// it only until return. Story-state lifetime is guaranteed by the enclosing mode.
struct UQM_SYSTEM_RUNTIME {
    uqm::StoryState &story;
    uqm::SimulationEngine &simulation; // Borrowed from the owning exploration mode.
    std::shared_ptr<uqm::WorldState> world;
    std::unique_ptr<uqm::SolarSystem> system;
    uqm::GameSession *session = nullptr;
    uqm::ExplorationView view = uqm::ExplorationView::Flight;
    uqm::SurfaceScanState scan{};
    SOLARSYS_STATE engine{}; // Owned descriptor backing the C engine, never serialized.
    TimeCount nextFlightFrame = 0;
};
namespace uqm {
// Compatibility helpers borrow the explicitly attached exploration context.
inline StoryState &systemStory(const SOLARSYS_STATE &system) {
    assert(system.worlds);
    return system.worlds->story;
}

// Borrowed lookup; system must have a prepared native runtime.
SystemBehavior &systemBehavior(const SOLARSYS_STATE &system);
// Mode-owned exploration lifetime. The story state outlives this object.
class SolarSystemExploration {
public:
    // session and simulation outlive this visit and all synchronous children.
    SolarSystemExploration(GameSession &session, SimulationEngine &simulation);
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
// Prepared system context is required; no process-wide selected-star lookup.
const STAR_DESC &systemStar(const SOLARSYS_STATE &system);
const UNICODE *namedPlanetaryBody(const SOLARSYS_STATE &system);
// Orbit descriptor and world layout must already be attached. Loading replaces
// local masks; publishing merges permanent retrieval bits without awarding cargo.
World &orbitalWorld(SOLARSYS_STATE &system);
void loadPlanetProgress(SOLARSYS_STATE &system);
void publishPlanetProgress(SOLARSYS_STATE &system);
void runScan(SOLARSYS_STATE &system);
void generatePlanetSide(SOLARSYS_STATE &system);
void redrawSurfaceScan(SOLARSYS_STATE &system, const POINT *newLocation);
void redrawExploration(SOLARSYS_STATE &system, ExplorationView view);
// Native engine entry, called only by SolarSystemExploration::run.
void runExploration(UQM_SYSTEM_RUNTIME &runtime);
// Create a child visit for this session and its prepared exploration system.
// The owning exploration mode and story state must outlive the child callbacks.
std::unique_ptr<GameMode> createLanderMode(GameSession &session, SOLARSYS_STATE &system, SimulationEngine &surfaceSource, POINT planetLoc);
}
#endif

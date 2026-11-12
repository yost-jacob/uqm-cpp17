#ifndef UQM_SYSTEM_RUNTIME_HPP_
#define UQM_SYSTEM_RUNTIME_HPP_
#include "world.hpp"
#include "planets/planets.h"
#include "game_state.hpp"
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <functional>
#include <vector>
namespace uqm {
enum class ExplorationView { Flight, Orbit, Scan };
// Own captured drawable frames; borrows passed to the C renderer expire at reset
// or mode destruction. ReleaseDrawable returns the original drawable to destroy.
struct SystemFrameDeleter { void operator()(FRAME frame) const noexcept; };
using SystemFrame = std::unique_ptr<std::remove_pointer_t<FRAME>, SystemFrameDeleter>;
struct SystemViewState {
    SystemFrame stars, scene;
    RECT zoomBounds{}; // Local to this exploration visit, never shared with another system.
    std::vector<std::reference_wrapper<const PLANET_DESC>> bodyOrder;
    // Borrow the sun and generated planets in back-to-front order. The owning
    // descriptor arrays must outlive this view. Refresh after regenerating them.
    void orderBodies(const SOLARSYS_STATE &system);
};
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
    uqm::SystemViewState viewState{};
    const STAR_DESC *catalogueStar = nullptr; // Catalogue borrow, available during construction.
    RandomContext generation{RandomDefaultSeed}; // Visit-owned generation workspace, not gameplay RNG.
};
namespace uqm {
// Generation workspace belongs to this explicit visit. No process-wide binding.
inline RandomContext &generationRandom(const SOLARSYS_STATE &system) {
    if (!system.worlds) throw std::logic_error("generation requires an explicit system owner");
    return system.worlds->generation;
}
// Compatibility helpers borrow the explicitly attached exploration context.
inline StoryState &systemStory(const SOLARSYS_STATE &system) {
    assert(system.worlds);
    return system.worlds->story;
}

// Expected alternative is a mode contract: std::get throws on dispatch errors.
inline SolarSystemLocation &systemLocation(const SOLARSYS_STATE &system) {
    return std::get<SolarSystemLocation>(system.worlds->session->player().location());
}
// Pure coordinate helpers. Positions retain legacy integer units. A positive
// scale radius is required; planet slots must refer to the generated layout.
POINT locationToDisplay(POINT point, SIZE scaleRadius);
POINT displayToLocation(POINT point, SIZE scaleRadius);
POINT planetOuterLocation(const SOLARSYS_STATE &system, COUNT planet);
POINT transformLocation(const SOLARSYS_STATE &system, POINT point, bool toDisplay);
// The flight loop establishes the descriptor layout before these calls.
bool inInnerSystem(const SOLARSYS_STATE &system) noexcept;
// Draw the mode-owned star field using the caller's current drawing context.
void drawStarBackground(const SOLARSYS_STATE &system);
// Prepare legacy save markers from this visit; live orbit state stays separate.
void saveSolarSystemLocation(SOLARSYS_STATE &system);
// Borrowed lookup; system must have a prepared native runtime.
SystemBehavior &systemBehavior(const SOLARSYS_STATE &system);
// Construct the complete native layout in the prepared active descriptor view.
void constructSystemWorlds(SOLARSYS_STATE &system, const STAR_DESC &star);
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
// Explicit analysis entry: output placement never determines policy. The caller
// supplies immutable planet-type data; generation mutates only its visit's RNG.
inline void analyzePlanet(const SOLARSYS_STATE &system, const PLANET_DESC &body,
        SYSTEM_INFO &output, PlanetAnalysisPolicy policy, const PlanetFrame *planetTypes) {
    const auto radius = body.pPrevDesc == system.SunDesc ? body.radius : body.pPrevDesc->radius;
    const PlanetAnalysisInput inputs{systemStar(system).Type, radius, planetTypes, policy};
    DoPlanetaryAnalysis(&output, &body, &generationRandom(system), &inputs);
}

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
// The owning world visit and story state outlive the child callbacks. The
// source must be the active orbital engine, which lends its prepared surface scene.
std::unique_ptr<GameMode> createLanderMode(GameSession &session, SOLARSYS_STATE &system, SimulationEngine &surfaceSource, POINT planetLoc);
}
#endif

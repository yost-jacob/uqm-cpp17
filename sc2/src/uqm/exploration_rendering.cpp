#include "system_runtime.hpp"
#include "exploration_rendering.hpp"
#include "mode_rendering.hpp"
#include "planets/scan.h"
#include "setup.h"
namespace uqm {
std::unique_ptr<ModeSuspension> suspendExplorationRendering(ExplorationView view) {
    // Flight-time story encounters have no established orbital view yet.
    if (view == ExplorationView::Flight) return {};
    return suspendDrawingBindings({PlanetContext, SpaceContext, RadarContext, GetScanContext(nullptr)});
}
std::unique_ptr<ModeSuspension> SolarSystemExploration::suspendRendering() {
    return suspendExplorationRendering(runtime_.view);
}
void SolarSystemExploration::resumeRendering() {
    // Legacy engine exits may precede propagation to the session mailbox.
    if (GLOBAL(CurrentActivity) & (CHECK_ABORT | CHECK_LOAD | START_ENCOUNTER)) return;
    redrawExploration(runtime_.engine, runtime_.view);
}
}

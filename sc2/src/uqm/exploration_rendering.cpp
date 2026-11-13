#include "system_runtime.hpp"
#include "exploration_rendering.hpp"
#include "mode_rendering.hpp"
#include "planets/scan.h"
#include "setup.h"
#include "world_visit.hpp"
#include "orbit_presentation.hpp"
#include "encounter_legacy.hpp"
namespace uqm {
std::unique_ptr<ModeSuspension> suspendExplorationRendering(ExplorationView view, OrbitPresentation *presentation) {
    // Flight-time story encounters have no established orbital view yet.
    if (view == ExplorationView::Flight) return {};
    if (!presentation) throw std::logic_error("orbital handoff requires its presentation owner");
    return suspendDrawingBindings({presentation->planetContext(), SpaceContext, RadarContext, presentation->scanContext()});
}
std::unique_ptr<ModeSuspension> SolarSystemExploration::suspendRendering() {
    return suspendExplorationRendering(runtime_.view,
        runtime_.view == ExplorationView::Flight ? nullptr : &runtime_.visit->presentation());
}
void SolarSystemExploration::resumeRendering() {
    // Legacy engine exits may precede propagation to the session mailbox.
    if (detail::encounterLegacy(runtime_.session->player()).activity & (CHECK_ABORT | CHECK_LOAD | START_ENCOUNTER)) return;
    redrawExploration(runtime_.engine, runtime_.view);
}
}

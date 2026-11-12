#include "system_runtime.hpp"
#include "system_behavior.hpp"
#include "starmap.h"
#include "setup.h"
#include "units.h"
#include <stdexcept>
#include <algorithm>
#include <utility>
namespace uqm {
void SystemFrameDeleter::operator()(FRAME frame) const noexcept {
    if (frame) DestroyDrawable(ReleaseDrawable(frame));
}
void SystemViewState::orderBodies(const SOLARSYS_STATE &system) {
    bodyOrder.clear();
    bodyOrder.reserve(system.SunDesc[0].NumPlanets + 1);
    bodyOrder.emplace_back(system.SunDesc[0]);
    for (std::size_t i = 0; i < system.SunDesc[0].NumPlanets; ++i)
        bodyOrder.emplace_back(system.PlanetDesc[i]);
    std::stable_sort(bodyOrder.begin(), bodyOrder.end(),
        [](const PLANET_DESC &left, const PLANET_DESC &right) {
            return left.image.origin.y < right.image.origin.y;
        });
}
POINT locationToDisplay(POINT point, SIZE scaleRadius) {
    const auto scale = DISPLAY_FACTOR >> 1;
    return {static_cast<COORD>((SIS_SCREEN_WIDTH >> 1) + static_cast<long>(point.x) * scale / scaleRadius),
            static_cast<COORD>((SIS_SCREEN_HEIGHT >> 1) + static_cast<long>(point.y) * scale / scaleRadius)};
}
POINT displayToLocation(POINT point, SIZE scaleRadius) {
    const auto scale = DISPLAY_FACTOR >> 1;
    return {static_cast<COORD>((static_cast<long>(point.x) - (SIS_SCREEN_WIDTH >> 1)) * scaleRadius / scale),
            static_cast<COORD>((static_cast<long>(point.y) - (SIS_SCREEN_HEIGHT >> 1)) * scaleRadius / scale)};
}
POINT planetOuterLocation(const SOLARSYS_STATE &system, COUNT planet) {
    return uqm::displayToLocation(system.PlanetDesc[planet].image.origin, system.SunDesc[0].radius);
}
POINT transformLocation(const SOLARSYS_STATE &system, POINT point, bool toDisplay) {
    return toDisplay ? uqm::locationToDisplay(point, system.SunDesc[0].radius)
                     : uqm::displayToLocation(point, system.SunDesc[0].radius);
}
bool inInnerSystem(const SOLARSYS_STATE &system) noexcept {
    return system.worlds->system->inInnerSystem();
}

const STAR_DESC &systemStar(const SOLARSYS_STATE &system) {
    if (!system.worlds || !system.worlds->catalogueStar)
        throw std::logic_error("star lookup requires explicit catalogue context");
    return *system.worlds->catalogueStar;
}

SystemBehavior &systemBehavior(const SOLARSYS_STATE &system) {
    if (!system.worlds || !system.worlds->system)
        throw std::logic_error("system behavior requires a prepared runtime");
    return system.worlds->system->behavior();
}
SolarSystemExploration::SolarSystemExploration(GameSession &session, SimulationEngine &simulation)
    : runtime_{session.player().story(), simulation, session.worldOwner(), nullptr, &session, ExplorationView::Flight, {}, {}} {
    if (!runtime_.world) throw std::invalid_argument("exploration requires world state");
}
void SolarSystemExploration::run() {
    SimulationBinding simulation{runtime_.simulation};
    runtime_.session->player().enterSolarSystem();
    runExploration(runtime_);
}
}

// C coordinate-buffer adapters. Native flight code passes its system explicitly.
extern "C" POINT locationToDisplay(POINT point, SIZE radius) { return uqm::locationToDisplay(point, radius); }
extern "C" POINT displayToLocation(POINT point, SIZE radius) { return uqm::displayToLocation(point, radius); }

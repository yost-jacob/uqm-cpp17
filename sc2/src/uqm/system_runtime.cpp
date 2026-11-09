#include "system_runtime.hpp"
#include "system_behavior.hpp"
#include "starmap.h"
#include <stdexcept>
#include <utility>
namespace uqm {
const STAR_DESC &systemStar(const SOLARSYS_STATE &system) {
    if (!system.worlds || !system.worlds->system)
        throw std::logic_error("star lookup requires a prepared runtime");
    const auto *star = GetStar(system.worlds->system->star().value);
    if (!star) throw std::logic_error("system star is outside the catalogue");
    return *star;
}

SystemBehavior &systemBehavior(const SOLARSYS_STATE &system) {
    if (!system.worlds || !system.worlds->system)
        throw std::logic_error("system behavior requires a prepared runtime");
    return system.worlds->system->behavior();
}
SolarSystemExploration::SolarSystemExploration(GameSession &session)
    : runtime_{session.player().story(), session.worldOwner(), nullptr, &session, ExplorationView::Flight, {}, {}} {
    if (!runtime_.world) throw std::invalid_argument("exploration requires world state");
}
void SolarSystemExploration::run() { runExploration(runtime_); }
}

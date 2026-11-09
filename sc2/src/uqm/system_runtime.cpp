#include "system_runtime.hpp"
#include "system_behavior.hpp"
#include <stdexcept>
#include <utility>
namespace uqm {
SystemBehavior &systemBehavior(const SOLARSYS_STATE &system) {
    if (!system.worlds || !system.worlds->system)
        throw std::logic_error("system behavior requires a prepared runtime");
    return system.worlds->system->behavior();
}
SolarSystemExploration::SolarSystemExploration(CampaignState &campaign, std::shared_ptr<WorldState> world)
    : runtime_{campaign, std::move(world), nullptr, {}} {
    if (!runtime_.world) throw std::invalid_argument("exploration requires world state");
}
void SolarSystemExploration::run() { runExploration(runtime_); }
}

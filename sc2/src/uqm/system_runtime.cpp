#include "system_runtime.hpp"
#include "planets/planets.h"
#include <stdexcept>
#include <utility>
namespace uqm {
void exploreSolarSystem(CampaignState &campaign, std::shared_ptr<WorldState> world) {
    if (!world) throw std::invalid_argument("exploration requires world state");
    UQM_SYSTEM_RUNTIME runtime{campaign, std::move(world), nullptr};
    ExploreSolarSys(&runtime);
    // All C callbacks have returned before runtime/system/behavior destruction.
}
}

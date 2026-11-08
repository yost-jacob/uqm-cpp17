#ifndef UQM_SYSTEM_RUNTIME_HPP_
#define UQM_SYSTEM_RUNTIME_HPP_
#include "world.hpp"
#include "planets/planets.h"
#include "game_state.hpp"
#include <cassert>
// Opaque to C. The production entry scope owns this context; the C loop borrows
// it only until return. Campaign lifetime is guaranteed by the enclosing mode.
struct UQM_SYSTEM_RUNTIME {
    uqm::CampaignState &campaign;
    std::shared_ptr<uqm::WorldState> world;
    std::unique_ptr<uqm::SolarSystem> system;
};
namespace uqm {
// Compatibility helpers borrow the explicitly attached exploration context.
inline CampaignState &systemCampaign(const SOLARSYS_STATE &system) {
    assert(system.worlds);
    return system.worlds->campaign;
}

void exploreSolarSystem(CampaignState &campaign, std::shared_ptr<WorldState> world);
}
#endif

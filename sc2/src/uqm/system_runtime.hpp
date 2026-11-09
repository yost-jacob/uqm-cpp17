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
    uqm::GameSession *session = nullptr;
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
    SolarSystemExploration(const SolarSystemExploration &) = delete;
    SolarSystemExploration &operator=(const SolarSystemExploration &) = delete;
private:
    UQM_SYSTEM_RUNTIME runtime_;
};
// Native engine entry, called only by SolarSystemExploration::run.
void runExploration(UQM_SYSTEM_RUNTIME &runtime);
// Run one native lander visit; owner and campaign must outlive every callback.
void runLander(SOLARSYS_STATE &system, POINT planetLoc);
}
#endif

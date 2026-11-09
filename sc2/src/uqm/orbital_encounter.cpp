#include "orbital_encounter.hpp"
#include "state.h"
#include "planets/planets.h"
#include <cassert>
namespace uqm {
OrbitalEncounter::OrbitalEncounter(QUEUE &groups, QUEUE &ships, ACTIVITY &activity)
    : ships_(ships), activity_(activity) {
    assert(CountLinks(&ships_) == 0);
    PutGroupInfo(GROUPS_RANDOM, GROUP_SAVE_IP);
    ReinitQueue(&groups);
}
OrbitalEncounter::~OrbitalEncounter() { restore(); }
void OrbitalEncounter::restore() {
    if (restored_) return;
    restored_ = true;
    if (activity_ & (CHECK_ABORT | CHECK_LOAD)) return;
    activity_ &= ~START_INTERPLANETARY;
    ReinitQueue(&ships_);
    GetGroupInfo(GROUPS_RANDOM, GROUP_LOAD_IP);
}
}

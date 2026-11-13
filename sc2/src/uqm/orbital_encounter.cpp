#include "orbital_encounter.hpp"
#include "game_state.hpp"
#include "state.h"
#include "planets/planets.h"
#include <cassert>
namespace uqm {
OrbitalEncounter::OrbitalEncounter(GameSession &session)
    : session_(session), campaign_(detail::encounterLegacy(session.player())) {
    assert(CountLinks(&campaign_.ships) == 0);
    session_.groupArchive().save(GROUPS_RANDOM, GROUP_SAVE_IP);
    ReinitQueue(&campaign_.groups);
}
OrbitalEncounter::~OrbitalEncounter() { restore(); }
void OrbitalEncounter::restore() {
    if (restored_) return;
    restored_ = true;
    if (campaign_.activity & (CHECK_ABORT | CHECK_LOAD)) return;
    campaign_.activity &= ~START_INTERPLANETARY;
    ReinitQueue(&campaign_.ships);
    session_.groupArchive().load(GROUPS_RANDOM, GROUP_LOAD_IP);
}
}

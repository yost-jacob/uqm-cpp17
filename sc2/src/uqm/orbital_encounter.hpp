#ifndef UQM_ORBITAL_ENCOUNTER_HPP_
#define UQM_ORBITAL_ENCOUNTER_HPP_
#include "globdata.h"
namespace uqm {
// Own a temporary orbital encounter's setup/cleanup against the current legacy
// group backend. Queues/activity are borrowed from the active campaign and must
// outlive this scope. Ships must initially be empty. Snapshot before clearing
// system groups; restore at the old explicit point or automatically on return.
// Load/abort deliberately leave cleanup to campaign teardown, as before.
class OrbitalEncounter {
public:
    OrbitalEncounter(QUEUE &groups, QUEUE &ships, ACTIVITY &activity);
    ~OrbitalEncounter();
    OrbitalEncounter(const OrbitalEncounter &) = delete;
    OrbitalEncounter &operator=(const OrbitalEncounter &) = delete;
    void restore(); // Idempotent; call before story actions that require restored groups.
private:
    QUEUE &ships_;
    ACTIVITY &activity_;
    bool restored_ = false;
};
}
#endif

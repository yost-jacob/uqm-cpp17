#ifndef UQM_ORBITAL_ENCOUNTER_HPP_
#define UQM_ORBITAL_ENCOUNTER_HPP_
#include "encounter_legacy.hpp"
namespace uqm {
class GameSession;
// Own a temporary orbital encounter's setup/cleanup against its session archive.
// The session and its initialized collections must outlive this scope. Ships
// must initially be empty. Snapshot before clearing
// system groups; restore at the old explicit point or automatically on return.
// Load/abort deliberately leave cleanup to campaign teardown, as before.
class OrbitalEncounter {
public:
    explicit OrbitalEncounter(GameSession &session);
    ~OrbitalEncounter();
    OrbitalEncounter(const OrbitalEncounter &) = delete;
    OrbitalEncounter &operator=(const OrbitalEncounter &) = delete;
    void restore(); // Idempotent; call before story actions that require restored groups.
private:
    GameSession &session_;
    detail::EncounterLegacyView campaign_;
    bool restored_ = false;
};
}
#endif

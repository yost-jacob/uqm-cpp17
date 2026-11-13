#ifndef UQM_ENCOUNTER_RUNTIME_HPP_
#define UQM_ENCOUNTER_RUNTIME_HPP_
#include "comm.h"
#include "encounter_arena.hpp"
#include "encounter_legacy.hpp"
#include "runtime_ship_roster.hpp"
#include "planets/planets.h"
namespace uqm {
class StoryState;
class GameSession;
class SimulationEngine;
// Reconstruct the special encounter selected by the old save flags. Fwiffo
// takes precedence over location bits; this query never mutates story state.
CONVERSATION conversationForRestoredEncounter(const StoryState &story);
// Synchronous encounter orchestration, owned by an encounter mode. The session
// and engine outlive this object and its synchronous children. star borrows an
// immutable catalogue entry, nullable in deep space; never retain a descriptor from a temporary encounter record.
class EncounterRuntime {
public:
    EncounterRuntime(GameSession &session, const STAR_DESC *star, SimulationEngine &engine) noexcept;
    void run(); // Select a race, communicate, and update the encountered fleet.
    COUNT communicate(CONVERSATION conversation); // Returns the legacy combat-status result.
    EncounterRuntime(const EncounterRuntime &) = delete;
    EncounterRuntime &operator=(const EncounterRuntime &) = delete;
private:
    void buildBattle(RosterSide side); // Copy source vessels into the owned participant pool.
    COUNT selectAction(); // Present the encounter UI; returns HAIL or ATTACK.
    COUNT finish(); // Apply the existing aftermath; returns ships destroyed.
    const STAR_DESC &star() const; // Requires a system encounter, throws if missing.
    StoryState &story_;
    const STAR_DESC *star_;
    SimulationEngine &engine_; // Owning mode outlives this runtime and its children.
    GameSession &session_;
    detail::EncounterLegacyView campaign_;
    EncounterArena arena_; // Released before the owning mode's engine/backend bindings.
};
} // namespace uqm
#endif

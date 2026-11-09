#ifndef UQM_ENCOUNTER_RUNTIME_HPP_
#define UQM_ENCOUNTER_RUNTIME_HPP_
#include "comm.h"
#include "planets/planets.h"
namespace uqm {
class StoryState;
class GameSession;
// Reconstruct the special encounter selected by the old save flags. Fwiffo
// takes precedence over location bits; this query never mutates story state.
CONVERSATION conversationForRestoredEncounter(const StoryState &story);
// Synchronous encounter orchestration, owned by an encounter mode. story
// outlives this object. star is a borrowed immutable catalogue handle, nullable
// in deep space; never retain a descriptor from a temporary encounter record.
class EncounterRuntime {
public:
    EncounterRuntime(StoryState &story, const STAR_DESC *star) noexcept
        : story_(story), star_(star) {}
    EncounterRuntime(GameSession &session, const STAR_DESC *star) noexcept;
    void run(); // Select a race, communicate, and update the encountered fleet.
    COUNT communicate(CONVERSATION conversation); // Returns the legacy combat-status result.
    EncounterRuntime(const EncounterRuntime &) = delete;
    EncounterRuntime &operator=(const EncounterRuntime &) = delete;
private:
    void buildBattle(COUNT player); // Populate the requested side's combat queue.
    COUNT selectAction(); // Present the encounter UI; returns HAIL or ATTACK.
    COUNT finish(); // Apply the existing aftermath; returns ships destroyed.
    const STAR_DESC &star() const; // Requires a system encounter, throws if missing.
    StoryState &story_;
    const STAR_DESC *star_;
    GameSession *session_ = nullptr;
};
} // namespace uqm
#endif

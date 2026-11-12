#ifndef UQM_ENCOUNTER_ARENA_HPP_
#define UQM_ENCOUNTER_ARENA_HPP_
#include "cons_res.h"

namespace uqm {
// Owns the legacy planet artwork for one synchronous encounter. The selected
// graphics backend must outlive this lease. Children borrow the artwork; they
// must not load a second arena while this one is live. No story effects belong
// in cleanup. The Sa-Matra scene may clear the backend frames early; the legacy
// release operation accepts already-cleared frames.
class EncounterArena {
public:
    EncounterArena() = default;
    ~EncounterArena() { reset(); }
    EncounterArena(const EncounterArena &) = delete;
    EncounterArena &operator=(const EncounterArena &) = delete;

    // selector is a planet type (optionally shielded) or the Sa-Matra sentinel.
    void load(BYTE selector) {
        reset();
        loaded_ = true; // Also release partially loaded frames if loading throws.
        load_gravity_well(selector);
    }
    void reset() noexcept {
        if (loaded_) {
            loaded_ = false;
            free_gravity_well();
        }
    }
private:
    bool loaded_ = false;
};
}
#endif

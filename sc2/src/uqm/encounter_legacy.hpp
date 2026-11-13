#ifndef UQM_ENCOUNTER_LEGACY_HPP_
#define UQM_ENCOUNTER_LEGACY_HPP_
#include "globdata.h"
namespace uqm { class PlayerEntity; }
namespace uqm::detail {
// Non-owning projections of a specific player's canonical campaign payload.
// The player outlives these borrows and all synchronous encounter children.
// C menus/services still see the same fields through their scoped bindings.
struct EncounterLegacyView {
    QUEUE &ships;
    QUEUE &groups;
    ACTIVITY &activity;
    BYTE &flags;
    STAMP &shipStamp;
    DWORD &battleGroupRef;
};
EncounterLegacyView encounterLegacy(PlayerEntity &player) noexcept;
}
#endif

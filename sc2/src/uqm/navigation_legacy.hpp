#ifndef UQM_NAVIGATION_LEGACY_HPP_
#define UQM_NAVIGATION_LEGACY_HPP_
#include "libs/gfxlib.h"
namespace uqm { class PlayerEntity; }
namespace uqm::detail {
// Borrow only the fields still shared with C frame/menu/drawing code. They are
// owned by the supplied player's session payload, not selected through a global
// getter. No copy or second authority; the session outlives this projection.
struct NavigationLegacyView {
    UWORD &activity;
    POINT &autopilot;
    STAMP &shipStamp;
};
NavigationLegacyView navigationLegacy(PlayerEntity &player) noexcept;
}
#endif

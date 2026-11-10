#ifndef UQM_SHIP_ASSETS_LEGACY_HPP_
#define UQM_SHIP_ASSETS_LEGACY_HPP_
#include "races.h"
namespace uqm::detail {
// Release a uniquely owned load_ship descriptor and all of its loaded assets.
// This is cancellation/teardown, not combat completion: never calls finish().
void discardLoadedShip(RACE_DESC &descriptor) noexcept;
}
#endif

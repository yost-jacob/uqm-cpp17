#ifndef UQM_SHIP_FACTORY_HPP_
#define UQM_SHIP_FACTORY_HPP_
#include "ship_behavior.hpp"
#include <memory>
namespace uqm::combat {
// Construct a fresh behavior directly from its existing species enum. Tuning
// must already be loaded. NO_ID/unsupported IDs return null; allocation/config
// errors propagate to the caller. No assets or resource-index entries are loaded.
std::unique_ptr<Ship> createShip(SPECIES_ID species);
}
#endif

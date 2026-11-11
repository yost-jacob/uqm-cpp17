#ifndef UQM_PROJECTILE_LEGACY_HPP_
#define UQM_PROJECTILE_LEGACY_HPP_
#include "projectile.hpp"
#include "combat_entity_legacy.hpp"
#include "weapon.h"
#include <utility>
namespace uqm::combat {
// C-buffer publishing boundary. Transfers ownership to the element pool on
// success; exhaustion destroys the unpublished projectile and returns null.
// Sprites are borrowed from ship assets, which must survive the active shots.
HELEMENT publishProjectile(std::unique_ptr<Projectile> projectile, FRAME *sprites,
    SIZE player, ElementProcessFunc *visualUpdate, ElementCollisionFunc *collision);
// Transitional launch adapter for native ship builders still using MISSILE_BLOCK.
// Definition/launch are detached values; custom flight classes own subsequent ticks.
struct PreparedMissile { ProjectileDefinition definition; ProjectileLaunch launch; };
PreparedMissile prepareMissile(const MISSILE_BLOCK &block, FlightTuning flight = {});
template<class Flight = ConfiguredProjectile>
HELEMENT publishMissile(const MISSILE_BLOCK &block, FlightTuning flight = {}) {
    const auto prepared = prepareMissile(block, flight);
    return publishProjectile(std::make_unique<Flight>(prepared.definition, prepared.launch),
        block.farray, block.sender, block.preprocess_func, nullptr);
}
}
namespace uqm::detail {
inline void enableProjectilePostprocess(ELEMENT &element) noexcept {
    enableEntityHooks(element, EntityHooks::Postprocess);
}
}
#endif

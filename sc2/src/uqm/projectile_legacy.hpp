#ifndef UQM_PROJECTILE_LEGACY_HPP_
#define UQM_PROJECTILE_LEGACY_HPP_
#include "projectile.hpp"
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
struct ProjectileAccess {
    static void visual(combat::Projectile &projectile, ElementProcessFunc *callback) noexcept;
    static void advance(combat::Projectile &projectile, ELEMENT &element);
    static void collision(combat::Projectile &projectile, ElementCollisionFunc *callback) noexcept;
    static void collide(combat::Projectile &projectile, ELEMENT &self, POINT &selfPoint, ELEMENT &other, POINT &otherPoint);
};
// Internal pool services: not a public gameplay interface. Borrow only until
// FreeLink/ReinitQueue/UninitQueue; binding requires an ELEMENT pool slot.
void enableProjectilePostprocess(ELEMENT &element) noexcept;
void bindProjectile(QUEUE &queue, HLINK link, std::unique_ptr<combat::Projectile> projectile);
combat::Projectile *borrowProjectile(const QUEUE &queue, HLINK link) noexcept;
}
#endif

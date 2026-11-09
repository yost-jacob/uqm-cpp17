#ifndef UQM_PROJECTILE_LEGACY_HPP_
#define UQM_PROJECTILE_LEGACY_HPP_
#include "projectile.hpp"
namespace uqm::combat {
// C-buffer publishing boundary. Transfers ownership to the element pool on
// success; exhaustion destroys the unpublished projectile and returns null.
// Sprites are borrowed from ship assets, which must survive the active shots.
HELEMENT publishProjectile(std::unique_ptr<Projectile> projectile, FRAME *sprites,
    SIZE player, ElementProcessFunc *visualUpdate, ElementCollisionFunc *collision);
}
namespace uqm::detail {
struct ProjectileAccess {
    static void visual(combat::Projectile &projectile, ElementProcessFunc *callback) noexcept;
    static void advance(combat::Projectile &projectile, ELEMENT &element);
};
// Internal pool services: not a public gameplay interface. Borrow only until
// FreeLink/ReinitQueue/UninitQueue; binding requires an ELEMENT pool slot.
void bindProjectile(QUEUE &queue, HLINK link, std::unique_ptr<combat::Projectile> projectile);
combat::Projectile *borrowProjectile(const QUEUE &queue, HLINK link) noexcept;
}
#endif

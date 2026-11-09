#include "projectile_legacy.hpp"
#include "weapon.h"
#include <utility>
namespace uqm::detail {
void ProjectileAccess::visual(combat::Projectile &projectile, ElementProcessFunc *callback) noexcept {
    projectile.visualUpdate_ = callback;
}
void ProjectileAccess::advance(combat::Projectile &projectile, ELEMENT &element) {
    if (projectile.visualUpdate_) projectile.visualUpdate_(&element);
    projectile.update(element);
}
}
namespace {
void updateProjectile(ELEMENT *element) {
    if (auto *projectile = uqm::detail::borrowProjectile(disp_q, element))
        uqm::detail::ProjectileAccess::advance(*projectile, *element);
}
}
HELEMENT uqm::combat::publishProjectile(std::unique_ptr<Projectile> projectile,
        FRAME *sprites, SIZE player, ElementProcessFunc *visualUpdate, ElementCollisionFunc *collision) {
    if (!projectile || !sprites) return nullptr;
    const auto &definition = projectile->definition();
    const auto &launch = projectile->launch();
    MISSILE_BLOCK block{};
    block.cx = launch.position.x;
    block.cy = launch.position.y;
    block.sender = player;
    block.flags = definition.flags;
    block.hit_points = definition.stats.hitPoints;
    block.damage = definition.stats.damage;
    block.life = definition.stats.lifetime;
    block.index = definition.frameIndex;
    block.blast_offs = definition.blastOffset;
    block.farray = sprites;
    block.face = launch.facing;
    block.preprocess_func = updateProjectile;
    // initialize_missile publishes the legacy graphics/collision representation.
    // Supply zero speed/offset, then apply the ship's explicit initial motion.
    const auto handle = initialize_missile(&block);
    if (!handle) return nullptr;
    ELEMENT *element;
    LockElement(handle, &element);
    element->velocity = launch.velocity;
    SIZE dx, dy;
    auto velocity = launch.velocity;
    GetCurrentVelocityComponents(&velocity, &dx, &dy);
    element->current.location.x -= VELOCITY_TO_WORLD(dx);
    element->current.location.y -= VELOCITY_TO_WORLD(dy);
    if (collision) element->collision_func = collision;
    uqm::detail::ProjectileAccess::visual(*projectile, visualUpdate);
    uqm::detail::bindProjectile(disp_q, handle, std::move(projectile));
    UnlockElement(handle);
    return handle;
}

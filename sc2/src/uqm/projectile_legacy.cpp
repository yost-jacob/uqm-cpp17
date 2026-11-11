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
void ProjectileAccess::collision(combat::Projectile &projectile, ElementCollisionFunc *callback) noexcept {
    projectile.legacyCollision_ = callback;
}
void ProjectileAccess::collide(combat::Projectile &projectile, ELEMENT &self, POINT &selfPoint, ELEMENT &other, POINT &otherPoint) {
    if (projectile.legacyCollision_) projectile.legacyCollision_(&self, &selfPoint, &other, &otherPoint);
    else projectile.collide(self, selfPoint, other, otherPoint);
}
}
namespace {
void updateProjectile(ELEMENT *element) {
    if (auto *projectile = uqm::detail::borrowProjectile(disp_q, element))
        uqm::detail::ProjectileAccess::advance(*projectile, *element);
}
void postprocessProjectile(ELEMENT *element) {
    if (auto *projectile = uqm::detail::borrowProjectile(disp_q, element)) projectile->postprocess(*element);
}
void collideProjectile(ELEMENT *self, POINT *point, ELEMENT *other, POINT *otherPoint) {
    if (auto *projectile = uqm::detail::borrowProjectile(disp_q, self))
        uqm::detail::ProjectileAccess::collide(*projectile, *self, *point, *other, *otherPoint);
}
}
void uqm::detail::enableProjectilePostprocess(ELEMENT &element) noexcept {
    element.postprocess_func = postprocessProjectile;
}
void uqm::combat::Projectile::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    weapon_collision(&self, &point, &other, &otherPoint);
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
    element->collision_func = collideProjectile;
    if (projectile->hasPostprocess()) uqm::detail::enableProjectilePostprocess(*element);
    uqm::detail::ProjectileAccess::collision(*projectile, collision);
    uqm::detail::ProjectileAccess::visual(*projectile, visualUpdate);
    uqm::detail::bindProjectile(disp_q, handle, std::move(projectile));
    UnlockElement(handle);
    return handle;
}

uqm::combat::PreparedMissile uqm::combat::prepareMissile(const MISSILE_BLOCK &block, FlightTuning flight) {
    ProjectileDefinition definition{};
    definition.stats = {block.speed, block.hit_points, block.damage, block.life};
    definition.flags = block.flags;
    definition.frameIndex = block.index;
    definition.blastOffset = block.blast_offs;
    definition.stats.flight = flight;
    const WeaponMount mount{definition, 0, block.pixoffs};
    return {definition, mount.launch({block.cx, block.cy}, block.face)};
}

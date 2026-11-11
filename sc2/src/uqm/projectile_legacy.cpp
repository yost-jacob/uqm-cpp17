#include "projectile_legacy.hpp"
#include "weapon.h"
#include <utility>
void uqm::combat::Projectile::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    weapon_collision(&self, &point, &other, &otherPoint);
}
HELEMENT uqm::combat::publishProjectile(std::unique_ptr<Projectile> projectile,
        FRAME *sprites, SIZE player, ElementProcessFunc *visualUpdate, ElementCollisionFunc *collision) {
    if (!projectile || !sprites) return nullptr;
    const auto &definition = projectile->definition();
    const auto &launch = projectile->launch();
    const auto handle = AllocElement();
    if (!handle) return nullptr;
    ELEMENT *element;
    LockElement(handle, &element);
    // The native definition owns launch/flight policy. Only this publishing
    // boundary writes the engine's packed physics and sprite representation.
    element->hit_points = static_cast<BYTE>(definition.stats.hitPoints);
    element->mass_points = static_cast<BYTE>(definition.stats.damage);
    element->playerNr = player;
    element->state_flags = APPEARING | FINITE_LIFE | definition.flags;
    element->life_span = definition.stats.lifetime;
    element->blast_offset = static_cast<BYTE>(definition.blastOffset);
    SetPrimType(&DisplayArray[element->PrimIndex], STAMP_PRIM);
    element->current.image.farray = sprites;
    element->current.image.frame = SetAbsFrameIndex(sprites[0], definition.frameIndex);
    element->current.location = launch.position;
    element->velocity = launch.velocity;
    SIZE dx, dy;
    auto velocity = launch.velocity;
    GetCurrentVelocityComponents(&velocity, &dx, &dy);
    element->current.location.x -= VELOCITY_TO_WORLD(dx);
    element->current.location.y -= VELOCITY_TO_WORLD(dy);
    uqm::detail::enableEntityHooks(*element, uqm::detail::EntityHooks::Update | uqm::detail::EntityHooks::Collision);
    if (projectile->hasPostprocess()) uqm::detail::enableProjectilePostprocess(*element);
    uqm::detail::EntityAccess::collision(*projectile, collision);
    uqm::detail::EntityAccess::visual(*projectile, visualUpdate);
    uqm::detail::bindEntity(disp_q, handle, std::move(projectile));
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

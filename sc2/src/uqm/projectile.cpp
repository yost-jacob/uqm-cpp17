#include "projectile.hpp"
#include "weapon.h"
#include "units.h"
#include <stdexcept>
#include <limits>
#include <algorithm>
namespace uqm::combat {
namespace {
void validate(const ProjectileDefinition &definition) {
    const auto &stats = definition.stats;
    if (stats.speed < 0 || stats.speed > std::numeric_limits<SIZE>::max() / VELOCITY_SCALE
            || stats.flight.acceleration < 0
            || stats.flight.maxSpeed < 0 || stats.flight.maxSpeed > 1023
            || (stats.flight.maxSpeed && stats.flight.maxSpeed < stats.speed)
            || stats.hitPoints <= 0 || stats.hitPoints > 255
            || stats.damage < 0 || stats.damage > 255 || !stats.lifetime
            || definition.blastOffset < 0 || definition.blastOffset > 255)
        throw std::invalid_argument("invalid projectile definition");
}
}
ConfiguredProjectile::ConfiguredProjectile(ProjectileDefinition definition, ProjectileLaunch launch)
    : Projectile(definition, launch), wait_(definition.stats.flight.initialDelay) {}
void ConfiguredProjectile::update(ELEMENT &element) {
    const auto &profile = definition().stats.flight;
    COUNT facing = profile.directionalSprite ? GetFrameIndex(element.next.image.frame)
        : NORMALIZE_FACING(ANGLE_TO_FACING(GetVelocityTravelAngle(&element.velocity)));
    bool changed = false;
    if (profile.steeringPeriod) {
        if (wait_) --wait_;
        else {
            const auto original = facing;
            const auto delta = TrackShip(&element, &facing);
            if (delta > 0) {
                if (profile.snapToTarget) facing = NORMALIZE_FACING(original + delta);
                changed = true;
                if (profile.directionalSprite) {
                    element.next.image.frame = SetAbsFrameIndex(element.next.image.frame, facing);
                    element.state_flags |= CHANGING;
                }
            }
            wait_ = profile.steeringPeriod - 1;
        }
    }
    if (profile.acceleration) {
        const auto age = std::max<int>(0, int(definition().stats.lifetime) - element.life_span);
        const auto speed = std::min<int>(profile.maxSpeed ? profile.maxSpeed : definition().stats.speed,
            int(definition().stats.speed) + age * profile.acceleration);
        SetVelocityVector(&element.velocity, static_cast<SIZE>(speed), facing);
    } else if (changed) SetVelocityVector(&element.velocity, definition().stats.speed, facing);
}
Projectile::Projectile(ProjectileDefinition definition, ProjectileLaunch launch)
    : definition_(definition), launch_(launch) {
    validate(definition);
    if (launch.facing >= 16) throw std::out_of_range("projectile launch facing");
}
ProjectileFactory::ProjectileFactory(ProjectileDefinition definition) : definition_(definition) { validate(definition); }
std::unique_ptr<Projectile> ProjectileFactory::create(ProjectileLaunch launch) const {
    return std::make_unique<ConfiguredProjectile>(definition_, launch);
}
WeaponMount::WeaponMount(ProjectileDefinition definition, COUNT direction, SIZE offset)
    : factory_(definition), direction_(NORMALIZE_FACING(direction)), offset_(offset) {}
ProjectileLaunch WeaponMount::launch(POINT origin, COUNT shipFacing,
        std::optional<VELOCITY_DESC> inheritedVelocity) const {
    const auto facing = NORMALIZE_FACING(shipFacing + direction_);
    const auto angle = FACING_TO_ANGLE(facing);
    ProjectileLaunch result{};
    result.facing = facing;
    result.position = {static_cast<COORD>(origin.x + COSINE(angle, DISPLAY_TO_WORLD(offset_))),
                       static_cast<COORD>(origin.y + SINE(angle, DISPLAY_TO_WORLD(offset_)))};
    SetVelocityVector(&result.velocity, factory_.definition().stats.speed, facing);
    if (inheritedVelocity) {
        SIZE dx, dy;
        GetCurrentVelocityComponents(&*inheritedVelocity, &dx, &dy);
        DeltaVelocityComponents(&result.velocity, dx, dy);
    }
    return result;
}
}

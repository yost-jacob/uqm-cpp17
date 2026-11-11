#include "projectile.hpp"
#include "weapon.h"
#include "units.h"
#include <stdexcept>
#include <limits>
namespace uqm::combat {
namespace {
void validate(const ProjectileDefinition &definition) {
    const auto &stats = definition.stats;
    if (stats.speed < 0 || stats.speed > std::numeric_limits<SIZE>::max() / VELOCITY_SCALE
            || stats.hitPoints <= 0 || stats.hitPoints > 255
            || stats.damage < 0 || stats.damage > 255 || !stats.lifetime
            || definition.blastOffset < 0 || definition.blastOffset > 255)
        throw std::invalid_argument("invalid projectile definition");
}
class StandardProjectile final : public Projectile {
public:
    StandardProjectile(ProjectileDefinition definition, ProjectileLaunch launch)
        : Projectile(definition, launch), wait_(definition.steeringPeriodTicks
            ? definition.steeringPeriodTicks - 1 : 0) {}
    void update(ELEMENT &element) override {
        const auto period = definition().steeringPeriodTicks;
        if (!period) return;
        if (wait_) { --wait_; return; }
        auto facing = NORMALIZE_FACING(ANGLE_TO_FACING(GetVelocityTravelAngle(&element.velocity)));
        if (TrackShip(&element, &facing) > 0)
            SetVelocityVector(&element.velocity, definition().stats.speed, facing);
        wait_ = period - 1;
    }
private:
    std::uint16_t wait_;
};
}
Projectile::Projectile(ProjectileDefinition definition, ProjectileLaunch launch)
    : definition_(definition), launch_(launch) {
    validate(definition);
    if (launch.facing >= 16) throw std::out_of_range("projectile launch facing");
}
ProjectileFactory::ProjectileFactory(ProjectileDefinition definition) : definition_(definition) { validate(definition); }
std::unique_ptr<Projectile> ProjectileFactory::create(ProjectileLaunch launch) const {
    return std::make_unique<StandardProjectile>(definition_, launch);
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

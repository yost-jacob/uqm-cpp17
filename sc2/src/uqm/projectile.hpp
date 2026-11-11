#ifndef UQM_PROJECTILE_HPP_
#define UQM_PROJECTILE_HPP_
#include "element.h"
#include "ship_tuning.hpp"
#include <cstdint>
#include <memory>
#include <optional>
namespace uqm::detail { struct ProjectileAccess; }
namespace uqm::combat {
struct ProjectileDefinition {
    ProjectileStats stats{};
    COUNT frameIndex = 0;
    SIZE blastOffset = 0;
    ELEMENT_FLAGS flags = IGNORE_SIMILAR;
    // Zero means unguided. Otherwise turn one facing step every N simulation
    // ticks, first adjustment after N ticks. Speed is independent of turn rate.
    std::uint16_t steeringPeriodTicks = 0;
};
// Initial values supplied by the ship/mount. These are not a second mutable
// physics state: after publication the engine's ELEMENT owns live kinematics.
struct ProjectileLaunch { POINT position{}; COUNT facing = 0; VELOCITY_DESC velocity{}; };
class Projectile {
public:
    // Validates finite-lifetime stats, representable velocity and facing 0..15.
    Projectile(ProjectileDefinition definition, ProjectileLaunch launch);
    virtual ~Projectile() = default;
    Projectile(const Projectile &) = delete;
    Projectile &operator=(const Projectile &) = delete;
    const ProjectileDefinition &definition() const noexcept { return definition_; }
    const ProjectileLaunch &launch() const noexcept { return launch_; }
    // Called once per tick on this projectile's engine element. The engine
    // integrates velocity and handles collision/lifetime after this callback.
    virtual void update(ELEMENT &element) = 0;
private:
    friend struct uqm::detail::ProjectileAccess;
    ElementProcessFunc *visualUpdate_ = nullptr;
    ProjectileDefinition definition_;
    ProjectileLaunch launch_;
};
class ProjectileFactory {
public:
    explicit ProjectileFactory(ProjectileDefinition definition);
    const ProjectileDefinition &definition() const noexcept { return definition_; }
    // The caller owns the new shot; no factory/source borrow escapes.
    std::unique_ptr<Projectile> create(ProjectileLaunch launch) const;
private:
    ProjectileDefinition definition_;
};
// Ship-owned mount geometry. Offsets use display pixels; launch positions and
// speed use world units and world units per tick respectively;
// direction is a relative 16-step facing. Inherit velocity only when the weapon
// explicitly requests it; existing SIS blasters do not inherit ship momentum.
class WeaponMount {
public:
    WeaponMount(ProjectileDefinition definition, COUNT direction, SIZE offset);
    ProjectileLaunch launch(POINT origin, COUNT shipFacing,
        std::optional<VELOCITY_DESC> inheritedVelocity = std::nullopt) const;
    const ProjectileFactory &factory() const noexcept { return factory_; }
private:
    ProjectileFactory factory_;
    COUNT direction_;
    SIZE offset_;
};
}
#endif

#include "sis_weapons.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace uqm::combat {
namespace {
constexpr SIZE VerticalMountOffset = 28;
constexpr SIZE SideMountOffset = 20;
constexpr SIZE BlastOffset = 8;
}
SISWeapons configureSISWeapons(const SISLoadout &loadout, ProjectileStats blaster) {
    SISWeapons result;
    result.mounts.reserve(6);
    result.trackingModules = static_cast<COUNT>(std::min<std::size_t>(3,
        std::count(loadout.modules.begin(), loadout.modules.end(), TRACKING_SYSTEM)));
    const auto steeringPeriod = result.trackingModules ? 5 - result.trackingModules : 0;
    for (const unsigned position : {0u, 1u, 2u, 15u}) {
        const auto module = loadout.modules[NUM_MODULE_SLOTS - 1 - position];
        if (module < GUN_WEAPON || module > CANNON_WEAPON) continue;
        const int tier = module - GUN_WEAPON + 1;
        ProjectileDefinition definition{};
        const auto lifetime = unsigned(blaster.lifetime) + (unsigned(blaster.lifetime) >> 2) * (tier - 1);
        if (int(blaster.hitPoints) * tier > 255 || int(blaster.damage) * tier > 255
                || lifetime > std::numeric_limits<COUNT>::max())
            throw std::out_of_range("SIS weapon exceeds projectile capacity");
        definition.stats = {blaster.speed, static_cast<std::int16_t>(blaster.hitPoints * tier),
            static_cast<std::int16_t>(blaster.damage * tier), static_cast<std::uint16_t>(lifetime), blaster.flight};
        definition.frameIndex = tier == 1 ? 0 : tier == 2 ? 9 : 16;
        definition.blastOffset = BlastOffset;
        definition.stats.flight.steeringPeriod = static_cast<std::uint16_t>(steeringPeriod);
        definition.stats.flight.initialDelay = steeringPeriod ? steeringPeriod - 1 : 0;
        result.weaponEnergyCost += tier * 2;
        switch (position) {
            case 0:
                result.firingFlags |= FIRES_FORE;
                result.mounts.emplace_back(definition, 0, VerticalMountOffset);
                break;
            case 1:
                result.firingFlags |= FIRES_FORE;
                result.mounts.emplace_back(definition, 1, VerticalMountOffset);
                result.mounts.emplace_back(definition, NORMALIZE_FACING(-1), VerticalMountOffset);
                break;
            case 2:
                result.firingFlags |= FIRES_LEFT | FIRES_RIGHT;
                result.mounts.emplace_back(definition, 4, SideMountOffset);
                result.mounts.emplace_back(definition, 12, SideMountOffset);
                break;
            case 15:
                result.firingFlags |= FIRES_AFT;
                result.mounts.emplace_back(definition, 8, VerticalMountOffset);
                break;
        }
    }
    return result;
}
}

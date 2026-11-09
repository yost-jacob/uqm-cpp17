#ifndef UQM_SIS_WEAPONS_HPP_
#define UQM_SIS_WEAPONS_HPP_
#include "sis_state.hpp"
#include "projectile.hpp"
#include "races.h"
#include <vector>
namespace uqm::combat {
struct SISWeapons {
    std::vector<WeaponMount> mounts;
    COUNT trackingModules = 0;
    unsigned weaponEnergyCost = 0; // Gun cost only; tracking cost is a module effect.
    UWORD firingFlags = 0;
};
// Build immutable per-combat mounts from installed equipment and JSON tuning.
// Scans the four supported weapon positions, expands paired mounts, caps
// tracking at three, and validates derived projectile quantities before use.
SISWeapons configureSISWeapons(const SISLoadout &loadout, ProjectileStats blaster);
}
#endif

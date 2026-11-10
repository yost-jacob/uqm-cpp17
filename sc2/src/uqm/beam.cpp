//Copyright Paul Reiche, Fred Ford. 1992-2002
// SPDX-License-Identifier: GPL-2.0-or-later
#include "beam.hpp"
#include "combat_entity_legacy.hpp"
#include "weapon.h"
#include "units.h"
#include "init.h"
#include <utility>
#include <memory>
namespace uqm::combat {
void Beam::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    weapon_collision(&self, &point, &other, &otherPoint);
}
HELEMENT publishBeam(const BeamLaunch &launch) {
    auto behavior = std::make_unique<Beam>();
    const auto handle = AllocElement();
    if (!handle) return nullptr;
    ELEMENT *element;
    LockElement(handle, &element);
    element->playerNr = launch.player;
    element->hit_points = 1;
    element->mass_points = 1;
    element->state_flags = APPEARING | FINITE_LIFE | launch.flags;
    element->life_span = 1;
    element->blast_offset = 1;
    const auto angle = FACING_TO_ANGLE(launch.facing);
    element->current.location = {
        static_cast<COORD>(launch.origin.x + COSINE(angle, DISPLAY_TO_WORLD(launch.muzzleOffset))),
        static_cast<COORD>(launch.origin.y + SINE(angle, DISPLAY_TO_WORLD(launch.muzzleOffset)))};
    SetPrimType(&DisplayArray[element->PrimIndex], LINE_PRIM);
    SetPrimColor(&DisplayArray[element->PrimIndex], launch.color);
    element->current.image.frame = DecFrameIndex(stars_in_space);
    element->current.image.farray = &stars_in_space;
    // The velocity encodes the entire segment, ending at origin+displacement.
    // Applying the offset must shorten that segment rather than move its end.
    SetVelocityComponents(&element->velocity,
        WORLD_TO_VELOCITY(launch.origin.x + launch.displacement.x - element->current.location.x),
        WORLD_TO_VELOCITY(launch.origin.y + launch.displacement.y - element->current.location.y));
    detail::attachEntity(*element, std::move(behavior), detail::EntityHooks::Collision);
    UnlockElement(handle);
    return handle;
}
}

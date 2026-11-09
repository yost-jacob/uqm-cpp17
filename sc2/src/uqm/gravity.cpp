//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "collide.h"
#include "combat_planet.hpp"
#include "combat_entity_legacy.hpp"
#include "races.h"
#include "units.h"
#include "libs/log.h"

// The physics pass applies each source's field. Planet owns parameters and the
// sprite collision shape; it does not integrate or discover targets itself.
BOOLEAN CalculateGravity(ELEMENT *element) {
    const bool hasGravity = CollidingElement(element) && GRAVITY_MASS(element->mass_points + 1);
    for (auto handle = GetHeadElement(); handle;) {
        ELEMENT *other;
        LockElement(handle, &other);
        const auto next = GetSuccElement(other);
        const bool otherHasGravity = GRAVITY_MASS(other->mass_points + 1);
        if (other != element && CollidingElement(other) && otherHasGravity != hasGravity) {
            const auto &location = element->state_flags & PRE_PROCESS ? element->next.location : element->current.location;
            const auto &otherLocation = element->state_flags & PRE_PROCESS ? other->next.location : other->current.location;
            const SIZE dx = WRAP_DELTA_X(location.x - otherLocation.x);
            const SIZE dy = WRAP_DELTA_Y(location.y - otherLocation.y);
            auto *source = hasGravity ? element : other;
            const auto *planet = dynamic_cast<const uqm::combat::Planet *>(uqm::detail::borrowEntity(disp_q, source));
            const auto field = planet ? planet->properties().gravity : uqm::combat::GravityField{};
            if (field.contains(dx, dy)) {
                if (otherHasGravity) {
                    UnlockElement(handle);
                    return TRUE;
                }
                field.apply(other->velocity, dx, dy);
                if (other->state_flags & PLAYER_SHIP) {
                    STARSHIP *ship;
                    GetElementStarShip(other, &ship);
                    ship->cur_status_flags &= ~SHIP_AT_MAX_SPEED;
                    ship->cur_status_flags |= SHIP_IN_GRAVITY_WELL;
                }
            }
        }
        UnlockElement(handle);
        handle = next;
    }
    return FALSE;
}

BOOLEAN
TimeSpaceMatterConflict (ELEMENT *ElementPtr)
{
	HELEMENT hTestElement, hSuccElement;
	INTERSECT_CONTROL ElementControl;

	ElementControl.IntersectStamp.origin.x =
			WORLD_TO_DISPLAY (ElementPtr->current.location.x);
	ElementControl.IntersectStamp.origin.y =
			WORLD_TO_DISPLAY (ElementPtr->current.location.y);
	ElementControl.IntersectStamp.frame =
			 SetEquFrameIndex (ElementPtr->current.image.farray[0],
			 ElementPtr->current.image.frame);
	ElementControl.EndPoint = ElementControl.IntersectStamp.origin;
	for (hTestElement = GetHeadElement ();
			hTestElement != 0; hTestElement = hSuccElement)
	{
		ELEMENT *TestElementPtr;

		LockElement (hTestElement, &TestElementPtr);
		hSuccElement = GetSuccElement (TestElementPtr);
		if (TestElementPtr != ElementPtr
				&& (CollidingElement (TestElementPtr)
						/* ship in transition */
				|| (TestElementPtr->state_flags & PLAYER_SHIP)))
		{
			INTERSECT_CONTROL TestElementControl;

			TestElementControl.IntersectStamp.origin.x =
					WORLD_TO_DISPLAY (TestElementPtr->current.location.x);
			TestElementControl.IntersectStamp.origin.y =
					WORLD_TO_DISPLAY (TestElementPtr->current.location.y);
			TestElementControl.IntersectStamp.frame =
					 SetEquFrameIndex (TestElementPtr->current.image.farray[0],
					 TestElementPtr->current.image.frame);
			TestElementControl.EndPoint = TestElementControl.IntersectStamp.origin;
			if (DrawablesIntersect (&ElementControl,
					&TestElementControl, MAX_TIME_VALUE))
			{
				UnlockElement (hTestElement);

				break;
			}
		}
		UnlockElement (hTestElement);
	}

	return (hTestElement != 0 ? TRUE : FALSE);
}

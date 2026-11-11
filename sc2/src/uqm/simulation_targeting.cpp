#include "simulation_engine.hpp"
#include "races.h"
#include "colors.h"
#include "globdata.h"
#include "weapon.h"
#include "units.h"
#include "libs/mathlib.h"
SIZE uqm::SimulationEngine::track(ELEMENT &tracker, COUNT &facing) {
    SimulationBinding binding{*this};
    auto *Tracker = &tracker; auto *pfacing = &facing;

	SIZE best_delta_facing, best_delta;
	HELEMENT hShip, hNextShip;
	ELEMENT *Trackee;

	best_delta = 0;
	best_delta_facing = -1;

	hShip = Tracker->hTarget;
	if (hShip)
	{
		LockElement (hShip, &Trackee);
		Tracker->hTarget = hNextShip = 0;

		goto CheckTracking;
	}

	for (hShip = GetHeadElement (); hShip != 0; hShip = hNextShip)
	{
		LockElement (hShip, &Trackee);
		hNextShip = GetSuccElement (Trackee);
		if ((Trackee->state_flags & PLAYER_SHIP)
				&& !elementsOfSamePlayer (Trackee, Tracker)
				&& (!OBJECT_CLOAKED (Trackee)
				|| ((Tracker->state_flags & PLAYER_SHIP)
				&& (Tracker->state_flags & APPEARING))
				))
		{
			STARSHIP *StarShipPtr;

CheckTracking:
			GetElementStarShip (Trackee, &StarShipPtr);
			if (Trackee->life_span
					&& StarShipPtr->RaceDescPtr->ship_info.crew_level)
			{
				SIZE delta_x, delta_y, delta_facing;

				if (Tracker->state_flags & PRE_PROCESS)
				{
					delta_x = Trackee->next.location.x
							- Tracker->next.location.x;
					delta_y = Trackee->next.location.y
							- Tracker->next.location.y;
				}
				else
				{
					delta_x = Trackee->current.location.x
							- Tracker->current.location.x;
					delta_y = Trackee->current.location.y
							- Tracker->current.location.y;
				}

				delta_x = WRAP_DELTA_X (delta_x);
				delta_y = WRAP_DELTA_Y (delta_y);
				delta_facing = NORMALIZE_FACING (
						ANGLE_TO_FACING (ARCTAN (delta_x, delta_y)) - *pfacing
						);

				if (delta_x < 0)
					delta_x = -delta_x;
				if (delta_y < 0)
					delta_y = -delta_y;
				delta_x += delta_y;
						// 'delta_x + delta_y' is used as an approximation
						// of the actual distance 'sqrt(sqr(delta_x) +
						// sqr(delta_y))'.

				if (best_delta == 0 || delta_x < best_delta)
				{
					best_delta = delta_x;
					best_delta_facing = delta_facing;
					Tracker->hTarget = hShip;
				}
			}
		}
		UnlockElement (hShip);
	}

	if (best_delta_facing > 0)
	{
		COUNT facing;

		facing = *pfacing;
		if (best_delta_facing == ANGLE_TO_FACING (HALF_CIRCLE))
			facing += (((BYTE)TFB_Random () & 1) << 1) - 1;
		else if (best_delta_facing < ANGLE_TO_FACING (HALF_CIRCLE))
			++facing;
		else
			--facing;
		*pfacing = NORMALIZE_FACING (facing);
	}

	return (best_delta_facing);

}
void uqm::SimulationEngine::clearTargets(const ELEMENT &target) {
    SimulationBinding binding{*this};
    for (auto handle = GetHeadElement(); handle;) {
        ELEMENT *element;
        LockElement(handle, &element);
        const auto next = GetSuccElement(element);
        if (element->hTarget == &target) element->hTarget = nullptr;
        UnlockElement(handle);
        handle = next;
    }
}

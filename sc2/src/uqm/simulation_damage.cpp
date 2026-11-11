#include "simulation_engine.hpp"
#include "races.h"
#include "ship.h"
#include "status.h"
void uqm::SimulationEngine::damage(ELEMENT &target, SIZE amount) {
    SimulationBinding binding{*this};
    auto *ElementPtr = &target; const auto damage = amount;

	if (ElementPtr->state_flags & PLAYER_SHIP)
	{
		if (!DeltaCrew (ElementPtr, -damage))
		{
			ElementPtr->life_span = 0;
			ElementPtr->state_flags |= NONSOLID;
		}
	}
	else if (!GRAVITY_MASS (ElementPtr->mass_points))
	{
		if ((BYTE)damage < ElementPtr->hit_points)
			ElementPtr->hit_points -= (BYTE)damage;
		else
		{
			ElementPtr->hit_points = 0;
			ElementPtr->life_span = 0;
			ElementPtr->state_flags |= NONSOLID;
		}
	}

}

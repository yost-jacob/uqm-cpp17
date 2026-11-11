#include "combat_planet.hpp"
#include "combat_entity_legacy.hpp"
#include "races.h"
#include "ship.h"
#include "globdata.h"
#include "collide.h"
#include "units.h"
#include "libs/mathlib.h"
extern "C" FRAME planet[];
namespace uqm::combat {
void Planet::postprocess(ELEMENT &self) { CalculateGravity(&self); }
void Planet::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    collision(&self, &point, &other, &otherPoint);
}
HELEMENT spawnPlanet(PlanetProperties properties) {
	HELEMENT hPlanetElement;

	hPlanetElement = AllocElement ();
	if (hPlanetElement)
	{
		ELEMENT *PlanetElementPtr;


		LockElement (hPlanetElement, &PlanetElementPtr);
		PlanetElementPtr->playerNr = NEUTRAL_PLAYER_NUM;
		PlanetElementPtr->hit_points = 200;
		PlanetElementPtr->state_flags = APPEARING;
		PlanetElementPtr->life_span = NORMAL_LIFE + 1;
		SetPrimType (&DisplayArray[PlanetElementPtr->PrimIndex], STAMP_PRIM);
		PlanetElementPtr->current.image.farray = planet;
		PlanetElementPtr->current.image.frame =
				SetAbsFrameIndex(PlanetElementPtr->current.image.farray[0], properties.spriteIndex);
		uqm::detail::attachEntity(*PlanetElementPtr, std::make_unique<Planet>(properties),
            uqm::detail::EntityHooks::Postprocess | uqm::detail::EntityHooks::Collision);
		ZeroVelocityComponents (&PlanetElementPtr->velocity);
		do
		{
			PlanetElementPtr->current.location.x =
					WRAP_X (DISPLAY_ALIGN_X (TFB_Random ()));
			PlanetElementPtr->current.location.y =
					WRAP_Y (DISPLAY_ALIGN_Y (TFB_Random ()));
		} while (CalculateGravity (PlanetElementPtr)
				|| TimeSpaceMatterConflict (PlanetElementPtr));
		PlanetElementPtr->mass_points = PlanetElementPtr->hit_points;
		UnlockElement (hPlanetElement);

		PutElement (hPlanetElement);
	}

    return hPlanetElement;
}
}
// C campaign/battle setup boundary; native code can supply explicit properties.
void spawn_planet(void) { uqm::combat::spawnPlanet(); }

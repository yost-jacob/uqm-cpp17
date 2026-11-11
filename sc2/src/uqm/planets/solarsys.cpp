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

#include "uqm/planet_render_legacy.hpp"
#include "../system_runtime.hpp"
#include "../sis_state.hpp"
#include "../device_menu.hpp"
#include "../system_behavior.hpp"
#include "solarsys.h"
#include "lander.h"
#include "../colors.h"
#include "../controls.h"
#include "../menustat.h"
		// for DrawMenuStateStrings()
#include "../starmap.h"
#include "../races.h"
#include "../gamestr.h"
#include "../gendef.h"
#include "../globdata.h"
#include "../sis.h"
#include "../init.h"
#include "../shipcont.h"
#include "../gameopt.h"
#include "../nameref.h"
#include "../resinst.h"
#include "../settings.h"
#include "../ipdisp.h"
#include "../grpinfo.h"
#include "../process.h"
#include "../setup.h"
#include "../sounds.h"
#include "../state.h"
#include "../uqmdebug.h"
#include "../save.h"
#include "options.h"
#include "libs/graphics/gfx_common.h"
#include "libs/mathlib.h"
#include "libs/log.h"
#include "libs/misc.h"

// Define DEBUG_SOLARSYS for collision tracing.

constexpr TimeCount InterplanetaryFrameInterval = ONE_SECOND / 30;

// Internal flight/view helpers borrow the mode-owned descriptor explicitly.
// The legacy selected-system pointer is resolved only by C adapters below.
static BOOLEAN DoIpFlight (SOLARSYS_STATE *pSS);
static void DrawSystem(SOLARSYS_STATE &system, SIZE radius, BOOLEAN IsInnerSystem);
static FRAME CreateStarBackGround(SOLARSYS_STATE &system);
static void DrawInnerSystem(SOLARSYS_STATE &system);
static void DrawOuterSystem(SOLARSYS_STATE &system);
static void ValidateOrbits(SOLARSYS_STATE &system);

// SolarSysMenu() items
enum SolarSysMenuMenuItems
{
	// XXX: Must match the enum in menustat.h
	STARMAP = 1,
	EQUIP_DEVICE,
	CARGO,
	ROSTER,
	GAME_MENU,
	NAVIGATION,
};

SOLARSYS_STATE *pSolarSysState;
FRAME SISIPFrame;
FRAME SunFrame;
FRAME OrbitalFrame;
FRAME SpaceJunkFrame;
COLORMAP OrbitalCMap;
COLORMAP SunCMap;
MUSIC_REF SpaceMusic;

SIZE EncounterRace;
BYTE EncounterGroup;
		// last encountered group info

RandomContext SysGenRNG = {RandomDefaultSeed};

bool
worldIsPlanet (const SOLARSYS_STATE *solarSys, const PLANET_DESC *world)
{
	return world->pPrevDesc == solarSys->SunDesc;
}

bool
worldIsMoon (const SOLARSYS_STATE *solarSys, const PLANET_DESC *world)
{
	return world->pPrevDesc != solarSys->SunDesc;
}

// Returns the planet index of the world. If the world is a moon, then
// this is the index of the planet it is orbiting.
COUNT
planetIndex (const SOLARSYS_STATE *solarSys, const PLANET_DESC *world)
{
	const PLANET_DESC *planet = worldIsPlanet (solarSys, world) ?
			world : world->pPrevDesc;
	return planet - solarSys->PlanetDesc;
}

COUNT
moonIndex (const SOLARSYS_STATE *solarSys, const PLANET_DESC *moon)
{
	assert (!worldIsPlanet (solarSys, moon));
	return moon - solarSys->MoonDesc;
}

// Test whether 'world' is the planetI-th planet, and if moonI is not
// set to MATCH_PLANET, also whether 'world' is the moonI-th moon.
bool
matchWorld (const SOLARSYS_STATE *solarSys, const PLANET_DESC *world,
		BYTE planetI, BYTE moonI)
{
	// Check whether we have the right planet.
	if (planetIndex (solarSys, world) != planetI)
		return false;

	if (moonI == MATCH_PLANET)
	{
		// Only test whether we are at the planet.
		if (!worldIsPlanet (solarSys, world))
			return false;
	}
	else
	{
		// Test whether the moon matches too
		if (!worldIsMoon (solarSys, world))
			return false;

		if (moonIndex (solarSys, world) != moonI)
			return false;
	}

	return true;
}

bool
playerInSolarSystem (void)
{
	return pSolarSysState != NULL;
}

bool
playerInPlanetOrbit (void)
{
	return playerInSolarSystem () && pSolarSysState->InOrbit;
}

bool
playerInInnerSystem(void) { return uqm::inInnerSystem(*pSolarSysState); }

// Sets the SysGenRNG to the required state first.
static void
GenerateMoons(SOLARSYS_STATE &system, PLANET_DESC &planet)
{
	COUNT i;
	PLANET_DESC *pMoonDesc;

	RandomContext_SeedRandom (&SysGenRNG, planet.rand_seed);

	uqm::systemBehavior(system).generateName(system, planet);
	uqm::systemBehavior(system).generateMoons(system, planet);
    BuildMoonWorlds (&system, &planet);

	for (i = 0, pMoonDesc = &system.MoonDesc[0];
			i < MAX_MOONS; ++i, ++pMoonDesc)
	{
		pMoonDesc->pPrevDesc = &planet;
		if (i >= planet.NumPlanets)
			continue;
		
		pMoonDesc->temp_color = planet.temp_color;
	}
}

void
FreeIPData (void)
{
	DestroyDrawable (ReleaseDrawable (SISIPFrame));
	SISIPFrame = 0;
	DestroyDrawable (ReleaseDrawable (SunFrame));
	SunFrame = 0;
	DestroyColorMap (ReleaseColorMap (SunCMap));
	SunCMap = 0;
	DestroyColorMap (ReleaseColorMap (OrbitalCMap));
	OrbitalCMap = 0;
	DestroyDrawable (ReleaseDrawable (OrbitalFrame));
	OrbitalFrame = 0;
	DestroyDrawable (ReleaseDrawable (SpaceJunkFrame));
	SpaceJunkFrame = 0;
	DestroyMusic (SpaceMusic);
	SpaceMusic = 0;

	RandomContext_SeedRandom (&SysGenRNG, RandomDefaultSeed);
}

void
LoadIPData (void)
{
	if (SpaceJunkFrame == 0)
	{
		SpaceJunkFrame = CaptureDrawable (
				LoadGraphic (IPBKGND_MASK_PMAP_ANIM));
		SISIPFrame = CaptureDrawable (LoadGraphic (SISIP_MASK_PMAP_ANIM));

		OrbitalCMap = CaptureColorMap (LoadColorMap (ORBPLAN_COLOR_MAP));
		OrbitalFrame = CaptureDrawable (
				LoadGraphic (ORBPLAN_MASK_PMAP_ANIM));
		SunCMap = CaptureColorMap (LoadColorMap (IPSUN_COLOR_MAP));
		SunFrame = CaptureDrawable (LoadGraphic (SUN_MASK_PMAP_ANIM));

		SpaceMusic = LoadMusic (IP_MUSIC);
	}

}

// Apply this player's equipped thrusters/jets to the visit's flight limits.
static void
initSolarSysSISCharacteristics(SOLARSYS_STATE &system)
{
    const auto loadout = system.worlds->session->player().sis().loadout();
	BYTE i;
	BYTE num_thrusters;

	num_thrusters = 0;
	for (i = 0; i < NUM_DRIVE_SLOTS; ++i)
	{
		if (loadout.drives[i] == FUSION_THRUSTER)
			++num_thrusters;
	}
	system.max_ship_speed = (BYTE)(
			(num_thrusters + 5) * IP_SHIP_THRUST_INCREMENT);

	system.turn_wait = IP_SHIP_TURN_WAIT;
	for (i = 0; i < NUM_JET_SLOTS; ++i)
	{
		if (loadout.jets[i] == TURNING_JETS)
			system.turn_wait -= IP_SHIP_TURN_DECREMENT;
	}
}

DWORD
GetRandomSeedForStar (const STAR_DESC *star)
{
	return MAKE_DWORD (star->star_pt.x, star->star_pt.y);
}

// Returns an orbital PLANET_DESC when player is in orbit
static PLANET_DESC *
LoadSolarSys(SOLARSYS_STATE &system)
{
    auto &flight = uqm::systemLocation(system).flight;
	COUNT i;
	PLANET_DESC *orbital = NULL;
	PLANET_DESC *pCurDesc;
    constexpr unsigned TemperatureRanges = 5;
	static const Color temp_color_array[TemperatureRanges] =
	{
		BUILD_COLOR (MAKE_RGB15_INIT (0x00, 0x00, 0x0E), 0x54),
		BUILD_COLOR (MAKE_RGB15_INIT (0x00, 0x06, 0x08), 0x62),
		BUILD_COLOR (MAKE_RGB15_INIT (0x00, 0x0B, 0x00), 0x6D),
		BUILD_COLOR (MAKE_RGB15_INIT (0x0F, 0x00, 0x00), 0x2D),
		BUILD_COLOR (MAKE_RGB15_INIT (0x0F, 0x08, 0x00), 0x75),
	};

	RandomContext_SeedRandom (&SysGenRNG, GetRandomSeedForStar (&uqm::systemStar(system)));

	SunFrame = SetAbsFrameIndex (SunFrame, STAR_TYPE (uqm::systemStar(system).Type));

	pCurDesc = &system.SunDesc[0];
	pCurDesc->pPrevDesc = 0;
	pCurDesc->rand_seed = RandomContext_Random (&SysGenRNG);

	pCurDesc->data_index = STAR_TYPE (uqm::systemStar(system).Type);
	pCurDesc->location.x = 0;
	pCurDesc->location.y = 0;
	pCurDesc->image.origin = pCurDesc->location;
	pCurDesc->image.frame = SunFrame;

	uqm::systemBehavior(system).generatePlanets(system);
    BuildSystemWorlds (&system, &uqm::systemStar(system));

	for (i = 0, pCurDesc = system.PlanetDesc;
			i < MAX_PLANETS; ++i, ++pCurDesc)
	{
		pCurDesc->pPrevDesc = &system.SunDesc[0];
		pCurDesc->image.origin = pCurDesc->location;
		if (i >= system.SunDesc[0].NumPlanets)
		{
			pCurDesc->image.frame = 0;
		}
		else
		{
			COUNT index;
			SYSTEM_INFO SysInfo;

			DoPlanetaryAnalysis (&SysInfo, pCurDesc);
			index = (SysInfo.PlanetInfo.SurfaceTemperature + 250) / 100;
			if (index >= TemperatureRanges)
				index = TemperatureRanges - 1;
			pCurDesc->temp_color = temp_color_array[index];
		}
	}

	system.worlds->viewState.orderBodies(system);

	if (!flight.planet)
	{	// Outer system
		system.pBaseDesc = system.PlanetDesc;
		system.pOrbitalDesc = NULL;
	}
	else
	{	// Inner system
		system.SunDesc[0].location = flight.position;
		flight.position = uqm::displayToLocation (
				GLOBAL (ShipStamp.origin), MAX_ZOOM_RADIUS);

		i = flight.planet - 1;
		system.pOrbitalDesc = &system.PlanetDesc[i];
		GenerateMoons(system, *system.pOrbitalDesc);
		system.pBaseDesc = system.MoonDesc;

		uqm::systemStory(system).clear(uqm::StoryFlag::PlanetaryLanding);
	}

	initSolarSysSISCharacteristics(system);

	if (flight.loadOrbit)
	{	// Only when loading a game into orbital
		i = flight.loadOrbit - 1;
		if (i == 0)
		{	// Orbiting the planet itself
			orbital = system.pBaseDesc->pPrevDesc;
		}
		else
		{	// Orbiting a moon
			// -1 because planet itself is 1, and moons have to be 1-based
			i -= 1;
			orbital = &system.MoonDesc[i];
		}
		flight.position = system.SunDesc[0].location;
		flight.loadOrbit = 0;
	}
	else
	{
		i = flight.facing;
		// XXX: Solar system reentry test depends on ShipFacing != 0
		if (i == 0)
			++i;

		GLOBAL (ShipStamp.frame) = SetAbsFrameIndex (SISIPFrame, i - 1);
	}

	return orbital;
}

static void
saveNonOrbitalLocation(SOLARSYS_STATE &system)
{
    auto &flight = uqm::systemLocation(system).flight;
	// XXX: Solar system reentry test depends on ShipFacing != 0
	flight.facing = GetFrameIndex (GLOBAL (ShipStamp.frame)) + 1;
	flight.loadOrbit = 0;
	if (!uqm::inInnerSystem(system))
	{
		flight.planet = 0;
	}
	else
	{
		// ip_planet is 1-based because code tests for ip_planet!=0
		flight.planet = 1 + planetIndex (&system,
				system.pOrbitalDesc);
		flight.position = system.SunDesc[0].location;
	}
}

static void
FreeSolarSys(SOLARSYS_STATE &system)
{
	if (system.InIpFlight)
	{
		system.InIpFlight = FALSE;
		
		if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
			saveNonOrbitalLocation(system);
	}

	system.worlds->viewState.scene.reset();

	StopMusic ();

}

static FRAME
getCollisionFrame(const SOLARSYS_STATE &system, const PLANET_DESC *planet, COUNT WaitPlanet)
{
	if (system.WaitIntersect != (COUNT)~0
			&& system.WaitIntersect != WaitPlanet)
	{	// New collisions are with a single point (center of planet)
		return DecFrameIndex (stars_in_space);
	}
	else
	{	// Existing collisions are cleared only once the ship does not
		// intersect anymore with a full planet image
		return planet->image.frame;
	}
}

// Returns the planet with which the flagship is colliding
static PLANET_DESC *
CheckIntersect(SOLARSYS_STATE &system)
{
	COUNT i;
	PLANET_DESC *pCurDesc;
	INTERSECT_CONTROL ShipIntersect, PlanetIntersect;
	COUNT NewWaitPlanet;
	BYTE PlanetOffset, MoonOffset;

	// Check collisions with the system center object
	// This may be the planet in inner view, or the sun
	pCurDesc = system.pBaseDesc->pPrevDesc;
	PlanetOffset = pCurDesc == &system.SunDesc[0] ? 0 : planetIndex(&system, pCurDesc) + 1;
	MoonOffset = 1; // the planet itself

	ShipIntersect.IntersectStamp.origin = GLOBAL (ShipStamp.origin);
	ShipIntersect.EndPoint = ShipIntersect.IntersectStamp.origin;
	ShipIntersect.IntersectStamp.frame = GLOBAL (ShipStamp.frame);

	PlanetIntersect.IntersectStamp.origin.x = SIS_SCREEN_WIDTH >> 1;
	PlanetIntersect.IntersectStamp.origin.y = SIS_SCREEN_HEIGHT >> 1;
	PlanetIntersect.EndPoint = PlanetIntersect.IntersectStamp.origin;

	PlanetIntersect.IntersectStamp.frame = getCollisionFrame(system, pCurDesc,
			MAKE_WORD (PlanetOffset, MoonOffset));

	// Start with no collisions
	NewWaitPlanet = 0;

	if (pCurDesc != system.SunDesc /* can't intersect with sun */
			&& DrawablesIntersect (&ShipIntersect,
			&PlanetIntersect, MAX_TIME_VALUE))
	{
#ifdef DEBUG_SOLARSYS
		log_add (log_Debug, "0: Planet %d, Moon %d", PlanetOffset,
				MoonOffset);
#endif /* DEBUG_SOLARSYS */
		NewWaitPlanet = MAKE_WORD (PlanetOffset, MoonOffset);
		if (system.WaitIntersect != (COUNT)~0
				&& system.WaitIntersect != NewWaitPlanet)
		{
			system.WaitIntersect = NewWaitPlanet;
#ifdef DEBUG_SOLARSYS
			log_add (log_Debug, "Star index = %d, Planet index = %d, <%d, %d>",
					GetStarIndex(&uqm::systemStar(system)),
					pCurDesc - system.PlanetDesc,
					system.SunDesc[0].location.x,
					system.SunDesc[0].location.y);
#endif /* DEBUG_SOLARSYS */
			return pCurDesc;
		}
	}

	for (i = pCurDesc->NumPlanets,
			pCurDesc = system.pBaseDesc; i; --i, ++pCurDesc)
	{
		PlanetIntersect.IntersectStamp.origin = pCurDesc->image.origin;
		PlanetIntersect.EndPoint = PlanetIntersect.IntersectStamp.origin;
		if (uqm::inInnerSystem(system))
		{
			PlanetOffset = pCurDesc->pPrevDesc -
					system.PlanetDesc;
			MoonOffset = pCurDesc - system.MoonDesc + 2;
		}
		else
		{
			PlanetOffset = pCurDesc - system.PlanetDesc;
			MoonOffset = 0;
		}
		++PlanetOffset;
		PlanetIntersect.IntersectStamp.frame = getCollisionFrame(system, pCurDesc,
				MAKE_WORD (PlanetOffset, MoonOffset));

		if (DrawablesIntersect (&ShipIntersect,
				&PlanetIntersect, MAX_TIME_VALUE))
		{
#ifdef DEBUG_SOLARSYS
			log_add (log_Debug, "1: Planet %d, Moon %d", PlanetOffset,
					MoonOffset);
#endif /* DEBUG_SOLARSYS */
			NewWaitPlanet = MAKE_WORD (PlanetOffset, MoonOffset);
			
			if (system.WaitIntersect == (COUNT)~0)
			{	// All collisions disallowed, but the ship is still colliding
				// with something. Collisions will remain disabled.
				break;
			}
			else if (system.WaitIntersect == NewWaitPlanet)
			{	// Existing and continued collision -- ignore
				continue;
			}
			
			// Collision with a new planet/moon. This may cause a transition
			// to an inner system or start an orbital view.
			system.WaitIntersect = NewWaitPlanet;
			return pCurDesc;
		}
	}

	// This records the planet/moon with which the ship just collided
	// It may be a previously existing collision also (the value won't change)
	// If all collisions were disabled, this will reenable then once the ship
	// stops colliding with any planets
	if (system.WaitIntersect != (COUNT)~0 || NewWaitPlanet == 0)
		system.WaitIntersect = NewWaitPlanet;

	return NULL;
}

static void
GetOrbitRect (RECT *pRect, COORD dx, COORD dy, SIZE radius,
		int xnumer, int ynumer, int denom)
{
	pRect->corner.x = (SIS_SCREEN_WIDTH >> 1) + (long)-dx * xnumer / denom;
	pRect->corner.y = (SIS_SCREEN_HEIGHT >> 1) + (long)-dy * ynumer / denom;
	pRect->extent.width = (long)radius * (xnumer << 1) / denom;
	pRect->extent.height = pRect->extent.width >> 1;
}

static void
GetPlanetOrbitRect (RECT *r, PLANET_DESC *planet, int sizeNumer,
		int dyNumer, int denom)
{
	COORD dx, dy;

	dx = planet->radius;
	dy = planet->radius;
	if (sizeNumer > DISPLAY_FACTOR)
	{
		dx = dx + planet->location.x;
		dy = (dy + planet->location.y) << 1;
	}
	GetOrbitRect (r, dx, dy, planet->radius, sizeNumer, dyNumer, denom);
}

static void
ValidateOrbit(SOLARSYS_STATE &system, PLANET_DESC *planet, int sizeNumer, int dyNumer, int denom)
{
	COUNT index;

	if (sizeNumer <= DISPLAY_FACTOR)
	{	// All planets in outer view, and moons in inner
		RECT r;

		GetPlanetOrbitRect (&r, planet, sizeNumer, dyNumer, denom);

		// Calculate the location of the planet's image
		r.corner.x += (r.extent.width >> 1);
		r.corner.y += (r.extent.height >> 1);
		r.corner.x += (long)planet->location.x * sizeNumer / denom;
		// Ellipse function always has coefficients a^2 = 2 * b^2
		r.corner.y += (long)planet->location.y * (sizeNumer / 2) / denom;

		planet->image.origin = r.corner;
	}

	// Calculate the size and lighting angle of planet's image and
	// set the image that will be drawn
	index = planet->data_index & ~WORLD_TYPE_SPECIAL;
	if (index < NUMBER_OF_PLANET_TYPES)
	{	// The world is a normal planetary body (planet or moon)
		BYTE Type;
		COUNT Size;
		COUNT angle;

		Type = PlanData[index].Type;
		Size = PLANSIZE (Type);
		if (sizeNumer > DISPLAY_FACTOR)
		{
			Size += 3;
		}
		else if (worldIsMoon (&system, planet))
		{
			Size += 2;
		}
		else if (denom <= (MAX_ZOOM_RADIUS >> 2))
		{
			++Size;
			if (denom == MIN_ZOOM_RADIUS)
				++Size;
		}
		
		if (worldIsPlanet (&system, planet))
		{	// Planet
			angle = ARCTAN (planet->location.x, planet->location.y);
		}
		else
		{	// Moon
			angle = ARCTAN (planet->pPrevDesc->location.x,
					planet->pPrevDesc->location.y);
		}
		planet->image.frame =	SetAbsFrameIndex (OrbitalFrame,
				(Size << FACING_SHIFT) + NORMALIZE_FACING (
				ANGLE_TO_FACING (angle)));
	}
	else if (planet->data_index == HIERARCHY_STARBASE)
	{
		planet->image.frame = SetAbsFrameIndex (SpaceJunkFrame, 16);
	}
	else if (planet->data_index == SA_MATRA)
	{
		planet->image.frame = SetAbsFrameIndex (SpaceJunkFrame, 19);
	}
}

static void
DrawOrbit (PLANET_DESC *planet, int sizeNumer, int dyNumer, int denom)
{
	RECT r;

	GetPlanetOrbitRect (&r, planet, sizeNumer, dyNumer, denom);

	SetContextForeGroundColor (planet->temp_color);
	DrawOval (&r, 1);
}

static SIZE
FindRadius(SOLARSYS_STATE &system, POINT shipLoc, SIZE fromRadius)
{
	SIZE nextRadius;
	POINT displayLoc;

	do
	{
		fromRadius >>= 1;
		if (fromRadius > MIN_ZOOM_RADIUS)
			nextRadius = fromRadius >> 1;
		else
			nextRadius = 0; // The next zoom rectangle will be empty

		GetOrbitRect (&system.worlds->viewState.zoomBounds, nextRadius, nextRadius, nextRadius,
				DISPLAY_FACTOR, DISPLAY_FACTOR >> 2, fromRadius);
		displayLoc = uqm::locationToDisplay (shipLoc, fromRadius);
	
	} while (pointWithinRect (system.worlds->viewState.zoomBounds, displayLoc));

	return fromRadius;
}

static UWORD
flagship_inertial_thrust(const SOLARSYS_STATE &system, COUNT CurrentAngle)
{
	BYTE max_speed;
	SIZE cur_delta_x, cur_delta_y;
	COUNT TravelAngle;
	VELOCITY_DESC *VelocityPtr;

	max_speed = system.max_ship_speed;
	VelocityPtr = &GLOBAL (velocity);
	GetCurrentVelocityComponents (VelocityPtr, &cur_delta_x, &cur_delta_y);
	TravelAngle = GetVelocityTravelAngle (VelocityPtr);
	if (TravelAngle == CurrentAngle
			&& cur_delta_x == COSINE (CurrentAngle, max_speed)
			&& cur_delta_y == SINE (CurrentAngle, max_speed))
		return (SHIP_AT_MAX_SPEED);
	else
	{
		SIZE delta_x, delta_y;
		DWORD desired_speed;

		delta_x = cur_delta_x
				+ COSINE (CurrentAngle, IP_SHIP_THRUST_INCREMENT);
		delta_y = cur_delta_y
				+ SINE (CurrentAngle, IP_SHIP_THRUST_INCREMENT);
		desired_speed = (DWORD) ((long) delta_x * delta_x)
				+ (DWORD) ((long) delta_y * delta_y);
		if (desired_speed <= (DWORD) ((UWORD) max_speed * max_speed))
			SetVelocityComponents (VelocityPtr, delta_x, delta_y);
		else if (TravelAngle == CurrentAngle)
		{
			SetVelocityComponents (VelocityPtr,
					COSINE (CurrentAngle, max_speed),
					SINE (CurrentAngle, max_speed));
			return (SHIP_AT_MAX_SPEED);
		}
		else
		{
			VELOCITY_DESC v;

			v = *VelocityPtr;

			DeltaVelocityComponents (&v,
					COSINE (CurrentAngle, IP_SHIP_THRUST_INCREMENT >> 1)
					- COSINE (TravelAngle, IP_SHIP_THRUST_INCREMENT),
					SINE (CurrentAngle, IP_SHIP_THRUST_INCREMENT >> 1)
					- SINE (TravelAngle, IP_SHIP_THRUST_INCREMENT));
			GetCurrentVelocityComponents (&v, &cur_delta_x, &cur_delta_y);
			desired_speed =
					(DWORD) ((long) cur_delta_x * cur_delta_x)
					+ (DWORD) ((long) cur_delta_y * cur_delta_y);
			if (desired_speed > (DWORD) ((UWORD) max_speed * max_speed))
			{
				SetVelocityComponents (VelocityPtr,
						COSINE (CurrentAngle, max_speed),
						SINE (CurrentAngle, max_speed));
				return (SHIP_AT_MAX_SPEED);
			}

			*VelocityPtr = v;
		}

		return 0;
	}
}

static void
ProcessShipControls(SOLARSYS_STATE &system)
{
	COUNT index;
	SIZE delta_x, delta_y;

	if (CurrentInputState.key[PlayerControls[0]][KEY_UP])
		delta_y = -1;
	else
		delta_y = 0;

	delta_x = 0;
	if (CurrentInputState.key[PlayerControls[0]][KEY_LEFT])
		delta_x -= 1;
	if (CurrentInputState.key[PlayerControls[0]][KEY_RIGHT])
		delta_x += 1;
		
	if (delta_x || delta_y < 0)
	{
		GLOBAL (autopilot.x) = ~0;
		GLOBAL (autopilot.y) = ~0;
	}
	else if (GLOBAL (autopilot.x) != ~0 && GLOBAL (autopilot.y) != ~0)
		delta_y = -1;
	else
		delta_y = 0;

	index = GetFrameIndex (GLOBAL (ShipStamp.frame));
	if (system.turn_counter)
		--system.turn_counter;
	else if (delta_x)
	{
		if (delta_x < 0)
			index = NORMALIZE_FACING (index - 1);
		else
			index = NORMALIZE_FACING (index + 1);

		GLOBAL (ShipStamp.frame) =
				SetAbsFrameIndex (GLOBAL (ShipStamp.frame), index);

		system.turn_counter = system.turn_wait;
	}
	if (system.thrust_counter)
		--system.thrust_counter;
	else if (delta_y < 0)
	{
        constexpr BYTE ThrustWait = 1;
		flagship_inertial_thrust(system, FACING_TO_ANGLE (index));

		system.thrust_counter = ThrustWait;
	}
}

static void
enterInnerSystem(SOLARSYS_STATE &system, PLANET_DESC *planet)
{
    auto &flight = uqm::systemLocation(system).flight;
    constexpr int InnerEntryDistance = MIN_MOON_RADIUS + (MAX_MOONS - 1) * MOON_DELTA + MOON_DELTA / 4;
	COUNT angle;

	// Calculate the inner system entry location and facing
	angle = FACING_TO_ANGLE (GetFrameIndex (GLOBAL (ShipStamp.frame)))
			+ HALF_CIRCLE;
	GLOBAL (ShipStamp.origin.x) = (SIS_SCREEN_WIDTH >> 1)
			+ COSINE (angle, InnerEntryDistance);
	GLOBAL (ShipStamp.origin.y) = (SIS_SCREEN_HEIGHT >> 1)
			+ SINE (angle, InnerEntryDistance);
	if (GLOBAL (ShipStamp.origin.y) < 0)
		GLOBAL (ShipStamp.origin.y) = 1;
	else if (GLOBAL (ShipStamp.origin.y) >= SIS_SCREEN_HEIGHT)
		GLOBAL (ShipStamp.origin.y) =
				(SIS_SCREEN_HEIGHT - 1) - 1;

	flight.position = uqm::displayToLocation (
			GLOBAL (ShipStamp.origin), MAX_ZOOM_RADIUS);
	
	system.SunDesc[0].location =
			uqm::planetOuterLocation(system, planetIndex (&system, planet));
	ZeroVelocityComponents (&GLOBAL (velocity));

	GenerateMoons(system, *planet);
	system.pBaseDesc = system.MoonDesc;
	system.pOrbitalDesc = planet;
}

static void
leaveInnerSystem(SOLARSYS_STATE &system, PLANET_DESC *planet)
{
    auto &flight = uqm::systemLocation(system).flight;
	COUNT outerPlanetWait;

	system.pBaseDesc = system.PlanetDesc;
	system.pOrbitalDesc = NULL;

	outerPlanetWait = MAKE_WORD (planet - system.PlanetDesc + 1, 0);
	flight.position = system.SunDesc[0].location;
	GLOBAL (ShipStamp.origin) = uqm::transformLocation(system, flight.position, true);
	ZeroVelocityComponents (&GLOBAL (velocity));

	// Now the ship is in outer system (as per game logic)

	system.WaitIntersect = outerPlanetWait;
	// See if we also intersect with another planet, and if we do,
	// disable collisions comletely until we stop intersecting
	// with any planet at all.
	CheckIntersect(system);
	if (system.WaitIntersect != outerPlanetWait)
		system.WaitIntersect = (COUNT)~0;
}

static void
enterOrbital(SOLARSYS_STATE &system, PLANET_DESC *planet)
{
	ZeroVelocityComponents (&GLOBAL (velocity));
	system.pOrbitalDesc = planet;
	system.InOrbit = TRUE;
}

static BOOLEAN
CheckShipLocation(SOLARSYS_STATE &system, SIZE *newRadius)
{
    auto &flight = uqm::systemLocation(system).flight;
	SIZE radius;

	radius = system.SunDesc[0].radius;
	*newRadius = system.SunDesc[0].radius;
	
	if (GLOBAL (ShipStamp.origin.x) < 0
			|| GLOBAL (ShipStamp.origin.x) >= SIS_SCREEN_WIDTH
			|| GLOBAL (ShipStamp.origin.y) < 0
			|| GLOBAL (ShipStamp.origin.y) >= SIS_SCREEN_HEIGHT)
	{
		// The ship leaves the screen.
		if (!uqm::inInnerSystem(system))
		{	// Outer zoom-out transition
			if (radius == MAX_ZOOM_RADIUS)
			{
				// The ship leaves IP.
				GLOBAL (CurrentActivity) |= END_INTERPLANETARY;
				return FALSE; // no location change
			}

			*newRadius = FindRadius(system, flight.position,
					MAX_ZOOM_RADIUS << 1);
		}
		else
		{
			leaveInnerSystem(system, system.pOrbitalDesc);
		}
		
		return TRUE;
	}

	if (!uqm::inInnerSystem(system)
			&& pointWithinRect (system.worlds->viewState.zoomBounds, GLOBAL (ShipStamp.origin)))
	{	// Outer zoom-in transition
		*newRadius = FindRadius(system, flight.position, radius);
		return TRUE;
	}

	if (GLOBAL (autopilot.x) == ~0 && GLOBAL (autopilot.y) == ~0)
	{	// Not on autopilot -- may collide with a planet
		PLANET_DESC *planet = CheckIntersect(system);
		if (planet)
		{	// Collision with a planet
			if (uqm::inInnerSystem(system))
			{	// Entering planet orbit (scans, etc.)
				enterOrbital(system, planet);
				return FALSE; // no location change
			}
			else
			{	// Transition to inner system
				enterInnerSystem(system, planet);
				return TRUE;
			}
		}
	}

	return FALSE; // no location change
}

static void
DrawSystemTransition(SOLARSYS_STATE &system, bool inner)
{
	SetTransitionSource (NULL);
	BatchGraphics ();
	if (inner)
		DrawInnerSystem(system);
	else
		DrawOuterSystem(system);
	RedrawQueue (FALSE);
	ScreenTransition (3, NULL);
	UnbatchGraphics ();
}

static void
TransitionSystemIn(SOLARSYS_STATE &system)
{
	SetContext (SpaceContext);
	DrawSystemTransition(system, uqm::inInnerSystem(system));
}

static void
ScaleSystem(SOLARSYS_STATE &system, SIZE new_radius)
{
    auto &flight = uqm::systemLocation(system).flight;
	RECT r;

	system.SunDesc[0].radius = new_radius;
	GLOBAL (ShipStamp.origin) = uqm::transformLocation(system, flight.position, true);

	GetContextClipRect (&r);
	SetTransitionSource (&r);
	BatchGraphics ();
	DrawOuterSystem(system);
	RedrawQueue (FALSE);
	ScreenTransition (3, &r);
	UnbatchGraphics ();
}

static void
RestoreSystemView(SOLARSYS_STATE &system)
{
	STAMP s;

	s.origin.x = 0;
	s.origin.y = 0;
	s.frame = system.worlds->viewState.scene.get();
	DrawStamp (&s);
}

// Normally called by DoIpFlight() to process a frame
static void
IP_frame(SOLARSYS_STATE &system)
{
	bool locChange;
	SIZE newRadius;

	SetContext (SpaceContext);

	GameClockTick ();
	ProcessShipControls(system);
	
	locChange = CheckShipLocation(system, &newRadius);
	if (locChange)
	{
		if (uqm::inInnerSystem(system))
		{	// Entering inner system
			DrawSystemTransition(system, TRUE);
		}
		else if (system.SunDesc[0].radius == newRadius)
		{	// Leaving inner system to outer
			DrawSystemTransition(system, FALSE);
		}
		else
		{	// Zooming outer system
			ScaleSystem(system, newRadius);
		}
	}
	else
	{	// Just flying around, minding own business..
		BatchGraphics ();
		RestoreSystemView(system);
		RedrawQueue (FALSE);
		DrawAutoPilotMessage (FALSE);
		UnbatchGraphics ();
	}
	
}

static bool
CheckZoomLevel(SOLARSYS_STATE &system)
{
    auto &flight = uqm::systemLocation(system).flight;
	bool InnerSystem;
	POINT shipLoc;

	InnerSystem = uqm::inInnerSystem(system);
	if (InnerSystem)
		shipLoc = system.SunDesc[0].location;
	else
		shipLoc = flight.position;

	system.SunDesc[0].radius = FindRadius(system, shipLoc,
			MAX_ZOOM_RADIUS << 1);
	if (!InnerSystem)
	{	// Update ship stamp since the radius probably changed
		GLOBAL (ShipStamp.origin) = uqm::transformLocation(system, shipLoc, true);
	}

	return InnerSystem;
}

static void
ValidateOrbits(SOLARSYS_STATE &system)
{
	COUNT i;
	PLANET_DESC *planet;

	for (i = system.SunDesc[0].NumPlanets,
			planet = &system.PlanetDesc[0]; i; --i, ++planet)
	{
		ValidateOrbit(system, planet, DISPLAY_FACTOR, DISPLAY_FACTOR / 4,
				system.SunDesc[0].radius);
	}
}

static void
ValidateInnerOrbits(SOLARSYS_STATE &system)
{
	COUNT i;
	PLANET_DESC *planet;

	assert (uqm::inInnerSystem(system));

	planet = system.pOrbitalDesc;
	ValidateOrbit(system, planet, DISPLAY_FACTOR * 4, DISPLAY_FACTOR,
			planet->radius);

	for (i = 0; i < planet->NumPlanets; ++i)
	{
		PLANET_DESC *moon = &system.MoonDesc[i];
		ValidateOrbit(system, moon, 2, 1, 2);
	}
}

static void
DrawInnerSystem(SOLARSYS_STATE &system)
{
	ValidateInnerOrbits(system);
	DrawSystem(system, system.pOrbitalDesc->radius, TRUE);
	DrawSISTitle (GLOBAL_SIS (PlanetName));
}

static void
DrawOuterSystem(SOLARSYS_STATE &system)
{
	ValidateOrbits(system);
	DrawSystem(system, system.SunDesc[0].radius, FALSE);
	DrawHyperCoords (uqm::systemStar(system).star_pt);
}

static void
ResetSolarSys(SOLARSYS_STATE &system)
{
	// Originally there was a flash_task test here, however, I found no cases
	// where flash_task could be set at the time of call. The test was
	// probably needed on 3DO when IP_frame(system) was a task.
	assert (!system.InIpFlight);

	DrawMenuStateStrings (PM_STARMAP, -(PM_NAVIGATE - PM_SCAN));

	InitDisplayList ();
	// This also spawns the flagship element
	DoMissions ();

	// Figure out and note which planet/moon we just left, if any
	// This records any existing collision and prevents the ship
	// from entering planets until a new collision occurs.
	// TODO: this may need logic similar to one in leaveInnerSystem(system)
	//   for when the ship collides with more than one planet at
	//   the same time. While quite rare, it's still possible.
	CheckIntersect(system);
	
	system.InIpFlight = TRUE;

	// Do not start playing the music if we entered the solarsys only
	// to load a game (load invoked from Main menu)
	// XXX: This is quite hacky
	if (!PLRPlaying ((MUSIC_REF)~0) &&
			(LastActivity != CHECK_LOAD || NextActivity))
	{
		PlayMusic (SpaceMusic, TRUE, 1);
	}
}

static void
EnterPlanetOrbit(SOLARSYS_STATE &system)
{
	if (system.InIpFlight)
	{	// This means we hit a planet in IP flight; not a Load into orbit
		FreeSolarSys(system);

		if (worldIsMoon (&system, system.pOrbitalDesc))
		{	// Moon -- use its origin
			// XXX: The conversion functions do not error-correct, so the
			//   point we set here will change once flag_ship_preprocess()
			//   in ipdisp.c starts over again.
			GLOBAL (ShipStamp.origin) =
					system.pOrbitalDesc->image.origin;
		}
		else
		{	// Planet -- its origin is for the outer view, so use mid-screen
			GLOBAL (ShipStamp.origin.x) = SIS_SCREEN_WIDTH >> 1;
			GLOBAL (ShipStamp.origin.y) = SIS_SCREEN_HEIGHT >> 1;
		}
	}

    {
        auto &body = uqm::orbitalWorld(system);
        auto &location = uqm::systemLocation(system);
        location.enterOrbit(body.id());
        struct OrbitLocationScope {
            uqm::SolarSystemLocation &location;
            ~OrbitLocationScope() { location.leaveOrbit(); }
        } orbitLocation{location};
        uqm::detail::PlanetRenderBorrow rendering{system, body};
        auto &graphics = body.renderResources();
        struct OrbitServices {
            uqm::PlanetRenderResources &graphics;
            ~OrbitServices() { if (graphics.hasSurface()) FinishPlanetOrbit(); }
        } services{graphics};
	uqm::loadPlanetProgress(system);
	uqm::systemBehavior(system).generateOrbital(system, *(system.pOrbitalDesc));
	LastActivity &= ~(CHECK_LOAD | CHECK_RESTART);
	if ((GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD |
			START_ENCOUNTER)) || system.worlds->session->player().crew() == (COUNT)~0
			|| uqm::systemStory(system).read(uqm::StoryCounter::BombProgress) == 2)
		return;

	// Implement a to-do in generate.h for a better test
	if (graphics.hasSurface())
	{	// We've entered orbit; LoadPlanet() called planet surface-gen code
        uqm::ExplorationViewScope view{*system.worlds, uqm::ExplorationView::Orbit};
		PlanetOrbitMenu ();
	}
	// Otherwise, generateOrbital function started a homeworld conversation,
	// and we did not get to the planet no matter what.

    } // Orbit services end; the World retains its graphics.

	// START_ENCOUNTER could be set by Devices menu a number of ways:
	// Talking Pet, Sun Device or a Caster over Chmmr, or
	// a Caster for Ilwrath
	// Could also have blown self up with Utwig Bomb
	if (!(GLOBAL (CurrentActivity) & (START_ENCOUNTER |
			CHECK_ABORT | CHECK_LOAD))
			&& system.worlds->session->player().crew() != (COUNT)~0)
	{	// Reload the system and return to the inner view
		PLANET_DESC *orbital = LoadSolarSys(system);
		assert (!orbital);
		CheckZoomLevel(system);
		ValidateOrbits(system);
		ValidateInnerOrbits(system);
		ResetSolarSys(system);

		RepairSISBorder ();
		TransitionSystemIn(system);
	}
}

static void
InitSolarSys(SOLARSYS_STATE &system)
{
    auto &flight = uqm::systemLocation(system).flight;
	bool InnerSystem;
	bool Reentry;
	PLANET_DESC *orbital;

	LoadIPData ();
	LoadLanderData ();

	Reentry = (flight.facing != 0);
	if (!Reentry)
	{
		GLOBAL (autopilot.x) = ~0;
		GLOBAL (autopilot.y) = ~0;

		GLOBAL (ShipStamp.origin.x) = SIS_SCREEN_WIDTH >> 1;
		GLOBAL (ShipStamp.origin.y) = SIS_SCREEN_HEIGHT - 2;
		
		flight.position = uqm::displayToLocation (GLOBAL (ShipStamp.origin),
				MAX_ZOOM_RADIUS);
	}

	system.worlds->viewState.stars.reset(CreateStarBackGround(system));
	
	SetContext (SpaceContext);
	SetContextFGFrame (Screen);
	SetContextBackGroundColor (BLACK_COLOR);

	orbital = LoadSolarSys(system);
	InnerSystem = CheckZoomLevel(system);
	ValidateOrbits(system);
	if (InnerSystem)
		ValidateInnerOrbits(system);

	if (Reentry)
	{
		uqm::systemBehavior(system).reinitNpcs(system);
	}
	else
	{
		EncounterRace = -1;
		EncounterGroup = 0;
		GLOBAL (BattleGroupRef) = 0;
		ReinitQueue (&GLOBAL (ip_group_q));
		ReinitQueue (&GLOBAL (npc_built_ship_q));
		uqm::systemBehavior(system).initNpcs(system);
	}

	if (orbital)
	{
		enterOrbital(system, orbital);
	}
	else
	{	// Draw the borders, the system (inner or outer) and fade/transition
		SetContext (SpaceContext);

		SetTransitionSource (NULL);
		BatchGraphics ();

		DrawSISFrame ();
		DrawSISMessage (NULL);

		ResetSolarSys(system);

		if (LastActivity == (CHECK_LOAD | CHECK_RESTART))
		{	// Starting a new game, NOT from load!
			// We have to fade the screen in from intro or menu
			DrawOuterSystem(system);
			RedrawQueue (FALSE);
			UnbatchGraphics ();
			FadeScreen (FadeAllToColor, ONE_SECOND / 2);

			LastActivity = 0;
		}
		else if (LastActivity == CHECK_LOAD && !NextActivity)
		{	// Called just to load a game; invoked from Main menu
			// No point in drawing anything
			UnbatchGraphics ();
		}
		else
		{	// Entered a new system, or loaded into inner or outer
			if (InnerSystem)
				DrawInnerSystem(system);
			else
				DrawOuterSystem(system);
			RedrawQueue (FALSE);
			ScreenTransition (3, NULL);
			UnbatchGraphics ();

			LastActivity &= ~CHECK_LOAD;
		}
	}
}

static void
endInterPlanetary(SOLARSYS_STATE &system)
{
	GLOBAL (CurrentActivity) &= ~END_INTERPLANETARY;
	
	if (!(GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD)))
	{
		// These are game state changing ops and so cannot be
		// called once another game has been loaded!
		uqm::systemBehavior(system).uninitNpcs(system);
		uqm::systemStory(system).clear(uqm::StoryFlag::UsedBroadcaster);
	}
}

// Find the closest planet to a point, in interplanetary.
static PLANET_DESC *
closestPlanetInterPlanetary(SOLARSYS_STATE &system, const POINT *point)
{
	BYTE i;
	BYTE numPlanets;
	DWORD bestDistSquared;
	PLANET_DESC *bestPlanet = NULL;


	numPlanets = system.SunDesc[0].NumPlanets;

	bestDistSquared = (DWORD) -1;  // Maximum value of DWORD.
	for (i = 0; i < numPlanets; i++)
	{
		PLANET_DESC *planet = &system.PlanetDesc[i];

		SIZE dx = point->x - planet->image.origin.x;
		SIZE dy = point->y - planet->image.origin.y;

		DWORD distSquared = (DWORD) ((long) dx * dx + (long) dy * dy);
		if (distSquared < bestDistSquared)
		{
			bestDistSquared = distSquared;
			bestPlanet = planet;
		}
	}

	return bestPlanet;
}

static void
UninitSolarSys(SOLARSYS_STATE &system)
{
    auto &flight = uqm::systemLocation(system).flight;
	FreeSolarSys(system);


	if (GLOBAL (CurrentActivity) & END_INTERPLANETARY)
	{
		endInterPlanetary(system);
		return;
	}

	if ((GLOBAL (CurrentActivity) & START_ENCOUNTER) && EncounterGroup)
	{
		GetGroupInfo (GLOBAL (BattleGroupRef), EncounterGroup);
		// Generate the encounter location name based on the closest planet

		if (flight.planet == 0)
		{
			PLANET_DESC *planet =
					closestPlanetInterPlanetary(system, &GLOBAL (ShipStamp.origin));

			uqm::systemBehavior(system).generateName(system, *(planet));
		}
	}
}

static void
CalcSunSize (PLANET_DESC *pSunDesc, SIZE radius)
{
	SIZE index = 0;

	if (radius <= (MAX_ZOOM_RADIUS >> 1))
	{
		++index;
		if (radius <= (MAX_ZOOM_RADIUS >> 2))
			++index;
	}

	pSunDesc->image.origin.x = SIS_SCREEN_WIDTH >> 1;
	pSunDesc->image.origin.y = SIS_SCREEN_HEIGHT >> 1;
	pSunDesc->image.frame = SetRelFrameIndex (SunFrame, index);
}

static void
SetPlanetColorMap (const PLANET_DESC *planet)
{
	COUNT index = planet->data_index & ~WORLD_TYPE_SPECIAL;
	assert (index < NUMBER_OF_PLANET_TYPES);
	SetColorMap (GetColorMapAddress (SetAbsColorMapIndex (OrbitalCMap,
			PLANCOLOR (PlanData[index].Type))));
}

static void
DrawInnerPlanets(SOLARSYS_STATE &system, PLANET_DESC *planet)
{
	STAMP s;
	COUNT i;
	PLANET_DESC *moon;

	// Draw the planet image
	SetPlanetColorMap (planet);
	s.origin.x = SIS_SCREEN_WIDTH >> 1;
	s.origin.y = SIS_SCREEN_HEIGHT >> 1;
	s.frame = planet->image.frame;

	i = planet->data_index & ~WORLD_TYPE_SPECIAL;
	if (i < NUMBER_OF_PLANET_TYPES
			&& (planet->data_index & PLANET_SHIELDED))
	{	// Shielded world looks "shielded" in inner view
		s.frame = SetAbsFrameIndex (SpaceJunkFrame, 17);
	}
	DrawStamp (&s);

	// Draw the moon images
	for (i = planet->NumPlanets, moon = system.MoonDesc;
			i; --i, ++moon)
	{
		if (!(moon->data_index & WORLD_TYPE_SPECIAL))
			SetPlanetColorMap (moon);
		DrawStamp (&moon->image);
	}
}

static void
DrawSystem(SOLARSYS_STATE &system, SIZE radius, BOOLEAN IsInnerSystem)
{
	BYTE i;
	PLANET_DESC *pCurDesc;
	PLANET_DESC *pBaseDesc;
	CONTEXT oldContext;
	STAMP s;

	if (!system.worlds->viewState.scene.get())
	{	// Create the saved view graphic
		RECT clipRect;

		GetContextClipRect (&clipRect);
		system.worlds->viewState.scene.reset(CaptureDrawable(CreateDrawable(WANT_PIXMAP,
                clipRect.extent.width, clipRect.extent.height, 1)));
	}

	oldContext = SetContext (OffScreenContext);
	SetContextFGFrame (system.worlds->viewState.scene.get());
	SetContextClipRect (NULL);

	uqm::drawStarBackground(system);

	pBaseDesc = system.pBaseDesc;
	if (IsInnerSystem)
	{	// Draw the inner system view *planet's* orbit segment
		pCurDesc = system.pOrbitalDesc;
		DrawOrbit (pCurDesc, DISPLAY_FACTOR * 4, DISPLAY_FACTOR, radius);
	}

	// Draw the planet orbits or moon orbits
	for (i = pBaseDesc->pPrevDesc->NumPlanets, pCurDesc = pBaseDesc;
			i; --i, ++pCurDesc)
	{
		if (IsInnerSystem)
			DrawOrbit (pCurDesc, 2, 1, 2);
		else
			DrawOrbit (pCurDesc, DISPLAY_FACTOR, DISPLAY_FACTOR / 4,
					radius);
	}

	if (IsInnerSystem)
	{	// Draw the inner system view
		DrawInnerPlanets(system, system.pOrbitalDesc);
	}
	else
	{	// Draw the outer system view
		CalcSunSize (&system.SunDesc[0], radius);

		for (const PLANET_DESC &body : system.worlds->viewState.bodyOrder)
        {
            if (&body == &system.SunDesc[0])
                SetColorMap(GetColorMapAddress(SetAbsColorMapIndex(
                    SunCMap, STAR_COLOR(uqm::systemStar(system).Type))));
            else
                SetPlanetColorMap(&body);
            auto stamp = body.image;
            DrawStamp(&stamp);
        }
	}

	SetContext (oldContext);

	// Draw the now-saved view graphic
	s.origin.x = 0;
	s.origin.y = 0;
	s.frame = system.worlds->viewState.scene.get();
	DrawStamp (&s);
}

void
uqm::drawStarBackground(const SOLARSYS_STATE &system)
{
	STAMP s;
	
	s.origin.x = 0;
	s.origin.y = 0;
	s.frame = system.worlds->viewState.stars.get();
	DrawStamp (&s);
}

static FRAME
CreateStarBackGround(SOLARSYS_STATE &system)
{
	COUNT i, j;
	DWORD rand_val;
	STAMP s;
	CONTEXT oldContext;
	RECT clipRect;
	FRAME frame;

	// Use SpaceContext to find out the dimensions of the background
	oldContext = SetContext (SpaceContext);
	GetContextClipRect (&clipRect);

	// Prepare a pre-drawn stars frame for this system
	frame = CaptureDrawable (CreateDrawable (WANT_PIXMAP,
			clipRect.extent.width, clipRect.extent.height, 1));
	SetContext (OffScreenContext);
	SetContextFGFrame (frame);
	SetContextClipRect (NULL);
	SetContextBackGroundColor (BLACK_COLOR);

	ClearDrawable ();

	RandomContext_SeedRandom (&SysGenRNG, GetRandomSeedForStar (&uqm::systemStar(system)));

    constexpr COUNT NUM_DIM_PIECES = 8;
	s.frame = SpaceJunkFrame;
	for (i = 0; i < NUM_DIM_PIECES; ++i)
	{
    constexpr COUNT NUM_DIM_DRAWN = 5;
		for (j = 0; j < NUM_DIM_DRAWN; ++j)
		{
			rand_val = RandomContext_Random (&SysGenRNG);
			s.origin.x = LOWORD (rand_val) % SIS_SCREEN_WIDTH;
			s.origin.y = HIWORD (rand_val) % SIS_SCREEN_HEIGHT;

			DrawStamp (&s);
		}
		s.frame = IncFrameIndex (s.frame);
	}
    constexpr COUNT NUM_BRT_PIECES = 8;
	for (i = 0; i < NUM_BRT_PIECES; ++i)
	{
    constexpr COUNT NUM_BRT_DRAWN = 30;
		for (j = 0; j < NUM_BRT_DRAWN; ++j)
		{
			rand_val = RandomContext_Random (&SysGenRNG);
			s.origin.x = LOWORD (rand_val) % SIS_SCREEN_WIDTH;
			s.origin.y = HIWORD (rand_val) % SIS_SCREEN_HEIGHT;

			DrawStamp (&s);
		}
		s.frame = IncFrameIndex (s.frame);
	}

	SetContext (oldContext);

	return frame;
}

void
uqm::runExploration(UQM_SYSTEM_RUNTIME &runtime)
{
	auto &system = runtime.engine;
    auto &location = std::get<uqm::SolarSystemLocation>(runtime.session->player().location());
	
	if (CurStarDescPtr == 0)
	{
		POINT universe;

		universe.x = LOGX_TO_UNIVERSE (location.position.x);
		universe.y = LOGY_TO_UNIVERSE (location.position.y);
		CurStarDescPtr = FindStar (0, &universe, 1, 1);
		if (!CurStarDescPtr)
		{
			log_add (log_Fatal, "ExploreSolarSys(): do not know where you are!");
			explode ();
		}
	}
	location.position.x = UNIVERSE_TO_LOGX (CurStarDescPtr->star_pt.x);
	location.position.y = UNIVERSE_TO_LOGY (CurStarDescPtr->star_pt.y);

    struct Binding {
        SOLARSYS_STATE *previous = pSolarSysState;
        ~Binding() { pSolarSysState = previous; }
    } binding;
    system = {};
    pSolarSysState = &system;

	system.worlds = &runtime;
	PrepareSystemWorlds (&system, CurStarDescPtr);
    location.system = runtime.system->star();

	InitSolarSys(system);
    struct Cleanup {
        SOLARSYS_STATE &system;
        ~Cleanup() { UninitSolarSys(system); }
    } cleanup{system};
	SetMenuSounds (MENU_SOUND_NONE, MENU_SOUND_NONE);
	system.InputFunc = DoIpFlight;
	DoInput (&system, FALSE);
}

const UNICODE *
uqm::namedPlanetaryBody(const SOLARSYS_STATE &system)
{
	if (system.pBaseDesc != system.MoonDesc || !system.pOrbitalDesc)
		return NULL; // Not inside an inner system, so no name

	assert (system.pOrbitalDesc != NULL);

	if (uqm::systemStar(system).Index == SOL_DEFINED)
	{	// Planets and moons in Sol
		int planet;
		int moon;

		planet = planetIndex (&system, system.pOrbitalDesc);

		if (worldIsPlanet (&system, system.pOrbitalDesc))
		{	// A planet
			return GAME_STRING (PLANET_NUMBER_BASE + planet);
		}

		// Moons
		moon = moonIndex (&system, system.pOrbitalDesc);
		switch (planet)
		{
			case 2: // Earth
				switch (moon)
				{
					case 0: // Starbase
						return GAME_STRING (STARBASE_STRING_BASE + 0);
					case 1: // Luna
						return GAME_STRING (PLANET_NUMBER_BASE + 9);
				}
				break;
			case 4: // Jupiter
				switch (moon)
				{
					case 0: // Io
						return GAME_STRING (PLANET_NUMBER_BASE + 10);
					case 1: // Europa
						return GAME_STRING (PLANET_NUMBER_BASE + 11);
					case 2: // Ganymede
						return GAME_STRING (PLANET_NUMBER_BASE + 12);
					case 3: // Callisto
						return GAME_STRING (PLANET_NUMBER_BASE + 13);
				}
				break;
			case 5: // Saturn
				if (moon == 0) // Titan
					return GAME_STRING (PLANET_NUMBER_BASE + 14);
				break;
			case 7: // Neptune
				if (moon == 0) // Triton
					return GAME_STRING (PLANET_NUMBER_BASE + 15);
				break;
		}
	}
	else if (uqm::systemStar(system).Index == SPATHI_DEFINED)
	{
		if (matchWorld (&system, system.pOrbitalDesc,
				0, MATCH_PLANET))
		{
#ifdef NOTYET
			return "Spathiwa";
#endif // NOTYET
		}
	}
	else if (uqm::systemStar(system).Index == SAMATRA_DEFINED)
	{
		if (matchWorld (&system, system.pOrbitalDesc, 4, 0))
		{	// Sa-Matra
			return GAME_STRING (PLANET_NUMBER_BASE + 32);
		}
	}

	return NULL;
}

void
GetPlanetOrMoonName (UNICODE *buf, COUNT bufsize)
{
	UNICODE *named;
	int moon;
	int i;

	named = GetNamedPlanetaryBody ();
	if (named)
	{
		utf8StringCopy (buf, bufsize, named);
		return;
	}
		
	// Either not named or we already have a name
	utf8StringCopy (buf, bufsize, GLOBAL_SIS (PlanetName));

	if (!playerInSolarSystem () || !playerInInnerSystem () ||
			worldIsPlanet (pSolarSysState, pSolarSysState->pOrbitalDesc))
	{	// Outer or inner system or orbiting a planet
		return;
	}

	// Orbiting an unnamed moon
	i = strlen (buf);
	buf += i;
	bufsize -= i;
	moon = moonIndex (pSolarSysState, pSolarSysState->pOrbitalDesc);
	if (bufsize >= 3)
	{
		snprintf (buf, bufsize, "-%c", 'A' + moon);
		buf[bufsize - 1] = '\0';
	}
}

void
uqm::saveSolarSystemLocation(SOLARSYS_STATE &system)
{
    auto &flight = uqm::systemLocation(system).flight;

	// This is a two-stage saving procedure
	// Stage 1: called when saving from inner/outer view
	// Stage 2: called when saving from orbital

	if (!system.InOrbit)
	{
		saveNonOrbitalLocation(system);
	}
	else
	{	// In orbit around a planet.
		BYTE moon;

		// flight.planet is already set
		assert (flight.planet != 0);

		// has to be at least 1 because code tests for in_orbit!=0
		moon = 1; /* the planet itself */
		if (worldIsMoon (&system, system.pOrbitalDesc))
		{
			moon += moonIndex (&system, system.pOrbitalDesc);
			// +1 because moons have to be 1-based
			moon += 1;
		}
		flight.loadOrbit = moon;
	}
}

static BOOLEAN
DoSolarSysMenu (MENU_STATE *pMS)
{
    auto &system = *static_cast<SOLARSYS_STATE *>(pMS->privData);
	bool select = PulsedInputState.menu[KEY_MENU_SELECT];
	bool handled;

	if ((GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
			|| system.worlds->session->player().crew() == (COUNT)~0)
		return FALSE;

	handled = DoMenuChooser (pMS, PM_STARMAP);
	if (handled)
		return TRUE;

	if (LastActivity == CHECK_LOAD)
		select = TRUE; // Selected LOAD from main menu

	if (!select)
		return TRUE;

	SetFlashRect (NULL);

	switch (pMS->CurState)
	{
		case EQUIP_DEVICE:
			select = uqm::DeviceMenu{uqm::systemStory(system), system}.run();
			if (GLOBAL (CurrentActivity) & START_ENCOUNTER)
			{	// Invoked Talking Pet or a Caster for Ilwrath
				// Going into conversation
				return FALSE;
			}
			break;
		case CARGO:
			CargoMenu ();
			break;
		case ROSTER:
			select = RosterMenu ();
			break;
		case GAME_MENU:
			if (!GameOptions ())
				return FALSE; // abort or load
			break;
		case STARMAP:
			StarMap ();
			if (GLOBAL (CurrentActivity) & CHECK_ABORT)
				return FALSE;

			TransitionSystemIn(system);
			// Fall through !!!
		case NAVIGATION:
			return FALSE;
	}

	if (!(GLOBAL (CurrentActivity) & CHECK_ABORT))
	{
		if (select)
		{	// 3DO menu jumps to NAVIGATE after a successful submenu run
			if (optWhichMenu != OPT_PC)
				pMS->CurState = NAVIGATION;
			DrawMenuStateStrings (PM_STARMAP, pMS->CurState);
		}
		SetFlashRect (SFR_MENU_3DO);
	}

	return TRUE;
}

static void
SolarSysMenu(SOLARSYS_STATE &system)
{
	MENU_STATE MenuState;

	memset (&MenuState, 0, sizeof MenuState);

	if (LastActivity == CHECK_LOAD)
	{	// Selected LOAD from main menu
		MenuState.CurState = GAME_MENU;
	}
	else
	{
		DrawMenuStateStrings (PM_STARMAP, STARMAP);
		MenuState.CurState = STARMAP;
	}

	DrawStatusMessage (NULL);
	SetFlashRect (SFR_MENU_3DO);

	SetMenuSounds (MENU_SOUND_ARROWS, MENU_SOUND_SELECT);
	MenuState.InputFunc = DoSolarSysMenu;
    MenuState.privData = &system;
	DoInput (&MenuState, TRUE);

	DrawMenuStateStrings (PM_STARMAP, -NAVIGATION);
}

static BOOLEAN
DoIpFlight (SOLARSYS_STATE *pSS)
{
    auto &system = *pSS;
	auto &NextTime = system.worlds->nextFlightFrame;
	bool cancel = PulsedInputState.menu[KEY_MENU_CANCEL];

	if (system.InOrbit)
	{	// CheckShipLocation(system) or InitSolarSys() sent us to orbital
		EnterPlanetOrbit(system);
		SetMenuSounds (MENU_SOUND_NONE, MENU_SOUND_NONE);
		system.InOrbit = FALSE;
	}
	else if (cancel || LastActivity == CHECK_LOAD)
	{
		SolarSysMenu(system);
		SetMenuSounds (MENU_SOUND_NONE, MENU_SOUND_NONE);
	}
	else
	{
		assert (system.InIpFlight);
		IP_frame(system);
		SleepThreadUntil (NextTime);
		NextTime = GetTimeCounter () + InterplanetaryFrameInterval;
	}

	return (!(GLOBAL (CurrentActivity)
			& (START_ENCOUNTER | END_INTERPLANETARY
			| CHECK_ABORT | CHECK_LOAD))
			&& system.worlds->session->player().crew() != (COUNT)~0) ? TRUE : FALSE;
}

// Legacy C UI adapter. Native scan code supplies its exploration directly.
UNICODE *GetNamedPlanetaryBody(void) {
    if (!pSolarSysState || !CurStarDescPtr) return nullptr;
    return const_cast<UNICODE *>(uqm::namedPlanetaryBody(*pSolarSysState));
}

// Remaining C boundaries resolve the legacy binding once. Native helpers above
// use references and never select a different system implicitly.
POINT planetOuterLocation(COUNT planet) { return uqm::planetOuterLocation(*pSolarSysState, planet); }
void DrawStarBackGround(void) { uqm::drawStarBackground(*pSolarSysState); }

void SaveSolarSysLocation(void) { uqm::saveSolarSystemLocation(*pSolarSysState); }

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

#ifndef UQM_PLANETS_PLANETS_H_
#define UQM_PLANETS_PLANETS_H_

#include "libs/mathlib.h"

#define END_INTERPLANETARY START_INTERPLANETARY

enum PlanetScanTypes
{
	MINERAL_SCAN = 0,
	ENERGY_SCAN,
	BIOLOGICAL_SCAN,

	NUM_SCAN_TYPES,
};

#define MAP_WIDTH SIS_SCREEN_WIDTH
#define MAP_HEIGHT (75 - SAFE_Y)

enum
{
	BIOLOGICAL_DISASTER = 0,
	EARTHQUAKE_DISASTER,
	LIGHTNING_DISASTER,
	LAVASPOT_DISASTER,

	/* additional lander sounds */
	LANDER_INJURED,
	LANDER_SHOOTS,
	LANDER_HITS,
	LIFEFORM_CANNED,
	LANDER_PICKUP,
	LANDER_FULL,
	LANDER_DEPARTS,
	LANDER_RETURNS,
	LANDER_DESTROYED
};

#define MAX_SCROUNGED 50 /* max lander can hold */

#define SCALE_RADIUS(r) ((r) << 6)
#define UNSCALE_RADIUS(r) ((r) >> 6)
#define MAX_ZOOM_RADIUS SCALE_RADIUS(128)
#define MIN_ZOOM_RADIUS (MAX_ZOOM_RADIUS>>3)
#define EARTH_RADIUS SCALE_RADIUS(8)

#define MIN_PLANET_RADIUS SCALE_RADIUS (4)
#define MAX_PLANET_RADIUS SCALE_RADIUS (124)

#define DISPLAY_FACTOR ((SIS_SCREEN_WIDTH >> 1) - 8)

#define NUM_SCANDOT_TRANSITIONS 4

#define MIN_MOON_RADIUS 35
#define MOON_DELTA 20

#define MAX_SUNS 1
#define MAX_PLANETS 16
#define MAX_MOONS 4

#define MAP_BORDER_HEIGHT  5
#define SCAN_SCREEN_HEIGHT (SIS_SCREEN_HEIGHT - MAP_HEIGHT - MAP_BORDER_HEIGHT)

#define PLANET_ROTATION_TIME (ONE_SECOND * 12)
#define PLANET_ROTATION_RATE (PLANET_ROTATION_TIME / MAP_WIDTH)
// XXX: -9 to match the original, but why? I have no idea
#define PLANET_ORG_Y ((SCAN_SCREEN_HEIGHT - 9) / 2)

#define NUM_RACE_RUINS  16

typedef struct planet_desc PLANET_DESC;
typedef struct star_desc STAR_DESC;
typedef struct node_info NODE_INFO;
typedef struct solarsys_state SOLARSYS_STATE;


#include "../world_c.h"
#include "../menustat.h"
#include "../units.h"

#include "elemdata.h"
#include "lifeform.h"
#include "../planet_render_data.h"
#include "plandata.h"
#include "sundata.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct planet_desc
{
	DWORD rand_seed;

	BYTE data_index;
	BYTE NumPlanets;
	SIZE radius;
	POINT location;

	Color temp_color;
	STAMP image;

	PLANET_DESC *pPrevDesc;
			// The Sun or planet that this world is orbiting around.
};

struct star_desc
{
	POINT star_pt;
	BYTE Type;
	BYTE Index;
	BYTE Prefix;
	BYTE Postfix;
	/* In-memory catalogue identity only; never serialized. */
	COUNT catalogue_index;
};

struct node_info
{
	// This structire is filled in when a generateMinerals, generateEnergy,
	// or generateLife call is made.
	POINT loc_pt;
			// Position of the mineral/bio/energy node on the planet.
	COUNT density;
			// For bio and energy: undefined
			// For minerals the low byte is the gross size of the
			// deposit (this determines the image), and the high
			// byte is the fine size (the actual quantity).
	COUNT type;
			// For minerals: the type of element
			// For bio: the type of the creature.
			//          0 through NUM_CREATURE_TYPES - 1 are normal creatures,
			//          NUM_CREATURE_TYPES     is an Evil One
			//          NUM_CREATURE_TYPES + 1 is a Brainbox Bulldozer
			//          NUM_CREATURE_TYPES + 2 is Zex' Beauty
			// For energy: undefined
};

// See doc/devel/generate for information on how this structure is
// filled.
struct solarsys_state
{
	// Standard field required by DoInput()
	BOOLEAN (*InputFunc) (struct solarsys_state *);

    // Compatibility projections during a visit; native SolarSystem/World
    // values own physical geometry. Generators instead fill private scratch
    // records before adoption. Only image/color state is mutable in live views.
    PLANET_DESC SunDesc[MAX_SUNS];
    PLANET_DESC PlanetDesc[MAX_PLANETS];
    // One parent's moon view. Borrows (including pPrevDesc) end when projected
    // for another parent; native WorldId values remain stable.
    PLANET_DESC MoonDesc[MAX_MOONS];

	SYSTEM_INFO SysInfo;

    UQM_SYSTEM_RUNTIME *worlds; // Opaque owner of native World/EnergyBehavior objects.

    PLANET_RENDER_DATA *graphics; // Scoped borrow from the current native World.
			// Homeworld encounters count as 'in orbit'
};

extern SOLARSYS_STATE *pSolarSysState;
extern MUSIC_REF SpaceMusic;
extern CONTEXT PlanetContext;

// Random contexts are explicitly owned by generation callers.

bool playerInSolarSystem (void);
bool playerInPlanetOrbit (void);
bool playerInInnerSystem (void);
bool worldIsPlanet (const SOLARSYS_STATE *solarSys, const PLANET_DESC *world);
bool worldIsMoon (const SOLARSYS_STATE *solarSys, const PLANET_DESC *world);
COUNT planetIndex (const SOLARSYS_STATE *solarSys, const PLANET_DESC *world);
COUNT moonIndex (const SOLARSYS_STATE *solarSys, const PLANET_DESC *moon);
#define MATCH_PLANET ((BYTE) -1)
bool matchWorld (const SOLARSYS_STATE *solarSys, const PLANET_DESC *world,
		BYTE planetI, BYTE moonI);

DWORD GetRandomSeedForStar (const STAR_DESC *star);

POINT locationToDisplay (POINT pt, SIZE scaleRadius);
POINT displayToLocation (POINT pt, SIZE scaleRadius);


extern void DrawOval (RECT *pRect, BYTE num_off_pixels);
extern void DrawFilledOval (RECT *pRect);
extern void FillOrbits (SOLARSYS_STATE *system, BYTE NumPlanets,
		PLANET_DESC *pBaseDesc, BOOLEAN TypesDefined, RandomContext *rng, const STAR_DESC *star);
extern void InitLander (BYTE LanderFlags);

extern void DrawScannedObjects (BOOLEAN Reversed);
extern void DeltaTopography (COUNT num_iterations, SBYTE *DepthArray,
		RECT *pRect, SIZE depth_delta, RandomContext *rng);

extern void DrawPlanetSurfaceBorder (void);

extern UNICODE* GetNamedPlanetaryBody (void);
extern void GetPlanetOrMoonName (UNICODE *buf, COUNT bufsize);

extern void SaveSolarSysLocation (void);

#if defined(__cplusplus)
}
#endif

#endif /* UQM_PLANETS_PLANETS_H_ */

//Copyright Paul Reiche, Fred Ford. 1992-2002
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef UQM_PLANET_RENDER_DATA_H_
#define UQM_PLANET_RENDER_DATA_H_
#include "libs/gfxlib.h"
#include "libs/strlib.h"
#include "planets/lifeform.h"
// Compatibility views only. PlanetRenderResources owns every allocation in
// this record; C generators/drawers borrow it only during the active visit.
struct planet_orbit
{
	FRAME TopoZoomFrame;
			// 4x scaled topo image for planet-side
	SBYTE  *lpTopoData;
			// normal topo data; expressed in elevation levels
			// data is signed for planets other than gas giants
			// transformed to light variance map for 3d planet
	FRAME SphereFrame;
			// rotating 3d planet frames (current and next)
	FRAME ObjectFrame;
			// any extra planetary object (shield, atmo, rings)
			// automatically drawn if present
	FRAME TintFrame;
			// tinted topo images for current scan type (dynamic)
	Color TintColor;
			// the color of the last used tint
	Color *TopoColors;
			// RGBA version of topo image; for 3d planet
	Color *ScratchArray;
			// temp RGBA data for whatever transforms (nuked often)
	FRAME WorkFrame;
			// any extra frame workspace (for dynamic objects)
};

typedef struct planet_orbit PLANET_ORBIT;
typedef struct PlanetRenderData {
    STRING XlatRef;
    const void *XlatPtr;
    COLORMAP OrbitalCMap;
    // [0] biological canister, [1] this body's energy-site art, [2] unused,
    // [3..] the generated creature variations used on this surface.
    FRAME PlanetSideFrame[3 + MAX_LIFE_VARIATION];
    FRAME TopoFrame;
    PLANET_ORBIT Orbit;
    struct PlanetSphereData *sphere; // Borrowed mutable workspace; owner is native.
} PLANET_RENDER_DATA;
#endif

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

#ifndef UQM_PLANETS_SUNDATA_H_
#define UQM_PLANETS_SUNDATA_H_

#include "plandata.h"
#include "libs/compiler.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*------------------------------ Global Data ------------------------------ */

#define NUMBER_OF_SUN_SIZES (SUPER_GIANT_STAR - DWARF_STAR + 1)

#define DWARF_ENERGY 1
#define GIANT_ENERGY 5
#define SUPERGIANT_ENERGY 20

typedef struct
{
	BYTE StarSize;
	BYTE StarIntensity;
	UWORD StarEnergy;

	PLANET_INFO PlanetInfo;
} SYSTEM_INFO;

// Explicit policy replaces the former output-address test in temperature analysis.
typedef enum PlanetAnalysisPolicy {
    PLANET_ANALYSIS_PREVIEW,
    PLANET_ANALYSIS_ORBIT
} PlanetAnalysisPolicy;
typedef struct PlanetAnalysisInput {
    BYTE starType; // Packed star size/color, as in STAR_DESC::Type.
    SIZE orbitalRadius; // Positive planet-to-star distance, including for moons.
    const PlanetFrame *planetTypes; // NUMBER_OF_PLANET_TYPES immutable entries.
    PlanetAnalysisPolicy policy;
} PlanetAnalysisInput;

#define GENERATE_ALL  ((COUNT)~0)
		
// Node generators reseed the supplied workspace from ScanSeed and return its
// resulting stream position. Map bounds are explicit and must exceed 16 on
// both axes; output nodes borrow no world, renderer or global environment.
extern COUNT GenerateMineralDeposits (const SYSTEM_INFO *, COUNT whichDeposit,
		NODE_INFO *info, RandomContext *rng, EXTENT mapSize);
extern COUNT GenerateLifeForms (const SYSTEM_INFO *, COUNT whichLife,
		NODE_INFO *info, RandomContext *rng, EXTENT mapSize);
extern void GenerateRandomLocation (POINT *loc, RandomContext *rng, EXTENT mapSize);
extern COUNT GenerateRandomNodes (const SYSTEM_INFO *, COUNT scan, COUNT numNodes,
		COUNT type, COUNT whichNode, NODE_INFO *info, RandomContext *rng, EXTENT mapSize);
// Generate lifeforms from a preset lifeTypes[] array
extern COUNT GeneratePresetLife (const SYSTEM_INFO *,
		const SBYTE *lifeTypes, COUNT whichLife, NODE_INFO *info, RandomContext *rng, EXTENT mapSize);

#define DWARF_ELEMENT_DENSITY  1
#define GIANT_ELEMENT_DENSITY 3
#define SUPERGIANT_ELEMENT_DENSITY 8

#define MAX_ELEMENT_DENSITY ((MAX_ELEMENT_UNITS * SUPERGIANT_ELEMENT_DENSITY) << 1)

// Output address has no meaning. Inputs and RNG are required; the planet-type
// table and descriptor remain borrowed, and the supplied stream is reseeded.
extern void DoPlanetaryAnalysis (SYSTEM_INFO *SysInfoPtr,
		const PLANET_DESC *pPlanetDesc, RandomContext *rng,
        const PlanetAnalysisInput *inputs);

#if defined(__cplusplus)
}
#endif

#endif /* UQM_PLANETS_SUNDATA_H_ */


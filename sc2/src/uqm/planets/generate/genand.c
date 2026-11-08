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

#include "genall.h"
#include "../lander.h"
#include "../planets.h"
#include "../scan.h"
#include "../../globdata.h"
#include "../../state.h"
#include "../../starmap.h"
#include "../../nameref.h"
#include "../../resinst.h"
#include "../../sounds.h"
#include "libs/mathlib.h"


static bool GenerateAndrosynth_generatePlanets (SOLARSYS_STATE *solarSys);
static bool GenerateAndrosynth_generateOrbital (SOLARSYS_STATE *solarSys,
		PLANET_DESC *world);
static COUNT GenerateAndrosynth_generateEnergy (const SOLARSYS_STATE *,
		const PLANET_DESC *world, COUNT whichNode, NODE_INFO *);
static bool GenerateAndrosynth_pickupEnergy (SOLARSYS_STATE *solarSys,
		PLANET_DESC *world, COUNT whichNode);


const GenerateFunctions generateAndrosynthFunctions = {
	/* .initNpcs         = */ GenerateDefault_initNpcs,
	/* .reinitNpcs       = */ GenerateDefault_reinitNpcs,
	/* .uninitNpcs       = */ GenerateDefault_uninitNpcs,
	/* .generatePlanets  = */ GenerateAndrosynth_generatePlanets,
	/* .generateMoons    = */ GenerateDefault_generateMoons,
	/* .generateName     = */ GenerateDefault_generateName,
	/* .generateOrbital  = */ GenerateAndrosynth_generateOrbital,
	/* .generateMinerals = */ GenerateDefault_generateMinerals,
	/* .generateEnergy   = */ GenerateAndrosynth_generateEnergy,
	/* .generateLife     = */ GenerateDefault_generateLife,
	/* .pickupMinerals   = */ GenerateDefault_pickupMinerals,
	/* .pickupEnergy     = */ GenerateAndrosynth_pickupEnergy,
	/* .pickupLife       = */ GenerateDefault_pickupLife,
};


static bool
GenerateAndrosynth_generatePlanets (SOLARSYS_STATE *solarSys)
{
	COUNT angle;

	GenerateDefault_generatePlanets (solarSys);

	solarSys->PlanetDesc[1].data_index = TELLURIC_WORLD;
	solarSys->PlanetDesc[1].radius = EARTH_RADIUS * 204L / 100;
	angle = ARCTAN (solarSys->PlanetDesc[1].location.x,
			solarSys->PlanetDesc[1].location.y);
	solarSys->PlanetDesc[1].location.x =
			COSINE (angle, solarSys->PlanetDesc[1].radius);
	solarSys->PlanetDesc[1].location.y =
			SINE (angle, solarSys->PlanetDesc[1].radius);

	return true;
}

static bool
GenerateAndrosynth_generateOrbital (SOLARSYS_STATE *solarSys, PLANET_DESC *world)
{
	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
        COUNT visits = PersonalDiscoveryCount ((COUNT)GetStarIndex(CurStarDescPtr), 1, -1);

		LoadStdLanderFont (&solarSys->SysInfo.PlanetInfo);
		solarSys->PlanetSideFrame[1] =
				CaptureDrawable (LoadGraphic (RUINS_MASK_PMAP_ANIM));
		solarSys->SysInfo.PlanetInfo.DiscoveryString =
				CaptureStringTable (
				LoadStringTable (ANDROSYNTH_RUINS_STRTAB));
        // The site remains in the shared world. This player's accepted visits
        // select the next report independently of physical retrieval masks.
		if (visits >= GetStringTableCount (
				solarSys->SysInfo.PlanetInfo.DiscoveryString))
		{	// All the reports were already given
			DestroyStringTable (ReleaseStringTable (
					solarSys->SysInfo.PlanetInfo.DiscoveryString));
			solarSys->SysInfo.PlanetInfo.DiscoveryString = 0;
		}
		else
		{	// Advance the report sequence to the first unread
			solarSys->SysInfo.PlanetInfo.DiscoveryString =
					SetRelStringTableIndex (
					solarSys->SysInfo.PlanetInfo.DiscoveryString, visits);
		}
	}

	GenerateDefault_generateOrbital (solarSys, world);

	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
		solarSys->SysInfo.PlanetInfo.AtmoDensity =
				EARTH_ATMOSPHERE * 144 / 100;
		solarSys->SysInfo.PlanetInfo.SurfaceTemperature = 28;
		solarSys->SysInfo.PlanetInfo.Weather = 1;
		solarSys->SysInfo.PlanetInfo.Tectonics = 1;
	}

	return true;
}

static bool
GenerateAndrosynth_pickupEnergy (SOLARSYS_STATE *solarSys, PLANET_DESC *world,
		COUNT whichNode)
{
	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
        if (!VisitPersonalDiscovery ((COUNT)GetStarIndex(CurStarDescPtr), 1, -1, whichNode))
            return false; // This player already visited; keep the physical site.

		// Androsynth ruins have several lander reports which form a story
		GenerateDefault_landerReportCycle (solarSys);

		return false; // do not remove the node from the surface
	}

	return false;
}

static COUNT
GenerateAndrosynth_generateEnergy (const SOLARSYS_STATE *solarSys,
		const PLANET_DESC *world, COUNT whichNode, NODE_INFO *info)
{
	if (matchWorld (solarSys, world, 1, MATCH_PLANET))
	{
		return GenerateDefault_generateRuins (solarSys, whichNode, info);
	}

	return 0;
}

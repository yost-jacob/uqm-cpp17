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

#include "state.h"

#include "starmap.h"

void
InitPlanetInfo (void)
{
	GAME_STATE_FILE *fp;

	fp = OpenStateFile (STARINFO_FILE, "wb");
	if (fp)
	{
		STAR_DESC *pSD;

		// Set record offsets for all stars to 0 (not present)
		pSD = GetStar(0);
		do
		{
			swrite_32 (fp, 0);
			pSD = GetStar(GetStarIndex(pSD) + 1);
		} while (pSD->star_pt.x <= MAX_X_UNIVERSE
				&& pSD->star_pt.y <= MAX_Y_UNIVERSE);

		CloseStateFile (fp);
	}
}

void
UninitPlanetInfo (void)
{
	DeleteStateFile (STARINFO_FILE);
}

#define OFFSET_SIZE       (sizeof (DWORD))
#define SCAN_RECORD_SIZE  (sizeof (DWORD) * NUM_SCAN_TYPES)

void
GetPlanetInfo (void)
{
	GAME_STATE_FILE *fp;

	pSolarSysState->SysInfo.PlanetInfo.ScanRetrieveMask[BIOLOGICAL_SCAN] = 0;
	pSolarSysState->SysInfo.PlanetInfo.ScanRetrieveMask[MINERAL_SCAN] = 0;
	pSolarSysState->SysInfo.PlanetInfo.ScanRetrieveMask[ENERGY_SCAN] = 0;

	fp = OpenStateFile (STARINFO_FILE, "rb");
	if (fp)
	{
		COUNT star_index, planet_index, moon_index;
		DWORD offset;

		star_index = (COUNT)(GetStarIndex(CurStarDescPtr));
		planet_index = (COUNT)(pSolarSysState->pBaseDesc->pPrevDesc
				- pSolarSysState->PlanetDesc);
		if (pSolarSysState->pOrbitalDesc->pPrevDesc == pSolarSysState->SunDesc)
			moon_index = 0;
		else
			moon_index = (COUNT)(pSolarSysState->pOrbitalDesc
					- pSolarSysState->MoonDesc + 1);

		SeekStateFile (fp, star_index * OFFSET_SIZE, SEEK_SET);
		sread_32 (fp, &offset);

		if (offset)
		{
			COUNT i;

			// Skip scan records for all preceeding planets to the one we need
			for (i = 0; i < planet_index; ++i)
				offset += (pSolarSysState->PlanetDesc[i].NumPlanets + 1) *
						SCAN_RECORD_SIZE;
				
			// Skip scan records for all preceeding moons to the one we need
			offset += moon_index * SCAN_RECORD_SIZE;

			SeekStateFile (fp, offset, SEEK_SET);
			sread_a32 (fp, pSolarSysState->SysInfo.PlanetInfo.ScanRetrieveMask,
					NUM_SCAN_TYPES);
		}

		CloseStateFile (fp);
	}
}

void
PutPlanetInfo (void)
{
	GAME_STATE_FILE *fp;

	fp = OpenStateFile (STARINFO_FILE, "r+b");
	if (fp)
	{
		COUNT i;
		COUNT star_index, planet_index, moon_index;
		DWORD offset;

		star_index = (COUNT)(GetStarIndex(CurStarDescPtr));
		planet_index = (COUNT)(pSolarSysState->pBaseDesc->pPrevDesc
				- pSolarSysState->PlanetDesc);
		if (pSolarSysState->pOrbitalDesc->pPrevDesc == pSolarSysState->SunDesc)
			moon_index = 0;
		else
			moon_index = (COUNT)(pSolarSysState->pOrbitalDesc
					- pSolarSysState->MoonDesc + 1);

		SeekStateFile (fp, star_index * OFFSET_SIZE, SEEK_SET);
		sread_32 (fp, &offset);

		if (offset == 0)
		{	// Scan record not present yet -- init it
			DWORD ScanRetrieveMask[NUM_SCAN_TYPES] =
			{
				0, 0, 0,
			};

			offset = LengthStateFile (fp);

			// Write the record offset
			SeekStateFile (fp, star_index * OFFSET_SIZE, SEEK_SET);
			swrite_32 (fp, offset);

			// Init scan records for all planets and moons in the system
			SeekStateFile (fp, offset, SEEK_SET);
			for (i = 0; i < pSolarSysState->SunDesc[0].NumPlanets; ++i)
			{
				COUNT j;

				swrite_a32 (fp, ScanRetrieveMask, NUM_SCAN_TYPES);
				// init moons
				for (j = 0; j < pSolarSysState->PlanetDesc[i].NumPlanets; ++j)
					swrite_a32 (fp, ScanRetrieveMask, NUM_SCAN_TYPES);
			}
		}

		// Skip scan records for all preceeding planets to the one we need
		for (i = 0; i < planet_index; ++i)
			offset += (pSolarSysState->PlanetDesc[i].NumPlanets + 1) *
					SCAN_RECORD_SIZE;
				
		// Skip scan records for all preceeding moons to the one we need
		offset += moon_index * SCAN_RECORD_SIZE;

		SeekStateFile (fp, offset, SEEK_SET);
		swrite_a32 (fp, pSolarSysState->SysInfo.PlanetInfo.ScanRetrieveMask,
				NUM_SCAN_TYPES);

		CloseStateFile (fp);
	}
}


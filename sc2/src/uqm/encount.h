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

#ifndef UQM_ENCOUNT_H_
#define UQM_ENCOUNT_H_

typedef struct brief_ship_info BRIEF_SHIP_INFO;
typedef struct encounter ENCOUNTER;

// XXX: temporary, for CONVERSATION
#include "commglue.h"
#include "displist.h"
#include <stdint.h>
#include "libs/gfxlib.h"
#include "planets/planets.h"
#include "element.h"
#include "races.h"

#if defined(__cplusplus)
extern "C" {
#endif



#define MAX_HYPER_SHIPS 7

// ENCOUNTER.flags
// XXX: Currently, the flags are combined with num_ships into a single BYTE
//   in the savegames: num_ships occupy the low nibble and flags the high one.
//   Bits 4 and 5 are available for more flags in the savegames,
//   and bits 0-3 available in the game but will not be saved.
#define ONE_SHOT_ENCOUNTER (1 << 7)
#define ENCOUNTER_REFORMING (1 << 6)
#define ENCOUNTER_SHIPS_MASK  0x0f
#define ENCOUNTER_FLAGS_MASK  0xf0

struct brief_ship_info
{
	// The only field actually used right now is crew_level
	BYTE race_id;
	COUNT crew_level;
	COUNT max_crew;
	BYTE max_energy;

};

struct encounter
{
	uint64_t runtime_id; // Session-local identity; not part of either save format.

	// Screen entities belong to the active navigation visit, not this record.

	SIZE transition_state;
	POINT origin;
	COUNT radius;
	BYTE race_id;
	BYTE num_ships;
	BYTE flags;
			// See ENCOUNTER.flags above
	POINT loc_pt;

	BRIEF_SHIP_INFO ShipList[MAX_HYPER_SHIPS];
			// Only the crew_level member is currently used

	SDWORD log_x, log_y;
};

// C save/load and legacy-menu adapters. Native callers borrow the session's
// CampaignEncounters directly. Pointers expire on erase/clear; no archive/load
// runs while a child mode holds an encounter borrow.
COUNT CampaignEncounterCount(void);
ENCOUNTER *AppendCampaignEncounter(void); // NULL when the gameplay cap is reached.
void ClearCampaignEncounters(void);
void SelectCampaignEncounter(COUNT index); // count clears selection.
// Existing save formats encode selection by putting the selected record first.
ENCOUNTER *CampaignEncounterForSave(COUNT index);

enum
{
	HAIL = 0,
	ATTACK
};

extern BOOLEAN FleetIsInfinite (COUNT playerNr);
extern void UpdateShipFragCrew (STARSHIP *);

// Last race the player battled with, or -1 if no battle took place.
// Set to -1 by some funcs to inhibit IP groups from intercepting
// the flagship.
extern SIZE EncounterRace;
extern BYTE EncounterGroup;

#if defined(__cplusplus)
}
#endif

#endif /* UQM_ENCOUNT_H_ */

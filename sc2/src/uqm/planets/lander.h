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

#ifndef UQM_PLANETS_LANDER_H_
#define UQM_PLANETS_LANDER_H_

#include "elemdata.h"
#include "libs/compiler.h"
#include "libs/gfxlib.h"
#include "libs/sndlib.h"
#include "libs/timelib.h"
#include "../element.h"

#if defined(__cplusplus)
extern "C" {
#endif

// Surface magnification shift (x4)
#define MAG_SHIFT 2

#define NUM_TEXT_FRAMES 32

extern MUSIC_REF LanderMusic;

// Report text and presentation handles are borrowed for this synchronous call.
extern void ShowDiscoveryReport (SOUND ReadOutSounds, FONT font, FRAME fontEffect, STRING text);
extern void SetPlanetMusic (BYTE planet_type);
extern void LoadLanderData (void);
extern void FreeLanderData (void);

extern void object_animation (ELEMENT *ElementPtr);

extern void SetLanderTakeoff (void);
extern bool KillLanderCrewSeq (COUNT numKilled, DWORD period);

extern unsigned GetThermalHazardRating (int temp);

// ELEMENT.playerNr constants
enum
{
	PS_HUMAN_PLAYER,
	PS_NON_PLAYER,
};

#if defined(__cplusplus)
}
#endif

#endif /* UQM_PLANETS_LANDER_H_ */

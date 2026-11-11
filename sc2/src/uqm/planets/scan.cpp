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


#include "libs/threadlib.h"
#include "../system_runtime.hpp"
#include "../world_legacy.hpp"
#include <stdexcept>
#include "../system_behavior.hpp"
#include "lander.h"
#include "lifeform.h"
#include "scan.h"
#include "../build.h"
#include "../colors.h"
#include "../cons_res.h"
#include "../controls.h"
#include "../menustat.h"
#include "../encount.h"
		// for EncounterGroup
#include "../gamestr.h"
#include "../nameref.h"
#include "../resinst.h"
#include "../settings.h"
#include "../util.h"
#include "../process.h"
#include "../setup.h"
#include "../sounds.h"
#include "../state.h"
#include "../sis.h"
#include "../save.h"
#include "options.h"
#include "libs/graphics/gfx_common.h"
#include "libs/graphics/drawable.h"
#include "libs/inplib.h"
#include "libs/mathlib.h"

extern FRAME SpaceJunkFrame;

// define SPIN_ON_SCAN to allow the planet to spin 
// while scaning  is going on
#undef SPIN_ON_SCAN

#define FLASH_INDEX 105

static CONTEXT ScanContext;


// ScanSystem(system) menu items
// The first three are from enum PlanetScanTypes in planets.h
enum ScanMenuItems
{
	EXIT_SCAN = NUM_SCAN_TYPES,
	AUTO_SCAN,
	DISPATCH_SHUTTLE,
};


void
RepairBackRect (RECT *pRect)
{
	RECT new_r, old_r;

	GetContextClipRect (&old_r);
	new_r.corner.x = pRect->corner.x + old_r.corner.x;
	new_r.corner.y = pRect->corner.y + old_r.corner.y;
	new_r.extent = pRect->extent;

	new_r.extent.height += new_r.corner.y & 1;
	new_r.corner.y &= ~1;
	DrawFromExtraScreen (&new_r);
}

static void
EraseCoarseScan(SOLARSYS_STATE &system)
{
	SetContext (PlanetContext);
	
	BatchGraphics ();
	uqm::drawStarBackground(system);
	DrawDefaultPlanetSphere ();
	UnbatchGraphics ();
}

static void
PrintScanTitlePC (TEXT *t, RECT *r, const char *txt, int xpos)
{
	t->baseline.x = xpos;
	SetContextForeGroundColor (SCAN_PC_TITLE_COLOR);
	t->pStr = txt;
	t->CharCount = (COUNT)~0;
	font_DrawText (t);
	TextRect (t, r, NULL);
	t->baseline.x += r->extent.width;
	SetContextForeGroundColor (SCAN_INFO_COLOR);
}

static void
MakeScanValue (UNICODE *buf, long val, const UNICODE *extra)
{
	if (val >= 10 * 100)
	{	// 1 decimal place
		sprintf (buf, "%ld.%ld%s", val / 100, (val / 10) % 10, extra);
	}
	else
	{	// 2 decimal places
		sprintf (buf, "%ld.%02ld%s", val / 100, val % 100, extra);
	}
}

static void
GetPlanetTitle(const SOLARSYS_STATE &system, UNICODE *buf, COUNT bufsize)
{
	int val;
	const UNICODE *named = uqm::namedPlanetaryBody(system);
	if (named)
	{
		utf8StringCopy (buf, bufsize, named);
		return;
	}

	// Unnamed body, use world type
	val = uqm::detail::selectedDescriptor(system).data_index & ~PLANET_SHIELDED;
	if (val >= FIRST_GAS_GIANT)
	{
		sprintf (buf, "%s", GAME_STRING (SCAN_STRING_BASE + 4 + 51));
					// Gas Giant
	}
	else
	{
		sprintf (buf, "%s %s",
				GAME_STRING (SCAN_STRING_BASE + 4 + val),
				GAME_STRING (SCAN_STRING_BASE + 4 + 50));
						// World
	}
}

static void
PrintCoarseScanPC(const SOLARSYS_STATE &system)
{
#define SCAN_LEADING_PC 14
	SDWORD val;
	TEXT t;
	RECT r;
	UNICODE buf[200];

	GetPlanetTitle(system, buf, sizeof (buf));

	SetContext (PlanetContext);

	t.align = ALIGN_CENTER;
	t.baseline.x = SIS_SCREEN_WIDTH >> 1;
	t.baseline.y = 13;
	t.pStr = buf;
	t.CharCount = (COUNT)~0;

	SetContextForeGroundColor (SCAN_PC_TITLE_COLOR);
	SetContextFont (MicroFont);
	font_DrawText (&t);

	SetContextFont (TinyFont);

#define LEFT_SIDE_BASELINE_X_PC 5
#define RIGHT_SIDE_BASELINE_X_PC (SIS_SCREEN_WIDTH - 75)
#define SCAN_BASELINE_Y_PC 40

	t.baseline.y = SCAN_BASELINE_Y_PC;
	t.align = ALIGN_LEFT;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE),
			LEFT_SIDE_BASELINE_X_PC); // "Orbit: "
	val = ((system.SysInfo.PlanetInfo.PlanetToSunDist * 100L
			+ (EARTH_RADIUS >> 1)) / EARTH_RADIUS);
	MakeScanValue (buf, val,
			GAME_STRING (ORBITSCAN_STRING_BASE + 1)); // " a.u."
	t.pStr = buf;
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 2),
			LEFT_SIDE_BASELINE_X_PC); // "Atmo: "
	if (system.SysInfo.PlanetInfo.AtmoDensity == GAS_GIANT_ATMOSPHERE)
		utf8StringCopy (buf, sizeof (buf),
				GAME_STRING (ORBITSCAN_STRING_BASE + 3)); // "Super Thick"
	else if (system.SysInfo.PlanetInfo.AtmoDensity == 0)
		utf8StringCopy (buf, sizeof (buf),
				GAME_STRING (ORBITSCAN_STRING_BASE + 4)); // "Vacuum"
	else
	{
		val = (system.SysInfo.PlanetInfo.AtmoDensity * 100
			+ (EARTH_ATMOSPHERE >> 1)) / EARTH_ATMOSPHERE;
		MakeScanValue (buf, val,
				GAME_STRING (ORBITSCAN_STRING_BASE + 5)); // " atm"
	}
	t.pStr = buf;
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 6),
			LEFT_SIDE_BASELINE_X_PC); // "Temp: "
	sprintf (buf, "%d" STR_DEGREE_SIGN " c",
			system.SysInfo.PlanetInfo.SurfaceTemperature);
	t.pStr = buf;
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 7),
			LEFT_SIDE_BASELINE_X_PC); // "Weather: "
	if (system.SysInfo.PlanetInfo.AtmoDensity == 0)
		t.pStr = GAME_STRING (ORBITSCAN_STRING_BASE + 8); // "None"
	else
	{
		sprintf (buf, "%s %u",
				GAME_STRING (ORBITSCAN_STRING_BASE + 9), // "Class"
				system.SysInfo.PlanetInfo.Weather + 1);
		t.pStr = buf;
	}
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 10),
			LEFT_SIDE_BASELINE_X_PC); // "Tectonics: "
	if (PLANSIZE (system.SysInfo.PlanetInfo.PlanDataPtr->Type) ==
			GAS_GIANT)
		t.pStr = GAME_STRING (ORBITSCAN_STRING_BASE + 8); // "None"
	else
	{
		sprintf (buf, "%s %u",
				GAME_STRING (ORBITSCAN_STRING_BASE + 9), // "Class"
				system.SysInfo.PlanetInfo.Tectonics + 1);
		t.pStr = buf;
	}
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);

	t.baseline.y = SCAN_BASELINE_Y_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 11),
			RIGHT_SIDE_BASELINE_X_PC); // "Mass: "
	val = system.SysInfo.PlanetInfo.PlanetRadius;
	val = ((DWORD) val * (DWORD) val * (DWORD) val / 100L
			* system.SysInfo.PlanetInfo.PlanetDensity
			+ ((100L * 100L) >> 1)) / (100L * 100L);
	if (val == 0)
		val = 1;
	MakeScanValue (buf, val,
			GAME_STRING (ORBITSCAN_STRING_BASE + 12)); // " e.s."
	t.pStr = buf;
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 13),
			RIGHT_SIDE_BASELINE_X_PC); // "Radius: "
	val = system.SysInfo.PlanetInfo.PlanetRadius;
	MakeScanValue (buf, val,
			GAME_STRING (ORBITSCAN_STRING_BASE + 12)); // " e.s."
	t.pStr = buf;
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 14),
			RIGHT_SIDE_BASELINE_X_PC); // "Gravity: "
	val = system.SysInfo.PlanetInfo.SurfaceGravity;
	if (val == 0)
		val = 1;
	MakeScanValue (buf, val,
			GAME_STRING (ORBITSCAN_STRING_BASE + 15)); // " g."
	t.pStr = buf;
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 16),
			RIGHT_SIDE_BASELINE_X_PC); // "Day: "
	val = (SDWORD)system.SysInfo.PlanetInfo.RotationPeriod
			* 10 / 24;
	MakeScanValue (buf, val,
			GAME_STRING (ORBITSCAN_STRING_BASE + 17)); // " days"
	t.pStr = buf;
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING_PC;

	PrintScanTitlePC (&t, &r, GAME_STRING (ORBITSCAN_STRING_BASE + 18),
			RIGHT_SIDE_BASELINE_X_PC); // "Tilt: "
	val = system.SysInfo.PlanetInfo.AxialTilt;
	if (val < 0)
		val = -val;
	t.pStr = buf;
	sprintf (buf, "%d" STR_DEGREE_SIGN, val);
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
}

static void
PrintCoarseScan3DO(const SOLARSYS_STATE &system)
{
#define SCAN_LEADING 19
	SDWORD val;
	TEXT t;
	STAMP s;
	UNICODE buf[200];

	GetPlanetTitle(system, buf, sizeof (buf));

	SetContext (PlanetContext);

	t.align = ALIGN_CENTER;
	t.baseline.x = SIS_SCREEN_WIDTH >> 1;
	t.baseline.y = 13;
	t.pStr = buf;
	t.CharCount = (COUNT)~0;

	SetContextForeGroundColor (SCAN_INFO_COLOR);
	SetContextFont (MicroFont);
	font_DrawText (&t);

	s.origin.x = s.origin.y = 0;
	s.origin.x = 16 - SAFE_X;
	s.frame = SetAbsFrameIndex (SpaceJunkFrame, 20);
	DrawStamp (&s);

#define LEFT_SIDE_BASELINE_X (27 + (16 - SAFE_X))
#define RIGHT_SIDE_BASELINE_X (SIS_SCREEN_WIDTH - LEFT_SIDE_BASELINE_X)
#define SCAN_BASELINE_Y 25

	t.baseline.x = LEFT_SIDE_BASELINE_X;
	t.baseline.y = SCAN_BASELINE_Y;
	t.align = ALIGN_LEFT;

	t.pStr = buf;
	val = ((system.SysInfo.PlanetInfo.PlanetToSunDist * 100L
			+ (EARTH_RADIUS >> 1)) / EARTH_RADIUS);
	MakeScanValue (buf, val, STR_EARTH_SIGN);
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING;

	t.pStr = buf;
	if (system.SysInfo.PlanetInfo.AtmoDensity == GAS_GIANT_ATMOSPHERE)
		strcpy (buf, STR_INFINITY_SIGN);
	else
	{
		val = (system.SysInfo.PlanetInfo.AtmoDensity * 100
				+ (EARTH_ATMOSPHERE >> 1)) / EARTH_ATMOSPHERE;
		MakeScanValue (buf, val, STR_EARTH_SIGN);
	}
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING;

	t.pStr = buf;
	sprintf (buf, "%d" STR_DEGREE_SIGN,
			system.SysInfo.PlanetInfo.SurfaceTemperature);
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING;

	t.pStr = buf;
	sprintf (buf, "<%u>", system.SysInfo.PlanetInfo.AtmoDensity == 0
			? 0 : (system.SysInfo.PlanetInfo.Weather + 1));
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING;

	t.pStr = buf;
	sprintf (buf, "<%u>",
			PLANSIZE (
			system.SysInfo.PlanetInfo.PlanDataPtr->Type
			) == GAS_GIANT
			? 0 : (system.SysInfo.PlanetInfo.Tectonics + 1));
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);

	t.baseline.x = RIGHT_SIDE_BASELINE_X;
	t.baseline.y = SCAN_BASELINE_Y;
	t.align = ALIGN_RIGHT;

	t.pStr = buf;
	val = system.SysInfo.PlanetInfo.PlanetRadius;
	val = ((DWORD) val * (DWORD) val * (DWORD) val / 100L
			* system.SysInfo.PlanetInfo.PlanetDensity
			+ ((100L * 100L) >> 1)) / (100L * 100L);
	if (val == 0)
		val = 1;
	MakeScanValue (buf, val, STR_EARTH_SIGN);
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING;

	t.pStr = buf;
	val = system.SysInfo.PlanetInfo.PlanetRadius;
	MakeScanValue (buf, val, STR_EARTH_SIGN);

	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING;

	t.pStr = buf;
	val = system.SysInfo.PlanetInfo.SurfaceGravity;
	if (val == 0)
		val = 1;
	MakeScanValue (buf, val, STR_EARTH_SIGN);
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING;

	t.pStr = buf;
	val = system.SysInfo.PlanetInfo.AxialTilt;
	if (val < 0)
		val = -val;
	sprintf (buf, "%d" STR_DEGREE_SIGN, val);
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
	t.baseline.y += SCAN_LEADING;

	t.pStr = buf;
	val = (SDWORD)system.SysInfo.PlanetInfo.RotationPeriod
			* 10 / 24;
	MakeScanValue (buf, val, STR_EARTH_SIGN);
	t.CharCount = (COUNT)~0;
	font_DrawText (&t);
}

static void
initPlanetLocationImage(SOLARSYS_STATE &system)
{
    auto &cursor = system.worlds->scan;
	FRAME cursorFrame;

	// Get the cursor image
	cursorFrame = SetAbsFrameIndex (MiscDataFrame, FLASH_INDEX);
	cursor.cursorRect.extent = GetFrameBounds (cursorFrame);
}

static void
savePlanetLocationImage(SOLARSYS_STATE &system)
{
    auto &cursor = system.worlds->scan;
	RECT r;
	FRAME cursorFrame = SetAbsFrameIndex (MiscDataFrame, FLASH_INDEX);
	HOT_SPOT hs = GetFrameHot (cursorFrame);

	DestroyDrawable (ReleaseDrawable (cursor.eraseFrame));

	r = cursor.cursorRect;
	r.corner.x -= hs.x;
	r.corner.y -= hs.y;
	cursor.eraseFrame = CaptureDrawable (CopyContextRect (&r));
	SetFrameHot (cursor.eraseFrame, hs);
}

static void
restorePlanetLocationImage(const SOLARSYS_STATE &system)
{
    const auto &cursor = system.worlds->scan;
	STAMP s;

	s.origin = cursor.cursorRect.corner;
	s.frame = cursor.eraseFrame; // saved image
	DrawStamp (&s);
}

static void
drawPlanetCursor(const SOLARSYS_STATE &system, BOOLEAN filled)
{
    const auto &cursor = system.worlds->scan;
	STAMP s;

	s.origin = cursor.cursorRect.corner;
	s.frame = SetAbsFrameIndex (MiscDataFrame, FLASH_INDEX);
	if (filled)
		DrawFilledStamp (&s);
	else
		DrawStamp (&s);
}

static void
setPlanetCursorLoc(SOLARSYS_STATE &system, POINT new_pt)
{
    auto &cursor = system.worlds->scan;
	new_pt.x >>= MAG_SHIFT;
	new_pt.y >>= MAG_SHIFT;
	cursor.cursorRect.corner = new_pt;
}

static void
setPlanetLoc(SOLARSYS_STATE &system, POINT new_pt, BOOLEAN restoreOld)
{
    auto &cursor = system.worlds->scan;
	cursor.location = new_pt;

	SetContext (ScanContext);
	if (restoreOld)
		restorePlanetLocationImage(system);
	setPlanetCursorLoc(system, new_pt);
	savePlanetLocationImage(system);
}

static void
flashPlanetLocation(SOLARSYS_STATE &system)
{
    auto &cursor = system.worlds->scan;
#define FLASH_FRAME_DELAY  (ONE_SECOND / 16)
	auto &c = cursor.flashColor;
	auto &val = cursor.flashStep;
	auto &prevPt = cursor.previousFlashLocation;
	auto &NextTime = cursor.nextFlash;
	bool locChanged;
	TimeCount Now = GetTimeCounter ();

	locChanged = prevPt.x != cursor.cursorRect.corner.x
				|| prevPt.y != cursor.cursorRect.corner.y;

	if (!locChanged && Now < NextTime)
		return; // nothing to do

	if (locChanged)
	{	// Reset the flashing cycle
		c = 0x00;
		val = -2;
		prevPt = cursor.cursorRect.corner;
		
		NextTime = Now + FLASH_FRAME_DELAY;
	}
	else
	{	// Continue the flashing cycle
		if (c == 0x00 || c == 0x1A)
			val = -val;
		c += val;

		if (Now - NextTime > FLASH_FRAME_DELAY)
			NextTime = Now + FLASH_FRAME_DELAY; // missed timing by too much
		else
			NextTime += FLASH_FRAME_DELAY; // stable frame rate
	}

	SetContext (ScanContext);
	SetContextForeGroundColor (BUILD_COLOR (MAKE_RGB15 (c, c, c), c));
	drawPlanetCursor(system, TRUE);
}

void
uqm::redrawSurfaceScan(SOLARSYS_STATE &system, const POINT *newLoc)
{
	CONTEXT OldContext;

	OldContext = SetContext (ScanContext);

	BatchGraphics ();
	DrawPlanet (0, BLACK_COLOR);
	DrawScannedObjects (TRUE);
	if (newLoc)
	{
		setPlanetLoc(system, *newLoc, FALSE);
		drawPlanetCursor(system, FALSE);
	}
	UnbatchGraphics ();

	SetContext (OldContext);
}

static COUNT
getLandingFuelNeeded(const SOLARSYS_STATE &system)
{
	COUNT fuel;

	fuel = system.SysInfo.PlanetInfo.SurfaceGravity << 1;
	if (fuel > 3 * FUEL_TANK_SCALE)
		fuel = 3 * FUEL_TANK_SCALE;

	return fuel;
}

static void
spawnFwiffo (void)
{
	HSHIPFRAG hStarShip;

	EncounterGroup = 0;
	PutGroupInfo (GROUPS_RANDOM, GROUP_SAVE_IP);
	ReinitQueue (&GLOBAL (ip_group_q));
	assert (CountLinks (&GLOBAL (npc_built_ship_q)) == 0);

	hStarShip = CloneShipFragment (SPATHI_SHIP,
			&GLOBAL (npc_built_ship_q), 1);
	if (hStarShip)
	{
		SHIP_FRAGMENT *StarShipPtr;

		StarShipPtr = LockShipFrag (&GLOBAL (npc_built_ship_q),
				hStarShip);
		// Name Fwiffo
		StarShipPtr->captains_name_index = NAME_OFFSET +
				NUM_CAPTAINS_NAMES;
		UnlockShipFrag (&GLOBAL (npc_built_ship_q), hStarShip);
	}
}

// Returns TRUE if the parent menu should remain
static BOOLEAN
DispatchLander(SOLARSYS_STATE &system)
{
    auto &cursor = system.worlds->scan;
	SIZE landingFuel = getLandingFuelNeeded(system);

	EraseCoarseScan(system);


	DeltaSISGauges (0, -landingFuel, 0);
	SetContext (ScanContext);
	drawPlanetCursor(system, FALSE);

    if (!system.worlds->session) throw std::logic_error("lander requires its owning session");
    auto &session = *system.worlds->session;
    if (auto exit = session.swapGameMode(uqm::createLanderMode(session, system, system.worlds->simulation, cursor.location))) {
        GLOBAL(CurrentActivity) |= exit->kind == uqm::SessionExitKind::LoadGame ? CHECK_LOAD : CHECK_ABORT;
        return FALSE;
    }
	if (GLOBAL (CurrentActivity) & (CHECK_ABORT | CHECK_LOAD))
		return FALSE;

	if (uqm::systemStory(system).read(uqm::StoryCounter::FoundPlutoSpathi) == 1)
	{
		/* Create Fwiffo group and go into comm with it */
		spawnFwiffo ();

		NextActivity |= CHECK_LOAD; /* fake a load game */
		GLOBAL (CurrentActivity) |= START_ENCOUNTER;
		uqm::saveSolarSystemLocation(system);

		return FALSE;
	}


	return TRUE;
}

struct LandingSiteSelection
{
    SOLARSYS_STATE &system;
    bool success;
			// true when player selected a location
};

static BOOLEAN
DoPickPlanetSide(MENU_STATE *pMS)
{
	auto *pickState = static_cast<LandingSiteSelection *>(pMS->privData);
	auto &system = pickState->system;
    auto &cursor = system.worlds->scan;
	DWORD TimeIn = GetTimeCounter ();
	bool select, cancel;

	select = PulsedInputState.menu[KEY_MENU_SELECT];
	cancel = PulsedInputState.menu[KEY_MENU_CANCEL];
	
	if (GLOBAL (CurrentActivity) & CHECK_ABORT)
	{
		pickState->success = false;
		return FALSE;
	}

	if (cancel)
	{
		pickState->success = false;
		return FALSE;
	}
	else if (select)
	{
		pickState->success = true;
		return FALSE;
	}
	else
	{
		SIZE dx = 0;
		SIZE dy = 0;
		POINT new_pt;

		new_pt = cursor.location;

		if (CurrentInputState.menu[KEY_MENU_LEFT])
			dx = -1;
		if (CurrentInputState.menu[KEY_MENU_RIGHT])
			dx = 1;
		if (CurrentInputState.menu[KEY_MENU_UP])
			dy = -1;
		if (CurrentInputState.menu[KEY_MENU_DOWN])
			dy = 1;

		BatchGraphics ();

		dx = dx << MAG_SHIFT;
		if (dx)
		{
			new_pt.x += dx;
			if (new_pt.x < 0)
				new_pt.x += (MAP_WIDTH << MAG_SHIFT);
			else if (new_pt.x >= (MAP_WIDTH << MAG_SHIFT))
				new_pt.x -= (MAP_WIDTH << MAG_SHIFT);
		}
		dy = dy << MAG_SHIFT;
		if (dy)
		{
			new_pt.y += dy;
			if (new_pt.y < 0 || new_pt.y >= (MAP_HEIGHT << MAG_SHIFT))
				new_pt.y = cursor.location.y;
		}

		if (!pointsEqual (new_pt, cursor.location))
		{
			setPlanetLoc(system, new_pt, TRUE);
		}

		flashPlanetLocation(system);

		UnbatchGraphics ();

		SleepThreadUntil (TimeIn + ONE_SECOND / 40);
	}

	return TRUE;
}

static void
drawLandingFuelUsage (COUNT fuel)
{
	UNICODE buf[100];

	sprintf (buf, "%s%1.1f",
			GAME_STRING (NAVIGATION_STRING_BASE + 5),
			(float) fuel / FUEL_TANK_SCALE);
	DrawStatusMessage (buf);
}

static void
eraseLandingFuelUsage (void)
{
	DrawStatusMessage (NULL);
}

static BOOLEAN
PickPlanetSide(SOLARSYS_STATE &system)
{
    auto &cursor = system.worlds->scan;
	MENU_STATE MenuState;
	LandingSiteSelection PickState{system, false};
	COUNT fuel = getLandingFuelNeeded(system);
	bool retval = TRUE;

	memset (&MenuState, 0, sizeof MenuState);
	MenuState.privData = &PickState;

	ClearSISRect (CLEAR_SIS_RADAR);
	SetContext (ScanContext);
	BatchGraphics ();
	DrawPlanet (0, BLACK_COLOR);
	DrawScannedObjects (FALSE);
	UnbatchGraphics ();

	drawLandingFuelUsage (fuel);
	// Set the current flash location
	setPlanetCursorLoc(system, cursor.location);
	savePlanetLocationImage(system);

	InitLander (0);

	SetMenuSounds (MENU_SOUND_NONE, MENU_SOUND_SELECT);

	PickState.success = false;
	MenuState.InputFunc = DoPickPlanetSide;
	DoInput (&MenuState, TRUE);

	eraseLandingFuelUsage ();
	if (PickState.success)
	{	// player chose a location
		retval = DispatchLander(system);
	}
	else
	{	// player bailed out
		restorePlanetLocationImage(system);
	}

	SetMenuSounds (MENU_SOUND_ARROWS, MENU_SOUND_SELECT);

	return retval ? TRUE : FALSE;
}

#define NUM_FLASH_COLORS 8

static void
DrawScannedStuff (COUNT y, COUNT scan)
{
	HELEMENT hElement, hNextElement;
	Color OldColor;

	OldColor = SetContextForeGroundColor (BLACK_COLOR);

	for (hElement = GetHeadElement (); hElement; hElement = hNextElement)
	{
		ELEMENT *ElementPtr;
		SIZE dy;
		STAMP s;
		
		LockElement (hElement, &ElementPtr);
		hNextElement = GetSuccElement (ElementPtr);

		dy = y - ElementPtr->current.location.y;
		if (LOBYTE (ElementPtr->scan_node) != scan || dy < 0)
		{	// node of wrong type, or not time for it yet
			UnlockElement (hElement);
			continue;
		}

		// XXX: flag this as 'found' scanned object
		ElementPtr->state_flags |= APPEARING;

		s.origin = ElementPtr->current.location;
		
		if (dy >= NUM_FLASH_COLORS)
		{	// flashing done for this node, draw normal
			s.frame = ElementPtr->next.image.frame;
			DrawStamp (&s);
		}
		else
		{
			BYTE grad;
			Color c = WHITE_COLOR;
			COUNT nodeSize;
			
			// mineral -- white --> turquoise?? (contrasts with red)
			// energy -- white --> red (contrasts with white)
			// bio -- white --> violet (contrasts with green)
			grad = 0xff - 0xff * dy / (NUM_FLASH_COLORS - 1);
			switch (scan)
			{
				case MINERAL_SCAN:
					c.r = grad;
					break;
				case ENERGY_SCAN:
					c.g = grad;
					c.b = grad;
					break;
				case BIOLOGICAL_SCAN:
					c.g = grad;
					break;
			}
			
			SetContextForeGroundColor (c);
			
			// flash the node from the smallest size to node size
			// Get the node size for mineral, or number of transitions
			// for other scan types (was set by generatePlanetSide)
			nodeSize = GetFrameIndex (ElementPtr->next.image.frame)
					- GetFrameIndex (ElementPtr->current.image.frame);
			if (dy > nodeSize)
				dy = nodeSize;
			
			s.frame = SetRelFrameIndex (ElementPtr->current.image.frame, dy);
			DrawFilledStamp (&s);
		}

		UnlockElement (hElement);
	}
	
	SetContextForeGroundColor (OldColor);
}

COUNT
callGenerateForScanType (const SOLARSYS_STATE *solarSys,
		const PLANET_DESC *world, COUNT node, BYTE scanType, NODE_INFO *info)
{
	switch (scanType)
	{
		case MINERAL_SCAN:
			return uqm::systemBehavior(*solarSys).generateMinerals(*(solarSys), *(world), node, info);
		case ENERGY_SCAN:
			return uqm::systemBehavior(*solarSys).generateEnergy(*(solarSys), *(world), node, info);
		case BIOLOGICAL_SCAN:
			return uqm::systemBehavior(*solarSys).generateLife(*(solarSys), *(world), node, info);
	}

	assert (false);
	return 0;
}

bool
callPickupForScanType (SOLARSYS_STATE *solarSys, PLANET_DESC *world,
		COUNT node, BYTE scanType)
{
	switch (scanType)
	{
		case MINERAL_SCAN:
			return uqm::systemBehavior(*solarSys).pickupMinerals(*(solarSys), *(world), node);
		case ENERGY_SCAN:
			return uqm::systemBehavior(*solarSys).pickupEnergy(*(solarSys), *(world), node);
		case BIOLOGICAL_SCAN:
			return uqm::systemBehavior(*solarSys).pickupLife(*(solarSys), *(world), node);
	}

	assert (false);
	return false;
}

static void
ScanPlanet (COUNT scanType)
{
#define SCAN_DURATION   (ONE_SECOND * 7 / 4)
// NUM_FLASH_COLORS for flashing blips; 1 for the final frame
#define SCAN_LINES      (MAP_HEIGHT + NUM_FLASH_COLORS + 1)
#define SCAN_LINE_WAIT  (SCAN_DURATION / SCAN_LINES)

	COUNT startScan, endScan;
	COUNT scan;
	RECT r;
	static const Color textColors[] =
	{
		SCAN_MINERAL_TEXT_COLOR,
		SCAN_ENERGY_TEXT_COLOR,
		SCAN_BIOLOGICAL_TEXT_COLOR,
	};
	static const Color tintColors[] =
	{
		SCAN_MINERAL_TINT_COLOR,
		SCAN_ENERGY_TINT_COLOR,
		SCAN_BIOLOGICAL_TINT_COLOR,
	};

	if (scanType == AUTO_SCAN)
	{
		startScan = MINERAL_SCAN;
		endScan = BIOLOGICAL_SCAN;
	}
	else
	{
		startScan = scanType;
		endScan = scanType;
	}

	for (scan = startScan; scan <= endScan; ++scan)
	{
		TEXT t;
		SWORD i;
		Color tintColor;
				// Alpha value will be ignored.
		TimeCount TimeOut;

		t.baseline.x = SIS_SCREEN_WIDTH >> 1;
		t.baseline.y = SIS_SCREEN_HEIGHT - MAP_HEIGHT - 7;
		t.align = ALIGN_CENTER;
		t.CharCount = (COUNT)~0;

		t.pStr = GAME_STRING (SCAN_STRING_BASE + scan);

		SetContext (PlanetContext);
		r.corner.x = 0;
		r.corner.y = t.baseline.y - 10;
		r.extent.width = SIS_SCREEN_WIDTH;
		r.extent.height = t.baseline.y - r.corner.y + 1;
		// XXX: I do not know why we are repairing it here, as there
		//   should not be anything drawn over the stars at the moment
		RepairBackRect (&r);

		SetContextFont (MicroFont);
		SetContextForeGroundColor (textColors[scan]);
		font_DrawText (&t);

		SetContext (ScanContext);

		// Draw a virgin surface
		BatchGraphics ();
		DrawPlanet (0, BLACK_COLOR);
		UnbatchGraphics ();

		tintColor = tintColors[scan];

		// Draw the scan slowly line by line
		TimeOut = GetTimeCounter ();
		for (i = 0; i < SCAN_LINES; i++)
		{
			TimeOut += SCAN_LINE_WAIT;
			if (WaitForAnyButtonUntil (TRUE, TimeOut, FALSE))
				break;

			BatchGraphics ();
			DrawPlanet (i, tintColor);
			DrawScannedStuff (i, scan);
			UnbatchGraphics ();
#ifdef SPIN_ON_SCAN
			RotatePlanetSphere (TRUE);
#endif
		}

		if (i < SCAN_LINES)
		{	// Aborted by a keypress; draw in finished state
			BatchGraphics ();
			DrawPlanet (SCAN_LINES - 1, tintColor);
			DrawScannedStuff (SCAN_LINES - 1, scan);
			UnbatchGraphics ();
		}
	}

	SetContext (PlanetContext);
	RepairBackRect (&r);

	SetContext (ScanContext);
	if (scanType == AUTO_SCAN)
	{	// clear the last scan
		DrawPlanet (0, BLACK_COLOR);
		DrawScannedObjects (FALSE);
	}

	FlushInput ();
}

static BOOLEAN
DoScan(MENU_STATE *pMS)
{
    auto &system = *static_cast<SOLARSYS_STATE *>(pMS->privData);
	bool select, cancel;

	select = PulsedInputState.menu[KEY_MENU_SELECT];
	cancel = PulsedInputState.menu[KEY_MENU_CANCEL];
	
	if (GLOBAL (CurrentActivity) & CHECK_ABORT)
		return FALSE;

	if (cancel || (select && pMS->CurState == EXIT_SCAN))
	{
		return FALSE;
	}
	else if (select)
	{
		if (pMS->CurState == DISPATCH_SHUTTLE)
		{
			COUNT fuel_required;

			if ((uqm::detail::selectedDescriptor(system).data_index & PLANET_SHIELDED)
					|| (system.SysInfo.PlanetInfo.AtmoDensity ==
						GAS_GIANT_ATMOSPHERE))
			{	// cannot dispatch to shielded planets or gas giants
				PlayMenuSound (MENU_SOUND_FAILURE);
				return TRUE;
			}

			fuel_required = getLandingFuelNeeded(system);
			if (GLOBAL_SIS (FuelOnBoard) < fuel_required
					|| GLOBAL_SIS (NumLanders) == 0
					|| GLOBAL_SIS (CrewEnlisted) == 0)
			{
				PlayMenuSound (MENU_SOUND_FAILURE);
				return TRUE;
			}

			SetFlashRect (NULL);

			if (!PickPlanetSide(system))
				return FALSE;

			DrawMenuStateStrings (PM_MIN_SCAN, pMS->CurState);
			SetFlashRect (SFR_MENU_3DO);

			return TRUE;
		}

		// Various scans
		if (uqm::detail::selectedDescriptor(system).data_index & PLANET_SHIELDED)
		{	// cannot scan shielded planets
			PlayMenuSound (MENU_SOUND_FAILURE);
			return TRUE;
		}

		ScanPlanet (pMS->CurState);
		if (pMS->CurState == AUTO_SCAN)
		{
			pMS->CurState = DISPATCH_SHUTTLE;
			DrawMenuStateStrings (PM_MIN_SCAN, pMS->CurState);
		}
	}
	else if (optWhichMenu == OPT_PC ||
			(!(uqm::detail::selectedDescriptor(system).data_index & PLANET_SHIELDED)
			&& system.SysInfo.PlanetInfo.AtmoDensity !=
				GAS_GIANT_ATMOSPHERE))
	{
		DoMenuChooser (pMS, PM_MIN_SCAN);
	}

	return TRUE;
}

static CONTEXT
CreateScanContext (void)
{
	CONTEXT oldContext;
	CONTEXT context;
	RECT r;

	// ScanContext rect is relative to SpaceContext
	oldContext = SetContext (SpaceContext);
	GetContextClipRect (&r);

	context = CreateContext ("ScanContext");
	SetContext (context);
	SetContextFGFrame (Screen);
	r.corner.x += r.extent.width - MAP_WIDTH;
	r.corner.y += r.extent.height - MAP_HEIGHT;
	r.extent.width = MAP_WIDTH;
	r.extent.height = MAP_HEIGHT;
	SetContextClipRect (&r);

	SetContext (oldContext);

	return context;
}

CONTEXT
GetScanContext (BOOLEAN *owner)
{
	// TODO: Make CONTEXT ref-counted
	if (ScanContext)
	{
		if (owner)
			*owner = FALSE;
	}
	else
	{
		if (owner)
			*owner = TRUE;
		ScanContext = CreateScanContext ();
	}
	return ScanContext;
}

void
DestroyScanContext (void)
{
	if (ScanContext)
	{
		DestroyContext (ScanContext);
		ScanContext = NULL;
	}
}

void
uqm::runScan(SOLARSYS_STATE &system)
{
    auto &cursor = system.worlds->scan;
    uqm::ExplorationViewScope view{*system.worlds, uqm::ExplorationView::Scan};
	struct CursorCleanup {
        uqm::SurfaceScanState &cursor;
        ~CursorCleanup() {
            DestroyDrawable(ReleaseDrawable(cursor.eraseFrame));
            cursor.eraseFrame = nullptr;
        }
    } cleanup{cursor};
    MENU_STATE MenuState{};
    MenuState.privData = &system;

	GetScanContext (NULL);

	if (optWhichMenu == OPT_3DO &&
			((uqm::detail::selectedDescriptor(system).data_index & PLANET_SHIELDED)
			|| system.SysInfo.PlanetInfo.AtmoDensity ==
				GAS_GIANT_ATMOSPHERE))
	{
		MenuState.CurState = EXIT_SCAN;
	}
	else
	{
		MenuState.CurState = AUTO_SCAN;
		cursor.location.x = (MAP_WIDTH >> 1) << MAG_SHIFT;
		cursor.location.y = (MAP_HEIGHT >> 1) << MAG_SHIFT;

		initPlanetLocationImage(system);
		SetContext (ScanContext);
		DrawScannedObjects (FALSE);
	}

	DrawMenuStateStrings (PM_MIN_SCAN, MenuState.CurState);
	SetFlashRect (SFR_MENU_3DO);

	if (optWhichCoarseScan == OPT_PC)
		PrintCoarseScanPC(system);
	else
		PrintCoarseScan3DO(system);

	SetMenuSounds (MENU_SOUND_ARROWS, MENU_SOUND_SELECT);

	MenuState.InputFunc = DoScan;
	DoInput (&MenuState, FALSE);

	SetFlashRect (NULL);

	// cleanup scan graphics
	BatchGraphics ();
	SetContext (ScanContext);
	DrawPlanet (0, BLACK_COLOR);
	EraseCoarseScan(system);
	UnbatchGraphics ();


}

static void
generateBioNode (SOLARSYS_STATE *system, ELEMENT *NodeElementPtr,
		BYTE *life_init_tab, COUNT creatureType)
{
	COUNT i;

	// NOTE: TFB_Random() calls here are NOT part of the deterministic planet
	//   generation PRNG flow.
	if (CreatureData[creatureType].Attributes & SPEED_MASK)
	{
		// Place moving creatures at a random location.
		i = TFB_Random ();
		NodeElementPtr->current.location.x =
				(LOBYTE (i) % (MAP_WIDTH - (8 << 1))) + 8;
		NodeElementPtr->current.location.y =
				(HIBYTE (i) % (MAP_HEIGHT - (8 << 1))) + 8;
	}

	if (system->graphics->PlanetSideFrame[0] == 0)
		system->graphics->PlanetSideFrame[0] =
				CaptureDrawable (LoadGraphic (CANNISTER_MASK_PMAP_ANIM));

	for (i = 0; i < MAX_LIFE_VARIATION
			&& life_init_tab[i] != (BYTE)(creatureType + 1);
			++i)
	{
		if (life_init_tab[i] != 0)
			continue;

		life_init_tab[i] = (BYTE)creatureType + 1;

		system->graphics->PlanetSideFrame[i + 3] = load_life_form (creatureType);
		break;
	}

	NodeElementPtr->mass_points = (BYTE)creatureType;
	NodeElementPtr->hit_points = HINIBBLE (
			CreatureData[creatureType].ValueAndHitPoints);
	DisplayArray[NodeElementPtr->PrimIndex].
			Object.Stamp.frame = SetAbsFrameIndex (
			system->graphics->PlanetSideFrame[i + 3], (COUNT)TFB_Random ());
}

void
uqm::generatePlanetSide(SOLARSYS_STATE &system)
{
	SIZE scan;
	BYTE life_init_tab[MAX_LIFE_VARIATION];
			// life_init_tab is filled with the creature types of already
			// selected creatures. If an entry is 0, none has been selected
			// yet, otherwise, it is 1 more than the creature type.

	InitDisplayList ();
	if (uqm::detail::selectedDescriptor(system).data_index & PLANET_SHIELDED)
		return;

	memset (life_init_tab, 0, sizeof life_init_tab);

	for (scan = BIOLOGICAL_SCAN; scan >= MINERAL_SCAN; --scan)
	{
		COUNT num_nodes;
		FRAME f;

		f = SetAbsFrameIndex (MiscDataFrame,
				NUM_SCANDOT_TRANSITIONS * (scan - ENERGY_SCAN));

		num_nodes = callGenerateForScanType (&system,
				&uqm::detail::selectedDescriptor(system), GENERATE_ALL, scan, NULL);

		while (num_nodes--)
		{
			HELEMENT hNodeElement;
			ELEMENT *NodeElementPtr;
			NODE_INFO info;

			if (isNodeRetrieved (&system.SysInfo.PlanetInfo,
					scan, num_nodes))
				continue;

			hNodeElement = AllocElement ();
			if (!hNodeElement)
				continue;

			LockElement (hNodeElement, &NodeElementPtr);

			callGenerateForScanType (&system,
					&uqm::detail::selectedDescriptor(system), num_nodes,
					scan, &info);

			NodeElementPtr->scan_node = MAKE_WORD (scan, num_nodes + 1);
			NodeElementPtr->playerNr = PS_NON_PLAYER;
			NodeElementPtr->current.location.x = info.loc_pt.x;
			NodeElementPtr->current.location.y = info.loc_pt.y;

			SetPrimType (&DisplayArray[NodeElementPtr->PrimIndex], STAMP_PRIM);
			if (scan == MINERAL_SCAN)
			{
				NodeElementPtr->turn_wait = info.type;
				NodeElementPtr->mass_points = HIBYTE (info.density);
				NodeElementPtr->current.image.frame = SetAbsFrameIndex (
						MiscDataFrame, (NUM_SCANDOT_TRANSITIONS * 2)
						+ ElementCategory (info.type) * 5);
				NodeElementPtr->next.image.frame = SetRelFrameIndex (
						NodeElementPtr->current.image.frame,
						LOBYTE (info.density) + 1);
				DisplayArray[NodeElementPtr->PrimIndex].Object.Stamp.frame =
						IncFrameIndex (NodeElementPtr->next.image.frame);
			}
			else  /* (scan == BIOLOGICAL_SCAN || scan == ENERGY_SCAN) */
			{
				NodeElementPtr->current.image.frame = f;
				NodeElementPtr->next.image.frame = SetRelFrameIndex (
						f, NUM_SCANDOT_TRANSITIONS - 1);
				NodeElementPtr->turn_wait = MAKE_BYTE (4, 4);
				NodeElementPtr->preprocess_func = object_animation;
				if (scan == ENERGY_SCAN)
				{
					NodeElementPtr->mass_points = MAX_SCROUNGED;
					DisplayArray[NodeElementPtr->PrimIndex].Object.Stamp.frame =
							system.graphics->PlanetSideFrame[1];
				}
				else /* (scan == BIOLOGICAL_SCAN) */
				{
					generateBioNode (&system, NodeElementPtr,
							life_init_tab, info.type);
				}
			}

			NodeElementPtr->next.location.x =
					NodeElementPtr->current.location.x << MAG_SHIFT;
			NodeElementPtr->next.location.y =
					NodeElementPtr->current.location.y << MAG_SHIFT;
			UnlockElement (hNodeElement);

			PutElement (hNodeElement);
		}
	}
}

bool
isNodeRetrieved (const PLANET_INFO *planetInfo, BYTE scanType, BYTE nodeNr)
{
	return (planetInfo->ScanRetrieveMask[scanType] & ((DWORD) 1 << nodeNr))
			!= 0;
}

COUNT
countNodesRetrieved (const PLANET_INFO *planetInfo, BYTE scanType)
{
	COUNT count;
	DWORD mask = planetInfo->ScanRetrieveMask[scanType];

	// count the number of bits set
	// Caution: 'mask' must be unsigned
	for (count = 0; mask != 0; mask >>= 1)
	{
		if (mask & 1)
			++count;
	}
	return count;
}

void
setNodeRetrieved (PLANET_INFO *planetInfo, BYTE scanType, BYTE nodeNr)
{
	planetInfo->ScanRetrieveMask[scanType] |= ((DWORD) 1 << nodeNr);
}

void uqm::redrawExploration(SOLARSYS_STATE &system, ExplorationView view) {
    if (view == ExplorationView::Flight || !system.graphics->TopoFrame) return;
    struct ContextRestore {
        CONTEXT previous;
        ~ContextRestore() { SetContext(previous); }
    } restore{SetContext(PlanetContext)};
    BatchGraphics();
    uqm::drawStarBackground(system);
    DrawDefaultPlanetSphere();
    DrawPlanetSurfaceBorder();
    uqm::redrawSurfaceScan(system, nullptr);
    UnbatchGraphics();
    if (view == ExplorationView::Scan) {
        if (optWhichCoarseScan == OPT_PC) PrintCoarseScanPC(system);
        else PrintCoarseScan3DO(system);
    }
}

// Remaining C planet-loop entry points resolve context only at the boundary.
void ScanSystem(void) { uqm::runScan(*pSolarSysState); }
void GeneratePlanetSide(void) { uqm::generatePlanetSide(*pSolarSysState); }

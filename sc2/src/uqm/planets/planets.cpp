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

#include "planets.h"
#include "../system_runtime.hpp"
#include "../world_visit.hpp"
#include "../orbit_runtime.hpp"
#include "../sis_legacy.hpp"
#include "../world_legacy.hpp"
#include "../planet_render.hpp"
#include "../planet_sphere.hpp"
#include "../device_menu.hpp"
#include <utility>
#include "../orbit_presentation.hpp"

#include "scan.h"
#include "lander.h"
#include "../colors.h"
#include "../element.h"
#include "../settings.h"
#include "../controls.h"
#include "../sounds.h"
#include "../gameopt.h"
#include "../shipcont.h"
#include "../setup.h"
#include "../uqmdebug.h"
#include "../resinst.h"
#include "../nameref.h"
#include "options.h"
#include "libs/graphics/gfx_common.h"


namespace {
thread_local PLANET_RENDER_DATA *orbitCallbackGraphics = nullptr;
// Legacy input callbacks have no userdata; retain the existing callback API,
// but restore its binding on every normal, cancellation and exception exit.
class InputCallbackScope {
public:
    explicit InputCallbackScope(InputFrameCallback *callback) : previous_(SetInputCallback(callback)) {}
    ~InputCallbackScope() { SetInputCallback(previous_); }
    InputCallbackScope(const InputCallbackScope &) = delete;
    InputCallbackScope &operator=(const InputCallbackScope &) = delete;
private:
    InputFrameCallback *previous_;
};
}

// Orbital menu items
enum PlanetMenuItems
{
	// XXX: Must match the enum in menustat.h
	SCAN = 0,
	STARMAP,
	EQUIP_DEVICE,
	CARGO,
	ROSTER,
	GAME_MENU,
	NAVIGATION,
};

void
DrawScannedObjects (BOOLEAN Reversed)
{
	HELEMENT hElement, hNextElement;

	for (hElement = Reversed ? GetTailElement () : GetHeadElement ();
			hElement; hElement = hNextElement)
	{
		ELEMENT *ElementPtr;

		LockElement (hElement, &ElementPtr);
		hNextElement = Reversed ?
				GetPredElement (ElementPtr) :
				GetSuccElement (ElementPtr);

		if (ElementPtr->state_flags & APPEARING)
		{
			STAMP s;

			s.origin = ElementPtr->current.location;
			s.frame = ElementPtr->next.image.frame;
			DrawStamp (&s);
		}

		UnlockElement (hElement);
	}
}

void
DrawPlanetSurfaceBorder (void)
{
	CONTEXT oldContext;
	RECT oldClipRect;
	RECT clipRect;
	RECT r;

	oldContext = SetContext (SpaceContext);
	GetContextClipRect (&oldClipRect);

	// Expand the context clip-rect so that we can tweak the existing border
	clipRect = oldClipRect;
	clipRect.corner.x -= 1;
	clipRect.extent.width += 2;
	clipRect.extent.height += 1;
	SetContextClipRect (&clipRect);

	BatchGraphics ();

	// Border bulk
	SetContextForeGroundColor (
			BUILD_COLOR (MAKE_RGB15 (0x0A, 0x0A, 0x0A), 0x08));
	r.corner.x = 0;
	r.corner.y = clipRect.extent.height - MAP_HEIGHT - MAP_BORDER_HEIGHT;
	r.extent.width = clipRect.extent.width;
	r.extent.height = MAP_BORDER_HEIGHT - 2;
	DrawFilledRectangle (&r);

	SetContextForeGroundColor (SIS_BOTTOM_RIGHT_BORDER_COLOR);
	
	// Border top shadow line
	r.extent.width -= 1;
	r.extent.height = 1;
	r.corner.x = 1;
	r.corner.y -= 1;
	DrawFilledRectangle (&r);
	
	// XXX: We will need bulk left and right rects here if MAP_WIDTH changes

	// Right shadow line
	r.extent.width = 1;
	r.extent.height = MAP_HEIGHT + 2;
	r.corner.y += MAP_BORDER_HEIGHT - 1;
	r.corner.x = clipRect.extent.width - 1;
	DrawFilledRectangle (&r);

	SetContextForeGroundColor (SIS_LEFT_BORDER_COLOR);
	
	// Left shadow line
	r.corner.x -= MAP_WIDTH + 1;
	DrawFilledRectangle (&r);

	// Border bottom shadow line
	r.extent.width = MAP_WIDTH + 2;
	r.extent.height = 1;
	DrawFilledRectangle (&r);
	
	UnbatchGraphics ();

	SetContextClipRect (&oldClipRect);
	SetContext (oldContext);
}

typedef enum
{
	DRAW_ORBITAL_FULL,
	DRAW_ORBITAL_WAIT,
	DRAW_ORBITAL_UPDATE,

} DRAW_ORBITAL_MODE;

static void
DrawOrbitalDisplay (SOLARSYS_STATE &system, DRAW_ORBITAL_MODE Mode)
{
	RECT r;

	SetContext (SpaceContext);
	GetContextClipRect (&r);

	BatchGraphics ();
	
	if (Mode != DRAW_ORBITAL_UPDATE)
	{
		SetTransitionSource (NULL);

		DrawSISFrame ();
		DrawSISMessage (NULL);
		DrawSISTitle (uqm::detail::SISAccess::record(uqm::systemVisit(system).session().player().sis()).PlanetName);
		uqm::drawStarBackground(system);
		DrawPlanetSurfaceBorder ();
	}

	if (Mode == DRAW_ORBITAL_WAIT)
	{
		STAMP s;

		SetContext (uqm::systemVisit(system).presentation().scanContext());
		s.frame = CaptureDrawable (LoadGraphic (ORBENTER_PMAP_ANIM));
		s.origin.x = -SAFE_X;
		s.origin.y = 0;
		DrawStamp (&s);
		DestroyDrawable (ReleaseDrawable (s.frame));
	}
	else if (Mode == DRAW_ORBITAL_FULL)
	{
		uqm::DrawDefaultPlanetSphere (*system.graphics);
	}

	if (Mode != DRAW_ORBITAL_WAIT)
	{
		SetContext (uqm::systemVisit(system).presentation().scanContext());
		uqm::DrawPlanet (*system.graphics, 0, BLACK_COLOR);
	}

	if (Mode != DRAW_ORBITAL_UPDATE)
	{
		ScreenTransition (3, &r);
	}

	UnbatchGraphics ();

	// for later RepairBackRect()
	LoadIntoExtraScreen (&r);
}

// Initialise the surface graphics, and start the planet music.
// Called by the native WorldVisit
// (when orbit is entered; either from IP, or from loading a saved game)
// and when "starmap" is selected from orbit and then cancelled;
// also after in-orbit comm and after defeating planet guards in combat.
// SurfDefFrame contains surface definition images when a planet comes
// with its own bitmap (currently only for Earth)
void
uqm::loadPlanetSurface(SOLARSYS_STATE &system, World &world, FRAME SurfDefFrame)
{
    SystemFrame source{SurfDefFrame};
	bool WaitMode = !(uqm::systemVisit(system).session().activities().last & CHECK_LOAD);
	auto &body = uqm::detail::worldDescriptor(system, world);

	assert (systemLocation(system).orbit && !system.graphics->TopoFrame);

	if (WaitMode)
	{
		DrawOrbitalDisplay (system, DRAW_ORBITAL_WAIT);
	}

	StopMusic ();

	world.renderResources().createSurface(system, body, source.release());
	uqm::generatePlanetSide(system);

    uqm::systemVisit(system).presentation().playMusic(body.data_index & ~PLANET_SHIELDED);

	if (WaitMode)
	{
		uqm::ZoomInPlanetSphere (*system.graphics);
		DrawOrbitalDisplay (system, DRAW_ORBITAL_UPDATE);
	}
	else
	{
		DrawOrbitalDisplay (system, DRAW_ORBITAL_FULL);
	}
}

static BOOLEAN
DoPlanetOrbit (MENU_STATE *pMS)
{
    auto &runtime = *static_cast<uqm::OrbitRuntime *>(pMS->privData);
    auto &system = runtime.system();
    auto &activity = runtime.campaign().activity;
	bool select = PulsedInputState.menu[KEY_MENU_SELECT] != 0;
	BOOLEAN handled;

	if ((activity & (CHECK_ABORT | CHECK_LOAD))
			|| runtime.session().player().sis().crew() == (COUNT)~0)
		return FALSE;

	// The menu callback borrows the active orbital menu state.
	handled = DoMenuChooser (pMS, PM_SCAN);
	if (handled)
		return TRUE;

	if (!select)
		return TRUE;

	SetFlashRect (NULL);

	switch (pMS->CurState)
	{
		case SCAN:
			uqm::runScan(system);
			if (activity & START_ENCOUNTER)
			{	// Found Fwiffo on Pluto
				return FALSE;
			}
			break;
		case EQUIP_DEVICE:
			select = uqm::DeviceMenu{uqm::systemStory(system), system}.run();
			if (activity & START_ENCOUNTER)
			{	// Invoked Talking Pet, a Caster or Sun Device over Chmmr,
				// or a Caster for Ilwrath
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
		{
			BOOLEAN AutoPilotSet;
            {
                InputCallbackScope pauseRotation{nullptr};
                RepairSISBorder();
                AutoPilotSet = StarMap();
                if (activity & CHECK_ABORT) return FALSE;
            } // Restore rotation before redrawing the orbital view.

			if (!AutoPilotSet)
			{	// Redraw the orbital display
				DrawOrbitalDisplay (system, DRAW_ORBITAL_FULL);
				break;
			}
			// Fall through !!!
		}
		case NAVIGATION:
			return FALSE;
	}

	if (!(activity & CHECK_ABORT))
	{
		if (select)
		{	// 3DO menu jumps to NAVIGATE after a successful submenu run
			if (optWhichMenu != OPT_PC)
				pMS->CurState = NAVIGATION;
			DrawMenuStateStrings (PM_SCAN, pMS->CurState);
		}
		SetFlashRect (SFR_MENU_3DO);
	}

	return TRUE;
}

static void
on_input_frame (void)
{
	// Legacy input callback has no userdata; resolve the active graphics once.
    assert(orbitCallbackGraphics);
    uqm::RotatePlanetSphere (*orbitCallbackGraphics, true);
}

void
uqm::OrbitRuntime::run()
{
    auto &system = system_;
	MENU_STATE MenuState;

	memset (&MenuState, 0, sizeof MenuState);

	DrawMenuStateStrings (PM_SCAN, SCAN);
	SetFlashRect (SFR_MENU_3DO);

    MenuState.privData = this;
	MenuState.CurState = SCAN;
	SetMenuSounds (MENU_SOUND_ARROWS, MENU_SOUND_SELECT);
    struct GraphicsBorrow {
        PLANET_RENDER_DATA *previous;
        ~GraphicsBorrow() { orbitCallbackGraphics = previous; }
    } graphicsBorrow{std::exchange(orbitCallbackGraphics, system.graphics)};
    InputCallbackScope rotation{on_input_frame};

	MenuState.InputFunc = DoPlanetOrbit;
	DoInput (&MenuState, TRUE);


	SetFlashRect (NULL);
	DrawMenuStateStrings (PM_STARMAP, -NAVIGATION);
}

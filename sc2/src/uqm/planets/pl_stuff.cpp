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
#include "planets.h"
#include "../planet_sphere.hpp"
#include "../colors.h"
#include "../setup.h"
#include "libs/graphics/gfx_common.h"
#include "libs/graphics/drawable.h"
#include "libs/mathlib.h"
#include "scan.h"
#include "options.h"

#include <math.h>


// define USE_ADDITIVE_SCAN_BLIT to use additive blittting
// instead of transparency for the planet scans.
// It still doesn't look right though (it is too bright)
#define USE_ADDITIVE_SCAN_BLIT

namespace uqm {
// Draw the planet sphere and any extra graphic (like a shield) if present
void
DrawPlanetSphere (PLANET_RENDER_DATA &graphics, int x, int y)
{
	STAMP s;
	PLANET_ORBIT *Orbit = &graphics.Orbit;

	s.origin.x = x;
	s.origin.y = y;

	BatchGraphics ();
	s.frame = Orbit->SphereFrame;
	DrawStamp (&s);
	if (Orbit->ObjectFrame)
	{
		s.frame = Orbit->ObjectFrame;
		DrawStamp (&s);
	}
	UnbatchGraphics ();
}

void
DrawDefaultPlanetSphere (PLANET_RENDER_DATA &graphics)
{
	CONTEXT oldContext;

	oldContext = SetContext (PlanetContext);
	DrawPlanetSphere (graphics, SIS_SCREEN_WIDTH / 2, PLANET_ORG_Y);
	SetContext (oldContext);
}

void
InitSphereRotation (PLANET_RENDER_DATA &graphics, int direction, bool shielded)
{
	PLANET_ORBIT *Orbit = &graphics.Orbit;

	graphics.sphere->direction = direction;
	graphics.sphere->point = 0;
    graphics.sphere->nextFrame = 0;
	graphics.sphere->throb = shielded && optWhichShield == OPT_3DO;

	if (graphics.sphere->throb)
	{
		// ObjectFrame must contain the shield graphic
		Orbit->WorkFrame = Orbit->ObjectFrame;
		// We need a scratch frame so that we can apply throbbing
		// to the shield, so create one
		Orbit->ObjectFrame = CaptureDrawable (CreateDrawable (
				WANT_PIXMAP | WANT_ALPHA,
				GetFrameWidth (Orbit->ObjectFrame),
				GetFrameHeight (Orbit->ObjectFrame), 2));
	}

	// Render the first sphere/shield frame
	// Prepare will set the next one
	graphics.sphere->frame = 1;
	PrepareNextRotationFrame (graphics);
}

void
UninitSphereRotation (PLANET_ORBIT &orbit)
{
    auto *Orbit = &orbit;

	if (Orbit->WorkFrame)
	{
		DestroyDrawable (ReleaseDrawable (Orbit->ObjectFrame));
		Orbit->ObjectFrame = Orbit->WorkFrame;
		Orbit->WorkFrame = NULL;
	}
}

void
PrepareNextRotationFrame (PLANET_RENDER_DATA &graphics)
{
	PLANET_ORBIT *Orbit = &graphics.Orbit;

	// Generate the next rotation frame
	// We alternate between the frames because we do not call FlushGraphics()
	// The frame we just drew may not have made it to the screen yet
    graphics.sphere->advance(MAP_WIDTH);

	// prepare the next sphere frame
	Orbit->SphereFrame = SetAbsFrameIndex (Orbit->SphereFrame, graphics.sphere->frame);
	RenderPlanetSphere (graphics, Orbit->SphereFrame, graphics.sphere->point, graphics.sphere->throb);
	
	if (graphics.sphere->throb)
	{	// prepare the next shield throb frame
		Orbit->ObjectFrame = SetAbsFrameIndex (Orbit->ObjectFrame,
				graphics.sphere->frame);
		SetShieldThrobEffect (graphics, Orbit->WorkFrame, graphics.sphere->point,
				Orbit->ObjectFrame);
	}
}

#define ZOOM_RATE  24
#define ZOOM_TIME  (ONE_SECOND * 6 / 5)

// This takes care of zooming the planet sphere into place
// when entering orbit
void
ZoomInPlanetSphere (PLANET_RENDER_DATA &graphics)
{
	PLANET_ORBIT *Orbit = &graphics.Orbit;
	const int base = GSCALE_IDENTITY;
	int dx, dy;
	int oldScale;
	int oldMode;
	int i;
	int frameCount;
	int zoomCorner;
	RECT frameRect;
	RECT repairRect;
	TimeCount NextTime;

	frameCount = ZOOM_TIME / (ONE_SECOND / ZOOM_RATE);

	// Planet zoom in from a randomly chosen corner
	zoomCorner = TFB_Random ();
	dx = 1 - (zoomCorner & 1) * 2;
	dy = 1 - (zoomCorner & 2);

	if (Orbit->ObjectFrame)
		GetFrameRect (Orbit->ObjectFrame, &frameRect);
	else
		GetFrameRect (Orbit->SphereFrame, &frameRect);
	repairRect = frameRect;

	for (i = 0; i <= frameCount; ++i)
	{
		double scale;
		POINT pt;

		NextTime = GetTimeCounter () + (ONE_SECOND / ZOOM_RATE);

		// Use 1 + e^-2 - e^(-2x / frameCount)) function to get a decelerating
		// zoom like the one 3DO does (supposedly)
		if (i < frameCount)
			scale = 1.134 - exp (-2.0 * i / frameCount);
		else
			scale = 1.0; // final frame

		// start from beyond the screen
		pt.x = SIS_SCREEN_WIDTH / 2 + (int) (dx * (1.0 - scale)
				* (SIS_SCREEN_WIDTH * 6 / 10) + 0.5);
		pt.y = PLANET_ORG_Y + (int) (dy * (1.0 - scale)
				* (SCAN_SCREEN_HEIGHT * 6 / 10) + 0.5);

		SetContext (PlanetContext);

		BatchGraphics ();
		if (i > 0)
			RepairBackRect (&repairRect);

		oldMode = SetGraphicScaleMode (TFB_SCALE_BILINEAR);
		oldScale = SetGraphicScale ((int)(base * scale + 0.5));
		DrawPlanetSphere (graphics, pt.x, pt.y);
		SetGraphicScale (oldScale);
		SetGraphicScaleMode (oldMode);

		UnbatchGraphics ();

		repairRect.corner.x = pt.x + frameRect.corner.x;
		repairRect.corner.y = pt.y + frameRect.corner.y;

		PrepareNextRotationFrame (graphics);

		SleepThreadUntil (NextTime);
	}
}

void
RotatePlanetSphere (PLANET_RENDER_DATA &graphics, bool keepRate)
{
	auto &NextTime = graphics.sphere->nextFrame;
	TimeCount Now = GetTimeCounter ();

	if (keepRate && Now < NextTime)
		return; // not time yet

	NextTime = Now + PLANET_ROTATION_RATE;
	DrawDefaultPlanetSphere (graphics);

	PrepareNextRotationFrame (graphics);
}

static void
renderTintFrame (PLANET_RENDER_DATA &graphics, Color tintColor)
{
	PLANET_ORBIT *Orbit = &graphics.Orbit;
	CONTEXT oldContext;
	DrawMode mode, oldMode;
	STAMP s;
	RECT r;

	oldContext = SetContext (OffScreenContext);
	SetContextFGFrame (Orbit->TintFrame);
	SetContextClipRect (NULL);
	// get the rect of the whole context (or our frame really)
	GetContextClipRect (&r);

	// copy the topo frame to the tint frame
	s.origin.x = 0;
	s.origin.y = 0;
	s.frame = graphics.TopoFrame;
	DrawStamp (&s);

	// apply the tint
#ifdef USE_ADDITIVE_SCAN_BLIT
	mode = MAKE_DRAW_MODE (DRAW_ADDITIVE, DRAW_FACTOR_1 / 2);
#else
	mode = MAKE_DRAW_MODE (DRAW_ALPHA, DRAW_FACTOR_1 / 2);	
#endif
	oldMode = SetContextDrawMode (mode);
	SetContextForeGroundColor (tintColor);
	DrawFilledRectangle (&r);
	SetContextDrawMode (oldMode);

	SetContext (oldContext);
}

// tintColor.a is ignored
void
DrawPlanet (PLANET_RENDER_DATA &graphics, int tintY, Color tintColor)
{
	STAMP s;
	PLANET_ORBIT *Orbit = &graphics.Orbit;

	s.origin.x = 0;
	s.origin.y = 0;
	
	BatchGraphics ();
	if (sameColor (tintColor, BLACK_COLOR))
	{	// no tint -- just draw the surface
		s.frame = graphics.TopoFrame;
		DrawStamp (&s);
	}
	else
	{	// apply different scan type tints
		FRAME tintFrame = Orbit->TintFrame;
		int height = GetFrameHeight (tintFrame);

		if (!sameColor (tintColor, Orbit->TintColor))
		{
			renderTintFrame (graphics, tintColor);
			Orbit->TintColor = tintColor;
		}
		
		if (tintY < height - 1)
		{	// untinted piece showing, draw regular topo
			s.frame = graphics.TopoFrame;
			DrawStamp (&s);
		}

		if (tintY >= 0)
		{	// tinted piece showing, draw tinted piece
			RECT oldClipRect;
			RECT clipRect;

			// adjust cliprect to confine the tint
			GetContextClipRect (&oldClipRect);
			clipRect = oldClipRect;
			clipRect.extent.height = tintY + 1;
			SetContextClipRect (&clipRect);
			s.frame = tintFrame;
			DrawStamp (&s);
			SetContextClipRect (&oldClipRect);
		}
	}
	UnbatchGraphics ();
}


} // namespace uqm

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

#ifndef UQM_UNITS_H_
#define UQM_UNITS_H_

#include "libs/gfxlib.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Fixed constants below are shared enum values for C compatibility. Geometry
 * macros that depend on ScreenWidth/ScreenHeight remain runtime expressions. */
extern int ScreenWidth;
extern int ScreenHeight;

#define SCREEN_WIDTH ScreenWidth
#define SCREEN_HEIGHT ScreenHeight
enum { SAFE_X = 0 };
		/* Left and right screen margin to be left unused */
enum { SAFE_Y = 0 };
		/* Top and bottom screen margin to be left unused */
enum { SIS_ORG_X = (7 + SAFE_X) };
enum { SIS_ORG_Y = (10 + SAFE_Y) };
enum { STATUS_WIDTH = 64 };
		/* Width of the status "window" (the right part of the screen) */
#define STATUS_HEIGHT (SCREEN_HEIGHT - (SAFE_Y * 2))
		/* Height of the status "window" (the right part of the screen) */
#define SPACE_WIDTH (SCREEN_WIDTH - STATUS_WIDTH - (SAFE_X * 2))
		/* Width of the space "window" (the left part of the screen) */
#define SPACE_HEIGHT (SCREEN_HEIGHT - (SAFE_Y * 2))
		/* Height of the space "window" (the left part of the screen) */
#define SIS_SCREEN_WIDTH (SPACE_WIDTH - 14)
		/* Width of the usable part of the space "window" */
#define SIS_SCREEN_HEIGHT (SPACE_HEIGHT - 13)
		/* Height of the usable part of the space "window" */
#define RADAR_X (4 + (SCREEN_WIDTH - STATUS_WIDTH - SAFE_X))
enum { RADAR_WIDTH = (STATUS_WIDTH - 8) };
enum { RADAR_HEIGHT = 53 };
#define RADAR_Y (SIS_ORG_Y + SIS_SCREEN_HEIGHT - RADAR_HEIGHT)

enum { SIS_TITLE_BOX_WIDTH = 57 };
enum { SIS_TITLE_WIDTH = (SIS_TITLE_BOX_WIDTH - 2) };
enum { SIS_TITLE_HEIGHT = 8 };
enum { SIS_SPACER_BOX_WIDTH = 12 };
#define SIS_MESSAGE_BOX_WIDTH  (SIS_SCREEN_WIDTH - SIS_TITLE_BOX_WIDTH \
			- SIS_SPACER_BOX_WIDTH)
#define SIS_MESSAGE_WIDTH      (SIS_MESSAGE_BOX_WIDTH - 2)
enum { SIS_MESSAGE_HEIGHT = SIS_TITLE_HEIGHT };

enum { STATUS_MESSAGE_WIDTH = (STATUS_WIDTH - 4) };
enum { STATUS_MESSAGE_HEIGHT = 7 };

enum { SHIP_NAME_WIDTH = (STATUS_WIDTH - 4) };
enum { SHIP_NAME_HEIGHT = 7 };

enum { MAX_REDUCTION = 3 };
enum { MAX_VIS_REDUCTION = 2 };
enum { REDUCTION_SHIFT = 1 };
enum { NUM_VIEWS = (MAX_VIS_REDUCTION + 1) };

enum { ZOOM_SHIFT = 8 };
enum { MAX_ZOOM_OUT = (1 << (ZOOM_SHIFT + MAX_REDUCTION - 1)) };

enum { ONE_SHIFT = 2 };
enum { BACKGROUND_SHIFT = 3 };
enum { SCALED_ONE = (1 << ONE_SHIFT) };
#define DISPLAY_TO_WORLD(x) ((x)<<ONE_SHIFT)
#define WORLD_TO_DISPLAY(x) ((x)>>ONE_SHIFT)
#define DISPLAY_ALIGN(x) ((COORD)(x)&~(SCALED_ONE-1))
#define DISPLAY_ALIGN_X(x) ((COORD)((COUNT)(x)%LOG_SPACE_WIDTH)&~(SCALED_ONE-1))
#define DISPLAY_ALIGN_Y(y) ((COORD)((COUNT)(y)%LOG_SPACE_HEIGHT)&~(SCALED_ONE-1))

#define LOG_SPACE_WIDTH \
		(DISPLAY_TO_WORLD (SPACE_WIDTH) << MAX_REDUCTION)
#define LOG_SPACE_HEIGHT \
		(DISPLAY_TO_WORLD (SPACE_HEIGHT) << MAX_REDUCTION)
#define TRANSITION_WIDTH \
		(DISPLAY_TO_WORLD (SPACE_WIDTH) << MAX_VIS_REDUCTION)
#define TRANSITION_HEIGHT \
		(DISPLAY_TO_WORLD (SPACE_HEIGHT) << MAX_VIS_REDUCTION)
		
enum { MAX_X_UNIVERSE = 9999 };
enum { MAX_Y_UNIVERSE = 9999 };
// Due to the added rounding error correction, the maximum logical X and Y
// in Hyperspace cannot go past 999.94999, otherwise the values will be
// rounded up to 1000.0. We do not want that so we subtract half a unit.
#define MAX_X_LOGICAL \
		(UNIVERSE_TO_LOGX (MAX_X_UNIVERSE + 1) - (UNIVERSE_TO_LOGX (1) >> 1) \
			- 1L)
// The Y axis is inverted with respect to the screen Y axis.
// (MAX_Y_UNIVERSE - 1) is really 1 for our purposes.
#define MAX_Y_LOGICAL \
		(UNIVERSE_TO_LOGY (-1) - (UNIVERSE_TO_LOGY (MAX_Y_UNIVERSE - 1) >> 1) \
			- 1L)

enum { SPHERE_RADIUS_INCREMENT = 11 };

enum { MAX_FLEET_STRENGTH = (254 * SPHERE_RADIUS_INCREMENT) };

// XXX: These corrected for the weird screen aspect ratio on DOS
//   In part because of them, hyperflight is slower vertically
enum { UNIT_SCREEN_WIDTH = 63 };
enum { UNIT_SCREEN_HEIGHT = 50 };

// Bug #945: Simplified, these set the speed of SIS in Hyperspace and
//   Quasispace. The ratio between UNIVERSE_UNITS_ and LOG_UNITS_ is
//   what sets the speed, and it should be 1:16 to match the original.
//   The unit factors are reduced to keep the translation math within
//   32 bits. The original math is unnecessarily complex and depends
//   on the screen resolution when it should not.
//   Using the new math will break old savegames.
#ifdef NORMALIZED_HYPERSPACE_SPEED
#define LOG_UNITS_X      ((SDWORD)(UNIVERSE_UNITS_X * 16))
#define LOG_UNITS_Y      ((SDWORD)(UNIVERSE_UNITS_Y * 16))
#define UNIVERSE_UNITS_X (((MAX_X_UNIVERSE + 1) >> 4))
#define UNIVERSE_UNITS_Y (((MAX_Y_UNIVERSE + 1) >> 4))
#else
// Original (and now broken) Hyperspace speed factors
enum { SECTOR_WIDTH = 195 };
enum { SECTOR_HEIGHT = 25 };

#define LOG_UNITS_X      ((SDWORD)(LOG_SPACE_WIDTH >> 4) * SECTOR_WIDTH)
#define LOG_UNITS_Y      ((SDWORD)(LOG_SPACE_HEIGHT >> 4) * SECTOR_HEIGHT)
#define UNIVERSE_UNITS_X (((MAX_X_UNIVERSE + 1) >> 4) * 10)
#define UNIVERSE_UNITS_Y (((MAX_Y_UNIVERSE + 1) >> 4))
#endif

#define ROUNDING_ERROR(div)  ((div) >> 1)

/* Convert logical X to universe coordinates, rounding to the nearest unit.
 * lx must be in the supported logical map range. */
static inline COORD
LOGX_TO_UNIVERSE (const SDWORD lx)
{
	return (COORD) ((lx * UNIVERSE_UNITS_X + ROUNDING_ERROR(LOG_UNITS_X))
			/ LOG_UNITS_X);
}
/* Convert logical Y to the inverted universe Y axis, rounding as above. */
static inline COORD
LOGY_TO_UNIVERSE (const SDWORD ly)
{
	return (COORD) (MAX_Y_UNIVERSE -
			((ly * UNIVERSE_UNITS_Y + ROUNDING_ERROR(LOG_UNITS_Y))
			/ LOG_UNITS_Y));
}
/* Convert an X universe coordinate to logical units. ux is a map coordinate. */
static inline SDWORD
UNIVERSE_TO_LOGX (const COORD ux)
{
	return (ux * LOG_UNITS_X + ROUNDING_ERROR(UNIVERSE_UNITS_X))
			/ UNIVERSE_UNITS_X;
}
/* Convert a Y universe coordinate to logical units on the inverted Y axis. */
static inline SDWORD
UNIVERSE_TO_LOGY (const COORD uy)
{
	return ((MAX_Y_UNIVERSE - uy) * LOG_UNITS_Y
			+ ROUNDING_ERROR(UNIVERSE_UNITS_Y))
			/ UNIVERSE_UNITS_Y;
}

enum { CIRCLE_SHIFT = 6 };
enum { FULL_CIRCLE = (1 << CIRCLE_SHIFT) };
enum { OCTANT_SHIFT = (CIRCLE_SHIFT - 3) };
enum { HALF_CIRCLE = (FULL_CIRCLE >> 1) };
enum { QUADRANT = (FULL_CIRCLE >> 2) };
enum { OCTANT = (FULL_CIRCLE >> 3) };

enum { FACING_SHIFT = 4 };

#define ANGLE_TO_FACING(a) (((a)+(1<<(CIRCLE_SHIFT-FACING_SHIFT-1))) \
										>>(CIRCLE_SHIFT-FACING_SHIFT))
#define FACING_TO_ANGLE(f) ((f)<<(CIRCLE_SHIFT-FACING_SHIFT))

#define NORMALIZE_ANGLE(a) ((COUNT)((a)&(FULL_CIRCLE-1)))
#define NORMALIZE_FACING(f) ((COUNT)((f)&((1 << FACING_SHIFT)-1)))

#define DEGREES_TO_ANGLE(d) NORMALIZE_ANGLE((((d) % 360) * FULL_CIRCLE \
				+ HALF_CIRCLE) / 360)
#define ANGLE_TO_DEGREES(d) (NORMALIZE_ANGLE(d) * 360 / FULL_CIRCLE)

enum { SIN_SHIFT = 14 };
enum { SIN_SCALE = (1 << SIN_SHIFT) };
#define INT_ADJUST(x) ((x)<<SIN_SHIFT)
#define FLT_ADJUST(x) (SIZE)((x)*SIN_SCALE)
#define UNADJUST(x) (SIZE)((x)>>SIN_SHIFT)
#define ROUND(x,y) ((x)+((x)>=0?((y)>>1):-((y)>>1)))

extern SIZE sinetab[];
#define SINVAL(a) sinetab[NORMALIZE_ANGLE(a)]
#define COSVAL(a) SINVAL((a)+QUADRANT)
#define SINE(a,m) ((SIZE)((((long)SINVAL(a))*(long)(m))>>SIN_SHIFT))
#define COSINE(a,m) SINE((a)+QUADRANT,m)
extern COUNT ARCTAN (SIZE delta_x, SIZE delta_y);

#define WRAP_VAL(v,w) ((COUNT)((v)<0?((v)+(w)):((v)>=(w)?((v)-(w)):(v))))
#define WRAP_X(x) WRAP_VAL(x,LOG_SPACE_WIDTH)
#define WRAP_Y(y) WRAP_VAL(y,LOG_SPACE_HEIGHT)
#define WRAP_DELTA_X(dx) ((dx)<0 ? \
				((-(dx)<=LOG_SPACE_WIDTH>>1)?(dx):(LOG_SPACE_WIDTH+(dx))) : \
				(((dx)<=LOG_SPACE_WIDTH>>1)?(dx):((dx)-LOG_SPACE_WIDTH)))
#define WRAP_DELTA_Y(dy) ((dy)<0 ? \
				((-(dy)<=LOG_SPACE_HEIGHT>>1)?(dy):(LOG_SPACE_HEIGHT+(dy))) : \
				(((dy)<=LOG_SPACE_HEIGHT>>1)?(dy):((dy)-LOG_SPACE_HEIGHT)))
#if defined(__cplusplus)
}
#endif

#endif /* UQM_UNITS_H_ */

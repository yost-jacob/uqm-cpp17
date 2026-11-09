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

#ifndef UQM_COMM_H_
#define UQM_COMM_H_

#include "globdata.h"
#include "units.h"
#include "libs/compiler.h"
#include "libs/gfxlib.h"
#include "commglue.h"
		// for CONVERSATION

#if defined(__cplusplus)
extern "C" {
#endif


#define TEXT_X_OFFS 1
#define TEXT_Y_OFFS 1
#define SIS_TEXT_WIDTH (SIS_SCREEN_WIDTH - (TEXT_X_OFFS << 1))


extern COUNT InitCommunication (CONVERSATION which_comm);

#define WAIT_TRACK_ALL  ((COUNT)~0)
extern void AlienTalkSegue (COUNT wait_track);
BOOLEAN getLineWithinWidth(TEXT *pText, const char **startNext,
		SIZE maxWidth, COUNT maxChars);

extern RECT CommWndRect; /* comm window rect */

typedef enum
{
	CIM_CROSSFADE_SPACE,
	CIM_CROSSFADE_WINDOW,
	CIM_CROSSFADE_SCREEN,
	CIM_FADE_IN_SCREEN,

	CIM_DEFAULT = CIM_CROSSFADE_SPACE,
} CommIntroMode;
extern void SetCommIntroMode (CommIntroMode, TimeCount howLong);

extern void EnableTalkingAnim (BOOLEAN enable);

#if defined(__cplusplus)
}
#endif

#endif  /* UQM_COMM_H_ */

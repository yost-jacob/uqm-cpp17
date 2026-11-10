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

#include "uqm_context.hpp"
#include "controls.h"
#include "starcon.h"
#include "uqmdebug.h"
#include "libs/tasklib.h"
#include "libs/inplib.h"
#include "libs/log.h"
#include "libs/graphics/gfx_common.h"
#include "libs/graphics/tfb_draw.h"

volatile int MainExited = FALSE;
#ifdef DEBUG_SLEEP
uint32 mainThreadId;
extern uint32 SDL_ThreadID(void);
#endif

// Executes on the main() thread
void
SignalStopMainThread (void)
{
	GamePaused = FALSE;
	// The gameplay thread projects this request into its own session state.
	QuitPosted = TRUE;
	TaskSwitch ();
}

// Executes on the main() thread
void
ProcessUtilityKeys (void)
{
	if (ImmediateInputState.menu[KEY_ABORT])
	{
		// Request normal session unwinding so owned resources are released.
		QuitPosted = TRUE;
		GamePaused = FALSE;
		return;
	}

	if (ImmediateInputState.menu[KEY_FULLSCREEN])
	{
		int flags = GfxFlags ^ TFB_GFXFLAGS_FULLSCREEN;
		// clear ImmediateInputState so we don't repeat this next frame
		FlushInput ();
		TFB_DrawScreen_ReinitVideo (GraphicsDriver, flags, ScreenWidthActual,
				ScreenHeightActual);
	}

#if defined(DEBUG) || defined(USE_DEBUG_KEY)
	{	// Only call the debug func on the rising edge of
		// ImmediateInputState[KEY_DEBUG] so it does not execute repeatedly.
		// This duplicates the PulsedInputState somewhat, but we cannot
		// use PulsedInputState here because it is meant for another thread.
		static int debugKeyState;

		if (ImmediateInputState.menu[KEY_DEBUG] && debugKeyState == 0)
		{
			debugKeyPressed ();
		}
		debugKeyState = ImmediateInputState.menu[KEY_DEBUG];
	}
#endif  /* DEBUG */
}

// The SDL pump remains in main(). Application/session ownership starts here on
// the gameplay worker until rendering/audio/input thread affinity is migrated.
int
Starcon2Main (void *threadArg)
{
    (void)threadArg;
#ifdef DEBUG_SLEEP
    mainThreadId = SDL_ThreadID();
#endif
    int result;
    {
        uqm::UQM_Context context;
        uqm::Game game;
        result = game.run(context);
    }
    log_showBox(false, false);
    MainExited = TRUE;
    return result;
}

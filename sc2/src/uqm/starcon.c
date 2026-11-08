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

#include <stdlib.h>

#include "comm.h"
#include "battle.h"
#include "fmv.h"
#include "gameev.h"
#include "types.h"
#include "globdata.h"
#include "resinst.h"
#include "restart.h"
#include "starbase.h"
#include "save.h"
#include "setup.h"
#include "master.h"
#include "controls.h"
#include "starcon.h"
#include "game_session_c.h"
#include "clock.h"
		// for GameClockTick()
#include "hyper.h"
		// for SeedUniverse()
#include "planets/planets.h"
		// for ExploreSolarSys()
#include "uqmdebug.h"
#include "libs/tasklib.h"
#include "libs/log.h"
#include "libs/gfxlib.h"
#include "libs/graphics/gfx_common.h"
#include "libs/graphics/tfb_draw.h"
#include "libs/misc.h"

#include "uqmversion.h"
#include "options.h"

volatile int MainExited = FALSE;
#ifdef DEBUG_SLEEP
uint32 mainThreadId;
extern uint32 SDL_ThreadID(void);
#endif

static void
BackgroundInitKernel (DWORD TimeOut)
{
	LoadMasterShipList (TaskSwitch);
	TaskSwitch ();
	InitGameKernel ();

	while ((GetTimeCounter () <= TimeOut) &&
	       !(GLOBAL (CurrentActivity) & CHECK_ABORT))
	{
		UpdateInputState ();
		TaskSwitch ();
	}
}

// Executes on the main() thread
void
SignalStopMainThread (void)
{
	GamePaused = FALSE;
	GLOBAL (CurrentActivity) |= CHECK_ABORT;
	TaskSwitch ();
}

// Executes on the main() thread
void
ProcessUtilityKeys (void)
{
	if (ImmediateInputState.menu[KEY_ABORT])
	{
		log_showBox (false, false);
		exit (EXIT_SUCCESS);
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

/* TODO: Remove these declarations once threading is gone. */
extern int snddriver, soundflags;

int
Starcon2Main (void *threadArg)
{
#ifdef DEBUG_SLEEP
	mainThreadId = SDL_ThreadID();
#endif

#if CREATE_JOURNAL
{
int ac = argc;
char **av = argv;

while (--ac > 0)
{
	++av;
	if ((*av)[0] == '-')
	{
		switch ((*av)[1])
		{
#if CREATE_JOURNAL
			case 'j':
				++create_journal;
				break;
#endif //CREATE_JOURNAL
		}
	}
}
}
#endif // CREATE_JOURNAL

	{
		/* TODO: Put initAudio back in main where it belongs once threading
		 *       is gone.
		 */
		extern sint32 initAudio (sint32 driver, sint32 flags);
		initAudio (snddriver, soundflags);
	}

	if (!LoadKernel (0,0))
	{
		log_add (log_Fatal, "\n  *** FATAL ERROR: Could not load basic content ***\n\nUQM requires at least the base content pack to run properly.");
		log_add (log_Fatal, "This file is typically called uqm-%d.%d.0-content.uqm.  UQM was expecting", UQM_MAJOR_VERSION, UQM_MINOR_VERSION);
		log_add (log_Fatal, "it in the %s/packages directory.", baseContentPath);
		log_add (log_Fatal, "Either your installation did not install the content pack at all, or it\ninstalled it in a different directory.\n\nFix your installation and rerun UQM.\n\n  *******************\n");
		log_showBox (true, true);

		MainExited = TRUE;
		return EXIT_FAILURE;
	}
	log_add (log_Info, "We've loaded the Kernel");

	GLOBAL (CurrentActivity) = 0;
	// show splash and init the kernel in the meantime
	SplashScreen (BackgroundInitKernel);

//	OpenJournal ();
	while (StartGame ())
	{
		UQM_SESSION_EXIT result = RunGameSession (-1);
		while (result.kind == UQM_SESSION_LOAD)
			result = RunGameSession ((int)result.slot);
		if (result.kind == UQM_SESSION_SHUTDOWN)
			break;
	}
//	CloseJournal ();

	UninitGameKernel ();
	FreeMasterShipList ();
	FreeKernel ();

	log_showBox (false, false);
	MainExited = TRUE;

	(void) threadArg;  /* Satisfying compiler (unused parameter) */
	return 0;
}


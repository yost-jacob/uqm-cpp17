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
#include "credits.h"

#include "controls.h"
#include "colors.h"
#include "options.h"
#include "oscill.h"
#include "comm.h"
#include "resinst.h"
#include "nameref.h"
#include "settings.h"
#include "sounds.h"
#include "setup.h"
#include "libs/graphics/drawable.h"
#include <cmath>
#include "game_state.hpp"
#include "mode_rendering.hpp"
#include "libs/inplib.h"
#include <stdexcept>

namespace {
class CreditsRuntime;
thread_local CreditsRuntime *activeCredits = nullptr;
class CreditsRuntime {
public:
    explicit CreditsRuntime(uqm::Session &session)
        : session_(session), drawing_(uqm::suspendDrawingBindings({ScreenContext})), previous_(activeCredits) {
        activeCredits = this;
    }
    ~CreditsRuntime() {
        SetInputCallback(nullptr); // Callback cannot observe released frames.
        SetContext(ScreenContext);
        StopMusic();
        DestroyMusic(music_);
        UninitCredits();
        FreeCredits();
        FadeMusic(NORMAL_VOLUME, 0);
        activeCredits = previous_;
    }
    CreditsRuntime(const CreditsRuntime &) = delete;
    CreditsRuntime &operator=(const CreditsRuntime &) = delete;
private:
    uqm::Session &session_;
    std::unique_ptr<uqm::ModeSuspension> drawing_;
    CreditsRuntime *previous_;
    MUSIC_REF music_ = nullptr;
    TimeCount NextTime = 0;
    RECT window_{};
    bool cancelled() const {
        return QuitPosted || session_.menuExit().has_value() || (session_.activity() & (CHECK_ABORT | CHECK_LOAD));
    }
// Rates in pixel lines per second
static constexpr int CREDITS_BASE_RATE = 9;
static constexpr int CREDITS_MAX_RATE = 130;
// Maximum frame rate
static constexpr int CREDITS_FRAME_RATE = 36;

static constexpr TimeCount CREDITS_TIMEOUT = ONE_SECOND * 5;

inline static constexpr Color TRANS_COLOR = BRIGHT_BLUE_COLOR;

// Positive or negative scroll rate in pixel lines per second
int CreditsRate{};

BOOLEAN OutTakesRunning{};
bool CreditsRunning{};
STRING CreditsTab{};
FRAME CreditsBack{};

// Context used for drawing to the screen
CONTEXT DrawContext{};
// Context used for pre-rendering a credits frame
CONTEXT LocalContext{};
// Pre-rendered frame, possibly with a cutout
FRAME CreditsFrame{};
// Size of the credits "window" (normally screen size)
EXTENT CreditsExtent{};

typedef struct
{
	FRAME frame;
	int strIndex;
} CreditTextFrame;

static constexpr int MAX_CREDIT_FRAMES = 32;
// Circular text frame buffer for scrolling
// Text frames are generated as needed, and when a text frame scrolls
// of the screen, it is destroyed
CreditTextFrame textFrames[MAX_CREDIT_FRAMES]{};
// Index of first active frame in the circular buffer (the first frame
// is the one on top)
int firstFrame{};
// Index of last active frame in the circular buffer + 1
int lastFrame{};
// Total height of all active frames in the circular buffer
int totalHeight{};
// Current vertical offset into the first text frame
int curFrameOfs{};

typedef struct
{
	int size;
	RESOURCE res;
	FONT font;
} FONT_SIZE_DEF;

FONT_SIZE_DEF CreditsFont[4] =
{
	{ 13, PT13AA_FONT, 0 },
	{ 17, PT17AA_FONT, 0 },
	{ 45, PT45AA_FONT, 0 },
	{  0,           0, 0 },
};


FRAME
Credits_MakeTransFrame (int w, int h, Color TransColor)
{
	FRAME OldFrame;
	FRAME f;

	f = CaptureDrawable (CreateDrawable (WANT_PIXMAP, w, h, 1));
    if (!f) throw std::runtime_error("could not create credits frame");
	SetFrameTransparentColor (f, TransColor);

	OldFrame = SetContextFGFrame (f);
	SetContextBackGroundColor (TransColor);
	ClearDrawable ();
	SetContextFGFrame (OldFrame);

	return f;
}

int
ParseTextLines (TEXT *Lines, int MaxLines, char *Buffer)
{
	int i;
	const char* pEnd = Buffer + strlen (Buffer);

	for (i = 0; i < MaxLines && Buffer < pEnd; ++i, ++Lines)
	{
		char* pTerm = strchr (Buffer, '\n');
		if (!pTerm)
			pTerm = Buffer + strlen (Buffer);
		*pTerm = '\0'; /* terminate string */
		Lines->pStr = Buffer;
		Lines->CharCount = ~0;
		Buffer = pTerm + 1;
	}
	return i;
}

static constexpr int MAX_TEXT_LINES = 50;
static constexpr int MAX_TEXT_COLS = 5;

FRAME
Credits_RenderTextFrame (CONTEXT TempContext, int *istr, int dir,
		Color BackColor, Color ForeColor)
{
	FRAME f;
	CONTEXT OldContext;
	FRAME OldFrame;
	TEXT TextLines[MAX_TEXT_LINES];
	char *pStr = NULL;
	int size;
	char salign[32];
	char *scol;
	int scaned;
	int i, rows, cnt;
	char buf[2048];
	FONT_SIZE_DEF *fdef;
	SIZE leading;
	TEXT t;
	RECT r;
	typedef struct
	{
		TEXT_ALIGN align;
		COORD basex;
	} col_format_t;
	col_format_t colfmt[MAX_TEXT_COLS];

	if (*istr < 0 || *istr >= GetStringTableCount (CreditsTab))
	{	// check if next one is within range
		int next_s = *istr + dir;

		if (next_s < 0 || next_s >= GetStringTableCount (CreditsTab))
			return 0;

		*istr = next_s;
	}

	// skip empty lines
	while (*istr >= 0 && *istr < GetStringTableCount (CreditsTab))
	{
		pStr = GetStringAddress (
				SetAbsStringTableIndex (CreditsTab, *istr));
		*istr += dir;
		if (pStr && *pStr != '\0')
			break;
	}

	if (!pStr || *pStr == '\0')
		return 0;
	
	if (2 != sscanf (pStr, "%d %31s %n", &size, salign, &scaned)
			|| size <= 0)
		return 0;
	pStr += scaned;
	
	utf8StringCopy (buf, sizeof (buf), pStr);
	rows = ParseTextLines (TextLines, MAX_TEXT_LINES, buf);
	if (rows == 0)
		return 0;
	// parse text columns
	for (i = 0, cnt = rows; i < rows; ++i)
	{
		char *nextcol;
		int icol;

		// we abuse the baseline here, but only a tiny bit
		// every line starts at col 0
		TextLines[i].baseline.x = 0;
		TextLines[i].baseline.y = i + 1;

		for (icol = 1, nextcol = strchr (buf + (TextLines[i].pStr - buf), '\t');
				icol < MAX_TEXT_COLS && nextcol;
				++icol, nextcol = strchr (nextcol, '\t'))
		{
			*nextcol = '\0';
			++nextcol;

			if (cnt < MAX_TEXT_LINES)
			{
				TextLines[cnt].pStr = nextcol;
				TextLines[cnt].CharCount = ~0;
				TextLines[cnt].baseline.x = icol;
				TextLines[cnt].baseline.y = i + 1;
				++cnt;
			}
		}
	}

	// init alignments
	for (i = 0; i < MAX_TEXT_COLS; ++i)
	{
		colfmt[i].align = ALIGN_LEFT;
		colfmt[i].basex = CreditsExtent.width / 64;
	}

	// find the right font
	for (fdef = CreditsFont; fdef->size && size > fdef->size; ++fdef)
		;
	if (!fdef->size)
		return 0;

	t.align = ALIGN_LEFT;
	t.baseline.x = 100; // any value will do
	t.baseline.y = 100; // any value will do
	t.pStr = " ";
	t.CharCount = 1;

	OldContext = SetContext (TempContext);

	// get font dimensions
	SetContextFont (fdef->font);
	GetContextFontLeading (&leading);
	// get left/right margin
	TextRect (&t, &r, NULL);

	// parse text column alignment
	for (i = 0, scol = strtok (salign, ",");
			scol && i < MAX_TEXT_COLS;
			++i, scol = strtok (NULL, ","))
	{
		char c;
		int x;
		int n;

		// default
		colfmt[i].align = ALIGN_LEFT;
		colfmt[i].basex = r.extent.width;

		n = sscanf (scol, "%c/%d", &c, &x);
		if (n < 1)
		{	// DOES NOT COMPUTE! :)
			continue;
		}

		switch (c)
		{
			case 'L':
				colfmt[i].align = ALIGN_LEFT;
				if (n >= 2)
					colfmt[i].basex = x;
				break;
			case 'C':
				colfmt[i].align = ALIGN_CENTER;
				if (n >= 2)
					colfmt[i].basex = x;
				else
					colfmt[i].basex = CreditsExtent.width / 2;
				break;
			case 'R':
				colfmt[i].align = ALIGN_RIGHT;
				if (n >= 2)
					colfmt[i].basex = x;
				else
					colfmt[i].basex = CreditsExtent.width - r.extent.width;
				break;
		}
	}

	for (i = 0; i < cnt; ++i)
	{	
		// baseline contains coords in row/col quantities
		col_format_t *fmt = colfmt + TextLines[i].baseline.x;
		
		TextLines[i].align = fmt->align;
		TextLines[i].baseline.x = fmt->basex;
		TextLines[i].baseline.y *= leading;
	}

	f = Credits_MakeTransFrame (CreditsExtent.width, leading * rows + (leading >> 1),
			BackColor);
	OldFrame = SetContextFGFrame (f);
	// draw text
	SetContextForeGroundColor (ForeColor);
	for (i = 0; i < cnt; ++i)
		font_DrawText (TextLines + i);
	
	SetContextFGFrame (OldFrame);
	SetContext (OldContext);

	return f;
}

inline int
frameIndex (int index)
{
	// Make sure index is positive before %
	return (index + MAX_CREDIT_FRAMES) % MAX_CREDIT_FRAMES;
}

void
RenderCreditsScreen (CONTEXT targetContext)
{
	CONTEXT oldContext;
	STAMP s;
	int i;

	oldContext = SetContext (targetContext);
	// draw background
	s.origin.x = 0;
	s.origin.y = 0;
	s.frame = CreditsBack;
	DrawStamp (&s);
	
	// draw text frames
	s.origin.y = -curFrameOfs;
	for (i = firstFrame; i != lastFrame; i = frameIndex (i + 1))
	{
		RECT fr;

		s.frame = textFrames[i].frame;
		DrawStamp (&s);
		GetFrameRect (s.frame, &fr);
		s.origin.y += fr.extent.height;
	}

	if (OutTakesRunning)
	{	// Cut out the Outtakes rect
		SetContextForeGroundColor (TRANS_COLOR);
		DrawFilledRectangle (&window_);
	}
	
	SetContext (oldContext);
}

void
InitCredits (void)
{
	RECT ctxRect;
	CONTEXT oldContext;
	FRAME targetFrame;

	memset (textFrames, 0, sizeof textFrames);

	LocalContext = CreateContext ("Credits.LocalContext");
	DrawContext = CreateContext ("Credits.DrawContext");
    if (!LocalContext || !DrawContext) throw std::runtime_error("could not create credits contexts");

	targetFrame = GetContextFGFrame ();
	GetContextClipRect (&ctxRect);
	CreditsExtent = ctxRect.extent;

	// prep our local context
	oldContext = SetContext (LocalContext);
	// Local screen copy. We draw everything to this frame, then cut
	// the Outtakes rect out and draw this frame to the screen.
	CreditsFrame = Credits_MakeTransFrame (CreditsExtent.width,
			CreditsExtent.height, TRANS_COLOR);
	SetContextFGFrame (CreditsFrame);
	
	// The first credits frame is fake, the height of the screen,
	// so that the credits can roll in from the bottom
	textFrames[0].frame = Credits_MakeTransFrame (1, CreditsExtent.height,
			TRANS_COLOR);
	textFrames[0].strIndex = -1;
	firstFrame = 0;
	lastFrame = firstFrame + 1;

	totalHeight = GetFrameHeight (textFrames[0].frame);
	curFrameOfs = 0;
	
	// We use an own screen draw context to avoid collisions
	SetContext (DrawContext);
	SetContextFGFrame (targetFrame);

	SetContext (oldContext);

	// Prepare the first screen frame
	RenderCreditsScreen (LocalContext);

	CreditsRate = CREDITS_BASE_RATE;
	CreditsRunning = TRUE;
}

void
freeCreditTextFrame (CreditTextFrame *tf)
{
	DestroyDrawable (ReleaseDrawable (tf->frame));
	tf->frame = NULL;
}

void
UninitCredits (void)
{
	DestroyContext (DrawContext);
	DrawContext = NULL;
	DestroyContext (LocalContext);
	LocalContext = NULL;

	// free remaining frames
	DestroyDrawable (ReleaseDrawable (CreditsFrame));
	CreditsFrame = NULL;
	for ( ; firstFrame != lastFrame; firstFrame = frameIndex (firstFrame + 1))
		freeCreditTextFrame (&textFrames[firstFrame]);
}

int
calcDeficitHeight (void)
{
	int i;
	int maxPos;

	maxPos = -curFrameOfs;
	for (i = firstFrame; i != lastFrame; i = frameIndex (i + 1))
	{
		RECT fr;

		GetFrameRect (textFrames[i].frame, &fr);
		maxPos += fr.extent.height;
	}

	return CreditsExtent.height - maxPos;
}

void
processCreditsFrame (void)
{
	// NextTime is visit-local, including repeated credit playback.
	TimeCount Now = GetTimeCounter ();

	if (Now >= NextTime)
	{
		RECT fr;
		CONTEXT OldContext;
		int rate, direction, dirstep;
		int deficitHeight;
		STAMP s;

		rate = abs (CreditsRate);
		if (rate != 0)
		{
			// scroll direction; forward or backward
			direction = CreditsRate / rate;
			// step in pixels
			dirstep = (rate + CREDITS_FRAME_RATE - 1) / CREDITS_FRAME_RATE;
			rate = ONE_SECOND * dirstep / rate;
			// step is also directional
			dirstep *= direction;
		}
		else
		{	// scroll stopped
			direction = 0;
			dirstep = 0;
			// one second interframe
			rate = ONE_SECOND;
		}

		NextTime = GetTimeCounter () + rate;

		// draw the credits
		//  comm animations play with contexts so we need to make
		//  sure the context is not desynced
		s.origin.x = 0;
		s.origin.y = 0;
		s.frame = CreditsFrame;

		OldContext = SetContext (DrawContext);
		DrawStamp (&s);
		SetContext (OldContext);
		FlushGraphics ();

		// prepare next screen frame
		deficitHeight = calcDeficitHeight ();
		curFrameOfs += dirstep;
		// cap scroll
		if (curFrameOfs < -(CreditsExtent.height / 20))
		{	// at the begining, deceleration
			if (CreditsRate < 0)
				CreditsRate -= CreditsRate / 10 - 1;
		}
		else if (deficitHeight > CreditsExtent.height / 25)
		{	// frame deficit -- credits almost over, deceleration
			if (CreditsRate > 0)
				CreditsRate -= CreditsRate / 10 + 1;

			CreditsRunning = (CreditsRate != 0);
		}
		else if (!CreditsRunning)
		{	// resumed
			CreditsRunning = TRUE;
		}

		if (firstFrame != lastFrame)
		{	// clean up frames that scrolled off the screen
			if (direction > 0)
			{	// forward scroll
				GetFrameRect (textFrames[firstFrame].frame, &fr);
				if (curFrameOfs >= fr.extent.height)
				{	// past this frame already
					totalHeight -= fr.extent.height;
					freeCreditTextFrame (&textFrames[firstFrame]);
					// next frame
					firstFrame = frameIndex (firstFrame + 1);
					curFrameOfs -= fr.extent.height;
				}
			}
			else if (direction < 0)
			{	// backward scroll
				int index = frameIndex (lastFrame - 1);
				int framePos;

				GetFrameRect (textFrames[index].frame, &fr);
				framePos = totalHeight - curFrameOfs - fr.extent.height;
				if (framePos >= CreditsExtent.height)
				{	// past this frame already
					lastFrame = index;
					totalHeight -= fr.extent.height;
					freeCreditTextFrame (&textFrames[lastFrame]);
				}
			}
		}

		// render new text frames if needed
		if (direction > 0)
		{	// forward scroll
			int next_s = 0;

			// get next string
			if (firstFrame != lastFrame)
				next_s = textFrames[frameIndex (lastFrame - 1)].strIndex + 1;

			while (totalHeight - curFrameOfs < CreditsExtent.height
					&& next_s < GetStringTableCount (CreditsTab))
			{
				CreditTextFrame *tf = &textFrames[lastFrame];

				tf->frame = Credits_RenderTextFrame (LocalContext, &next_s,
						direction, BLACK_COLOR, CREDITS_TEXT_COLOR);
				tf->strIndex = next_s - 1;
				if (tf->frame)
				{
					GetFrameRect (tf->frame, &fr);
					totalHeight += fr.extent.height;
					
					lastFrame = frameIndex (lastFrame + 1);
				}
			}
		}
		else if (direction < 0)
		{	// backward scroll
			int next_s = GetStringTableCount (CreditsTab) - 1;

			// get next string
			if (firstFrame != lastFrame)
				next_s = textFrames[firstFrame].strIndex - 1;
			
			while (curFrameOfs < 0 && next_s >= 0)
			{
				int index = frameIndex (firstFrame - 1);
				CreditTextFrame *tf = &textFrames[index];

				tf->frame = Credits_RenderTextFrame (LocalContext, &next_s,
						direction, BLACK_COLOR, CREDITS_TEXT_COLOR);
				tf->strIndex = next_s + 1;
				if (tf->frame)
				{
					GetFrameRect (tf->frame, &fr);
					totalHeight += fr.extent.height;
					
					firstFrame = index;
					curFrameOfs += fr.extent.height;
				}
			}
		}

		// draw next screen frame
		RenderCreditsScreen (LocalContext);
	}
}

BOOLEAN
LoadCredits (void)
{
	FONT_SIZE_DEF *fdef;

	CreditsTab = CaptureStringTable (LoadStringTable (CREDITS_STRTAB));
	if (!CreditsTab)
		return FALSE;
	CreditsBack = CaptureDrawable (LoadGraphic (CREDITS_BACK_ANIM));
    if (!CreditsBack) throw std::runtime_error("could not load credits background");
	// load fonts
	for (fdef = CreditsFont; fdef->size; ++fdef) {
		fdef->font = LoadFont (fdef->res);
        if (!fdef->font) throw std::runtime_error("could not load credits font");
    }

	return TRUE;
}

void
FreeCredits (void)
{
	FONT_SIZE_DEF *fdef;

	DestroyStringTable (ReleaseStringTable (CreditsTab));
	CreditsTab = NULL;
	
	DestroyDrawable (ReleaseDrawable (CreditsBack));
	CreditsBack = NULL;

	// free fonts
	for (fdef = CreditsFont; fdef->size; ++fdef)
	{
		DestroyFont (fdef->font);
		fdef->font = NULL;
	}
}

void
OutTakes (void)
{
static constexpr int NUM_OUTTAKES = 15;
	static constexpr CONVERSATION outtake_list[NUM_OUTTAKES] =
	{
		ZOQFOTPIK_CONVERSATION,
		TALKING_PET_CONVERSATION,
		ORZ_CONVERSATION,
		UTWIG_CONVERSATION,
		THRADD_CONVERSATION,
		SUPOX_CONVERSATION,
		SYREEN_CONVERSATION,
		SHOFIXTI_CONVERSATION,
		PKUNK_CONVERSATION,
		YEHAT_CONVERSATION,
		DRUUGE_CONVERSATION,
		URQUAN_CONVERSATION,
		VUX_CONVERSATION,
		BLACKURQ_CONVERSATION,
		ARILOU_CONVERSATION
	};

    struct OuttakeOptions {
        BOOLEAN subtitles = optSubtitles, slider = sliderDisabled, oscill = oscillDisabled;
        ~OuttakeOptions() { optSubtitles = subtitles; sliderDisabled = slider; oscillDisabled = oscill; }
    } options;
	int i = 0;

	// Outtakes have no voice tracks, so the subtitles are always on
	optSubtitles = TRUE;
	sliderDisabled = TRUE;
	oscillDisabled = TRUE;

	for (i = 0; (i < NUM_OUTTAKES) &&
			!cancelled(); i++)
	{
		SetCommIntroMode (CIM_CROSSFADE_WINDOW, 0);
		InitCommunication (outtake_list[i]);
	}


}

typedef struct
{
	// standard state required by DoInput
	BOOLEAN (*InputFunc) (void *pInputState);

	CreditsRuntime *owner;
	bool AllowCancel;
	BOOLEAN AllowSpeedChange;
	bool CloseWhenDone;
	DWORD CloseTimeOut;

} CREDITS_INPUT_STATE;

BOOLEAN
DoCreditsInput (void *pIS)
{
	CREDITS_INPUT_STATE *pCIS = (CREDITS_INPUT_STATE *) pIS;

	if (CreditsRunning)
	{	// cancel timeout if resumed (or just running)
		pCIS->CloseTimeOut = 0;
	}

	if (cancelled()
			|| (pCIS->AllowCancel &&
				(PulsedInputState.menu[KEY_MENU_SELECT] ||
				PulsedInputState.menu[KEY_MENU_CANCEL]))
		)
	{	// aborted
		return FALSE;
	}
	
	if (pCIS->AllowSpeedChange
			&& (PulsedInputState.menu[KEY_MENU_UP]
			|| PulsedInputState.menu[KEY_MENU_DOWN]))
	{	// speed adjustment
		int newrate = CreditsRate;
		int step = abs (CreditsRate) / 5 + 1;

		if (PulsedInputState.menu[KEY_MENU_DOWN])
			newrate += step;
		else if (PulsedInputState.menu[KEY_MENU_UP])
			newrate -= step;
		if (newrate < -CREDITS_MAX_RATE)
			newrate = -CREDITS_MAX_RATE;
		else if (newrate > CREDITS_MAX_RATE)
			newrate = CREDITS_MAX_RATE;
			
		CreditsRate = newrate;
	}

	if (!CreditsRunning)
	{	// always allow cancelling once credits run through
		pCIS->AllowCancel = TRUE;
	}

	if (!CreditsRunning && pCIS->CloseWhenDone)
	{	// auto-close controlled by timeout
		if (pCIS->CloseTimeOut == 0)
		{	// set timeout
			pCIS->CloseTimeOut = GetTimeCounter () + CREDITS_TIMEOUT;
		}
		else if (GetTimeCounter () > pCIS->CloseTimeOut)
		{	// all done!
			return FALSE;
		}
	}
	
	if (!CreditsRunning
			&& (PulsedInputState.menu[KEY_MENU_SELECT]
			|| PulsedInputState.menu[KEY_MENU_CANCEL]))
	{	// credits finished and exit requested
		return FALSE;
	}

	SleepThread (ONE_SECOND / CREDITS_FRAME_RATE);

	return TRUE;
}

static void
on_input_frame (void)
{
	activeCredits->processCreditsFrame ();
}

public:
void
run (BOOLEAN WithOuttakes)
{
	CREDITS_INPUT_STATE cis{};
	RECT screenRect;
	STAMP s;

	music_ = LoadMusic (CREDITS_MUSIC);

	SetContext (ScreenContext);
	SetContextClipRect (NULL);
	GetContextClipRect (&screenRect);
	SetContextBackGroundColor (BLACK_COLOR);
	ClearDrawable ();

	if (!LoadCredits ())
		return;

	// Fade in the background
	s.origin.x = 0;
	s.origin.y = 0;
	s.frame = CreditsBack;
	DrawStamp (&s);
	FadeScreen (FadeAllToColor, ONE_SECOND / 2);

	// set the position of outtakes comm
    struct WindowRestore {
        RECT *window = nullptr;
        RECT saved{};
        ~WindowRestore() { if (window) *window = saved; }
    } restore;
    if (WithOuttakes) {
        restore.window = GetConversationWindow();
        restore.saved = *restore.window;
        restore.window->corner.x = (screenRect.extent.width - restore.window->extent.width) / 2;
        restore.window->corner.y = 5;
        window_ = *restore.window;
    }
	
	InitCredits ();
	SetInputCallback (on_input_frame);

	if (WithOuttakes)
	{
		OutTakesRunning = TRUE;
		OutTakes ();
		OutTakesRunning = FALSE;
	}
	
	if (!cancelled())
	{
		if (music_)
			PlayMusic (music_, TRUE, 1);

		// nothing to do now but wait until credits
		//  are done or canceled by user
		cis.owner = this;
        cis.InputFunc = [](void *input) -> BOOLEAN {
            auto &state = *static_cast<CREDITS_INPUT_STATE *>(input);
            return state.owner->DoCreditsInput(input);
        };
		cis.AllowCancel = !WithOuttakes;
		cis.CloseWhenDone = !WithOuttakes;
		cis.AllowSpeedChange = TRUE;
		SetMenuSounds (MENU_SOUND_NONE, MENU_SOUND_NONE);
		DoInput (&cis, TRUE);
	}

	SetInputCallback (NULL);
	FadeMusic (0, ONE_SECOND / 2);


	SetContext (ScreenContext);
	SleepThreadUntil (FadeScreen (FadeAllToBlack, ONE_SECOND / 2));
	FlushColorXForms ();

}
};
} // namespace

extern "C" void Credits(BOOLEAN withOuttakes) {
    auto *view = uqm::detail::boundRuntime();
    auto &session = view && view->owner ? *view->owner : uqm::gameSession();
    CreditsRuntime runtime{session};
    runtime.run(withOuttakes);
}
